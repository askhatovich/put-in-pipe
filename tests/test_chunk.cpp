// Tests for TransferSessionDetails::Chunk

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

#include "chunk.h"
#include "atomicset.h"
#include "config/config.h"

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

using TransferSessionDetails::AtomicSet;
using TransferSessionDetails::AtomicSetSizeAccess;
using TransferSessionDetails::Chunk;

class ChunkTest : public ::testing::Test {
protected:
    void SetUp() override {
        Config::instance().setTransferSessionMaxChunkSize(1024 * 1024);
        Config::instance().setTransferSessionChunkQueueMaxSize(10);
        Config::instance().setTransferSessionMaxConsumerCount(5);

        consumers = std::make_shared<AtomicSet<std::string>>();
    }

    std::shared_ptr<AtomicSet<std::string>> consumers;

    Chunk makeChunk(const std::vector<uint8_t>& data) {
        AtomicSetSizeAccess access(consumers);
        return Chunk(access, data.data(), data.size());
    }
};

// Constructor stores data correctly
TEST_F(ChunkTest, ConstructorStoresDataCorrectly) {
    std::vector<uint8_t> input = {0x01, 0x02, 0x03, 0x04, 0x05};
    Chunk chunk = makeChunk(input);

    auto result = chunk.data();
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result, input);
}

// data() returns shared_ptr to correct vector
TEST_F(ChunkTest, DataReturnsSharedPtrToCorrectVector) {
    std::string str = "Hello, Chunk!";
    std::vector<uint8_t> input(str.begin(), str.end());
    Chunk chunk = makeChunk(input);

    auto ptr1 = chunk.data();
    auto ptr2 = chunk.data();

    // Both calls return the same shared_ptr (same underlying data)
    EXPECT_EQ(ptr1.get(), ptr2.get());
    EXPECT_EQ(ptr1->size(), input.size());

    std::string retrieved(ptr1->begin(), ptr1->end());
    EXPECT_EQ(retrieved, str);
}

// dataSize() matches input size
TEST_F(ChunkTest, DataSizeMatchesInputSize) {
    std::vector<uint8_t> input(256);
    for (size_t i = 0; i < input.size(); ++i) {
        input[i] = static_cast<uint8_t>(i);
    }
    Chunk chunk = makeChunk(input);

    EXPECT_EQ(chunk.dataSize(), 256u);
}

// dataSize() with empty data
TEST_F(ChunkTest, DataSizeWithEmptyData) {
    std::vector<uint8_t> input;
    Chunk chunk = makeChunk(input);

    EXPECT_EQ(chunk.dataSize(), 0u);
}

// howMuchIsLeft with 0 uses equals consumer count
TEST_F(ChunkTest, HowMuchIsLeftWithZeroUsesEqualsConsumerCount) {
    consumers->add("consumer1");
    consumers->add("consumer2");
    consumers->add("consumer3");

    std::vector<uint8_t> input = {0xAA, 0xBB};
    Chunk chunk = makeChunk(input);

    EXPECT_EQ(chunk.howMuchIsLeft(), 3u);
    EXPECT_EQ(chunk.usesCount(), 0u);
}

// howMuchIsLeft with zero consumers returns 0
TEST_F(ChunkTest, HowMuchIsLeftWithZeroConsumersReturnsZero) {
    // No consumers added
    std::vector<uint8_t> input = {0x01};
    Chunk chunk = makeChunk(input);

    // uses (0) >= expected (0), so howMuchIsLeft returns 0
    EXPECT_EQ(chunk.howMuchIsLeft(), 0u);
}

// incrementUses decrements howMuchIsLeft
TEST_F(ChunkTest, IncrementUsesDecrementsHowMuchIsLeft) {
    consumers->add("consumer1");
    consumers->add("consumer2");

    std::vector<uint8_t> input = {0x01};
    Chunk chunk = makeChunk(input);

    EXPECT_EQ(chunk.howMuchIsLeft(), 2u);

    chunk.incrementUses();
    EXPECT_EQ(chunk.howMuchIsLeft(), 1u);
    EXPECT_EQ(chunk.usesCount(), 1u);

    chunk.incrementUses();
    EXPECT_EQ(chunk.howMuchIsLeft(), 0u);
    EXPECT_EQ(chunk.usesCount(), 2u);
}

// incrementUses stops at consumer count (doesn't go negative / overflow)
TEST_F(ChunkTest, IncrementUsesStopsAtConsumerCount) {
    consumers->add("consumer1");

    std::vector<uint8_t> input = {0x01};
    Chunk chunk = makeChunk(input);

    EXPECT_EQ(chunk.howMuchIsLeft(), 1u);

    chunk.incrementUses();
    EXPECT_EQ(chunk.howMuchIsLeft(), 0u);
    EXPECT_EQ(chunk.usesCount(), 1u);

    // Calling incrementUses beyond consumer count should not increment further
    chunk.incrementUses();
    EXPECT_EQ(chunk.howMuchIsLeft(), 0u);
    EXPECT_EQ(chunk.usesCount(), 1u);

    // Call many more times
    for (int i = 0; i < 100; ++i) {
        chunk.incrementUses();
    }
    EXPECT_EQ(chunk.usesCount(), 1u);
    EXPECT_EQ(chunk.howMuchIsLeft(), 0u);
}

// howMuchIsLeft returns 0 when uses >= consumers
TEST_F(ChunkTest, HowMuchIsLeftReturnsZeroWhenUsesEqualConsumers) {
    consumers->add("consumer1");
    consumers->add("consumer2");

    std::vector<uint8_t> input = {0x01};
    Chunk chunk = makeChunk(input);

    chunk.incrementUses();
    chunk.incrementUses();

    EXPECT_EQ(chunk.howMuchIsLeft(), 0u);
}

// howMuchIsLeft adjusts dynamically when consumers are removed
TEST_F(ChunkTest, HowMuchIsLeftAdjustsDynamicallyOnConsumerRemoval) {
    consumers->add("consumer1");
    consumers->add("consumer2");
    consumers->add("consumer3");

    std::vector<uint8_t> input = {0x01};
    Chunk chunk = makeChunk(input);

    EXPECT_EQ(chunk.howMuchIsLeft(), 3u);

    chunk.incrementUses();
    EXPECT_EQ(chunk.howMuchIsLeft(), 2u);

    // Remove a consumer from the shared set
    consumers->remove("consumer3");
    EXPECT_EQ(chunk.howMuchIsLeft(), 1u);

    chunk.incrementUses();
    EXPECT_EQ(chunk.howMuchIsLeft(), 0u);
}

// Large data chunk
TEST_F(ChunkTest, LargeDataChunk) {
    std::vector<uint8_t> input(100000, 0xFF);
    Chunk chunk = makeChunk(input);

    EXPECT_EQ(chunk.dataSize(), 100000u);
    auto result = chunk.data();
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->size(), 100000u);
    EXPECT_EQ((*result)[0], 0xFF);
    EXPECT_EQ((*result)[99999], 0xFF);
}
