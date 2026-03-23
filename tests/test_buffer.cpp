// Tests for TransferSessionDetails::Buffer

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

#include "buffer.h"
#include "chunk.h"
#include "config/config.h"

#include <gtest/gtest.h>
#include <string>
#include <list>
#include <vector>

using TransferSessionDetails::Buffer;

class BufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        Config::instance().setTransferSessionMaxChunkSize(1024 * 1024);
        Config::instance().setTransferSessionChunkQueueMaxSize(10);
        Config::instance().setTransferSessionMaxConsumerCount(5);
    }

    Buffer buffer;
};

// addChunk with valid data returns non-zero index
TEST_F(BufferTest, AddChunkValidDataReturnsNonZero) {
    std::string data(100, 'A');
    size_t index = buffer.addChunk(data);
    EXPECT_NE(index, 0u);
    EXPECT_EQ(index, 1u);
}

// addChunk returns sequential indices
TEST_F(BufferTest, AddChunkReturnsSequentialIndices) {
    std::string data(100, 'A');
    EXPECT_EQ(buffer.addChunk(data), 1u);
    EXPECT_EQ(buffer.addChunk(data), 2u);
    EXPECT_EQ(buffer.addChunk(data), 3u);
}

// addChunk with oversized data returns 0
TEST_F(BufferTest, AddChunkOversizedDataReturnsZero) {
    // Max chunk size is 1MB, create data slightly larger
    std::string oversized(1024 * 1024 + 1, 'X');
    size_t index = buffer.addChunk(oversized);
    EXPECT_EQ(index, 0u);
    EXPECT_EQ(buffer.chunkCount(), 0u);
}

// addChunk at exactly max size succeeds
TEST_F(BufferTest, AddChunkExactMaxSizeSucceeds) {
    std::string exactMax(1024 * 1024, 'X');
    size_t index = buffer.addChunk(exactMax);
    EXPECT_NE(index, 0u);
}

// addChunk when queue full returns 0
TEST_F(BufferTest, AddChunkQueueFullReturnsZero) {
    std::string data(100, 'A');
    // Fill the queue to capacity (max size = 10)
    for (int i = 0; i < 10; ++i) {
        EXPECT_NE(buffer.addChunk(data), 0u);
    }
    EXPECT_EQ(buffer.chunkCount(), 10u);

    // 11th chunk should fail
    size_t index = buffer.addChunk(data);
    EXPECT_EQ(index, 0u);
    EXPECT_EQ(buffer.chunkCount(), 10u);
}

// addChunk after EOF returns 0
TEST_F(BufferTest, AddChunkAfterEOFReturnsZero) {
    buffer.setEndOfFile();
    std::string data(100, 'A');
    size_t index = buffer.addChunk(data);
    EXPECT_EQ(index, 0u);
}

// newChunkIsAllowed returns true when space available
TEST_F(BufferTest, NewChunkIsAllowedWhenSpaceAvailable) {
    EXPECT_TRUE(buffer.newChunkIsAllowed());

    std::string data(100, 'A');
    buffer.addChunk(data);
    EXPECT_TRUE(buffer.newChunkIsAllowed());
}

// newChunkIsAllowed returns false when full
TEST_F(BufferTest, NewChunkIsAllowedReturnsFalseWhenFull) {
    std::string data(100, 'A');
    for (int i = 0; i < 10; ++i) {
        buffer.addChunk(data);
    }
    EXPECT_FALSE(buffer.newChunkIsAllowed());
}

// operator[] returns data for valid index
TEST_F(BufferTest, SubscriptReturnsDataForValidIndex) {
    std::string data = "Hello, World!";
    size_t index = buffer.addChunk(data);

    auto result = buffer[index];
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->size(), data.size());

    // Verify content matches
    std::string retrieved(result->begin(), result->end());
    EXPECT_EQ(retrieved, data);
}

// operator[] returns nullptr for invalid index
TEST_F(BufferTest, SubscriptReturnsNullptrForInvalidIndex) {
    auto result = buffer[999];
    EXPECT_EQ(result, nullptr);
}

// operator[] returns nullptr for index 0 (no chunk at index 0)
TEST_F(BufferTest, SubscriptReturnsNullptrForIndexZero) {
    std::string data(100, 'A');
    buffer.addChunk(data);
    auto result = buffer[0];
    EXPECT_EQ(result, nullptr);
}

// setChunkAsReceived triggers cleanup after all consumers received
TEST_F(BufferTest, SetChunkAsReceivedTriggersCleanup) {
    // Add a consumer
    EXPECT_TRUE(buffer.addNewToExpectedConsumers("consumer1"));
    EXPECT_EQ(buffer.expectedConsumerCount(), 1u);

    // Drop freeze so sanitize can actually remove chunks
    std::list<size_t> removedChunks;
    buffer.setInitialChunksFreezingDropped(removedChunks);
    EXPECT_FALSE(buffer.initialChunksFreezing());

    // Add a chunk
    std::string data(100, 'A');
    size_t index = buffer.addChunk(data);
    ASSERT_NE(index, 0u);
    EXPECT_EQ(buffer.chunkCount(), 1u);

    // Mark chunk as received by the only consumer
    bool result = buffer.setChunkAsReceived(index, removedChunks);
    EXPECT_TRUE(result);

    // Chunk should have been removed by sanitize
    EXPECT_EQ(removedChunks.size(), 1u);
    EXPECT_EQ(removedChunks.front(), index);
    EXPECT_EQ(buffer.chunkCount(), 0u);
}

