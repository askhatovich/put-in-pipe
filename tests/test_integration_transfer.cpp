// Integration tests for the full transfer pipeline:
// ClientList -> TransferSessionList -> TransferSession -> Buffer -> Chunk lifecycle

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

#include "config/config.h"
#include "clientlist.h"
#include "transfersessionlist.h"
#include "transfersession.h"
#include "client.h"
#include "buffer.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

class TransferIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& cfg = Config::instance();
        cfg.setTransferSessionMaxChunkSize(1024 * 1024);      // 1 MB
        cfg.setTransferSessionChunkQueueMaxSize(10);           // max 10 chunks in buffer
        cfg.setTransferSessionMaxConsumerCount(5);             // max 5 receivers
        cfg.setClientTimeout(60);                              // 60 seconds
        cfg.setTransferSessionMaxLifetime(3600);               // 1 hour
        cfg.setTransferSessionMaxInitialFreezeDuration(120);   // 2 minutes
        cfg.setTransferSessionCountLimit(100);                 // max 100 sessions
        cfg.setApiMaxClientCount(500);                         // max 500 clients
    }

    void TearDown() override {
        // Remove sessions first (session destructor cleans up its clients)
        for (auto& id : createdSessionIds_) {
            TransferSessionList::instanse().remove(id);
        }
        // Remove any remaining clients that were not cleaned up by session destruction
        for (auto& id : createdClientTokens_) {
            ClientList::instanse().remove(id);
        }
        // Allow asio timers and destructor side-effects to settle
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Helper: create a client and track it for cleanup
    std::shared_ptr<Client> createClient(const std::string& token) {
        auto client = ClientList::instanse().create(token);
        if (client) {
            createdClientTokens_.push_back(token);
        }
        return client;
    }

    // Helper: create a session and track it for cleanup
    TransferSessionList::SessionAndTimeout createSession(std::shared_ptr<Client> sender) {
        auto result = TransferSessionList::instanse().create(sender);
        if (result.first) {
            createdSessionIds_.push_back(result.first->id());
        }
        return result;
    }

    std::vector<std::string> createdClientTokens_;
    std::vector<std::string> createdSessionIds_;
};

// ---------------------------------------------------------------------------
// FullTransferFlow
// End-to-end: sender creates session, receiver joins, file is uploaded
// chunk-by-chunk, receiver downloads and confirms, session completes.
// ---------------------------------------------------------------------------
TEST_F(TransferIntegrationTest, FullTransferFlow) {
    // 1. Create sender
    auto sender = createClient("sender_full_1");
    ASSERT_NE(sender, nullptr);

    // 2. Create session
    auto [session, timeout] = createSession(sender);
    ASSERT_NE(session, nullptr);
    EXPECT_GT(timeout, 0u);

    // 3. Sender joins session
    EXPECT_TRUE(sender->joinSession(session->id()));

    // 4. Create receiver
    auto receiver = createClient("receiver_full_1");
    ASSERT_NE(receiver, nullptr);

    // 5. Receiver joins session
    EXPECT_TRUE(receiver->joinSession(session->id()));

    // 6. Add receiver to session
    EXPECT_TRUE(session->addReceiver(receiver));

    // 7. Set file info
    EXPECT_TRUE(session->setFileInfo({"test.bin", 1024}));

    // 8. Drop initial freeze
    session->dropInitialChunksFreeze();
    EXPECT_FALSE(session->initialChunksFreeze());

    // 9. Upload a chunk (512 bytes of 0xAB)
    std::string chunkData(512, '\xAB');
    EXPECT_TRUE(session->addChunk(chunkData));
    EXPECT_EQ(session->currentMaxChunkIndex(), 1u);

    // 10. Download chunk - verify data matches
    auto downloaded = session->getChunk(1, receiver);
    ASSERT_NE(downloaded, nullptr);
    EXPECT_EQ(downloaded->size(), 512u);
    // Verify contents byte by byte
    for (size_t i = 0; i < downloaded->size(); ++i) {
        EXPECT_EQ((*downloaded)[i], 0xAB) << "Mismatch at byte " << i;
    }

    // 11. Set EOF (must be set before final confirmation so the auto-removal
    //     check in setChunkAsReceived sees eof == true when chunkCount hits 0)
    session->setEndOfFile();
    EXPECT_TRUE(session->eof());

    // 12. Confirm chunk received - triggers auto-removal since eof + empty buffer
    session->setChunkAsReceived(1, receiver);

    // 13. Verify session auto-removed (buffer empty + eof -> session removes itself)
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto [found, _] = TransferSessionList::instanse().get(session->id());
    EXPECT_EQ(found, nullptr);

    // Clear tracked IDs since session already self-removed
    createdSessionIds_.clear();
    createdClientTokens_.clear();
}

