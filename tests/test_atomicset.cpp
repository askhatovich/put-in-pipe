// Tests for TransferSessionDetails::AtomicSet<T>

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

#include "atomicset.h"

#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <vector>
#include <set>

using TransferSessionDetails::AtomicSet;

class AtomicSetTest : public ::testing::Test {
protected:
    AtomicSet<std::string> set;
};

// add inserts unique values, size increases
TEST_F(AtomicSetTest, AddInsertsUniqueValues) {
    EXPECT_EQ(set.size(), 0u);
    EXPECT_TRUE(set.add("alpha"));
    EXPECT_EQ(set.size(), 1u);
    EXPECT_TRUE(set.add("beta"));
    EXPECT_EQ(set.size(), 2u);
    EXPECT_TRUE(set.add("gamma"));
    EXPECT_EQ(set.size(), 3u);
}

// add rejects duplicates (returns false)
TEST_F(AtomicSetTest, AddRejectsDuplicates) {
    EXPECT_TRUE(set.add("alpha"));
    EXPECT_FALSE(set.add("alpha"));
    EXPECT_EQ(set.size(), 1u);
}

// remove removes existing values (returns true)
TEST_F(AtomicSetTest, RemoveExistingReturnsTrue) {
    set.add("alpha");
    set.add("beta");
    EXPECT_TRUE(set.remove("alpha"));
    EXPECT_EQ(set.size(), 1u);
    EXPECT_TRUE(set.remove("beta"));
    EXPECT_EQ(set.size(), 0u);
}

// remove returns false for non-existent
TEST_F(AtomicSetTest, RemoveNonExistentReturnsFalse) {
    EXPECT_FALSE(set.remove("nonexistent"));
    set.add("alpha");
    EXPECT_FALSE(set.remove("beta"));
}

// size tracks correctly after add/remove
TEST_F(AtomicSetTest, SizeTracksCorrectly) {
    EXPECT_EQ(set.size(), 0u);

    set.add("a");
    set.add("b");
    set.add("c");
    EXPECT_EQ(set.size(), 3u);

    set.remove("b");
    EXPECT_EQ(set.size(), 2u);

    set.add("d");
    EXPECT_EQ(set.size(), 3u);

    set.remove("a");
    set.remove("c");
    set.remove("d");
    EXPECT_EQ(set.size(), 0u);
}

// Re-add after remove works
TEST_F(AtomicSetTest, ReAddAfterRemove) {
    set.add("alpha");
    set.remove("alpha");
    EXPECT_EQ(set.size(), 0u);
    EXPECT_TRUE(set.add("alpha"));
    EXPECT_EQ(set.size(), 1u);
}

// Thread-safety: 4 threads adding/removing concurrently, verify final state is consistent
TEST_F(AtomicSetTest, ThreadSafetyConcurrentAddRemove) {
    const int numThreads = 4;
    const int opsPerThread = 1000;
    std::vector<std::thread> threads;

    // Each thread adds its own unique set of values, then removes them
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, t, opsPerThread]() {
            for (int i = 0; i < opsPerThread; ++i) {
                std::string val = "t" + std::to_string(t) + "_" + std::to_string(i);
                set.add(val);
            }
            for (int i = 0; i < opsPerThread; ++i) {
                std::string val = "t" + std::to_string(t) + "_" + std::to_string(i);
                set.remove(val);
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    // After all threads complete add+remove of their own values, size should be 0
    EXPECT_EQ(set.size(), 0u);
}

// Thread-safety: concurrent adds of overlapping values
TEST_F(AtomicSetTest, ThreadSafetyConcurrentOverlappingAdds) {
    const int numThreads = 4;
    const int numValues = 500;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    // All threads try to add the same set of values
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, numValues, &successCount]() {
            for (int i = 0; i < numValues; ++i) {
                std::string val = "shared_" + std::to_string(i);
                if (set.add(val)) {
                    successCount.fetch_add(1);
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    // Exactly numValues unique values should have been added
    EXPECT_EQ(set.size(), static_cast<size_t>(numValues));
    // Total successful inserts should equal numValues (one per unique value)
    EXPECT_EQ(successCount.load(), numValues);
}
