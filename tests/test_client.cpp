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

#include "client.h"
#include "config/config.h"
#include "observerpattern.h"

class ClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        Config::instance().setClientTimeout(60);
    }

    asio::io_context ioContext;

    std::shared_ptr<Client> makeClient(const std::string& id) {
        return createSubscriber<Client>(id, ioContext, [](){});
    }
};

TEST_F(ClientTest, IdAndPublicIdAreSetCorrectly) {
    auto client = makeClient("test-id-123");
    EXPECT_EQ(client->id(), "test-id-123");

    // publicId should not be empty and should differ from the raw id
    EXPECT_FALSE(client->publicId().empty());
    EXPECT_NE(client->publicId(), "test-id-123");

    // publicId is deterministic for the same id within the same process
    auto client2 = makeClient("test-id-123");
    EXPECT_EQ(client->publicId(), client2->publicId());
}

TEST_F(ClientTest, JoinSessionWithValidIdSucceeds) {
    auto client = makeClient("client-1");
    EXPECT_TRUE(client->joinSession("session-abc"));
}

TEST_F(ClientTest, JoinSessionWithEmptyIdFails) {
    auto client = makeClient("client-1");
    EXPECT_FALSE(client->joinSession(""));
}

TEST_F(ClientTest, JoinSessionWhenAlreadyJoinedFails) {
    auto client = makeClient("client-1");
    EXPECT_TRUE(client->joinSession("session-abc"));
    EXPECT_FALSE(client->joinSession("session-def"));
}

TEST_F(ClientTest, JoinedSessionReturnsEmptyStringInitially) {
    auto client = makeClient("client-1");
    EXPECT_TRUE(client->joinedSession().empty());
}

TEST_F(ClientTest, SetNameStoresAndTruncatesToMaxLength) {
    auto client = makeClient("client-1");

    // Normal name
    client->setName("Alice");
    EXPECT_EQ(client->name(), "Alice");

    // Long name gets truncated to NAME_MAX_LENGTH (20)
    std::string longName(30, 'x');
    client->setName(longName);
    EXPECT_EQ(client->name().length(), Client::NAME_MAX_LENGTH);
    EXPECT_EQ(client->name(), std::string(Client::NAME_MAX_LENGTH, 'x'));
}

TEST_F(ClientTest, SetNameWithEmptyStringIsNoOp) {
    auto client = makeClient("client-1");

    client->setName("Bob");
    EXPECT_EQ(client->name(), "Bob");

    client->setName("");
    EXPECT_EQ(client->name(), "Bob");
}

TEST_F(ClientTest, OnlineReturnsFalseInitially) {
    auto client = makeClient("client-1");
    // Timer is started in constructor, so client is offline (online = !timer.isRunning())
    EXPECT_FALSE(client->online());
}
