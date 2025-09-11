#pragma once

#include <string>

namespace Event {
namespace Data { enum class TransferSessionCompleteType; }
}

namespace SerializableEvent {

struct Online
{
    std::string publicId;
    bool connected = false;

    std::string json() const;
};

struct NameChanged
{
    std::string publicId;
    std::string name;

    std::string json() const;
};

struct NewReceiver
{
    std::string publicId;
    std::string name;

    std::string json() const;
};

struct ReceiverRemoved
{
    std::string publicId;

    std::string json() const;
};

struct FileInfoUpdated
{
    std::string name;
    size_t size = 0;

    std::string json() const;
};

struct ChunkDownload
{
    std::string publicId;
    size_t chunkId = 0;
    bool started = false; // else finished

    std::string json() const;
};

struct NewChunkAvailable
{
    size_t chunkId = 0;
    size_t size = 0;

    std::string json() const;
};

struct ChunkCount
{
    size_t value = 0;

    std::string json() const;
};

struct UploadFinished
{
    std::string json() const;
};

struct SessionComplete
{
    Event::Data::TransferSessionCompleteType type;

    std::string json() const;
};

struct TotalBytesCount
{
    size_t value = 0;
    bool in = false; // else - out

    std::string json() const;
};

// The event is for sender only
struct NewChunkIsAllowed
{
    bool status = false;

    std::string json() const;
};

struct ChunksAreUnfrozen
{
    std::string json() const;
};

struct PersonalReceivedUpdated
{
    size_t bytes = 0;

    std::string json() const;
};

struct AddingChunkFailure
{
    std::string json() const;
};

struct SetFileNameFailure
{
    std::string json() const;
};


} // namespace SerializableEvent
