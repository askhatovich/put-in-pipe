// Copyright (C) 2025-2026  Roman Lyubimov
// SPDX-License-Identifier: GPL-3.0-or-later
// For full license text, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#include "transfersession.h"
#include "transfersessionlist.h"
#include "clientlist.h"
#include "serializableevent.h"
#include "crowlib/crow/utility.h"
#include "config/config.h"

#include "log.h"

#include <mutex>

TransferSession::TransferSession(std::shared_ptr<Client> sender, const std::string& sessionId,
                                 asio::io_context& ioContext, const Options& options)
    : m_id(sessionId)
    , m_dataSender(sender)
    , m_ioContext(ioContext)
    , m_options(options)
{
    PLOG_DEBUG << "Session " << sessionId << " created"
               << (options.autoDropFreezeOnFirstChunk ? " [auto-drop freeze]" : "");
}

TransferSession::~TransferSession()
{
    PLOG_INFO << "Session " << m_id << " destroyed";

    // Each subscribed client's update() handler sends the "complete" event
    // via sendTextWithAck and, on ACK (or fallback timer), removes itself
    // from ClientList. No session-wide coordination required.
    Publisher<Event::TransferSession>::notifySubscribers(Event::TransferSession::complete, m_completeType);
}

void TransferSession::initTimers(std::shared_ptr<TransferSession> me)
{
    std::weak_ptr<TransferSession> weakSelf = me;
    m_initialFreezeTimer = std::make_unique<TimerCallback>(m_ioContext,
                     [weakSelf]() {
                        if (auto sharedSelf = weakSelf.lock()) {
                            sharedSelf->dropInitialChunksFreeze();
                        }
                     },
                    TimerCallback::Duration(Config::instance().transferSessionMaxInitialFreezeDuration()));

    m_initialFreezeTimer->start();

    PLOG_DEBUG << "Session " << m_id << " timers initialized";
}

const TransferSession::FileInfo TransferSession::fileInfo() const
{
    std::shared_lock lock (m_fileInfoMutex);
    return m_fileInfo;
}

bool TransferSession::addReceiver(std::shared_ptr<Client> client)
{
    if (client == nullptr) return false;

    std::unique_lock lock (m_receiversMutex);

    for (const auto& c: m_dataReceivers)
    {
        if (auto spc = c.lock())
        {
            if (spc->id() == client->id())
            {
                return false;
            }
        }
    }

    if (not m_buffer.addNewToExpectedConsumers(client->publicId()))
    {
        return false;
    }

    try {
        for (auto& anotherReceiver: m_dataReceivers)
        {
            if (auto spAnotherReceiver = anotherReceiver.lock())
            {
                if (spAnotherReceiver->publicId() == client->publicId())
                {
                    PLOG_WARNING << "TransferSession::addReceiver() anomaly: client "
                                 << client->name() << "/" << client->id() << " already exists";
                    continue;
                }

                client->Publisher<Event::ClientsDirect>::addSubscriber(spAnotherReceiver);
                spAnotherReceiver->Publisher<Event::ClientsDirect>::addSubscriber(client);
            }
        }

        // Cross-subscribe sender ↔ new receiver for online/offline events
        if (auto spSender = m_dataSender.lock())
        {
            client->Publisher<Event::ClientsDirect>::addSubscriber(spSender);
            spSender->Publisher<Event::ClientsDirect>::addSubscriber(client);
        }

        m_dataReceivers.push_back(client);

        client->Publisher<Event::ClientInternal>::addSubscriber(shared_from_this());

        Publisher<Event::TransferSession>::addSubscriber(client);
        Publisher<Event::TransferSession>::notifySubscribers(Event::TransferSession::newReceiver, client);
    } catch (...) {
        std::list<size_t> removedChunks;
        m_buffer.removeOneFromExpectedConsumers(client->publicId(), removedChunks);
        throw;
    }

    return true;
}

