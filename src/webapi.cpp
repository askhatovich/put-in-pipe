// Copyright (C) 2025  Roman Lyubimov
// SPDX-License-Identifier: GPL-3.0-or-later
// For full license text, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#include "webapi.h"
#include "config/config.h"
#include "clientlist.h"
#include "captcha/skaptcha.h"
#include "transfersessionlist.h"
#include "crowlib/crow/json.h"
#include "websocketconnection.h"
#include "client.h"
#include "transfersession.h"
#include "serializableevent.h"

const char CLIENT_ID_TOKEN[] = "putin";

WebAPI::WebAPI()
{
    initRoutes();
}

void WebAPI::run()
{
    const auto& cfg = Config::instance();
    m_app.websocket_max_payload(cfg.transferSessionMaxChunkSize());
    m_app.bindaddr(cfg.bindAddress()).port(cfg.bindPort()).multithreaded().run();
}

void WebAPI::initRoutes()
{
    CROW_ROUTE(m_app, "/").methods("GET"_method)
    ([](const crow::request& req, crow::response& resp) {
        resp.set_header("Content-Type", "text/plain; charset=utf-8");
        resp.body = "На проде здесь будет отдаваться бандл веб-приложения";
        resp.end();
    });

    CROW_WEBSOCKET_ROUTE(m_app, "/api/ws")
        .onaccept([&](const crow::request& req, void** userdata){
            return wsOnAccept(req, userdata);
        })
        .onopen([&](crow::websocket::connection& conn) {
            wsOnConnect(conn);
        })
        .onclose([&](crow::websocket::connection& conn, const std::string& reason, uint16_t code) {
            wsOnClose(conn, reason, code);
        })
        .onmessage([&](crow::websocket::connection& conn, const std::string& data, bool isBinary) {
            wsOnMessage(conn, data, isBinary);
        });

    // 0. Service endpoints

    CROW_ROUTE(m_app, "/api/statistics/current").methods("GET"_method)
    ([this](const crow::request &req, crow::response &resp){ currentStatistics(req, resp); });

    CROW_ROUTE(m_app, "/api/me/info").methods("GET"_method)
    ([this](const crow::request &req, crow::response &resp){ meInfo(req, resp); });

    CROW_ROUTE(m_app, "/api/me/leave").methods("POST"_method)
    ([this](const crow::request &req, crow::response &resp){ meLeave(req, resp); });

    // 1. Create a Client /////

    CROW_ROUTE(m_app, "/api/identity/request").methods("GET"_method)
    .CROW_MIDDLEWARES(m_app, WebAPIDetails::XForwardedFor)
    ([this](const crow::request &req, crow::response &resp){ identityRequest(req, resp); });

    CROW_ROUTE(m_app, "/api/identity/confirmation").methods("POST"_method)
    ([this](const crow::request &req, crow::response &resp){ identityValidation(req, resp); });

    // 2. Create a Session or join it /////

    CROW_ROUTE(m_app, "/api/session/create").methods("POST"_method)
    ([this](const crow::request &req, crow::response &resp){ sessionCreate(req, resp); });

    CROW_ROUTE(m_app, "/api/session/join").methods("GET"_method)
    ([this](const crow::request &req, crow::response &resp){ sessionJoin(req, resp); });
}

void WebAPI::currentStatistics(const crow::request &req, crow::response &res)
{
    const auto& cfg = Config::instance();

    crow::json::wvalue json {
        {"current_user_count", ClientList::instanse().count()},
        {"current_session_count", TransferSessionList::instanse().count()},
        {"max_user_count", cfg.apiMaxClientCount()},
        {"max_session_count", cfg.transferSessionCountLimit()}
    };

    res.code = 200;
    res.set_header("Content-Type", "application/json; charset=utf-8");
    res.body = json.dump();
    res.end();
}

