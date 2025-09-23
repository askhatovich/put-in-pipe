// Copyright (C) 2025  Roman Lyubimov
// SPDX-License-Identifier: GPL-3.0-or-later
// For full license text, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#pragma once

#include "crowlib/crow/websocket.h"

class Client;

namespace WebSocketConnectionDetails {
class WebSocketConnectionRAIIWrapper;
}

class WebSocketConnection
{
    friend WebSocketConnectionDetails::WebSocketConnectionRAIIWrapper;
public:
    ~WebSocketConnection();

    void sendText(const std::string& string);
    void sendBinary(const std::string& binary);
    void close();

private:
    WebSocketConnection(std::shared_ptr<Client> с)
        : m_client(с) {}

    crow::websocket::connection* m_connection = nullptr;
    std::weak_ptr<Client> m_client;
};

namespace WebSocketConnectionDetails {
/*
 * An object for managing WebSocketConnection via new/delete and storing it in Crow void** userdata.
 */
struct WebSocketConnectionRAIIWrapper
{
    WebSocketConnectionRAIIWrapper(std::shared_ptr<Client> client)
        : ws(new WebSocketConnection(client))
    {
        std::cout << "WebSocketConnectionRAIIWrapper()" << std::endl; // DEBUG
    }
    ~WebSocketConnectionRAIIWrapper();

    std::shared_ptr<WebSocketConnection> ws;

    std::weak_ptr<Client> client() { return ws->m_client; }
    void setConnection(crow::websocket::connection& conn);
};
} // namespace WebSocketConnectionDetails