void TransferSession::removeReceiver(const std::string &publicId)
{
    PLOG_INFO << "Session " << m_id << ": removing receiver " << publicId;

    /*
     * Collect the client shared_ptr under lock, then release the lock
     * before destroying the client — the client destructor notifies
     * subscribers which can re-enter removeReceiver, causing deadlock
     * if the mutex is still held.
     */

    std::shared_ptr<Client> removedClient;
    bool noReceiversLeft = false;

    {
        std::unique_lock lock (m_receiversMutex);

        for (auto iter = m_dataReceivers.begin(); iter != m_dataReceivers.end(); )
        {
            if (auto spReceiver = iter->lock())
            {
                if (spReceiver->publicId() == publicId)
                {
                    removedClient = spReceiver;
                    iter = m_dataReceivers.erase(iter);
                    continue;
                }
                ++iter;
            }
            else
            {
                // Expired weak_ptr — client already destroyed, clean up
                iter = m_dataReceivers.erase(iter);
            }
        }

        noReceiversLeft = m_dataReceivers.empty();
    }

    // Remove from expected consumers and sanitize buffer
    std::list<size_t> removedChunks;
    m_buffer.removeOneFromExpectedConsumers(publicId, removedChunks);
    if (not removedChunks.empty())
    {
        std::string idxs;
        for (auto id : removedChunks) { if (!idxs.empty()) idxs += ','; idxs += std::to_string(id); }
        PLOG_INFO << "[sess=" << m_id << "] removeReceiver " << publicId
                  << " triggered sanitize of chunks [" << idxs << "]";
        Publisher<Event::TransferSession>::notifySubscribers(Event::TransferSession::chunksWasRemoved, removedChunks);
    }

    // Notify the removed client with an ACK-required "kicked" event;
    // the client removes itself from ClientList on ACK or fallback timer.
    // Done outside the receivers mutex to avoid deadlock in the ACK callback.
    if (removedClient)
    {
        const std::string removedInternalId = removedClient->id();
        removedClient->sendTextWithAck(
            SerializableEvent::Kicked{}.json(),
            [removedInternalId]() { ClientList::instanse().remove(removedInternalId); }
        );
        removedClient.reset();
    }

    Publisher<Event::TransferSession>::notifySubscribers(Event::TransferSession::receiverRemoved, publicId);

    // Terminate when no receivers are left. Three cases:
    //
    // 1. Auto-drop mode (sender opted into unattended operation) — terminate
    //    immediately with "ok" regardless of buffer state. The sender asked
    //    for fire-and-forget; whatever was delivered to the (now departed)
    //    receivers is considered a successful outcome.
    //
    // 2. Full transfer completed (EOF + empty buffer) before the last
    //    receiver left — terminate with "ok".
    //
    // 3. Chunks were removed or freeze already dropped, but the transfer
    //    wasn't finished — terminate with "no_receivers".
    //
    // If freeze is still active and no chunks were removed, keep the session
    // alive so another receiver can still join (default mode only).
    if (noReceiversLeft)
    {
        if (m_options.autoDropFreezeOnFirstChunk)
        {
            PLOG_INFO << "Session " << m_id << ": no receivers left (auto-drop mode), terminating (ok)";
            m_completeType = Event::Data::TransferSessionCompleteType::ok;
            TransferSessionList::instanse().remove(m_id);
            return;
        }
        if (m_buffer.someChunksWasRemoved() or not m_buffer.initialChunksFreezing())
        {
            if (m_buffer.eof() and m_buffer.chunkCount() == 0)
            {
                PLOG_INFO << "Session " << m_id << ": last receiver left after full transfer, terminating (ok)";
                m_completeType = Event::Data::TransferSessionCompleteType::ok;
            }
            else
            {
                PLOG_INFO << "Session " << m_id << ": no receivers left, terminating (no_receivers)";
                m_completeType = Event::Data::TransferSessionCompleteType::noReceivers;
            }
            TransferSessionList::instanse().remove(m_id);
            return;
        }
    }
}

