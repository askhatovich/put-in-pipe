// Copyright (C) 2025  Roman Lyubimov
// SPDX-License-Identifier: GPL-3.0-or-later
// For full license text, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#pragma once

#include <set>
#include <shared_mutex>
#include <mutex> // unique_lock

namespace TransferSessionDetails {

template<typename T>
class AtomicSet
{
public:
    size_t size() const
    {
        std::shared_lock lock (m_mutex);
        return m_set.size();
    }

    bool remove(const T& value)
    {
        std::unique_lock lock (m_mutex);
        return m_set.erase(value) > 0;
    }

    bool add(const T& value)
    {
        std::unique_lock lock (m_mutex);
        const auto [iter, inserted] = m_set.insert(value);
        return inserted;
    }

private:
    std::set<T> m_set;
    mutable std::shared_mutex m_mutex;
};

}