// ---------------------------------------------------------------------------
// BufferOverflowPrevention
// Buffer respects the chunk queue max size: once full, addChunk returns false.
// After confirming a chunk (freeing space), addChunk succeeds again.
// ---------------------------------------------------------------------------
TEST_F(TransferIntegrationTest, BufferOverflowPrevention) {
    auto sender = createClient("sender_overflow_1");
    ASSERT_NE(sender, nullptr);

    auto [session, timeout] = createSession(sender);
    ASSERT_NE(session, nullptr);

    EXPECT_TRUE(sender->joinSession(session->id()));

    auto receiver = createClient("receiver_overflow_1");
    ASSERT_NE(receiver, nullptr);
    EXPECT_TRUE(receiver->joinSession(session->id()));
    EXPECT_TRUE(session->addReceiver(receiver));
    EXPECT_TRUE(session->setFileInfo({"overflow.bin", 5000}));

    session->dropInitialChunksFreeze();

    // Fill buffer to max (queue max = 10)
    std::string smallChunk(100, '\x01');
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(session->addChunk(smallChunk)) << "Chunk " << i << " should succeed";
    }
    EXPECT_EQ(session->currentMaxChunkIndex(), 10u);

    // Buffer is full - next addChunk must fail
    EXPECT_FALSE(session->addChunk(smallChunk));
    EXPECT_FALSE(session->newChunkIsAllowed());

    // Download and confirm one chunk to free space
    auto data = session->getChunk(1, receiver);
    ASSERT_NE(data, nullptr);
    session->setChunkAsReceived(1, receiver);

    // Now there should be space
    EXPECT_TRUE(session->newChunkIsAllowed());
    EXPECT_TRUE(session->addChunk(smallChunk));
}

// ---------------------------------------------------------------------------
// ReceiverDisconnect
// One receiver is removed mid-transfer; session continues with the other.
// ---------------------------------------------------------------------------
TEST_F(TransferIntegrationTest, ReceiverDisconnect) {
    auto sender = createClient("sender_disconnect_1");
    ASSERT_NE(sender, nullptr);

    auto [session, timeout] = createSession(sender);
    ASSERT_NE(session, nullptr);

    EXPECT_TRUE(sender->joinSession(session->id()));

    auto receiver1 = createClient("receiver_disconnect_1");
    auto receiver2 = createClient("receiver_disconnect_2");
    ASSERT_NE(receiver1, nullptr);
    ASSERT_NE(receiver2, nullptr);

    EXPECT_TRUE(receiver1->joinSession(session->id()));
    EXPECT_TRUE(receiver2->joinSession(session->id()));
    EXPECT_TRUE(session->addReceiver(receiver1));
    EXPECT_TRUE(session->addReceiver(receiver2));
    EXPECT_TRUE(session->setFileInfo({"disconnect.bin", 2000}));

    session->dropInitialChunksFreeze();

    // Upload chunks
    std::string chunkData(200, '\xCC');
    EXPECT_TRUE(session->addChunk(chunkData));
    EXPECT_TRUE(session->addChunk(chunkData));

    // Both receivers download chunk 1
    auto d1 = session->getChunk(1, receiver1);
    auto d2 = session->getChunk(1, receiver2);
    ASSERT_NE(d1, nullptr);
    ASSERT_NE(d2, nullptr);

    // Receiver1 confirms chunk 1
    session->setChunkAsReceived(1, receiver1);

    // Remove receiver1 - session should continue with receiver2
    std::string receiver1PublicId = receiver1->publicId();
    session->removeReceiver(receiver1PublicId);

    // Give async effects time to settle
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Session should still exist
    auto [found, _] = TransferSessionList::instanse().get(session->id());
    EXPECT_NE(found, nullptr);

    // Receiver2 can still download and confirm
    auto d2_chunk2 = session->getChunk(2, receiver2);
    ASSERT_NE(d2_chunk2, nullptr);
    session->setChunkAsReceived(1, receiver2);

    // Set EOF before the last confirmation so auto-removal triggers
    session->setEndOfFile();

    session->setChunkAsReceived(2, receiver2);

    // Session should auto-complete (all chunks confirmed, eof set)
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto [found2, _2] = TransferSessionList::instanse().get(session->id());
    EXPECT_EQ(found2, nullptr);

    createdSessionIds_.clear();
    createdClientTokens_.clear();
}

