// Copyright (C) 2025  Roman Lyubimov
// SPDX-License-Identifier: GPL-3.0-or-later
// For full license text, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#include "chunk.h"

#include <mutex>

namespace TransferSessionDetails {

Chunk::Chunk(AtomicSetSizeAccess consumerCount, const uint8_t *data, size_t size) :
    m_data(new std::vector<uint8_t>(data, data+size)),
    m_consumerExpected(consumerCount)
{

}

size_t Chunk::howMuchIsLeft() const
{
    std::shared_lock guard(m_usesMutex);

    if (m_uses > m_consumerExpected.value())
    {
        return 0;
    }

    return m_consumerExpected.value() - m_uses;
}

size_t Chunk::usesCount() const
{
    return m_uses;
}

const std::shared_ptr<const std::vector<uint8_t>> Chunk::data() const
{
    return m_data;
}

void Chunk::incrementUses()
{
    std::unique_lock guard(m_usesMutex);

    if (m_consumerExpected.value() > m_uses)
    {
        ++m_uses;
    }
}

size_t Chunk::dataSize() const
{
    return m_data->size();
}

} // namespace TransferSessionDetails
