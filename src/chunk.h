// Copyright (C) 2025  Roman Lyubimov
// SPDX-License-Identifier: GPL-3.0-or-later
// For full license text, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#pragma once

#include <atomic>
#include <memory>
#include <shared_mutex>
#include <vector>

namespace TransferSessionDetails {

class SharedExpectedConsumerCount
{
public:
    SharedExpectedConsumerCount(std::shared_ptr<std::atomic<size_t>> sourceNumber)
        : m_number(sourceNumber) {}
    size_t value() const { return *m_number; }

private:
    std::shared_ptr<std::atomic<size_t>> m_number;
};

class Chunk
{
public:
    Chunk(SharedExpectedConsumerCount consumerCount, const uint8_t* data, size_t size);

    size_t howMuchIsLeft() const;
    size_t usesCount() const;
    const std::shared_ptr<const std::vector<uint8_t>> data() const;
    void incrementUses();
    size_t dataSize() const;

private:
    const std::shared_ptr<const std::vector<uint8_t>> m_data;
    const SharedExpectedConsumerCount m_consumerExpected;
    mutable std::shared_mutex m_usesMutex;
    mutable std::atomic<size_t> m_uses = 0;
};

} // namespace TransferSessionDetails
