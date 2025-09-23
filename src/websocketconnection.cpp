// Copyright (C) 2025  Roman Lyubimov
// SPDX-License-Identifier: GPL-3.0-or-later
// For full license text, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#include "websocketconnection.h"
#include "client.h"

WebSocketConnection::~WebSocketConnection()
{
    std::cout << "~WebSocketConnection()" << std::endl; // DEBUG
    // if (auto sp = m_client.lock())
    // {
    //     sp->onWebSocketDisconnected();
    // }
}

void WebSocketConnection::sendText(const std::string &string)
{
    if (!m_connection)
    {
        std::cerr << "WebSocketConnection::sendText: connection is nullptr" << std::endl;
        return;
    }
    m_connection->send_text(string);
}

void WebSocketConnection::sendBinary(const std::string &binary)
{
    if (!m_connection)
    {
        std::cerr << "WebSocketConnection::sendBinary: connection is nullptr" << std::endl;
        return;
    }
    m_connection->send_binary(binary);
}

void WebSocketConnection::close()
{
    if (!m_connection)
    {
        std::cerr << "WebSocketConnection::close: connection is nullptr" << std::endl;
        return;
    }
    m_connection->close();
}

WebSocketConnectionDetails::WebSocketConnectionRAIIWrapper::~WebSocketConnectionRAIIWrapper()
{
    std::cout << "~WebSocketConnectionRAIIWrapper()" << std::endl; // DEBUG
}

void WebSocketConnectionDetails::WebSocketConnectionRAIIWrapper::setConnection(crow::websocket::connection &conn)
{
    ws->m_connection = &conn;

    if (auto sp = ws->m_client.lock())
    {
        sp->onWebSocketConnected(ws);
    }
}