bool TransferSession::setFileInfo(const FileInfo &info)
{
    if (info.name.empty() or info.name.size() > FileInfo::NAME_MAX_LENGTH)
    {
        return false;
    }

    if (info.size == 0)
    {
        return false;
    }

    std::unique_lock lock (m_fileInfoMutex);

    m_fileInfo = info;

    // Remove path separators and control characters, preserving UTF-8 multibyte sequences
    std::string& name = m_fileInfo.name;
    for (size_t i = 0; i < name.size(); ++i)
    {
        unsigned char c = name[i];
        if (c < 0x20 || c == '/' || c == '\\' || c == ':' || c == '*'
            || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
        {
            name[i] = '_';
        }
    }

    Publisher<Event::TransferSession>::notifySubscribers(Event::TransferSession::fileInfoUpdated, m_fileInfo);

    return true;
}

bool TransferSession::addChunk(const std::string &binaryData)
{
    const auto oldAllowed = m_buffer.newChunkIsAllowed();

    const auto newIndex = m_buffer.addChunk(binaryData);
    if (newIndex == 0)
    {
        return false;
    }

    PLOG_DEBUG << "[sess=" << m_id << "] addChunk -> index=" << newIndex
               << " size=" << binaryData.size()
               << " bufferCount=" << m_buffer.chunkCount();

    Event::Data::ChunkInfo info;
    info.index = newIndex;
    info.size = binaryData.size();

    Publisher<Event::TransferSession>::notifySubscribers(Event::TransferSession::newChunkIsAvailable, info);

    const auto newAllowed = m_buffer.newChunkIsAllowed();
    if (oldAllowed != newAllowed)
    {
        Publisher<Event::TransferSessionForSender>::notifySubscribers(Event::TransferSessionForSender::newChunkIsAllowed, newAllowed);
    }
    Publisher<Event::TransferSession>::notifySubscribers(Event::TransferSession::bytesInUpdated, m_buffer.bytesIn());

    return true;
}

const std::shared_ptr<const std::vector<uint8_t>> TransferSession::getChunk(size_t index, std::shared_ptr<Client> client)
{
    if (client == nullptr)
    {
        PLOG_WARNING << "TransferSession::getChunk(): client is nullptr";
        return nullptr;
    }

    const auto data = m_buffer[index];
    if (data == nullptr)
    {
        return nullptr;
    }

    Event::Data::TransferSessionDownloadInfo info;
    info.publicId = client->publicId();
    info.chunkId = index;

    Publisher<Event::TransferSession>::notifySubscribers(Event::TransferSession::chunkDownloadStarted, info);

    return data;
}

void TransferSession::setChunkAsReceived(size_t index, std::shared_ptr<Client> client)
{
    if (client == nullptr)
    {
        PLOG_WARNING << "TransferSession::setChunkAsReceived(): client is nullptr";
        return;
    }

    // Auto-drop the initial freeze as soon as any receiver confirms any
    // chunk. The atomic flag guarantees we only call dropInitialChunksFreeze
    // once, even under concurrent confirmations.
    if (m_options.autoDropFreezeOnFirstChunk and m_buffer.initialChunksFreezing())
    {
        bool expected = false;
        if (m_autoDropFreezeFired.compare_exchange_strong(expected, true))
        {
            PLOG_INFO << "Session " << m_id << ": auto-dropping initial freeze on first confirm";
            dropInitialChunksFreeze();
        }
    }

    const auto chunk = m_buffer[index];
    if (chunk)
    {
        /*
         * The size of the overhead is adjusted so that users can be informed
         * of the practical size of the useful data (for displaying the progress bar, for example)
         */
        client->incrementReceived(chunk->size());
    }

    const auto oldAllowed = m_buffer.newChunkIsAllowed();
    std::list<size_t> removedChunks;

    if (not m_buffer.setChunkAsReceived(index, removedChunks)) return;

    const auto newCount = m_buffer.chunkCount();

    if (not removedChunks.empty())
    {
        // Per-confirm diagnostic — kept at DEBUG to avoid spamming the
        // journal on normal high-volume transfers (one line per chunk).
        IF_PLOG(plog::debug)
        {
            std::string idxs;
            for (auto id : removedChunks) { if (!idxs.empty()) idxs += ','; idxs += std::to_string(id); }
            PLOG_DEBUG << "[sess=" << m_id << "] confirm chunk " << index
                       << " by " << client->publicId() << " -> sanitized [" << idxs
                       << "] bufferCount=" << newCount;
        }
        Publisher<Event::TransferSession>::notifySubscribers(Event::TransferSession::chunksWasRemoved, removedChunks);
    }
    else
    {
        PLOG_DEBUG << "[sess=" << m_id << "] confirm chunk " << index
                   << " by " << client->publicId() << " (no removal, bufferCount=" << newCount << ")";
    }

    const auto newAllowed = m_buffer.newChunkIsAllowed();
    if (oldAllowed != newAllowed)
    {
        Publisher<Event::TransferSessionForSender>::notifySubscribers(Event::TransferSessionForSender::newChunkIsAllowed, newAllowed);
    }
    Publisher<Event::TransferSession>::notifySubscribers(Event::TransferSession::bytesOutUpdated, m_buffer.bytesOut());

    Event::Data::TransferSessionDownloadInfo info;
    info.publicId = client->publicId();
    info.chunkId = index;

    Publisher<Event::TransferSession>::notifySubscribers(Event::TransferSession::chunkDownloadFinished, info);

    if (newCount == 0 and m_buffer.eof())
    {
        PLOG_INFO << "Session " << m_id << ": transfer complete";
        TransferSessionList::instanse().remove(m_id);
    }
}

void TransferSession::manualTerminate()
{
    PLOG_INFO << "Session " << m_id << ": manually terminated by sender";
    m_completeType = Event::Data::TransferSessionCompleteType::senderIsGone;
    TransferSessionList::instanse().remove(m_id);
}

void TransferSession::setTimedout()
{
    m_completeType = Event::Data::TransferSessionCompleteType::timeout;
}

void TransferSession::setEndOfFile()
{
    if (m_buffer.eof()) return;
    m_buffer.setEndOfFile();

    Publisher<Event::TransferSession>::notifySubscribers(Event::TransferSession::fileUploadFinished, nullptr);

    // If all chunks were already confirmed before EOF arrived, the completion
    // check in setChunkAsReceived would have missed (eof was false then).
    // Re-check here so the session terminates deterministically.
    if (m_buffer.chunkCount() == 0)
    {
        PLOG_INFO << "Session " << m_id << ": transfer complete (EOF after all chunks confirmed)";
        TransferSessionList::instanse().remove(m_id);
    }
}

void TransferSession::dropInitialChunksFreeze()
{
    std::list<size_t> removedChunks;
    if (not m_buffer.setInitialChunksFreezingDropped(removedChunks))
    {
        return;
    }

    {
        std::string idxs;
        for (auto id : removedChunks) { if (!idxs.empty()) idxs += ','; idxs += std::to_string(id); }
        PLOG_INFO << "[sess=" << m_id << "] initial freeze dropped, " << removedChunks.size()
                  << " chunks sanitized: [" << idxs << "]";
    }

    bool shouldRemove = false;
    {
        std::shared_lock lock (m_receiversMutex);
        if (m_dataReceivers.empty())
        {
            PLOG_INFO << "Session " << m_id << ": no receivers after freeze, terminating";
            m_completeType = Event::Data::TransferSessionCompleteType::noReceivers;
            shouldRemove = true;
        }
        else if (m_fileInfo.name.empty())
        {
            PLOG_INFO << "Session " << m_id << ": file info not set after freeze, terminating";
            m_completeType = Event::Data::TransferSessionCompleteType::senderIsGone;
            shouldRemove = true;
        }
    }

    if (shouldRemove)
    {
        TransferSessionList::instanse().remove(m_id);
        return;
    }

    if (not removedChunks.empty())
    {
        Publisher<Event::TransferSession>::notifySubscribers(Event::TransferSession::chunksWasRemoved, removedChunks);
    }

    const auto newAllowed = m_buffer.newChunkIsAllowed();
    Publisher<Event::TransferSessionForSender>::notifySubscribers(Event::TransferSessionForSender::newChunkIsAllowed, newAllowed);

    Publisher<Event::TransferSession>::notifySubscribers(Event::TransferSession::chunksAreUnfrozen, nullptr);

    // Check if transfer is already complete (all chunks confirmed during freeze)
    if (m_buffer.chunkCount() == 0 and m_buffer.eof())
    {
        PLOG_INFO << "Session " << m_id << ": transfer complete (all confirmed during freeze)";
        TransferSessionList::instanse().remove(m_id);
    }
}

std::chrono::seconds TransferSession::remainingUntilAutoDropInitialFreeze() const
{
    if (m_initialFreezeTimer == nullptr) return std::chrono::seconds(0);

    return m_initialFreezeTimer->timeRemaining();
}

void TransferSession::update(Event::ClientInternal event, std::any data)
{
    if (event == Event::ClientInternal::destroyed)
    {
        try {
            const std::string publicId = std::any_cast<std::string>(data);
            const auto spSender = m_dataSender.lock();
            if (spSender == nullptr or spSender->publicId() == publicId)
            {
                if (m_buffer.eof())
                {
                    // File fully uploaded. If no receivers left, terminate —
                    // otherwise let session continue for remaining receivers.
                    std::shared_lock lock(m_receiversMutex);
                    if (m_dataReceivers.empty())
                    {
                        PLOG_INFO << "Session " << m_id << ": sender left after upload, no receivers, terminating";
                        m_completeType = Event::Data::TransferSessionCompleteType::noReceivers;
                        lock.unlock();
                        TransferSessionList::instanse().remove(m_id);
                    }
                    return;
                }

                PLOG_INFO << "Session " << m_id << ": sender disconnected before upload finished, terminating";
                m_completeType = Event::Data::TransferSessionCompleteType::senderIsGone;
                TransferSessionList::instanse().remove(m_id);
                return;
            }
            removeReceiver(publicId);
        } catch (const std::bad_any_cast& e) {
            PLOG_ERROR << "TransferSession::update - Event::ClientInternal::destroyed - expected std::string: " << e.what();
            return;
        }
    }
    else
    {
        PLOG_WARNING << "TransferSession::update(Event::ClientInternal) unknown event";
    }
}
