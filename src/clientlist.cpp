// Copyright (C) 2025  Roman Lyubimov
// SPDX-License-Identifier: GPL-3.0-or-later
// For full license text, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#include "clientlist.h"
#include "client.h"
#include "observerpattern.h"
#include "captcha/skaptcha_tools.h"

#include <mutex>
#include <chrono>
#include <random>

ClientList::~ClientList()
{
    m_ioContext.stop();
    m_ioContextThreadPtr->join();
    delete m_ioContextThreadPtr;
}

ClientList &ClientList::instanse()
{
    static ClientList list;
    return list;
}

std::string ClientList::generateIdCondidate(const std::string &seed)
{
    const auto timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const std::string combinedSeed = seed + std::to_string(timestamp);

    const size_t seedHash = std::hash<std::string>()(combinedSeed);

    std::mt19937_64 generator(seedHash);
    std::uniform_int_distribution<uint8_t> distribution(0, 255);

    std::string binary;
    binary.resize(ID_LENGTH_BYTES);

    for (uint8_t i = 0; i < ID_LENGTH_BYTES; ++i)
    {
        binary[i] = distribution(generator);
    }

    return skaptcha_tools::base64::encodeUrlsafe(binary);
}

std::shared_ptr<Client> ClientList::create(const std::string &token)
{
    std::unique_lock lock (m_mutex);

    if (m_map.contains(token))
    {
        return nullptr;
    }

    auto client = createSubscriber<Client>(token, m_ioContext, [&, token](){ this->remove(token); });

    m_map[token] = client;

    return client;
}

std::shared_ptr<Client> ClientList::get(const std::string &token) const
{
    std::shared_lock lock (m_mutex);

    const auto iter = m_map.find(token);
    if (iter == m_map.end())
    {
        return nullptr;
    }

    return iter->second;
}

size_t ClientList::count() const
{
    std::shared_lock lock (m_mutex);
    return m_map.size();
}

void ClientList::remove(const std::string &id)
{
    /*
     * A design to avoid deadlocks on recursive calls to this function
     */

    std::shared_ptr<Client> client = nullptr;
    {
        std::unique_lock lock (m_mutex);
        auto iter = m_map.find(id);
        if (iter == m_map.end())
        {
            return;
        }
        client = iter->second;
        m_map.erase(iter);
    }
}

ClientList::ClientList()
{
    m_ioContextThreadPtr = new std::thread([&]() {
        asio::executor_work_guard<asio::io_context::executor_type> work = asio::make_work_guard(m_ioContext);
        m_ioContext.run();
    });
}