// setChunkAsReceived returns false for invalid index
TEST_F(BufferTest, SetChunkAsReceivedReturnsFalseForInvalid) {
    std::list<size_t> removedChunks;
    EXPECT_FALSE(buffer.setChunkAsReceived(999, removedChunks));
    EXPECT_TRUE(removedChunks.empty());
}

// setEndOfFile prevents further additions
TEST_F(BufferTest, SetEndOfFilePreventsFurtherAdditions) {
    std::string data(100, 'A');
    EXPECT_NE(buffer.addChunk(data), 0u);

    buffer.setEndOfFile();
    EXPECT_TRUE(buffer.eof());

    EXPECT_EQ(buffer.addChunk(data), 0u);
}

// EOF state
TEST_F(BufferTest, EOFStateInitiallyFalse) {
    EXPECT_FALSE(buffer.eof());
}

TEST_F(BufferTest, EOFStateAfterSet) {
    buffer.setEndOfFile();
    EXPECT_TRUE(buffer.eof());
}

// addNewToExpectedConsumers respects max count
TEST_F(BufferTest, AddConsumersRespectsMaxCount) {
    // Max consumer count is 5
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(buffer.addNewToExpectedConsumers("consumer" + std::to_string(i)));
    }
    EXPECT_EQ(buffer.expectedConsumerCount(), 5u);

    // 6th consumer should be rejected
    EXPECT_FALSE(buffer.addNewToExpectedConsumers("consumer5"));
    EXPECT_EQ(buffer.expectedConsumerCount(), 5u);
}

// addNewToExpectedConsumers rejects duplicate IDs
TEST_F(BufferTest, AddConsumerRejectsDuplicateId) {
    EXPECT_TRUE(buffer.addNewToExpectedConsumers("consumer1"));
    EXPECT_FALSE(buffer.addNewToExpectedConsumers("consumer1"));
    EXPECT_EQ(buffer.expectedConsumerCount(), 1u);
}

// addNewToExpectedConsumers rejects after chunks removed
TEST_F(BufferTest, AddConsumerRejectsAfterChunksRemoved) {
    EXPECT_TRUE(buffer.addNewToExpectedConsumers("consumer1"));

    // Drop freeze and add a chunk, then have consumer receive it so it gets removed
    std::list<size_t> freezeRemoved;
    buffer.setInitialChunksFreezingDropped(freezeRemoved);

    std::string data(100, 'A');
    size_t index = buffer.addChunk(data);
    ASSERT_NE(index, 0u);

    std::list<size_t> removedChunks;
    buffer.setChunkAsReceived(index, removedChunks);

    // Verify some chunks were removed
    EXPECT_TRUE(buffer.someChunksWasRemoved());

    // Now try adding a new consumer - should fail because chunks have been removed
    EXPECT_FALSE(buffer.addNewToExpectedConsumers("consumer2"));
}

// initialChunksFreezing prevents chunk removal in sanitize
TEST_F(BufferTest, InitialChunksFreezingPreventsRemoval) {
    EXPECT_TRUE(buffer.addNewToExpectedConsumers("consumer1"));

    // Freeze is on by default
    EXPECT_TRUE(buffer.initialChunksFreezing());

    std::string data(100, 'A');
    size_t index = buffer.addChunk(data);
    ASSERT_NE(index, 0u);

    // Mark as received, but freeze should prevent removal
    std::list<size_t> removedChunks;
    buffer.setChunkAsReceived(index, removedChunks);

    EXPECT_TRUE(removedChunks.empty());
    EXPECT_EQ(buffer.chunkCount(), 1u);
}

// setInitialChunksFreezingDropped works
TEST_F(BufferTest, SetInitialChunksFreezingDropped) {
    EXPECT_TRUE(buffer.initialChunksFreezing());
    std::list<size_t> removedChunks;
    EXPECT_TRUE(buffer.setInitialChunksFreezingDropped(removedChunks));
    EXPECT_FALSE(buffer.initialChunksFreezing());

    // Second call returns false
    EXPECT_FALSE(buffer.setInitialChunksFreezingDropped(removedChunks));
}

// bytesIn tracking
TEST_F(BufferTest, BytesInTracking) {
    EXPECT_EQ(buffer.bytesIn(), 0u);

    std::string data1(100, 'A');
    buffer.addChunk(data1);
    EXPECT_EQ(buffer.bytesIn(), 100u);

    std::string data2(200, 'B');
    buffer.addChunk(data2);
    EXPECT_EQ(buffer.bytesIn(), 300u);
}

