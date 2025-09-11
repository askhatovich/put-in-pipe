#include "serializableevent.h"
#include "crowlib/crow/json.h"
#include "transfersession.h"

std::string SerializableEvent::Online::json() const
{
    crow::json::wvalue root = {
        {"event", "online"},
        {"data", {
             {"id", publicId},
             {"status", connected}
         }}
    };

    return root.dump();
}

std::string SerializableEvent::NameChanged::json() const
{
    crow::json::wvalue root = {
        {"event", "name_changed"},
        {"data", {
             {"id", publicId},
             {"name", name}
         }}
    };

    return root.dump();
}

std::string SerializableEvent::NewReceiver::json() const
{
    crow::json::wvalue root = {
        {"event", "new_receiver"},
        {"data", {
             {"id", publicId},
             {"name", name}
         }}
    };

    return root.dump();
}

std::string SerializableEvent::ReceiverRemoved::json() const
{
    crow::json::wvalue root = {
        {"event", "receiver_removed"},
        {"data", {
             {"id", publicId}
         }}
    };

    return root.dump();
}


std::string SerializableEvent::FileInfoUpdated::json() const
{
    crow::json::wvalue root = {
        {"event", "file_info"},
        {"data", {
             {"name", name},
             {"size", size}
         }}
    };

    return root.dump();
}

std::string SerializableEvent::ChunkDownload::json() const
{
    crow::json::wvalue root = {
        {"event", "chunk_download"},
        {"data", {
             {"id", publicId},
             {"index", chunkId},
             {"action", started ? "started" : "finished"}
         }}
    };

    return root.dump();
}

std::string SerializableEvent::NewChunkAvailable::json() const
{
    crow::json::wvalue root = {
        {"event", "new_chunk"},
        {"data", {
             {"index", chunkId},
             {"size", size}
         }}
    };

    return root.dump();
}

std::string SerializableEvent::ChunkCount::json() const
{
    crow::json::wvalue root = {
        {"event", "chunk_count"},
        {"data", {
             {"count", value}
         }}
    };

    return root.dump();
}

std::string SerializableEvent::UploadFinished::json() const
{
    crow::json::wvalue root = {
        {"event", "upload_finished"},
        {"data", nullptr}
    };

    return root.dump();
}

std::string SerializableEvent::SessionComplete::json() const
{
    using t = Event::Data::TransferSessionCompleteType;

    crow::json::wvalue root = {
        {"event", "complete"},
        {"data", {
            {"status", type == t::ok ? "ok" : type == t::timeout ? "timeout" :
                       type == t::senderIsGone ? "sender_is_gone" :
                       type == t::receiversIsGone ? "receivers_is_gone" : "error"}
         }}
    };

    return root.dump();
}

std::string SerializableEvent::TotalBytesCount::json() const
{
    crow::json::wvalue root = {
        {"event", "bytes_count"},
        {"data", {
             {"value", value},
             {"direction", in ? "from_sender" : "to_receivers"}
         }}
    };

    return root.dump();
}

std::string SerializableEvent::NewChunkIsAllowed::json() const
{
    // The event is for sender only
    crow::json::wvalue root = {
        {"event", "new_chunk_allowed"},
        {"data", {
            {"status", status}
        }}
    };

    return root.dump();
}

std::string SerializableEvent::ChunksAreUnfrozen::json() const
{
    crow::json::wvalue root = {
        {"event", "chunks_unfrozen"},
        {"data", nullptr}
    };

    return root.dump();
}

std::string SerializableEvent::PersonalReceivedUpdated::json() const
{
    crow::json::wvalue root = {
        {"event", "personal_received"},
        {"data", {
            {"bytes", bytes}
        }
    }};

    return root.dump();
}

std::string SerializableEvent::AddingChunkFailure::json() const
{
    crow::json::wvalue root = {
        {"event", "add_chunk_failure"},
        {"data", nullptr}
    };

    return root.dump();
}

std::string SerializableEvent::SetFileNameFailure::json() const
{
    crow::json::wvalue root = {
        {"event", "set_file_name_failure"},
        {"data", nullptr}
    };

    return root.dump();
}
