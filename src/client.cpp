// Copyright (C) 2025  Roman Lyubimov
// SPDX-License-Identifier: GPL-3.0-or-later
// For full license text, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#include "client.h"
#include "captcha/skaptcha_tools.h"
#include "serializableevent.h"
#include "transfersession.h"
#include "config/config.h"

#include <mutex>

Client::Client(const std::string &id, asio::io_context& ioContext, std::function<void()> onTimeout) :
    m_id(id),
    m_publicId(skaptcha_tools::crypto::HashSignature::instance().sign(m_id)),
    m_timeoutTimer(ioContext, onTimeout, TimerCallback::Duration(Config::instance().clientTimeout()))
{
    std::cerr << "Client " << m_id << " created" << std::endl; // DEBUG
    m_timeoutTimer.start();
}

void Client::onWebSocketDisconnected()
{
    std::unique_lock lock (m_mutex);

    m_webSocketConnection.reset();
    m_timeoutTimer.start();

    Publisher<Event::ClientDirect>::notifySubscribers(Event::ClientDirect::disconnected, m_publicId);
}

void Client::onWebSocketConnected(std::shared_ptr<WebSocketConnection> connection)
{
    std::unique_lock lock (m_mutex);

    m_webSocketConnection = connection;
    m_timeoutTimer.stop();

    Publisher<Event::ClientDirect>::notifySubscribers(Event::ClientDirect::connected, m_publicId);
}

std::string Client::joinedSession() const
{
    std::shared_lock lock (m_mutex);
    return m_joinedSession;
}

bool Client::joinSession(const std::string &id)
{
    if (id.empty()) return false; // ???

    std::shared_lock lock (m_mutex);
    if (not m_joinedSession.empty())
    {
        return false;
    }
    m_joinedSession = id;
    return true;
}

void Client::resetTimeoutTimerIfItActive()
{
    std::unique_lock lock (m_mutex);

    if (m_timeoutTimer.isRunning())
    {
        m_timeoutTimer.restart();
    }
}

void Client::update(Event::ClientDirect event, std::any data)
{
    if (event == Event::ClientDirect::connected)
    {
        try {
            const std::string publicId = std::any_cast<std::string>(data);
            if (auto sp = m_webSocketConnection.lock())
            {
                sp->sendText( SerializableEvent::Online{publicId, true}.json() );
            }
        } catch (const std::bad_any_cast& e) {
            std::cerr << "Client::update - Event::ClientDirect::connected "
                         "- expected std::string: " << e.what() << std::endl;
        }
        return;
    }
    else if (event == Event::ClientDirect::disconnected)
    {
        try {
            const std::string publicId = std::any_cast<std::string>(data);
            if (auto sp = m_webSocketConnection.lock())
            {
                sp->sendText( SerializableEvent::Online{publicId, false}.json() );
            }
        } catch (const std::bad_any_cast& e) {
            std::cerr << "Client::update - Event::ClientDirect::disconnected "
                         "- expected std::string: " << e.what() << std::endl;
        }
        return;
    }
    else if (event == Event::ClientDirect::nameChanged)
    {
        try {
            const Event::Data::NameInfo nameInfo = std::any_cast<Event::Data::NameInfo>(data);
            if (auto sp = m_webSocketConnection.lock())
            {
                sp->sendText( SerializableEvent::NameChanged{nameInfo.publicId, nameInfo.name}.json() );
            }
        } catch (const std::bad_any_cast& e) {
            std::cerr << "Client::update - Event::ClientDirect::nameChanged "
                         "- expected Event::Data::NameInfo: " << e.what() << std::endl;
        }
        return;
    }
    else
    {
        std::cerr << "Client::update(Event::ClientDirect) unknown event" << std::endl;
    }
}

void Client::update(Event::TransferSession event, std::any data)
{
    if (event == Event::TransferSession::newReceiver)
    {
        std::cout << "New receiver event for " << m_id << std::endl;
        try {
            std::shared_ptr<Client> client = std::any_cast<std::shared_ptr<Client>>(data);
            if (client == nullptr)
            {
                std::cerr << "Client::update - Event::TransferSession::newReceiver - data nullptr" << std::endl;
                return;
            }
            if (auto sp = m_webSocketConnection.lock())
            {
                sp->sendText( SerializableEvent::NewReceiver{client->publicId(), client->name()}.json() );
            }
        } catch (const std::bad_any_cast& e) {
            std::cerr << "Client::update - Event::TransferSession::newReceiver "
                         "- expected std::shared_ptr<Client>: " << e.what() << std::endl;
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
            std::cerr << "Client::update - Event::TransferSession::receiverRemoved "
                         "- expected std::string: " << e.what() << std::endl;
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
            std::cerr << "Client::update - Event::TransferSession::fileInfoUpdated "
                         "- expected FileInfo: " << e.what() << std::endl;
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
            std::cerr << "Client::update - Event::TransferSession::chunkDownloadStarted "
                         "- expected TransferSessionDownloadInfo: " << e.what() << std::endl;
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
            std::cerr << "Client::update - Event::TransferSession::chunkDownloadFinished "
                         "- expected TransferSessionDownloadInfo: " << e.what() << std::endl;
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
            std::cerr << "Client::update - Event::TransferSession::newChunkIsAvailable "
                         "- expected ChunkInfo: " << e.what() << std::endl;
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
            std::cerr << "Client::update - Event::TransferSession::chunksWasRemoved "
                         "- expected std::list<size_t>: " << e.what() << std::endl;
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
            if (auto sp = m_webSocketConnection.lock())
            {
                sp->sendText( SerializableEvent::SessionComplete{type}.json() );
            }
        } catch (const std::bad_any_cast& e) {
            std::cerr << "Client::update - Event::TransferSession::complete "
                         "- expected TransferSessionCompleteType: " << e.what() << std::endl;
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
            std::cerr << "Client::update - Event::TransferSession::bytesInUpdated "
                         "- expected size_t: " << e.what() << std::endl;
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
            std::cerr << "Client::update - Event::TransferSession::bytesOutUpdated "
                         "- expected size_t: " << e.what() << std::endl;
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
        std::cerr << "Client::update(Event::TransferSession) unknown event" << std::endl;
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
            std::cerr << "Client::update - Event::TransferSessionForSender::newChunkIsAllowed "
                         "- expected bool: " << e.what() << std::endl;
        }
        return;
    }
    else
    {
        std::cerr << "Client::update(Event::TransferSessionForSender) unknown event" << std::endl;
    }
}

Client::~Client()
{
    std::cerr << "~Client " << m_id << " destructing" << std::endl; // DEBUG
    Publisher<Event::ClientInternal>::notifySubscribers(Event::ClientInternal::destroyed, m_publicId);

    if (auto sp = m_webSocketConnection.lock())
    {
        sp->close();
    }
    std::cerr << "~Client " << m_id << " destructed" << std::endl; // DEBUG
}

std::string Client::name() const
{
    std::shared_lock lock (m_mutex);
    return m_name;
}

bool Client::online() const
{
    std::shared_lock lock (m_mutex);
    return not m_timeoutTimer.isRunning();
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

    Publisher<Event::ClientDirect>::notifySubscribers(Event::ClientDirect::nameChanged, info);
}