void WebAPI::meInfo(const crow::request &req, crow::response &res)
{
    auto& cookieCtx = m_app.get_context<crow::CookieParser>(req);
    const auto token = cookieCtx.get_cookie(CLIENT_ID_TOKEN);
    if (token.empty())
    {
        res.code = 401;
        res.body = "You have not been identified";
        res.end();
        return;
    }

    const auto client = ClientList::instanse().get(token);
    if (client == nullptr)
    {
        res.code = 401;
        res.body = "You have not been identified";
        res.end();
        return;
    }

    crow::json::wvalue json {
        {"id", client->publicId()},
        {"name", client->name()},
        {"session", client->joinedSession()}
    };

    res.code = 200;
    res.set_header("Content-Type", "application/json; charset=utf-8");
    res.body = json.dump();
    res.end();
}

void WebAPI::meLeave(const crow::request &req, crow::response &res)
{
    auto& cookieCtx = m_app.get_context<crow::CookieParser>(req);
    const auto token = cookieCtx.get_cookie(CLIENT_ID_TOKEN);
    if (token.empty())
    {
        res.code = 401;
        res.body = "You have not been identified";
        res.end();
        return;
    }

    const auto client = ClientList::instanse().get(token);
    if (client == nullptr)
    {
        res.code = 401;
        res.body = "You have not been identified";
        res.end();
        return;
    }

    ClientList::instanse().remove(client->id());

    res.code = 200;
    res.body = "Your ID has been deleted";
    res.end();
}

void WebAPI::identityRequest(const crow::request &req, crow::response &res)
{
    auto& cookieCtx = m_app.get_context<crow::CookieParser>(req);
    const auto token = cookieCtx.get_cookie(CLIENT_ID_TOKEN);
    if (!token.empty() and ClientList::instanse().get(token))
    {
        res.code = 400;
        res.body = "You are already authorized";
        res.end();
        return;
    }

    const auto nameParam = req.url_params.get("name");
    if (nameParam == nullptr)
    {
        res.code = 403;
        res.body = "You must pass the name";
        res.end();
        return;
    }

    if (ClientList::instanse().count() >= Config::instance().apiMaxClientCount())
    {
        res.code = 503;
        res.body = "The maximum number of clients has been reached, please try again later";
        res.end();
        return;
    }

    if (ClientList::instanse().count() < Config::instance().apiWithoutCaptchaThreshold())
    {
        internalCreateClient(req, res, nameParam);
        return;
    }

    const auto clientIdCondidate = ClientList::generateIdCondidate(req.remote_ip_address);
    const auto captcha = Skaptcha::instance().generate( clientIdCondidate, std::chrono::seconds(Config::instance().apiCaptchaLifetime()) );
    crow::json::wvalue json {
        {"captcha_image", crow::utility::base64encode( captcha->png.data(), captcha->png.size() )},
        {"captcha_token", captcha->token},
        {"captcha_lifetime", Config::instance().apiCaptchaLifetime()},
        {"client_id", clientIdCondidate},
        {"captcha_answer_length", Skaptcha::answerLength()}
    };

    res.code = 401;
    res.set_header("Content-Type", "application/json; charset=utf-8");
    res.body = json.dump();
    res.end();
}