// ---------------------------------------------------------------------------
// MultipleReceivers
// Chunk is only removed from buffer once ALL receivers have confirmed it.
// ---------------------------------------------------------------------------
TEST_F(TransferIntegrationTest, MultipleReceivers) {
    auto sender = createClient("sender_multi_1");
    ASSERT_NE(sender, nullptr);

    auto [session, timeout] = createSession(sender);
    ASSERT_NE(session, nullptr);

    EXPECT_TRUE(sender->joinSession(session->id()));

    auto recv1 = createClient("receiver_multi_1");
    auto recv2 = createClient("receiver_multi_2");
    auto recv3 = createClient("receiver_multi_3");
    ASSERT_NE(recv1, nullptr);
    ASSERT_NE(recv2, nullptr);
    ASSERT_NE(recv3, nullptr);

    EXPECT_TRUE(recv1->joinSession(session->id()));
    EXPECT_TRUE(recv2->joinSession(session->id()));
    EXPECT_TRUE(recv3->joinSession(session->id()));
    EXPECT_TRUE(session->addReceiver(recv1));
    EXPECT_TRUE(session->addReceiver(recv2));
    EXPECT_TRUE(session->addReceiver(recv3));
    EXPECT_TRUE(session->setFileInfo({"multi.bin", 3000}));

    session->dropInitialChunksFreeze();

    // Upload a chunk
    std::string chunkData(256, '\xDD');
    EXPECT_TRUE(session->addChunk(chunkData));

    // All three receivers download the chunk
    ASSERT_NE(session->getChunk(1, recv1), nullptr);
    ASSERT_NE(session->getChunk(1, recv2), nullptr);
    ASSERT_NE(session->getChunk(1, recv3), nullptr);

    // Receiver 1 confirms - chunk should NOT be removed yet
    session->setChunkAsReceived(1, recv1);
    EXPECT_FALSE(session->someChunkWasRemoved());

    // Receiver 2 confirms - still not removed (recv3 hasn't confirmed)
    session->setChunkAsReceived(1, recv2);
    EXPECT_FALSE(session->someChunkWasRemoved());

    // Receiver 3 confirms - NOW the chunk should be removed
    session->setChunkAsReceived(1, recv3);
    EXPECT_TRUE(session->someChunkWasRemoved());

    // Verify the chunk is actually gone from the buffer
    auto gone = session->getChunk(1, recv1);
    EXPECT_EQ(gone, nullptr);
}

