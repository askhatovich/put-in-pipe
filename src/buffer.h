// Copyright (C) 2025  Roman Lyubimov
// SPDX-License-Identifier: GPL-3.0-or-later
// For full license text, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#pragma once

#include <map>
#include <shared_mutex>
#include <memory>
#include <atomic>
#include <vector>

namespace TransferSessionDetails {

class Chunk;

class Buffer
{
public:
    Buffer();

    // return index of new chunk or 0
    size_t addChunk(const std::string& binaryData);
    const std::shared_ptr<const std::vector<uint8_t>> operator[](size_t index) const;
    bool setChunkAsReceived(size_t index);

    size_t bytesIn() const;
    size_t bytesOut() const;

    size_t currentMaxChunkIndex() const;
    size_t chunkCount() const;
    bool newChunkIsAllowed() const;

    void setEndOfFile();
    bool eof() const;

    bool setInitialChunksFreezingDropped();
    bool initialChunksFreezing() const;

    bool someChunksWasRemoved() const;
    bool setExpectedConsumerCount(size_t value);
    size_t expectedConsumerCount() const;

private:
    mutable std::shared_mutex m_sharedMtx;

    std::shared_ptr<std::atomic<size_t>> m_expectedConsumerCount;
    std::map<size_t, std::shared_ptr<Chunk>> m_chunks;
    size_t m_chunksMaxIndex = 0;
    size_t m_bytesInTotal = 0;
    mutable size_t m_bytesOutTotal = 0;
    bool m_someChunkWasRemoved = false;
    bool m_EOF = false;

    /* The freeze is necessary so that new users can connect.
     * If the freeze is lifted, the downloaded chunks
     * by current users will be deleted and connecting new users
     * will become impossible, since some of the data has already been deleted.
     */
    bool m_initialChunksFreezing = true;

    void sanitize();
};

} // namespace TransferSessionDetails