void WebAPI::identityValidation(const crow::request &req, crow::response &res)
{
    // -> captcha_answer, client_id, captcha_token, name

    auto& cookieCtx = m_app.get_context<crow::CookieParser>(req);
    const auto token = cookieCtx.get_cookie(CLIENT_ID_TOKEN);
    if (!token.empty() and ClientList::instanse().get(token))
    {
        res.code = 400;
        res.body = "You are already authorized";
        res.end();
        return;
    }

    if (ClientList::instanse().count() >= Config::instance().apiMaxClientCount())
    {
        res.code = 503;
        res.body = "The maximum number of clients has been reached, please try again later";
        res.end();
        return;
    }

    crow::json::rvalue json;
    try {
        json = crow::json::load(req.body);
    } catch (...) {
        res.code = 400;
        res.body = "Invalid JSON";
        res.end();
        return;
    }

    std::string answer;
    try {
        answer = json["captcha_answer"].s();
    } catch (...) {
        res.code = 400;
        res.body = ".captcha_answer should be a string";
        res.end();
        return;
    }
    if (answer.empty())
    {
        res.code = 400;
        res.body = ".captcha_answer is empty";
        res.end();
        return;
    }

    std::string clientId;
    try {
        clientId = json["client_id"].s();
    } catch (...) {
        res.code = 400;
        res.body = ".client_id should be a string";
        res.end();
        return;
    }
    if (clientId.empty())
    {
        res.code = 400;
        res.body = ".client_id is empty";
        res.end();
        return;
    }

    std::string captchaToken;
    try {
        captchaToken = json["captcha_token"].s();
    } catch (...) {
        res.code = 400;
        res.body = ".captcha_token should be a string";
        res.end();
        return;
    }
    if (captchaToken.empty())
    {
        res.code = 400;
        res.body = ".captcha_token is empty";
        res.end();
        return;
    }

    std::string name;
    try {
        name = json["name"].s();
    } catch (...) {
        res.code = 400;
        res.body = ".name should be a string";
        res.end();
        return;
    }
    if (captchaToken.empty())
    {
        res.code = 400;
        res.body = ".name is empty";
        res.end();
        return;
    }

    if (not Skaptcha::instance().validate(clientId, captchaToken, answer))
    {
        res.code = 403;
        res.body = "Incorrect response or captcha expired";
        res.end();
        return;
    }

    internalCreateClient(req, res, name, clientId);
}

void WebAPI::sessionCreate(const crow::request &req, crow::response &res)
{
    auto& cookieCtx = m_app.get_context<crow::CookieParser>(req);
    const auto token = cookieCtx.get_cookie(CLIENT_ID_TOKEN);
    if (token.empty())
    {
        res.code = 401;
        res.body = "You have not been identified";
        res.end();
        return;
    }

    const auto client = ClientList::instanse().get(token);
    if (client == nullptr)
    {
        res.code = 401;
        res.body = "You have not been identified";
        res.end();
        return;
    }

    if (not client->joinedSession().empty())
    {
        res.code = 403;
        res.body = "You are already a participant in the session";
        res.end();
        return;
    }

    if (not TransferSessionList::instanse().possibleToCreateNew())
    {
        res.code = 503;
        client->resetTimeoutTimerIfItActive();
        res.body = "The maximum number of sessions has been reached, please try again later";
        res.end();
        return;
    }

    const auto newSession = TransferSessionList::instanse().create(client);
    if (newSession.first == nullptr)
    {
        res.code = 500;
        res.body = "Session creation failed";
        res.end();
        return;
    }
    if (not client->joinSession(newSession.first->id()))
    {
        TransferSessionList::instanse().remove(newSession.first->id());
        res.code = 500;
        res.body = "Session creation failed";
        res.end();
        return;
    }

    crow::json::wvalue json {
        {"id", newSession.first->id()}
    };

    res.code = 201;
    res.set_header("Content-Type", "application/json; charset=utf-8");
    res.body = json.dump();
    res.end();
}

