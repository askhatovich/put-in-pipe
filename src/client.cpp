// Copyright (C) 2025-2026  Roman Lyubimov
// SPDX-License-Identifier: GPL-3.0-or-later
// For full license text, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#include "client.h"
#include "clientlist.h"
#include "captcha/skaptcha_tools.h"
#include "serializableevent.h"
#include "transfersession.h"
#include "config/config.h"
#include "log.h"

#include <mutex>

Client::Client(const std::string &id, asio::io_context& ioContext, std::function<void()> onTimeout) :
    m_id(id),
    m_publicId(skaptcha_tools::crypto::HashSignature::instance().sign(m_id)),
    m_wsTimeoutTimer(ioContext, onTimeout, TimerCallback::Duration(Config::instance().clientTimeout())),
    m_ioContext(ioContext)
{
    PLOG_DEBUG << "Client " << m_id << " created";
    m_wsTimeoutTimer.start();
}

void Client::sendTextWithAck(const std::string& eventJson,
                             std::function<void()> onAck,
                             std::chrono::milliseconds fallback)
{
    const uint64_t id = m_nextAckId.fetch_add(1);

    // Inject "id":<n> right after the opening brace. All event JSON
    // produced by SerializableEvent begins with '{', so this is safe.
    std::string withId;
    withId.reserve(eventJson.size() + 32);
    withId.push_back('{');
    withId.append("\"id\":");
    withId.append(std::to_string(id));
    if (eventJson.size() > 2)
    {
        withId.push_back(',');
        withId.append(eventJson, 1, std::string::npos);
    }
    else
    {
        withId.append(eventJson, 1, std::string::npos);
    }

    auto timer = std::make_unique<asio::steady_timer>(m_ioContext);
    timer->expires_after(fallback);

    // Timer handler looks the client up via ClientList (which owns the
    // shared_ptr). This avoids enable_shared_from_this, which is
    // ambiguous under Client's multiple Subscriber<T> inheritance.
    const std::string myInternalId = m_id;

    {
        std::lock_guard lock(m_pendingAcksMutex);
        m_pendingAcks.emplace(id, PendingAck{std::move(onAck), std::move(timer)});
        auto it = m_pendingAcks.find(id);
        it->second.fallback->async_wait([myInternalId, id](const asio::error_code& ec) {
            if (ec) return; // cancelled — ACK arrived first or client destroyed
            auto client = ClientList::instanse().get(myInternalId);
            if (!client) return; // client already removed
            client->resolveAck(id);
        });
    }

    if (auto sp = m_webSocketConnection.lock())
    {
        sp->sendText(withId);
    }
    else
    {
        // No WS — fire the callback immediately via the fallback path.
        resolveAck(id);
    }
}

void Client::processAck(uint64_t ackId)
{
    resolveAck(ackId);
}

void Client::resolveAck(uint64_t id)
{
    std::function<void()> cb;
    {
        std::lock_guard lock(m_pendingAcksMutex);
        auto it = m_pendingAcks.find(id);
        if (it == m_pendingAcks.end()) return;
        cb = std::move(it->second.callback);
        // Cancel timer if still pending; ignore errors (it may have fired already).
        if (it->second.fallback)
        {
            asio::error_code ignore;
            it->second.fallback->cancel(ignore);
        }
        m_pendingAcks.erase(it);
    }
    if (cb) cb();
}

void Client::onWebSocketDisconnected()
{
    {
        std::unique_lock lock (m_mutex);
        m_webSocketConnection.reset();
        m_wsTimeoutTimer.start();
    }

    Publisher<Event::ClientsDirect>::notifySubscribers(Event::ClientsDirect::disconnected, m_publicId);
}

void Client::onWebSocketConnected(std::shared_ptr<WebSocketConnection> connection)
{
    {
        std::unique_lock lock (m_mutex);
        m_webSocketConnection = connection;
        m_wsTimeoutTimer.stop();
    }

    Publisher<Event::ClientsDirect>::notifySubscribers(Event::ClientsDirect::connected, m_publicId);
}

std::string Client::joinedSession() const
{
    std::shared_lock lock (m_mutex);
    return m_joinedSession;
}

bool Client::joinSession(const std::string &id)
{
    if (id.empty()) return false;

    std::unique_lock lock (m_mutex);
    if (not m_joinedSession.empty())
    {
        return false;
    }
    m_joinedSession = id;
    return true;
}

void Client::resetWsTimeoutTimerIfItActive()
{
    std::unique_lock lock (m_mutex);

    if (m_wsTimeoutTimer.isRunning())
    {
        m_wsTimeoutTimer.restart();
    }
}

