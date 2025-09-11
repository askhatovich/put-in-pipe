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
    void sendText(const std::string& string);
    void sendBinary(const std::string& binary);
    void close();

private:
    WebSocketConnection(std::shared_ptr<Client> с)
        : client(с) {}

    crow::websocket::connection* connection = nullptr;
    std::weak_ptr<Client> client;
};

namespace WebSocketConnectionDetails {
/*
 * An object for managing WebSocketConnection via new/delete and storing it in Crow void** userdata.
 */
struct WebSocketConnectionRAIIWrapper
{
    WebSocketConnectionRAIIWrapper(std::shared_ptr<Client> client)
        : ws(new WebSocketConnection(client))
    {}
    ~WebSocketConnectionRAIIWrapper();

    std::shared_ptr<WebSocketConnection> ws;

    std::weak_ptr<Client> client() { return ws->client; }
    void setConnection(crow::websocket::connection& conn);
};
} // namespace WebSocketConnectionDetails

