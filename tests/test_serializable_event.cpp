// Copyright (C) 2025  Roman Lyubimov
// SPDX-License-Identifier: GPL-3.0-or-later
// For full license text, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#include <gtest/gtest.h>

#include "log.h"
#include <plog/Appenders/ConsoleAppender.h>
struct PlogInit {
    PlogInit() {
        static plog::ConsoleAppender<plog::TxtFormatter> appender;
        static bool initialized = false;
        if (!initialized) {
            plog::init(plog::none, &appender);
            initialized = true;
        }
    }
};
static PlogInit plogInit;

#include "serializableevent.h"
#include "transfersession.h"
#include "crowlib/crow/json.h"

TEST(SerializableEventTest, OnlineEvent) {
    SerializableEvent::Online evt{"user123", true};
    auto json = crow::json::load(evt.json());
    ASSERT_TRUE(json);
    EXPECT_EQ(json["event"].s(), "online");
    EXPECT_EQ(json["data"]["id"].s(), "user123");
    EXPECT_EQ(json["data"]["status"].b(), true);

    SerializableEvent::Online evtOff{"user456", false};
    auto jsonOff = crow::json::load(evtOff.json());
    ASSERT_TRUE(jsonOff);
    EXPECT_EQ(jsonOff["data"]["status"].b(), false);
}

TEST(SerializableEventTest, NameChangedEvent) {
    SerializableEvent::NameChanged evt{"user123", "Alice"};
    auto json = crow::json::load(evt.json());
    ASSERT_TRUE(json);
    EXPECT_EQ(json["event"].s(), "name_changed");
    EXPECT_EQ(json["data"]["id"].s(), "user123");
    EXPECT_EQ(json["data"]["name"].s(), "Alice");
}

TEST(SerializableEventTest, NewReceiverEvent) {
    SerializableEvent::NewReceiver evt{"recv1", "Bob"};
    auto json = crow::json::load(evt.json());
    ASSERT_TRUE(json);
    EXPECT_EQ(json["event"].s(), "new_receiver");
    EXPECT_EQ(json["data"]["id"].s(), "recv1");
    EXPECT_EQ(json["data"]["name"].s(), "Bob");
}

TEST(SerializableEventTest, ReceiverRemovedEvent) {
    SerializableEvent::ReceiverRemoved evt{"recv1"};
    auto json = crow::json::load(evt.json());
    ASSERT_TRUE(json);
    EXPECT_EQ(json["event"].s(), "receiver_removed");
    EXPECT_EQ(json["data"]["id"].s(), "recv1");
}

TEST(SerializableEventTest, FileInfoUpdatedEvent) {
    SerializableEvent::FileInfoUpdated evt{"document.pdf", 1024};
    auto json = crow::json::load(evt.json());
    ASSERT_TRUE(json);
    EXPECT_EQ(json["event"].s(), "file_info");
    EXPECT_EQ(json["data"]["name"].s(), "document.pdf");
    EXPECT_EQ(json["data"]["size"].i(), 1024);
}

TEST(SerializableEventTest, ChunkDownloadEvent) {
    // Started
    SerializableEvent::ChunkDownload evtStarted{"user1", 5, true};
    auto jsonStarted = crow::json::load(evtStarted.json());
    ASSERT_TRUE(jsonStarted);
    EXPECT_EQ(jsonStarted["event"].s(), "chunk_download");
    EXPECT_EQ(jsonStarted["data"]["id"].s(), "user1");
    EXPECT_EQ(jsonStarted["data"]["index"].i(), 5);
    EXPECT_EQ(jsonStarted["data"]["action"].s(), "started");

    // Finished
    SerializableEvent::ChunkDownload evtFinished{"user2", 10, false};
    auto jsonFinished = crow::json::load(evtFinished.json());
    ASSERT_TRUE(jsonFinished);
    EXPECT_EQ(jsonFinished["data"]["action"].s(), "finished");
}

TEST(SerializableEventTest, NewChunkAvailableEvent) {
    SerializableEvent::NewChunkAvailable evt{3, 512};
    auto json = crow::json::load(evt.json());
    ASSERT_TRUE(json);
    EXPECT_EQ(json["event"].s(), "new_chunk");
    EXPECT_EQ(json["data"]["index"].i(), 3);
    EXPECT_EQ(json["data"]["size"].i(), 512);
}

TEST(SerializableEventTest, ChunksRemovedEvent) {
    SerializableEvent::ChunksRemoved evt{{1, 3, 7}};
    auto json = crow::json::load(evt.json());
    ASSERT_TRUE(json);
    EXPECT_EQ(json["event"].s(), "chunk_removed");
    // data.id is an array
    auto arr = json["data"]["id"];
    EXPECT_EQ(arr.size(), 3u);
    EXPECT_EQ(arr[0].i(), 1);
    EXPECT_EQ(arr[1].i(), 3);
    EXPECT_EQ(arr[2].i(), 7);
}

