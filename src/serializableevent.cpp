#include "serializableevent.h"
#include "crowlib/crow/json.h"
#include "transfersession.h"
#include "buffer.h"

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

std::string SerializableEvent::ChunksRemoved::json() const
{
    crow::json::wvalue idxs;
    size_t index = 0;
    for (const auto& id: list)
    {
        idxs[index++] = id;
    }

    crow::json::wvalue root = {
        {"event", "chunk_removed"},
        {"data", {
            {"id", std::move(idxs)}
        }}
    };

    return root.dump();
}

std::string SerializableEvent::UploadFinished::json() const
{
    crow::json::wvalue root = {
        {"event", "upload_finished"},
        {"data", crow::json::wvalue::empty_object()}
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
        {"data", crow::json::wvalue::empty_object()}
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
        {"data", crow::json::wvalue::empty_object()}
    };

    return root.dump();
}

std::string SerializableEvent::SetFileInfoFailure::json() const
{
    crow::json::wvalue root = {
        {"event", "set_file_info_failure"},
        {"data", crow::json::wvalue::empty_object()}
    };

    return root.dump();
}

std::string SerializableEvent::GetChunkFailure::json() const
{
    crow::json::wvalue infoJson;
    size_t counter = 0;
    for (const auto& info: list)
    {
        infoJson[counter]["index"] = info.index;
        infoJson[counter]["size"] = info.size;

        ++counter;
    }

    crow::json::wvalue root = {
        {"event", "requested_chunk_not_found"},
        {"data", {
            {"available", std::move(infoJson)}
        }}
    };

    return root.dump();
}

std::string SerializableEvent::UnknownAction::json() const
{
    crow::json::wvalue root = {
        {"event", "unknown_action"},
        {"data", {
            {"name", action}
        }
    }};

    return root.dump();
}

