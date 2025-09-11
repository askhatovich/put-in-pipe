// Copyright (C) 2025  Roman Lyubimov
// SPDX-License-Identifier: GPL-3.0-or-later
// For full license text, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#include "websocketconnection.h"
#include "client.h"

void WebSocketConnection::sendText(const std::string &string)
{
    if (connection)
    {
        connection->send_text(string);
    }
}

void WebSocketConnection::sendBinary(const std::string &binary)
{
    if (connection)
    {
        connection->send_binary(binary);
    }
}

void WebSocketConnection::close()
{
    if (connection)
    {
        connection->close();
    }
}

WebSocketConnectionDetails::WebSocketConnectionRAIIWrapper::~WebSocketConnectionRAIIWrapper()
{
    if (auto sp = ws->client.lock())
    {
        sp->onWebSocketDisconnected();
    }
}

void WebSocketConnectionDetails::WebSocketConnectionRAIIWrapper::setConnection(crow::websocket::connection &conn)
{
    ws->connection = &conn;

    if (auto sp = ws->client.lock())
    {
        sp->onWebSocketConnected(ws);
    }
}