TEST(SerializableEventTest, UploadFinishedEvent) {
    SerializableEvent::UploadFinished evt;
    auto json = crow::json::load(evt.json());
    ASSERT_TRUE(json);
    EXPECT_EQ(json["event"].s(), "upload_finished");
}

TEST(SerializableEventTest, SessionCompleteEvent) {
    using Type = Event::Data::TransferSessionCompleteType;

    {
        SerializableEvent::SessionComplete evt{Type::ok};
        auto json = crow::json::load(evt.json());
        ASSERT_TRUE(json);
        EXPECT_EQ(json["event"].s(), "complete");
        EXPECT_EQ(json["data"]["status"].s(), "ok");
    }
    {
        SerializableEvent::SessionComplete evt{Type::timeout};
        auto json = crow::json::load(evt.json());
        ASSERT_TRUE(json);
        EXPECT_EQ(json["data"]["status"].s(), "timeout");
    }
    {
        SerializableEvent::SessionComplete evt{Type::senderIsGone};
        auto json = crow::json::load(evt.json());
        ASSERT_TRUE(json);
        EXPECT_EQ(json["data"]["status"].s(), "sender_is_gone");
    }
    {
        SerializableEvent::SessionComplete evt{Type::noReceivers};
        auto json = crow::json::load(evt.json());
        ASSERT_TRUE(json);
        EXPECT_EQ(json["data"]["status"].s(), "no_receivers");
    }
}

TEST(SerializableEventTest, TotalBytesCountEvent) {
    // direction: in = from_sender
    SerializableEvent::TotalBytesCount evtIn{2048, true};
    auto jsonIn = crow::json::load(evtIn.json());
    ASSERT_TRUE(jsonIn);
    EXPECT_EQ(jsonIn["event"].s(), "bytes_count");
    EXPECT_EQ(jsonIn["data"]["value"].i(), 2048);
    EXPECT_EQ(jsonIn["data"]["direction"].s(), "from_sender");

    // direction: out = to_receivers
    SerializableEvent::TotalBytesCount evtOut{4096, false};
    auto jsonOut = crow::json::load(evtOut.json());
    ASSERT_TRUE(jsonOut);
    EXPECT_EQ(jsonOut["data"]["direction"].s(), "to_receivers");
}

TEST(SerializableEventTest, NewChunkIsAllowedEvent) {
    SerializableEvent::NewChunkIsAllowed evtTrue{true};
    auto jsonTrue = crow::json::load(evtTrue.json());
    ASSERT_TRUE(jsonTrue);
    EXPECT_EQ(jsonTrue["event"].s(), "new_chunk_allowed");
    EXPECT_EQ(jsonTrue["data"]["status"].b(), true);

    SerializableEvent::NewChunkIsAllowed evtFalse{false};
    auto jsonFalse = crow::json::load(evtFalse.json());
    ASSERT_TRUE(jsonFalse);
    EXPECT_EQ(jsonFalse["data"]["status"].b(), false);
}

TEST(SerializableEventTest, ChunksAreUnfrozenEvent) {
    SerializableEvent::ChunksAreUnfrozen evt;
    auto json = crow::json::load(evt.json());
    ASSERT_TRUE(json);
    EXPECT_EQ(json["event"].s(), "chunks_unfrozen");
}

TEST(SerializableEventTest, PersonalReceivedUpdatedEvent) {
    SerializableEvent::PersonalReceivedUpdated evt{8192};
    auto json = crow::json::load(evt.json());
    ASSERT_TRUE(json);
    EXPECT_EQ(json["event"].s(), "personal_received");
    EXPECT_EQ(json["data"]["bytes"].i(), 8192);
}

TEST(SerializableEventTest, AddingChunkFailureEvent) {
    SerializableEvent::AddingChunkFailure evt;
    auto json = crow::json::load(evt.json());
    ASSERT_TRUE(json);
    EXPECT_EQ(json["event"].s(), "add_chunk_failure");
}

TEST(SerializableEventTest, SetFileInfoFailureEvent) {
    SerializableEvent::SetFileInfoFailure evt;
    auto json = crow::json::load(evt.json());
    ASSERT_TRUE(json);
    EXPECT_EQ(json["event"].s(), "set_file_info_failure");
}

TEST(SerializableEventTest, UnknownActionEvent) {
    SerializableEvent::UnknownAction evt{"do_something"};
    auto json = crow::json::load(evt.json());
    ASSERT_TRUE(json);
    EXPECT_EQ(json["event"].s(), "unknown_action");
    EXPECT_EQ(json["data"]["name"].s(), "do_something");
}
