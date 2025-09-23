// Copyright (C) 2025  Roman Lyubimov
// SPDX-License-Identifier: GPL-3.0-or-later
// For full license text, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#pragma once

#include "crowlib/crow/app.h"
#include "crowlib/crow/middlewares/cookie_parser.h"
#include "transfersessionlist.h"
#include "websocketconnection.h"

namespace WebAPIDetails {
struct XForwardedFor : crow::ILocalMiddleware
{
    struct context
    {};
    void before_handle(crow::request& req, crow::response& res, context& ctx)
    {
        /*
         * The logic is very simple and does not handle cases where multiple
         * addresses are transmitted, because the address value is used only
         * as an additional unique factor when generating the client ID.
         */

        const auto forwarededHeaderValue = req.get_header_value("X-Forwarded-For");
        if (not forwarededHeaderValue.empty())
        {
            req.remote_ip_address = forwarededHeaderValue;
        }
    }
    void after_handle(crow::request& req, crow::response& res, context& ctx)
    {}
};
} // WebAPIDetails

class WebAPI
{
public:
    WebAPI();
    void run();

private:
    using WsRaiiWrapper = WebSocketConnectionDetails::WebSocketConnectionRAIIWrapper;

    void initRoutes();

    void currentStatistics(const crow::request& req, crow::response& res);
    void meInfo(const crow::request& req, crow::response& res);
    void meLeave(const crow::request& req, crow::response& res);
    void identityRequest(const crow::request& req, crow::response& res);
    void identityValidation(const crow::request& req, crow::response& res);
    void sessionCreate(const crow::request& req, crow::response& res);
    void sessionJoin(const crow::request& req, crow::response& res);
    void sessionChunkPost(const crow::request& req, crow::response& res);
    void sessionChunkGet(const crow::request& req, crow::response& res);

    bool wsOnAccept(const crow::request& req, void** userdata);
    void wsOnConnect(crow::websocket::connection& conn);
    void wsOnClose(crow::websocket::connection& conn, const std::string& reason, uint16_t code);
    void wsOnMessage(crow::websocket::connection& conn, const std::string& data, bool isBinary);

    void internalCreateClient(const crow::request& req, crow::response& res, const std::string& name, const std::string& clientId = std::string());
    void internalWsMessageProcessing(crow::websocket::connection &conn,
                                     std::shared_ptr<Client>& client,
                                     std::shared_ptr<TransferSession>& session,
                                     const std::string& action,
                                     const crow::json::rvalue &data);

    crow::App<crow::CookieParser, WebAPIDetails::XForwardedFor> m_app;
};