// ---------------------------------------------------------------------------
// RejectReceiverAfterChunkRemoved
// Once any chunk has been removed from the buffer, adding a new receiver
// must fail because the new receiver would miss data.
// ---------------------------------------------------------------------------
TEST_F(TransferIntegrationTest, RejectReceiverAfterChunkRemoved) {
    auto sender = createClient("sender_reject_1");
    ASSERT_NE(sender, nullptr);

    auto [session, timeout] = createSession(sender);
    ASSERT_NE(session, nullptr);

    EXPECT_TRUE(sender->joinSession(session->id()));

    auto receiver = createClient("receiver_reject_1");
    ASSERT_NE(receiver, nullptr);
    EXPECT_TRUE(receiver->joinSession(session->id()));
    EXPECT_TRUE(session->addReceiver(receiver));
    EXPECT_TRUE(session->setFileInfo({"reject.bin", 1000}));

    session->dropInitialChunksFreeze();

    // Upload a chunk
    std::string chunkData(128, '\xEE');
    EXPECT_TRUE(session->addChunk(chunkData));

    // Receiver downloads and confirms - chunk gets removed
    ASSERT_NE(session->getChunk(1, receiver), nullptr);
    session->setChunkAsReceived(1, receiver);
    EXPECT_TRUE(session->someChunkWasRemoved());

    // Try to add a late receiver - should fail
    auto lateReceiver = createClient("receiver_reject_late_1");
    ASSERT_NE(lateReceiver, nullptr);
    EXPECT_FALSE(session->addReceiver(lateReceiver));
}

// ---------------------------------------------------------------------------
// InitialFreezePreservesChunks
// While initial freeze is active, confirmed chunks are NOT removed from
// the buffer. After the freeze is dropped, they get cleaned up.
// ---------------------------------------------------------------------------
TEST_F(TransferIntegrationTest, InitialFreezePreservesChunks) {
    auto sender = createClient("sender_freeze_1");
    ASSERT_NE(sender, nullptr);

    auto [session, timeout] = createSession(sender);
    ASSERT_NE(session, nullptr);

    EXPECT_TRUE(sender->joinSession(session->id()));

    auto receiver = createClient("receiver_freeze_1");
    ASSERT_NE(receiver, nullptr);
    EXPECT_TRUE(receiver->joinSession(session->id()));
    EXPECT_TRUE(session->addReceiver(receiver));
    EXPECT_TRUE(session->setFileInfo({"freeze.bin", 2000}));

    // Do NOT drop freeze - it should remain active
    EXPECT_TRUE(session->initialChunksFreeze());

    // Upload chunks
    std::string chunkData(150, '\xFF');
    EXPECT_TRUE(session->addChunk(chunkData));
    EXPECT_TRUE(session->addChunk(chunkData));
    EXPECT_EQ(session->currentMaxChunkIndex(), 2u);

    // Receiver downloads and confirms both chunks
    ASSERT_NE(session->getChunk(1, receiver), nullptr);
    ASSERT_NE(session->getChunk(2, receiver), nullptr);
    session->setChunkAsReceived(1, receiver);
    session->setChunkAsReceived(2, receiver);

    // Chunks should NOT be removed because freeze is still active
    EXPECT_FALSE(session->someChunkWasRemoved());
    EXPECT_NE(session->getChunk(1, receiver), nullptr);
    EXPECT_NE(session->getChunk(2, receiver), nullptr);

    // Drop the freeze
    session->dropInitialChunksFreeze();
    EXPECT_FALSE(session->initialChunksFreeze());

    // The chunks were already fully confirmed before freeze was dropped.
    // However, sanitize only runs during setChunkAsReceived or removeOneFromExpectedConsumers.
    // We need to trigger a sanitize pass. Upload one more chunk and confirm it.
    EXPECT_TRUE(session->addChunk(chunkData));
    ASSERT_NE(session->getChunk(3, receiver), nullptr);
    session->setChunkAsReceived(3, receiver);

    // Now all confirmed chunks (including the previously frozen ones) should be removed
    EXPECT_TRUE(session->someChunkWasRemoved());
    EXPECT_EQ(session->getChunk(1, receiver), nullptr);
    EXPECT_EQ(session->getChunk(2, receiver), nullptr);
}
