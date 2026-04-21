// Copyright (C) 2025-2026  Roman Lyubimov
// SPDX-License-Identifier: GPL-3.0-or-later
// For full license text, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#pragma once

#include <string>
#include <shared_mutex>
#include <functional>
#include <unordered_map>
#include <atomic>
#include <asio.hpp>

#include "observerpattern.h"
#include "websocketconnection.h"
#include "timercallback.h"

namespace Event {
enum class TransferSession;
enum class TransferSessionSender;
enum class TransferSessionForSender;

enum class ClientsDirect { // direct communication between users
//  Event            Data
    connected,    // publicId
    disconnected, // publicId
    nameChanged   // Data::NameInfo
};

enum class ClientInternal {
//  Event            Data
    destroyed     // publicId
};

namespace Data {

struct NameInfo
{
    std::string name;
    std::string publicId;
};

} // namespace Data
} // namespace Event

class Client : public Publisher<Event::ClientsDirect>,
               public Publisher<Event::ClientInternal>,
               public Subscriber<Event::ClientsDirect>,
               public Subscriber<Event::TransferSession>,
               public Subscriber<Event::TransferSessionForSender>
{
public:
    /*
     * A long name doesn't make sense and can be a problem for the UI in some cases.
     */
    static constexpr uint8_t NAME_MAX_LENGTH = 20;

    ~Client();

    std::string id() const { return m_id; }
    /*
     * This identifier is permanent for a client,
     * but it does not allow you to find out the real ID of the client
     * for security reasons: having an ID will allow anyone to impersonate a customer.
     * PublicID is sent to other users, and it is safe.
     */
    std::string publicId() const { return m_publicId; }
    std::string name() const;
    size_t currentChunkIndex() const { return m_currentChunkIndex; }
    void setCurrentChunkIndex(size_t index) { m_currentChunkIndex = index; }
    bool online() const;
    void dropCurrentWsConnection();
    size_t bytesReceived() const { return m_bytesReceived; }

    void incrementReceived(size_t bytes);
    void setName(const std::string& name);

    void onWebSocketConnected(std::shared_ptr<WebSocketConnection> connection);
    void onWebSocketDisconnected();

    /*
     * Sends a server→client event and waits for the client to ACK it
     * via {"action":"ack","data":{"id":<n>}}. On ACK or fallback timer
     * fire, the callback runs exactly once. Intended for terminal events
     * (complete, kicked) where the server must know delivery occurred
     * before closing the WS.
     */
    static constexpr std::chrono::milliseconds DEFAULT_ACK_FALLBACK {2000};
    void sendTextWithAck(const std::string& eventJson,
                         std::function<void()> onAck,
                         std::chrono::milliseconds fallback = DEFAULT_ACK_FALLBACK);
    void processAck(uint64_t ackId);

    std::string joinedSession() const;
    bool joinSession(const std::string& id);
    void resetWsTimeoutTimerIfItActive();

    // Subscriber interface
    void update(Event::ClientsDirect event, std::any data) override;
    void update(Event::TransferSession event, std::any data) override;
    void update(Event::TransferSessionForSender event, std::any data) override;

protected:
    // we explicit use shared_ptr only (Subscriber)
    Client(const std::string& id, asio::io_context& ioContext, std::function<void()> onTimeout);

private:
    const std::string m_id;
    const std::string m_publicId;
    std::string m_joinedSession;
    std::string m_name;
    std::weak_ptr<WebSocketConnection> m_webSocketConnection;
    std::atomic<size_t> m_currentChunkIndex = 0;
    std::atomic<size_t> m_bytesReceived = 0;
    mutable std::shared_mutex m_mutex;
    TimerCallback m_wsTimeoutTimer;

    asio::io_context& m_ioContext;

    struct PendingAck
    {
        std::function<void()> callback;
        std::unique_ptr<asio::steady_timer> fallback;
    };
    std::atomic<uint64_t> m_nextAckId {1};
    std::unordered_map<uint64_t, PendingAck> m_pendingAcks;
    std::mutex m_pendingAcksMutex;

    void resolveAck(uint64_t id);
};