void WebAPI::sessionJoin(const crow::request &req, crow::response &res)
{
    auto& cookieCtx = m_app.get_context<crow::CookieParser>(req);
    const auto token = cookieCtx.get_cookie(CLIENT_ID_TOKEN);
    if (token.empty())
    {
        res.code = 401;
        res.body = "You have not been identified";
        res.end();
        return;
    }

    const auto client = ClientList::instanse().get(token);
    if (client == nullptr)
    {
        res.code = 401;
        res.body = "You have not been identified";
        res.end();
        return;
    }

    const auto idSessionParam = req.url_params.get("id");
    if (idSessionParam == nullptr)
    {
        res.code = 400;
        res.body = "You need to send the session ID";
        res.end();
        return;
    }

    const auto session = TransferSessionList::instanse().get(idSessionParam);
    if (session.first == nullptr)
    {
        res.code = 404;
        res.body = "Session not found";
        res.end();
        return;
    }

    if (not client->joinedSession().empty())
    {
        if (session.first->id() == client->joinedSession())
        {
            crow::json::wvalue json {
                {"id", session.first->id()}
            };

            res.code = 202;
            res.set_header("Content-Type", "application/json; charset=utf-8");
            res.body = json.dump();
            res.end();
            return;
        }

        res.code = 400;
        res.body = "You are already a participant in some another session";
        res.end();
        return;
    }

    if (session.first->someChunkWasRemoved())
    {
        /*
         * If some chunk has been deleted, that is, part of the data is lost,
         * it is impossible to join a new user.
         */

        bool isMember = false;
        for (const auto& received: session.first->receivers())
        {
            auto sp = received.lock();
            if (sp->id() == client->id())
            {
                isMember = true;
                break;
            }
        }

        if (not isMember)
        {
            res.code = 403;
            res.body = "It is impossible to join the session";
            res.end();
            return;
        }
    }

    if (not session.first->addReceiver(client))
    {
        res.code = 500;
        res.body = "Couldn't join the session";
        res.end();
        return;
    }

    if (not client->joinSession(session.first->id()))
    {
        res.code = 500;
        res.body = "Couldn't join the session (2)";
        res.end();
        return;
    }

    crow::json::wvalue json {
        {"id", session.first->id()}
    };

    res.code = 202;
    res.set_header("Content-Type", "application/json; charset=utf-8");
    res.body = json.dump();
    res.end();
}

bool WebAPI::wsOnAccept(const crow::request &req, void **userdata)
{
    auto& cookieCtx = m_app.get_context<crow::CookieParser>(req);
    const auto token = cookieCtx.get_cookie(CLIENT_ID_TOKEN);
    if (token.empty())
    {
        return false;
    }

    const auto client = ClientList::instanse().get(token);
    if (client == nullptr)
    {
        return false;
    }

    if (client->joinedSession().empty())
    {
        return false;
    }

    const bool sessExists = TransferSessionList::instanse()
                                .get( client->joinedSession() ).first != nullptr;
    if (not sessExists)
    {
        return false;
    }

    *userdata = new WsRaiiWrapper(client);

    return true;
}

void WebAPI::wsOnConnect(crow::websocket::connection &conn)
{
    auto wsWrapperPtr = static_cast<WsRaiiWrapper*>(conn.userdata());

    const auto client = wsWrapperPtr->client().lock();
    if (client == nullptr)
    {
        conn.close("Client not found", crow::websocket::CloseStatusCode::ClosedAbnormally);
        return;
    }

    wsWrapperPtr->setConnection(conn);

    const auto session = TransferSessionList::instanse().get(client->joinedSession());
    if (session.first == nullptr)
    {
        conn.close("Session not found", crow::websocket::CloseStatusCode::ClosedAbnormally);
        return;
    }

    crow::json::wvalue receiverArray = crow::json::wvalue::list();
    const auto receiverList = session.first->receivers();
    size_t receiverCounter = 0;
    for (const auto& c: receiverList)
    {
        const auto sp = c.lock();
        if (sp == nullptr) continue;

        receiverArray[receiverCounter]["id"] = sp->publicId();
        receiverArray[receiverCounter]["name"] = sp->name();
        receiverArray[receiverCounter]["current_chunk"] = sp->currentChunkIndex();
        receiverArray[receiverCounter]["is_online"] = sp->online();

        ++receiverCounter;
    }

    const auto sender = session.first->sender().lock();

    crow::json::wvalue senderInfo;
    if (sender != nullptr)
    {
        /*
         * The sender can disconnect and it will not be
         * a logical error if the file has aelready been fully uploaded.
         */

        senderInfo["id"] = sender->publicId();
        senderInfo["name"] = sender->name();
        senderInfo["is_online"] = sender->online();
    }

    const auto& cfg = Config::instance();

    const auto fileInfo = session.first->fileInfo();

    const auto chunksInfoList = session.first->chunksInfo();
    crow::json::wvalue chunksInfo;
    size_t chunksCounter = 0;
    for (const auto& info: chunksInfoList)
    {
        chunksInfo[chunksCounter]["index"] = info.index;
        chunksInfo[chunksCounter]["size"] = info.size;

        ++chunksCounter;
    }

    crow::json::wvalue json {
        {"session_id", session.first->id()},
        {"limits", {
            {"max_receiver_count", cfg.transferSessionMaxConsumerCount()},
            {"max_chunk_size", cfg.transferSessionMaxChunkSize()},
            {"max_initial_freeze", cfg.transferSessionMaxInitialFreezeDuration()},
            {"max_chunk_queue", cfg.transferSessionChunkQueueMaxSize()}
        }},
        {"members", {
            {"receivers", receiverArray},
            {"sender", sender ? senderInfo : nullptr}
        }},
        {"state", {
            {"current_chunk", session.first->currentMaxChunkIndex()},
            {"upload_finished", session.first->eof()},
            {"some_chunk_was_removed", session.first->someChunkWasRemoved()},
            {"initial_freeze", session.first->initialChunksFreeze()},
            {"chunks", std::move(chunksInfo)},
            {"expiration_in", session.second},
            {"file", {
                {"name", fileInfo.name},
                {"size", fileInfo.size}
            }}
        }},
        {"transferred", {
            {"global", {
                {"from_sender", session.first->bytesIn()},
                {"to_receivers", session.first->bytesOut()}
            }},
            {"received_by_you", client->bytesReceived()}
        }}
    };

    crow::json::wvalue root;
    root["data"] = std::move(json);
    root["event"] = "start_init";

    conn.send_text(root.dump());
}

