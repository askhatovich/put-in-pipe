// Copyright (C) 2025  Roman Lyubimov
// SPDX-License-Identifier: GPL-3.0-or-later
// For full license text, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#include "transfersession.h"
#include "transfersessionlist.h"
#include "clientlist.h"
#include "crowlib/crow/utility.h"
#include "config/config.h"

#include <iostream>
#include <mutex>

TransferSession::TransferSession(std::shared_ptr<Client> sender, const std::string& sessionId, asio::io_context& ioContext)
    : m_id(sessionId)
    , m_dataSender(sender)
    , m_ioContext(ioContext)
{
    std::cerr << "Session " << sessionId << " created" << std::endl; // DEBUG
}

TransferSession::~TransferSession()
{
    std::cerr << "~Session " << m_id << " destructing..." << std::endl; // DEBUG

    Publisher<Event::TransferSession>::notifySubscribers(Event::TransferSession::complete, m_completeType);

    for (auto& receiver: m_dataReceivers)
    {
        if (auto sp = receiver.lock())
        {
            ClientList::instanse().remove(sp->id());
        }
    }

    if (auto sp = m_dataSender.lock())
    {
        ClientList::instanse().remove(sp->id());
    }

    std::cerr << "~Session " << m_id << " destructed" << std::endl; // DEBUG
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

    std::cerr << "Session " << m_id << " initTimers() called" << std::endl; // DEBUG
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

    for (auto& anotherReceiver: m_dataReceivers)
    {
        if (auto spAnotherReceiver = anotherReceiver.lock())
        {
            if (spAnotherReceiver->publicId() == client->publicId())
            {
                std::cerr << "TransferSession::addReceiver() anomaly: client "
                          << client->name() << "/" << client->id() << " already exists" << std::endl;
                continue;
            }

            client->Publisher<Event::ClientsDirect>::addSubscriber(spAnotherReceiver);
            spAnotherReceiver->Publisher<Event::ClientsDirect>::addSubscriber(client);
        }
    }

    m_dataReceivers.push_back(client);

    client->Publisher<Event::ClientInternal>::addSubscriber(shared_from_this());

    Publisher<Event::TransferSession>::addSubscriber(client);
    Publisher<Event::TransferSession>::notifySubscribers(Event::TransferSession::newReceiver, client);

    return true;
}

void TransferSession::removeReceiver(const std::string &publicId)
{
    std::cout << "TransferSession::removeReceiver 1 " << publicId << std::endl; // DEBUG
    std::unique_lock lock (m_receiversMutex);

    /*
     * If the event was triggered manually, the client will be explicitly deleted.
     * If the event is triggered after the natural deletion of the client,
     * the others will simply receive a notification.
     */

    for (auto iter = m_dataReceivers.begin(), end = m_dataReceivers.end(); iter != end; ++iter)
    {
        if (auto spReceiver = iter->lock())
        {
            if (spReceiver->publicId() != publicId)
            {
                continue;
            }

            m_dataReceivers.erase(iter);
            ClientList::instanse().remove(spReceiver->id());
            std::cout << "TransferSession::removeReceiver 2 " << publicId << std::endl; // DEBUG

            break; // no any job after iter erase!
        }
    }

    if (m_dataReceivers.empty() and m_buffer.someChunksWasRemoved())
    {
        /*
         * All recipients have left, new ones are not possible,
         * because the initial part of the file has already been lost.
         */

        m_completeType = Event::Data::TransferSessionCompleteType::noReceivers;
        TransferSessionList::instanse().remove(m_id);
        return;
    }

    std::list<size_t> removedChunks;
    m_buffer.removeOneFromExpectedConsumers(publicId, removedChunks);
    if (not removedChunks.empty())
    {
        Publisher<Event::TransferSession>::notifySubscribers(Event::TransferSession::chunksWasRemoved, removedChunks);
    }
    Publisher<Event::TransferSession>::notifySubscribers(Event::TransferSession::receiverRemoved, publicId);
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
    crow::utility::sanitize_filename(m_fileInfo.name);

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
        std::cerr << "TransferSession::getChunk() anomaly: client is nullptr" << std::endl;
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
        std::cerr << "TransferSession::setChunkAsReceived() anomaly: client is nullptr" << std::endl;
        return;
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
        Publisher<Event::TransferSession>::notifySubscribers(Event::TransferSession::chunksWasRemoved, removedChunks);
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
        TransferSessionList::instanse().remove(m_id);
    }
}

void TransferSession::manualTerminate()
{
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
}

void TransferSession::dropInitialChunksFreeze()
{
    if (not m_buffer.setInitialChunksFreezingDropped())
    {
        return;
    }

    std::shared_lock lock (m_receiversMutex);
    if (m_dataReceivers.empty())
    {
        m_completeType = Event::Data::TransferSessionCompleteType::noReceivers;
        TransferSessionList::instanse().remove(m_id);
        return;
    }
    if (m_fileInfo.name.empty())
    {
        m_completeType = Event::Data::TransferSessionCompleteType::senderIsGone;
        TransferSessionList::instanse().remove(m_id);
        return;
    }

    Publisher<Event::TransferSession>::notifySubscribers(Event::TransferSession::chunksAreUnfrozen, nullptr);
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
                    // file fully uploaded - sender's fault is ignored
                    return;
                }

                // data sender (creator of this session) is gone
                m_completeType = Event::Data::TransferSessionCompleteType::senderIsGone;
                TransferSessionList::instanse().remove(m_id);
                return;
            }
            removeReceiver(publicId);
        } catch (const std::bad_any_cast& e) {
            std::cerr << "TransferSession::update - Event::ClientInternal::destroyed - expected std::string: " << e.what() << std::endl;
            return;
        }
    }
    else
    {
        std::cerr << "TransferSession::update(Event::ClientInternal) unknown event" << std::endl;
    }
}