// bytesOut tracking
TEST_F(BufferTest, BytesOutTracking) {
    EXPECT_EQ(buffer.bytesOut(), 0u);

    std::string data(150, 'C');
    size_t index = buffer.addChunk(data);

    // Reading the chunk via operator[] should increase bytesOut
    auto result = buffer[index];
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(buffer.bytesOut(), 150u);

    // Reading again adds more
    result = buffer[index];
    EXPECT_EQ(buffer.bytesOut(), 300u);
}

// removeOneFromExpectedConsumers decreases count and triggers sanitize
TEST_F(BufferTest, RemoveConsumerTriggersCleanup) {
    EXPECT_TRUE(buffer.addNewToExpectedConsumers("consumer1"));
    EXPECT_TRUE(buffer.addNewToExpectedConsumers("consumer2"));
    EXPECT_EQ(buffer.expectedConsumerCount(), 2u);

    std::list<size_t> freezeRemoved;
    buffer.setInitialChunksFreezingDropped(freezeRemoved);

    std::string data(100, 'A');
    size_t idx = buffer.addChunk(data);
    ASSERT_NE(idx, 0u);

    // Both consumers receive chunk
    std::list<size_t> removedChunks;
    buffer.setChunkAsReceived(idx, removedChunks);
    // Not all consumers received yet, chunk should remain
    EXPECT_TRUE(removedChunks.empty());
    EXPECT_EQ(buffer.chunkCount(), 1u);

    buffer.setChunkAsReceived(idx, removedChunks);
    // Now all consumers received, chunk should be removed
    EXPECT_FALSE(removedChunks.empty());
    EXPECT_EQ(buffer.chunkCount(), 0u);
}

// Multiple chunks with consumer removal
TEST_F(BufferTest, RemoveConsumerCleanupMultipleChunks) {
    EXPECT_TRUE(buffer.addNewToExpectedConsumers("consumer1"));
    std::list<size_t> freezeRemoved;
    buffer.setInitialChunksFreezingDropped(freezeRemoved);

    std::string data(50, 'D');
    size_t idx1 = buffer.addChunk(data);
    size_t idx2 = buffer.addChunk(data);
    EXPECT_EQ(buffer.chunkCount(), 2u);

    // Consumer receives chunk 1 only
    std::list<size_t> removedChunks;
    buffer.setChunkAsReceived(idx1, removedChunks);
    EXPECT_EQ(removedChunks.size(), 1u);
    EXPECT_EQ(buffer.chunkCount(), 1u);

    // Now remove the consumer - chunk 2 should get cleaned up
    // since howMuchIsLeft will be 0 (0 consumers expected)
    removedChunks.clear();
    buffer.removeOneFromExpectedConsumers("consumer1", removedChunks);
    EXPECT_EQ(removedChunks.size(), 1u);
    EXPECT_EQ(removedChunks.front(), idx2);
    EXPECT_EQ(buffer.chunkCount(), 0u);
}

// chunksIndex returns correct indices
TEST_F(BufferTest, ChunksIndexReturnsCorrectIndices) {
    std::string data(10, 'Z');
    size_t idx1 = buffer.addChunk(data);
    size_t idx2 = buffer.addChunk(data);
    size_t idx3 = buffer.addChunk(data);

    auto indices = buffer.chunksIndex();
    EXPECT_EQ(indices.size(), 3u);

    std::vector<size_t> indexVec(indices.begin(), indices.end());
    EXPECT_EQ(indexVec[0], idx1);
    EXPECT_EQ(indexVec[1], idx2);
    EXPECT_EQ(indexVec[2], idx3);
}

// chunksInfo returns correct info
TEST_F(BufferTest, ChunksInfoReturnsCorrectInfo) {
    std::string data1(100, 'A');
    std::string data2(200, 'B');
    buffer.addChunk(data1);
    buffer.addChunk(data2);

    auto info = buffer.chunksInfo();
    EXPECT_EQ(info.size(), 2u);

    auto it = info.begin();
    EXPECT_EQ(it->index, 1u);
    EXPECT_EQ(it->size, 100u);
    ++it;
    EXPECT_EQ(it->index, 2u);
    EXPECT_EQ(it->size, 200u);
}

// currentMaxChunkIndex tracks the max index
TEST_F(BufferTest, CurrentMaxChunkIndex) {
    EXPECT_EQ(buffer.currentMaxChunkIndex(), 0u);

    std::string data(10, 'X');
    buffer.addChunk(data);
    EXPECT_EQ(buffer.currentMaxChunkIndex(), 1u);

    buffer.addChunk(data);
    EXPECT_EQ(buffer.currentMaxChunkIndex(), 2u);
}

// Empty data chunk
TEST_F(BufferTest, AddEmptyChunk) {
    std::string empty;
    size_t index = buffer.addChunk(empty);
    // Empty string is within max chunk size, so it should succeed
    EXPECT_NE(index, 0u);
    EXPECT_EQ(buffer.bytesIn(), 0u);
}