void WebAPI::wsOnClose(crow::websocket::connection &conn, const std::string& /*reason*/, uint16_t /*code*/)
{
    auto wsWrapperPtr = static_cast<WsRaiiWrapper*>(conn.userdata());
    delete wsWrapperPtr;
}

void WebAPI::wsOnMessage(crow::websocket::connection &conn, const std::string &data, bool isBinary)
{
    auto wsWrapperPtr = static_cast<WsRaiiWrapper*>(conn.userdata());
    auto client = wsWrapperPtr->client().lock();
    if (client == nullptr)
    {
        conn.close("Client not found", crow::websocket::CloseStatusCode::ClosedAbnormally);
        return;
    }
    const auto joinedSessionId = client->joinedSession();
    if (joinedSessionId.empty())
    {
        conn.close("Protocol error: it is impossible to receive data before connecting to the session", crow::websocket::CloseStatusCode::ClosedAbnormally);
        return;
    }
    auto session = TransferSessionList::instanse().get(joinedSessionId);
    if (session.first == nullptr)
    {
        conn.close("Session not found", crow::websocket::CloseStatusCode::NormalClosure);
        return;
    }

    if (isBinary)
    {
        // Binary data can only be sent by the sender of the file
        const auto sessionCreator = session.first->sender().lock();
        if (sessionCreator == nullptr)
        {
            conn.close("It is impossible to verify the session creator", crow::websocket::CloseStatusCode::UnacceptableData);
            return;
        }
        if (client->id() != sessionCreator->id())
        {
            conn.close("Only the session creator can send binary data", crow::websocket::CloseStatusCode::UnacceptableData);
            return;
        }
        if (not session.first->addChunk(data))
        {
            conn.send_text( SerializableEvent::AddingChunkFailure{}.json() );
        }
        return;
    }

    try {
        const auto json = crow::json::load(data);
        if (! json)
        {
            conn.close("Text messages are expected only in JSON format", crow::websocket::CloseStatusCode::UnacceptableData);
            return;
        }

        if (not json.has("action") or json["action"].t() != crow::json::type::String)
        {
            conn.close("Invalid JSON: the 'action' string must be passed", crow::websocket::CloseStatusCode::UnacceptableData);
            return;
        }

        if (not json.has("data") or json["data"].t() != crow::json::type::Object)
        {
            conn.close("Invalid JSON: the 'data' object must be passed", crow::websocket::CloseStatusCode::UnacceptableData);
            return;
        }

        internalWsMessageProcessing(conn, client, session.first, json["action"].s(), json["data"]);
    } catch (const std::exception& e) {
        conn.close("Exception: " + std::string(e.what()), crow::websocket::CloseStatusCode::UnacceptableData);
        return;
    }
}

