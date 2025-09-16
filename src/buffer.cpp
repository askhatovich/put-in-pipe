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

Buffer::Buffer() : m_expectedConsumers(new AtomicSet<std::string>())
{

}

size_t Buffer::addChunk(const std::string &binaryData)
{
    if (m_EOF)
    {
        std::cerr << "Buffer::addChunk() anomaly: EOF is true" << std::endl;
        return 0;
    }

    if (binaryData.size() > Config::instance().transferSessionMaxChunkSize())
    {
        return 0;
    }

    std::unique_lock lock(m_sharedMtx);

    if (Config::instance().transferSessionChunkQueueMaxSize() <= m_chunks.size())
    {
        return 0;
    }

    auto [iterator, inserted] = m_chunks.try_emplace(++m_chunksMaxIndex,
                                                     new Chunk(m_expectedConsumers,
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

bool Buffer::setChunkAsReceived(size_t index, std::list<size_t>& removedChunks)
{
    std::unique_lock lock(m_sharedMtx);

    auto iter = m_chunks.find(index);
    if (iter == m_chunks.end())
    {
        return false;
    }

    (*iter->second).incrementUses();

    sanitize(removedChunks);

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

std::list<size_t> Buffer::chunksIndex() const
{
    std::shared_lock lock(m_sharedMtx);

    std::list<size_t> list;

    for (const auto& c: m_chunks)
    {
        list.push_back(c.first);
    }

    return list;
}

std::list<Event::Data::ChunkInfo> Buffer::chunksInfo() const
{
    std::shared_lock lock(m_sharedMtx);

    std::list<Event::Data::ChunkInfo> list;

    for (const auto& c: m_chunks)
    {
        list.push_back({c.first, c.second->dataSize()});
    }

    return list;
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

bool Buffer::addNewToExpectedConsumers(const std::string &publicId)
{
    std::unique_lock lock(m_sharedMtx);

    // If the new number is greater than the previous one, checks are required.
    if (m_someChunkWasRemoved or
        m_expectedConsumers->size()+1 > Config::instance().transferSessionMaxConsumerCount())
    {
        return false;
    }

    if (not m_expectedConsumers->add(publicId))
    {
        return false;
    }

    return true;
}

void Buffer::removeOneFromExpectedConsumers(const std::string &publicId, std::list<size_t>& removedChunks)
{
    if (m_expectedConsumers->remove(publicId))
    {
        sanitize(removedChunks);
    }
}

size_t Buffer::expectedConsumerCount() const
{
    std::shared_lock lock(m_sharedMtx);

    return m_expectedConsumers->size();
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

void Buffer::sanitize(std::list<size_t>& removed)
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

            removed.push_back(iter->first);
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