void Client::update(Event::ClientsDirect event, std::any data)
{
    if (event == Event::ClientsDirect::connected)
    {
        try {
            const std::string publicId = std::any_cast<std::string>(data);
            if (auto sp = m_webSocketConnection.lock())
            {
                sp->sendText( SerializableEvent::Online{publicId, true}.json() );
            }
        } catch (const std::bad_any_cast& e) {
            PLOG_ERROR << "Client::update - Event::ClientsDirect::connected "
                         "- expected std::string: " << e.what();
        }
        return;
    }
    else if (event == Event::ClientsDirect::disconnected)
    {
        try {
            const std::string publicId = std::any_cast<std::string>(data);
            if (auto sp = m_webSocketConnection.lock())
            {
                sp->sendText( SerializableEvent::Online{publicId, false}.json() );
            }
        } catch (const std::bad_any_cast& e) {
            PLOG_ERROR << "Client::update - Event::ClientsDirect::disconnected "
                         "- expected std::string: " << e.what();
        }
        return;
    }
    else if (event == Event::ClientsDirect::nameChanged)
    {
        try {
            const Event::Data::NameInfo nameInfo = std::any_cast<Event::Data::NameInfo>(data);
            if (auto sp = m_webSocketConnection.lock())
            {
                sp->sendText( SerializableEvent::NameChanged{nameInfo.publicId, nameInfo.name}.json() );
            }
        } catch (const std::bad_any_cast& e) {
            PLOG_ERROR << "Client::update - Event::ClientsDirect::nameChanged "
                         "- expected Event::Data::NameInfo: " << e.what();
        }
        return;
    }
    else
    {
        PLOG_WARNING << "Client::update(Event::ClientsDirect) unknown event";
    }
}

void Client::update(Event::TransferSession event, std::any data)
{
    if (event == Event::TransferSession::newReceiver)
    {
        PLOG_DEBUG << "New receiver event for " << m_id;
        try {
            std::shared_ptr<Client> client = std::any_cast<std::shared_ptr<Client>>(data);
            if (client == nullptr)
            {
                PLOG_WARNING << "Client::update - Event::TransferSession::newReceiver - data nullptr";
                return;
            }
            if (auto sp = m_webSocketConnection.lock())
            {
                sp->sendText( SerializableEvent::NewReceiver{client->publicId(), client->name()}.json() );
            }
        } catch (const std::bad_any_cast& e) {
            PLOG_ERROR << "Client::update - Event::TransferSession::newReceiver "
                         "- expected std::shared_ptr<Client>: " << e.what();
        }
        return;
    }
    else if (event == Event::TransferSession::receiverRemoved)
    {
        try {
            const std::string publicId = std::any_cast<std::string>(data);
            if (auto sp = m_webSocketConnection.lock())
            {
                sp->sendText( SerializableEvent::ReceiverRemoved{publicId}.json() );
            }
        } catch (const std::bad_any_cast& e) {
            PLOG_ERROR << "Client::update - Event::TransferSession::receiverRemoved "
                         "- expected std::string: " << e.what();
        }
        return;
    }
    else if (event == Event::TransferSession::fileInfoUpdated)
    {
        try {
            TransferSession::FileInfo fileInfo = std::any_cast<TransferSession::FileInfo>(data);
            if (auto sp = m_webSocketConnection.lock())
            {
                sp->sendText( SerializableEvent::FileInfoUpdated{fileInfo.name, fileInfo.size}.json() );
            }
        } catch (const std::bad_any_cast& e) {
            PLOG_ERROR << "Client::update - Event::TransferSession::fileInfoUpdated "
                         "- expected FileInfo: " << e.what();
        }
        return;
    }
    else if (event == Event::TransferSession::chunkDownloadStarted)
    {
        try {
            const auto info = std::any_cast<Event::Data::TransferSessionDownloadInfo>(data);
            if (auto sp = m_webSocketConnection.lock())
            {
                sp->sendText( SerializableEvent::ChunkDownload{info.publicId, info.chunkId, true}.json() );
            }
        } catch (const std::bad_any_cast& e) {
            PLOG_ERROR << "Client::update - Event::TransferSession::chunkDownloadStarted "
                         "- expected TransferSessionDownloadInfo: " << e.what();
        }
        return;
    }
    else if (event == Event::TransferSession::chunkDownloadFinished)
    {
        try {
            const auto info = std::any_cast<Event::Data::TransferSessionDownloadInfo>(data);
            if (auto sp = m_webSocketConnection.lock())
            {
                sp->sendText( SerializableEvent::ChunkDownload{info.publicId, info.chunkId, false}.json() );
            }
        } catch (const std::bad_any_cast& e) {
            PLOG_ERROR << "Client::update - Event::TransferSession::chunkDownloadFinished "
                         "- expected TransferSessionDownloadInfo: " << e.what();
        }
        return;
    }
    else if (event == Event::TransferSession::newChunkIsAvailable)
    {
        try {
            const auto info = std::any_cast<Event::Data::ChunkInfo>(data);
            if (auto sp = m_webSocketConnection.lock())
            {
                sp->sendText( SerializableEvent::NewChunkAvailable{info.index, info.size}.json() );
            }
        } catch (const std::bad_any_cast& e) {
            PLOG_ERROR << "Client::update - Event::TransferSession::newChunkIsAvailable "
                         "- expected ChunkInfo: " << e.what();
        }
        return;
    }
    else if (event == Event::TransferSession::chunksWasRemoved)
    {
        try {
            const auto list = std::any_cast<std::list<size_t>>(data);
            if (auto sp = m_webSocketConnection.lock())
            {
                sp->sendText( SerializableEvent::ChunksRemoved{list}.json() );
            }
        } catch (const std::bad_any_cast& e) {
            PLOG_ERROR << "Client::update - Event::TransferSession::chunksWasRemoved "
                         "- expected std::list<size_t>: " << e.what();
        }
        return;
    }
    else if (event == Event::TransferSession::fileUploadFinished)
    {
        if (auto sp = m_webSocketConnection.lock())
        {
            sp->sendText( SerializableEvent::UploadFinished().json() );
        }
        return;
    }
    else if (event == Event::TransferSession::complete)
    {
         try {
            const auto type = std::any_cast<Event::Data::TransferSessionCompleteType>(data);
            const std::string myInternalId = m_id;
            sendTextWithAck(
                SerializableEvent::SessionComplete{type}.json(),
                [myInternalId]() { ClientList::instanse().remove(myInternalId); }
            );
        } catch (const std::bad_any_cast& e) {
            PLOG_ERROR << "Client::update - Event::TransferSession::complete "
                         "- expected TransferSessionCompleteType: " << e.what();
        }
        return;
    }
    else if (event == Event::TransferSession::bytesInUpdated)
    {
        try {
            const auto value = std::any_cast<size_t>(data);
            if (auto sp = m_webSocketConnection.lock())
            {
                sp->sendText( SerializableEvent::TotalBytesCount{value, true}.json() );
            }
        } catch (const std::bad_any_cast& e) {
            PLOG_ERROR << "Client::update - Event::TransferSession::bytesInUpdated "
                         "- expected size_t: " << e.what();
        }
        return;
    }
    else if (event == Event::TransferSession::bytesOutUpdated)
    {
        try {
            const auto value = std::any_cast<size_t>(data);
            if (auto sp = m_webSocketConnection.lock())
            {
                sp->sendText( SerializableEvent::TotalBytesCount{value, false}.json() );
            }
        } catch (const std::bad_any_cast& e) {
            PLOG_ERROR << "Client::update - Event::TransferSession::bytesOutUpdated "
                         "- expected size_t: " << e.what();
        }
        return;
    }
    else if (event == Event::TransferSession::chunksAreUnfrozen)
    {
        if (auto sp = m_webSocketConnection.lock())
        {
            sp->sendText( SerializableEvent::ChunksAreUnfrozen{}.json() );
        }
        return;
    }
    else
    {
        PLOG_WARNING << "Client::update(Event::TransferSession) unknown event";
    }
}