void WebAPI::internalCreateClient(const crow::request &req, crow::response &res, const std::string& name, const std::string& clientId)
{
    const auto client = ClientList::instanse().create(
            clientId.empty() ? ClientList::generateIdCondidate( req.remote_ip_address ) : clientId
        );

    if (client == nullptr)
    {
        res.code = 403;
        res.body = "The captcha has already been used";
        res.end();
        return;
    }

    client->setName(name);

    crow::json::wvalue body {
        {"id", client->publicId()},
        {"name", client->name()}
    };

    std::string cookieHeader = CLIENT_ID_TOKEN;
    cookieHeader += "=" + client->id() +
                    "; HttpOnly" +
                    "; Path=/" +
                    "; Max-Age=" + std::to_string(
                            /*
                             * The maximum time that a client can exist without creating a session
                             * (if he is the creator) is added to the session lifetime.
                             */
                            Config::instance().transferSessionMaxLifetime() + Config::instance().clientTimeout()
                        );

    res.add_header("Set-Cookie", cookieHeader);

    res.code = 201;
    res.set_header("Content-Type", "application/json; charset=utf-8");
    res.body = body.dump();
    res.end();
}

void WebAPI::internalWsMessageProcessing(crow::websocket::connection &conn,
                                         std::shared_ptr<Client>& client, std::shared_ptr<TransferSession>& session,
                                         const std::string& action, const crow::json::rvalue &data)
{
    /*
     * Exception handling is not needed here, it is available at a higher level.
     * Any exception is an invalid request.
     */

    if (action == "set_file_info") // creator only
    {
        const auto creatorSp = session->sender().lock();
        if (creatorSp == nullptr or creatorSp->id() != client->id())
        {
            conn.close("Only the session creator can set the file information", crow::websocket::CloseStatusCode::UnacceptableData);
            return;
        }

        const std::string name = data["name"].s();
        const size_t size = data["size"].u();

        if (not session->setFileInfo({name, size}))
        {
            conn.send_text( SerializableEvent::SetFileInfoFailure{}.json() );
        }
        return;
    }
    else if (action == "upload_finished") // creator only
    {
        const auto creatorSp = session->sender().lock();
        if (creatorSp == nullptr or creatorSp->id() != client->id())
        {
            conn.close("Only the session creator can set the upload finish", crow::websocket::CloseStatusCode::UnacceptableData);
            return;
        }

        session->setEndOfFile();
    }
    else if (action == "kick_receiver") // creator only
    {
        const auto creatorSp = session->sender().lock();
        if (creatorSp == nullptr or creatorSp->id() != client->id())
        {
            conn.close("Only the session creator can delete participants", crow::websocket::CloseStatusCode::UnacceptableData);
            return;
        }

        const std::string publicId = data["id"].s();
        if (client->publicId() == publicId)
        {
            conn.close("You can't delete yourself", crow::websocket::CloseStatusCode::UnacceptableData);
            return;
        }

        session->removeReceiver(publicId);
    }
    else if (action == "terminate_session") // creator only
    {
        const auto creatorSp = session->sender().lock();
        if (creatorSp == nullptr or creatorSp->id() != client->id())
        {
            conn.close("Only the creator of the session can forcibly terminate it", crow::websocket::CloseStatusCode::UnacceptableData);
            return;
        }

        session->manualTerminate();
    }
    else if (action == "new_name")
    {
        const std::string name = data["name"].s();
        client->setName(name);
    }
    else if (action == "get_chunk")
    {
        const size_t chunkId = data["id"].u();
        const auto data = session->getChunk(chunkId, client);
        if (data == nullptr)
        {
            conn.send_text( SerializableEvent::GetChunkFailure{session->chunksInfo()}.json() );
        }
        else
        {
            conn.send_binary( {data->begin(), data->end()} );
        }
    }
    else if (action == "confirm_chunk")
    {
        const size_t chunkId = data["id"].u();
        session->setChunkAsReceived(chunkId, client);
    }
    else
    {
        conn.send_text( SerializableEvent::UnknownAction{action}.json() );
    }
}
