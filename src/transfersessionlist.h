// Copyright (C) 2025  Roman Lyubimov
// SPDX-License-Identifier: GPL-3.0-or-later
// For full license text, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#pragma once

#include "timercallback.h"

#include <string>
#include <shared_mutex>
#include <unordered_map>
#include <memory>
#include <thread>

class TransferSession;
class Client;

class TransferSessionList
{
public:
    ~TransferSessionList();

    static TransferSessionList& instanse();

    using SessionAndTimeout = std::pair<std::shared_ptr<TransferSession>, size_t/*remaining*/>;

    SessionAndTimeout create(std::shared_ptr<Client> creator);
    SessionAndTimeout get(const std::string& id);
    size_t count() const;
    bool possibleToCreateNew();

    void remove(const std::string& id);

private:
    TransferSessionList();
    TransferSessionList(const TransferSessionList&) = delete;
    TransferSessionList(TransferSessionList&&) = delete;
    TransferSessionList& operator=(const TransferSessionList&) = delete;

    void removeDueTimeout(const std::string& id);

    struct SessionWithTimer
    {
        SessionWithTimer(std::shared_ptr<TransferSession> s, TimerCallback&& t)
            : session(s), timer(std::move(t)) {}
        SessionWithTimer(SessionWithTimer && another) noexcept
            : session(another.session), timer(std::move(another.timer)) {}

        std::shared_ptr<TransferSession> session = nullptr;
        TimerCallback timer;
    };

    mutable std::shared_mutex m_mutex;
    std::unordered_map<std::string, SessionWithTimer> m_map;

    asio::io_context m_ioContext;
    std::thread* m_ioContextThreadPtr;
};
