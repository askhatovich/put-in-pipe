#pragma once

#include <string>
#include <list>

namespace Event {
namespace Data {
enum class TransferSessionCompleteType;
struct ChunkInfo;}
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

struct ChunksRemoved
{
    std::list<size_t> list;

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

struct SetFileInfoFailure
{
    std::string json() const;
};

struct GetChunkFailure
{
    std::list<Event::Data::ChunkInfo> list;

    std::string json() const;
};

struct UnknownAction
{
    std::string action;

    std::string json() const;
};


} // namespace SerializableEvent
