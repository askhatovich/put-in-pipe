// Copyright (C) 2025  Roman Lyubimov
// SPDX-License-Identifier: GPL-3.0-or-later
// For full license text, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#pragma once

#include "observerpattern.h"
#include "client.h"
#include "buffer.h"
#include "timercallback.h"

#include <vector>
#include <memory>
#include <list>
#include <asio.hpp>

namespace Event {

enum class TransferSession {
//  Event                      Data
    newReceiver,            // std::shared_ptr<Client>
    receiverRemoved,        // publicId
    fileInfoUpdated,        // FileInfo
    chunkDownloadStarted,   // Data::TransferSessionDownloadInfo
    chunkDownloadFinished,  // Data::TransferSessionDownloadInfo
    newChunkIsAvailable,    // Data::TransferSessionNewChunkInfo
    availableChunksUpdated, // size_t
    bytesInUpdated,         // size_t
    bytesOutUpdated,        // size_t
    chunksAreUnfrozen,      // nullptr
    fileUploadFinished,     // nullptr
    complete                // Data::TransferSessionCompleteType

};
enum class TransferSessionForSender {
    newChunkIsAllowed      // bool
};

namespace Data {
enum class TransferSessionCompleteType
{
    ok,
    timeout,
    senderIsGone,
    receiversIsGone
};

struct TransferSessionNewChunkInfo
{
    size_t chunkId = 0;
    size_t chunkSize = 0;
};

struct TransferSessionDownloadInfo
{
    std::string publicId;
    size_t chunkId = 0;
};
} // namespace Data
} // namespace Event

class TransferSession : public Subscriber<Event::ClientInternal>,
                        public Publisher<Event::TransferSession>,
                        public Publisher<Event::TransferSessionForSender>
{
public:
    ~TransferSession();

    struct FileInfo
    {
        static constexpr uint8_t NAME_MAX_LENGTH = 255;

        std::string name;
        size_t size;
    };

    std::string id() const { return m_id; }

    const FileInfo fileInfo() const;
    const std::weak_ptr<Client> sender() const { return m_dataSender; }
    const std::list<std::weak_ptr<Client>> receivers() const { return m_dataReceivers; }

    bool addReceiver(std::shared_ptr<Client> client);
    void removeReceiver(const std::string& publicId);
    bool setFileInfo(const FileInfo& info);
    bool addChunk(const std::string& binaryData);
    const std::shared_ptr<const std::vector<uint8_t>> getChunk(size_t index, std::shared_ptr<Client> client);
    void setChunkAsReceived(size_t index, std::shared_ptr<Client> client);
    void setTimedout();

    size_t bytesIn() const              { return m_buffer.bytesIn(); }
    size_t bytesOut() const             { return m_buffer.bytesOut(); }
    bool someChunkWasRemoved() const    { return m_buffer.someChunksWasRemoved(); }
    size_t currentMaxChunkIndex() const { return m_buffer.currentMaxChunkIndex(); }
    size_t chunkCount() const           { return m_buffer.chunkCount(); }
    bool newChunkIsAllowed() const      { return m_buffer.newChunkIsAllowed(); }
    void setEndOfFile();
    bool eof() const                    { return m_buffer.eof(); }
    bool initialChunksFreeze() const    { return m_buffer.initialChunksFreezing(); }
    void dropInitialChunksFreeze();
    std::chrono::seconds remainingUntilAutoDropInitialFreeze() const;

    // Subscriber interface
    void update(Event::ClientInternal event, std::any data) override;

protected:
    // we explicit use shared_ptr only (Subscriber)
    TransferSession(std::shared_ptr<Client> sender, const std::string& sessionId, asio::io_context& ioContext);

private:
    std::string m_id;
    std::weak_ptr<Client> m_dataSender;
    std::list<std::weak_ptr<Client>> m_dataReceivers;
    mutable std::shared_mutex m_fileInfoMutex;
    FileInfo m_fileInfo;
    TransferSessionDetails::Buffer m_buffer;
    mutable std::shared_mutex m_receiversMutex;
    TimerCallback m_initialFreezeTimer;

    Event::Data::TransferSessionCompleteType m_completeType = Event::Data::TransferSessionCompleteType::ok;
};


