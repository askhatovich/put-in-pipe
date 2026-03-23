// Tests for TimerCallback

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

#include "timercallback.h"

#include <gtest/gtest.h>
#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <thread>

class TimerCallbackTest : public ::testing::Test {
protected:
    void SetUp() override {
        workGuard = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(
            ioContext.get_executor()
        );
        ioThread = std::thread([this]() {
            ioContext.run();
        });
    }

    void TearDown() override {
        workGuard.reset();
        if (!ioContext.stopped()) {
            ioContext.stop();
        }
        if (ioThread.joinable()) {
            ioThread.join();
        }
    }

    asio::io_context ioContext;
    std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> workGuard;
    std::thread ioThread;
};

// Timer fires callback after specified duration
TEST_F(TimerCallbackTest, TimerFiresCallbackAfterDuration) {
    std::atomic<bool> called{false};

    TimerCallback timer(ioContext, [&called]() {
        called.store(true);
    }, std::chrono::seconds(1));

    timer.start();
    EXPECT_TRUE(timer.isRunning());

    // Wait enough time for the 1-second timer to fire
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    EXPECT_TRUE(called.load());
    EXPECT_FALSE(timer.isRunning());
}

// Timer stop prevents callback from firing
TEST_F(TimerCallbackTest, StopPreventsCallbackFromFiring) {
    std::atomic<bool> called{false};

    TimerCallback timer(ioContext, [&called]() {
        called.store(true);
    }, std::chrono::seconds(2));

    timer.start();
    EXPECT_TRUE(timer.isRunning());

    // Stop before the timer fires
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    timer.stop();

    EXPECT_FALSE(timer.isRunning());

    // Wait past the original duration
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));

    EXPECT_FALSE(called.load());
}

// isRunning reflects state correctly
TEST_F(TimerCallbackTest, IsRunningReflectsState) {
    std::atomic<bool> called{false};

    TimerCallback timer(ioContext, [&called]() {
        called.store(true);
    }, std::chrono::seconds(1));

    // Initially not running
    EXPECT_FALSE(timer.isRunning());

    timer.start();
    EXPECT_TRUE(timer.isRunning());

    timer.stop();
    EXPECT_FALSE(timer.isRunning());
}

// isRunning becomes false after callback fires
TEST_F(TimerCallbackTest, IsRunningFalseAfterCallbackFires) {
    std::atomic<bool> called{false};

    TimerCallback timer(ioContext, [&called]() {
        called.store(true);
    }, std::chrono::seconds(1));

    timer.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    EXPECT_TRUE(called.load());
    EXPECT_FALSE(timer.isRunning());
}

// timeRemaining returns approximate value
TEST_F(TimerCallbackTest, TimeRemainingReturnsApproximateValue) {
    TimerCallback timer(ioContext, []() {}, std::chrono::seconds(10));

    // Before start, not running => should return 0
    EXPECT_EQ(timer.timeRemaining().count(), 0);

    timer.start();

    // Just started, should have close to 10 seconds remaining
    auto remaining = timer.timeRemaining();
    EXPECT_GE(remaining.count(), 8);
    EXPECT_LE(remaining.count(), 10);

    // Wait a couple seconds and check again
    std::this_thread::sleep_for(std::chrono::seconds(2));
    remaining = timer.timeRemaining();
    EXPECT_GE(remaining.count(), 6);
    EXPECT_LE(remaining.count(), 9);

    timer.stop();

    // After stop, should return 0
    EXPECT_EQ(timer.timeRemaining().count(), 0);
}

// restart resets the timer
TEST_F(TimerCallbackTest, RestartResetsTheTimer) {
    std::atomic<int> callCount{0};

    TimerCallback timer(ioContext, [&callCount]() {
        callCount.fetch_add(1);
    }, std::chrono::seconds(1));

    timer.start();

    // Restart before the timer fires, with a new duration
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    timer.restart(std::chrono::seconds(2));

    // The original 1s timer should not have fired
    std::this_thread::sleep_for(std::chrono::milliseconds(700));
    EXPECT_EQ(callCount.load(), 0);
    EXPECT_TRUE(timer.isRunning());

    // Wait for the restarted timer to fire (2s from restart)
    std::this_thread::sleep_for(std::chrono::milliseconds(1800));
    EXPECT_EQ(callCount.load(), 1);
    EXPECT_FALSE(timer.isRunning());
}

// restart without new duration
TEST_F(TimerCallbackTest, RestartWithoutNewDuration) {
    std::atomic<bool> called{false};

    TimerCallback timer(ioContext, [&called]() {
        called.store(true);
    }, std::chrono::seconds(1));

    timer.start();

    // Restart without changing duration
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    timer.restart();
    EXPECT_TRUE(timer.isRunning());

    // Should still fire after ~1s from restart
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    EXPECT_TRUE(called.load());
}

// Multiple start calls are idempotent
TEST_F(TimerCallbackTest, MultipleStartCallsAreIdempotent) {
    std::atomic<int> callCount{0};

    TimerCallback timer(ioContext, [&callCount]() {
        callCount.fetch_add(1);
    }, std::chrono::seconds(1));

    // Call start multiple times
    timer.start();
    timer.start();
    timer.start();

    EXPECT_TRUE(timer.isRunning());

    // Wait for the timer to fire
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    // Callback should have been called exactly once
    EXPECT_EQ(callCount.load(), 1);
    EXPECT_FALSE(timer.isRunning());
}

// Destructor stops the timer
TEST_F(TimerCallbackTest, DestructorStopsTimer) {
    std::atomic<bool> called{false};

    {
        TimerCallback timer(ioContext, [&called]() {
            called.store(true);
        }, std::chrono::seconds(2));

        timer.start();
        EXPECT_TRUE(timer.isRunning());
        // Timer goes out of scope here, destructor should stop it
    }

    // Wait past the original duration
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));
    EXPECT_FALSE(called.load());
}
