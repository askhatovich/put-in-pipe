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

#include "observerpattern.h"

enum class TestEvent { eventA, eventB };

class TestSubscriber : public Subscriber<TestEvent> {
public:
    void update(TestEvent event, std::any data) override {
        lastEvent = event;
        callCount++;
        lastData = data;
    }
    TestEvent lastEvent = TestEvent::eventA;
    int callCount = 0;
    std::any lastData;

protected:
    TestSubscriber() = default;
};

class TestPublisher : public Publisher<TestEvent> {
public:
    using Publisher<TestEvent>::notifySubscribers;
    using Publisher<TestEvent>::addSubscriber;
    using Publisher<TestEvent>::removeSubscriber;
    using Publisher<TestEvent>::countSubscribers;
};

TEST(ObserverTest, AddSubscriberIncreasesCount) {
    TestPublisher publisher;
    EXPECT_EQ(publisher.countSubscribers(), 0u);

    auto sub1 = createSubscriber<TestSubscriber>();
    publisher.addSubscriber(sub1);
    EXPECT_EQ(publisher.countSubscribers(), 1u);

    auto sub2 = createSubscriber<TestSubscriber>();
    publisher.addSubscriber(sub2);
    EXPECT_EQ(publisher.countSubscribers(), 2u);
}

TEST(ObserverTest, RemoveSubscriberDecreasesCount) {
    TestPublisher publisher;
    auto sub1 = createSubscriber<TestSubscriber>();
    auto sub2 = createSubscriber<TestSubscriber>();

    publisher.addSubscriber(sub1);
    publisher.addSubscriber(sub2);
    EXPECT_EQ(publisher.countSubscribers(), 2u);

    publisher.removeSubscriber(sub1);
    EXPECT_EQ(publisher.countSubscribers(), 1u);

    publisher.removeSubscriber(sub2);
    EXPECT_EQ(publisher.countSubscribers(), 0u);
}

TEST(ObserverTest, NotifySubscribersCallsUpdateWithCorrectEventAndData) {
    TestPublisher publisher;
    auto sub1 = createSubscriber<TestSubscriber>();
    auto sub2 = createSubscriber<TestSubscriber>();

    publisher.addSubscriber(sub1);
    publisher.addSubscriber(sub2);

    std::string testData = "hello";
    publisher.notifySubscribers(TestEvent::eventB, testData);

    EXPECT_EQ(sub1->lastEvent, TestEvent::eventB);
    EXPECT_EQ(sub1->callCount, 1);
    EXPECT_EQ(std::any_cast<std::string>(sub1->lastData), "hello");

    EXPECT_EQ(sub2->lastEvent, TestEvent::eventB);
    EXPECT_EQ(sub2->callCount, 1);
    EXPECT_EQ(std::any_cast<std::string>(sub2->lastData), "hello");
}

TEST(ObserverTest, ExpiredWeakPtrSubscribersCleanedUpDuringNotify) {
    TestPublisher publisher;
    auto sub1 = createSubscriber<TestSubscriber>();

    {
        auto sub2 = createSubscriber<TestSubscriber>();
        publisher.addSubscriber(sub1);
        publisher.addSubscriber(sub2);
        EXPECT_EQ(publisher.countSubscribers(), 2u);
    }
    // sub2 is now destroyed, its weak_ptr is expired

    publisher.notifySubscribers(TestEvent::eventA, std::any{});

    // After notify, expired subscribers are cleaned up
    EXPECT_EQ(publisher.countSubscribers(), 1u);
    EXPECT_EQ(sub1->callCount, 1);
}

TEST(ObserverTest, SubscriberNotNotifiedAfterRemoval) {
    TestPublisher publisher;
    auto sub = createSubscriber<TestSubscriber>();

    publisher.addSubscriber(sub);
    publisher.removeSubscriber(sub);

    publisher.notifySubscribers(TestEvent::eventB, std::any{});
    EXPECT_EQ(sub->callCount, 0);
}

TEST(ObserverTest, DuplicateSubscriberNotAddedTwice) {
    TestPublisher publisher;
    auto sub = createSubscriber<TestSubscriber>();

    publisher.addSubscriber(sub);
    publisher.addSubscriber(sub);

    EXPECT_EQ(publisher.countSubscribers(), 1u);

    // Verify it only gets notified once
    publisher.notifySubscribers(TestEvent::eventA, std::any{});
    EXPECT_EQ(sub->callCount, 1);
}