void Client::update(Event::TransferSessionForSender event, std::any data)
{
    if (event == Event::TransferSessionForSender::newChunkIsAllowed)
    {
        try {
            const auto status = std::any_cast<bool>(data);
            if (auto sp = m_webSocketConnection.lock())
            {
                sp->sendText( SerializableEvent::NewChunkIsAllowed{status}.json() );
            }
        } catch (const std::bad_any_cast& e) {
            PLOG_ERROR << "Client::update - Event::TransferSessionForSender::newChunkIsAllowed "
                         "- expected bool: " << e.what();
        }
        return;
    }
    else
    {
        PLOG_WARNING << "Client::update(Event::TransferSessionForSender) unknown event";
    }
}

Client::~Client()
{
    PLOG_DEBUG << "Client " << m_publicId << " destroyed";

    // Fire any remaining ACK callbacks so callers relying on them
    // (e.g. session cleanup) do not stall. Cancel their timers first.
    std::unordered_map<uint64_t, PendingAck> pending;
    {
        std::lock_guard lock(m_pendingAcksMutex);
        pending.swap(m_pendingAcks);
    }
    for (auto& [_id, entry] : pending)
    {
        if (entry.fallback)
        {
            asio::error_code ignore;
            entry.fallback->cancel(ignore);
        }
        if (entry.callback) entry.callback();
    }

    Publisher<Event::ClientInternal>::notifySubscribers(Event::ClientInternal::destroyed, m_publicId);

    if (auto sp = m_webSocketConnection.lock())
    {
        sp->close();
    }
}

std::string Client::name() const
{
    std::shared_lock lock (m_mutex);
    return m_name;
}

bool Client::online() const
{
    std::shared_lock lock (m_mutex);
    return not m_wsTimeoutTimer.isRunning();
}

void Client::dropCurrentWsConnection()
{
    if (auto ws = m_webSocketConnection.lock())
    {
        ws->close();
    }
}

void Client::incrementReceived(size_t bytes)
{
    m_bytesReceived += bytes;

    if (auto ws = m_webSocketConnection.lock())
    {
        ws->sendText( SerializableEvent::PersonalReceivedUpdated{m_bytesReceived}.json() );
    }
}

void Client::setName(const std::string &name)
{
    if (name.empty()) return;

    std::unique_lock lock(m_mutex);

    m_name = name;

    if (m_name.length() > NAME_MAX_LENGTH)
    {
        m_name.resize(NAME_MAX_LENGTH);
    }

    Event::Data::NameInfo info;
    info.name = m_name;
    info.publicId = m_publicId;

    Publisher<Event::ClientsDirect>::notifySubscribers(Event::ClientsDirect::nameChanged, info);
}
