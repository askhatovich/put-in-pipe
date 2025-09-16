// Copyright (C) 2025  Roman Lyubimov
// SPDX-License-Identifier: GPL-3.0-or-later
// For full license text, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#pragma once

#include "atomicset.h"

#include <atomic>
#include <memory>
#include <shared_mutex>
#include <vector>

namespace TransferSessionDetails {

class AtomicSetSizeAccess
{
public:
    AtomicSetSizeAccess(std::shared_ptr<AtomicSet<std::string>> set)
        : m_set(set) {}
    size_t value() const { return m_set->size(); }

private:
    std::shared_ptr<AtomicSet<std::string>> m_set;
};

class Chunk
{
public:
    Chunk(AtomicSetSizeAccess consumerCount, const uint8_t* data, size_t size);

    size_t howMuchIsLeft() const;
    size_t usesCount() const;
    const std::shared_ptr<const std::vector<uint8_t>> data() const;
    void incrementUses();
    size_t dataSize() const;

private:
    const std::shared_ptr<const std::vector<uint8_t>> m_data;
    const AtomicSetSizeAccess m_consumerExpected;
    mutable std::shared_mutex m_usesMutex;
    mutable std::atomic<size_t> m_uses = 0;
};

} // namespace TransferSessionDetails
