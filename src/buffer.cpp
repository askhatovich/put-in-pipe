// Copyright (C) 2025  Roman Lyubimov
// SPDX-License-Identifier: GPL-3.0-or-later
// For full license text, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#include "buffer.h"
#include "config/config.h"
#include "chunk.h"

#include <mutex>
#include <shared_mutex>
#include <iostream>


namespace TransferSessionDetails {

Buffer::Buffer() : m_expectedConsumerCount(new std::atomic<size_t>( 0 ))
{

}

size_t Buffer::addChunk(const std::string &binaryData)
{
    if (binaryData.size() > Config::instance().transferSessionMaxChunkSize())
    {
        return 0;
    }
    if (binaryData.size() < Config::instance().apiEveryChunkOverhead())
    {
        return 0;
    }

    std::unique_lock lock(m_sharedMtx);

    if (Config::instance().transferSessionChunkQueueMaxSize() <= m_chunks.size())
    {
        return 0;
    }

    if (m_EOF)
    {
        std::cerr << "Buffer::addChunk() anomaly: EOF is true" << std::endl;
        return 0;
    }

    auto [iterator, inserted] = m_chunks.try_emplace(++m_chunksMaxIndex,
                                                     new Chunk(m_expectedConsumerCount,
                                                               reinterpret_cast<const uint8_t*>(binaryData.data()),
                                                               binaryData.size()));

    if (inserted)
    {
        /*
         * Config::instance().apiEveryChunkOverhead() is ignored because it is a shared counter,
         * not a personal one for drawing progress bars.
         */
        m_bytesInTotal += binaryData.size();
    }
    else
    {
        --m_chunksMaxIndex;
    }

    return m_chunksMaxIndex;
}

const std::shared_ptr<const std::vector<uint8_t>> Buffer::operator[](size_t index) const
{
    std::shared_lock lock(m_sharedMtx);

    auto iter = m_chunks.find(index);
    if (iter == m_chunks.end())
    {
        return nullptr;
    }

    const auto data = iter->second->data();

    m_bytesOutTotal += data->size();

    return data;
}

bool Buffer::setChunkAsReceived(size_t index)
{
    std::unique_lock lock(m_sharedMtx);

    auto iter = m_chunks.find(index);
    if (iter == m_chunks.end())
    {
        return false;
    }

    (*iter->second).incrementUses();

    sanitize();

    return true;
}

size_t Buffer::bytesIn() const
{
    return m_bytesInTotal;
}

size_t Buffer::bytesOut() const
{
    return m_bytesOutTotal;
}

size_t Buffer::currentMaxChunkIndex() const
{
    return m_chunksMaxIndex;
}

size_t Buffer::chunkCount() const
{
    std::shared_lock lock(m_sharedMtx);

    return m_chunks.size();
}

bool Buffer::newChunkIsAllowed() const
{
    return Config::instance().transferSessionChunkQueueMaxSize() < chunkCount();
}

void Buffer::setEndOfFile()
{
    std::unique_lock lock(m_sharedMtx);

    if (m_EOF)
    {
        std::cerr << "Buffer::setEndOfFile() anomaly: EOF already is true" << std::endl;
        return;
    }

    m_EOF = true;
}

bool Buffer::someChunksWasRemoved() const
{
    std::shared_lock lock(m_sharedMtx);

    return m_someChunkWasRemoved;
}

bool Buffer::setExpectedConsumerCount(size_t value)
{
    std::unique_lock lock(m_sharedMtx);

    if (*m_expectedConsumerCount == value)
    {
        return true;
    }

    if (*m_expectedConsumerCount < value)
    {
        // If the new number is greater than the previous one, checks are required.
        if (m_someChunkWasRemoved or
            value > Config::instance().transferSessionMaxConsumerCount())
        {
            return false;
        }
    }

    *m_expectedConsumerCount = value;

    sanitize();

    return true;
}

size_t Buffer::expectedConsumerCount() const
{
    std::shared_lock lock(m_sharedMtx);

    return *m_expectedConsumerCount;
}

bool Buffer::setInitialChunksFreezingDropped()
{
    std::unique_lock lock(m_sharedMtx);

    if (not m_initialChunksFreezing)
    {
        return false;
    }

    m_initialChunksFreezing = false;
    return true;
}

bool Buffer::initialChunksFreezing() const
{
    std::shared_lock lock(m_sharedMtx);

    return m_initialChunksFreezing;
}

void Buffer::sanitize()
{
    // no mutex here - called privately with upstream block

    /*
     * We do not delete the data until the initial freeze is lifted.
     * This allows new clients to connect.
     */
    if (m_initialChunksFreezing) return;

    for (auto iter = m_chunks.begin(), end = m_chunks.end(); iter != end; )
    {
        if (iter->second->howMuchIsLeft() == 0)
        {
            if (not m_someChunkWasRemoved)
            {
                m_someChunkWasRemoved = true;
            }

            iter = m_chunks.erase(iter);
        }
        else
        {
            ++iter;
        }
    }
}

bool Buffer::eof() const
{
    std::shared_lock lock(m_sharedMtx);

    return m_EOF;
}

} // namespace TransferSessionDetails
