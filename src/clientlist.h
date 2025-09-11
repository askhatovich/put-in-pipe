// Copyright (C) 2025  Roman Lyubimov
// SPDX-License-Identifier: GPL-3.0-or-later
// For full license text, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#pragma once

#include <string>
#include <shared_mutex>
#include <unordered_map>
#include <memory>
#include <thread>
#include <asio.hpp>

class Client;

class ClientList
{
public:
    static constexpr uint8_t ID_LENGTH_BYTES = 18;

    ~ClientList();

    static ClientList& instanse();
    static std::string generateIdCondidate(const std::string& ipAddressAsASeed);

    std::shared_ptr<Client> create(const std::string& id);
    std::shared_ptr<Client> get(const std::string& id) const;
    size_t count() const;
    void remove(const std::string& id);

private:
    ClientList();
    ClientList(const ClientList&) = delete;
    ClientList(ClientList&&) = delete;
    ClientList& operator=(const ClientList&) = delete;

    std::thread* m_ioContextThreadPtr = nullptr;
    asio::io_context m_ioContext;

    mutable std::shared_mutex m_mutex;
    std::unordered_map<std::string, std::shared_ptr<Client>> m_map;
};
