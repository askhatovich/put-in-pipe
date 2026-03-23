// Tests for Config::loadFromFile()

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

#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <cstdio>
#include <unistd.h>

class ConfigTest : public ::testing::Test {
protected:
    std::string tmpPath;

    void SetUp() override {
        // Generate a unique temp file path for each test
        char tmpl[] = "/tmp/test_config_XXXXXX";
        int fd = mkstemp(tmpl);
        ASSERT_NE(fd, -1) << "Failed to create temp file";
        close(fd);
        tmpPath = tmpl;
    }

    void TearDown() override {
        if (!tmpPath.empty()) {
            std::remove(tmpPath.c_str());
        }
    }

    void writeFile(const std::string& content) {
        std::ofstream ofs(tmpPath);
        ASSERT_TRUE(ofs.is_open()) << "Failed to open temp file for writing";
        ofs << content;
        ofs.close();
    }
};

// 1. Load a full INI with custom values for every field and verify all getters.
TEST_F(ConfigTest, LoadValidFullConfig) {
    writeFile(
        "[server]\n"
        "bind_address = 192.168.1.100\n"
        "bind_port = 8080\n"
        "\n"
        "[client]\n"
        "max_count = 1000\n"
        "without_captcha_threshold = 200\n"
        "captcha_lifetime = 300\n"
        "timeout = 120\n"
        "\n"
        "[session]\n"
        "count_limit = 50\n"
        "max_chunk_size = 1048576\n"
        "chunk_queue_max_size = 20\n"
        "max_lifetime = 3600\n"
        "max_consumer_count = 10\n"
        "max_initial_freeze_duration = 240\n"
    );

    auto& cfg = Config::instance();
    ASSERT_TRUE(cfg.loadFromFile(tmpPath));

    EXPECT_EQ(cfg.bindAddress(), "192.168.1.100");
    EXPECT_EQ(cfg.bindPort(), 8080);
    EXPECT_EQ(cfg.apiMaxClientCount(), 1000u);
    EXPECT_EQ(cfg.apiWithoutCaptchaThreshold(), 200u);
    EXPECT_EQ(cfg.apiCaptchaLifetime(), 300u);
    EXPECT_EQ(cfg.clientTimeout(), 120u);
    EXPECT_EQ(cfg.transferSessionCountLimit(), 50u);
    EXPECT_EQ(cfg.transferSessionMaxChunkSize(), 1048576u);
    EXPECT_EQ(cfg.transferSessionChunkQueueMaxSize(), 20u);
    EXPECT_EQ(cfg.transferSessionMaxLifetime(), 3600u);
    EXPECT_EQ(cfg.transferSessionMaxConsumerCount(), 10u);
    EXPECT_EQ(cfg.transferSessionMaxInitialFreezeDuration(), 240u);
}

// 2. Load an empty INI file and verify all fields get their defaults.
TEST_F(ConfigTest, LoadDefaultValues) {
    writeFile("; empty config\n");

    auto& cfg = Config::instance();
    ASSERT_TRUE(cfg.loadFromFile(tmpPath));

    EXPECT_EQ(cfg.bindAddress(), "0.0.0.0");
    EXPECT_EQ(cfg.bindPort(), 2233);
    EXPECT_EQ(cfg.apiMaxClientCount(), 500u);
    EXPECT_EQ(cfg.apiWithoutCaptchaThreshold(), 500u);
    EXPECT_EQ(cfg.apiCaptchaLifetime(), 180u);
    EXPECT_EQ(cfg.clientTimeout(), 60u);
    EXPECT_EQ(cfg.transferSessionCountLimit(), 100u);
    EXPECT_EQ(cfg.transferSessionMaxChunkSize(), 5242880u);
    EXPECT_EQ(cfg.transferSessionChunkQueueMaxSize(), 10u);
    EXPECT_EQ(cfg.transferSessionMaxLifetime(), 7200u);
    EXPECT_EQ(cfg.transferSessionMaxConsumerCount(), 5u);
    EXPECT_EQ(cfg.transferSessionMaxInitialFreezeDuration(), 120u);
}

// 3. Load INI with only [server] section; server fields are custom, all others are defaults.
TEST_F(ConfigTest, LoadPartialConfig) {
    writeFile(
        "[server]\n"
        "bind_address = 10.0.0.1\n"
        "bind_port = 9999\n"
    );

    auto& cfg = Config::instance();
    ASSERT_TRUE(cfg.loadFromFile(tmpPath));

    // Server fields should be custom
    EXPECT_EQ(cfg.bindAddress(), "10.0.0.1");
    EXPECT_EQ(cfg.bindPort(), 9999);

    // Client fields should be defaults
    EXPECT_EQ(cfg.apiMaxClientCount(), 500u);
    EXPECT_EQ(cfg.apiWithoutCaptchaThreshold(), 500u);
    EXPECT_EQ(cfg.apiCaptchaLifetime(), 180u);
    EXPECT_EQ(cfg.clientTimeout(), 60u);

    // Session fields should be defaults
    EXPECT_EQ(cfg.transferSessionCountLimit(), 100u);
    EXPECT_EQ(cfg.transferSessionMaxChunkSize(), 5242880u);
    EXPECT_EQ(cfg.transferSessionChunkQueueMaxSize(), 10u);
    EXPECT_EQ(cfg.transferSessionMaxLifetime(), 7200u);
    EXPECT_EQ(cfg.transferSessionMaxConsumerCount(), 5u);
    EXPECT_EQ(cfg.transferSessionMaxInitialFreezeDuration(), 120u);
}

// 4. Call loadFromFile with a non-existent path; returns false.
TEST_F(ConfigTest, LoadNonExistentFile) {
    auto& cfg = Config::instance();
    EXPECT_FALSE(cfg.loadFromFile("/tmp/this_file_does_not_exist_at_all.ini"));
}

// 5. Write a file with invalid INI syntax; returns false.
TEST_F(ConfigTest, LoadMalformedFile) {
    writeFile("not_a_valid_ini[[[\n");

    auto& cfg = Config::instance();
    EXPECT_FALSE(cfg.loadFromFile(tmpPath));
}

// 6. Write INI with only one session field overridden; verify that field and all defaults.
TEST_F(ConfigTest, OverridesSingleField) {
    writeFile(
        "[session]\n"
        "max_lifetime = 9999\n"
    );

    auto& cfg = Config::instance();
    ASSERT_TRUE(cfg.loadFromFile(tmpPath));

    // The overridden field
    EXPECT_EQ(cfg.transferSessionMaxLifetime(), 9999u);

    // Server defaults
    EXPECT_EQ(cfg.bindAddress(), "0.0.0.0");
    EXPECT_EQ(cfg.bindPort(), 2233);

    // Client defaults
    EXPECT_EQ(cfg.apiMaxClientCount(), 500u);
    EXPECT_EQ(cfg.apiWithoutCaptchaThreshold(), 500u);
    EXPECT_EQ(cfg.apiCaptchaLifetime(), 180u);
    EXPECT_EQ(cfg.clientTimeout(), 60u);

    // Remaining session defaults
    EXPECT_EQ(cfg.transferSessionCountLimit(), 100u);
    EXPECT_EQ(cfg.transferSessionMaxChunkSize(), 5242880u);
    EXPECT_EQ(cfg.transferSessionChunkQueueMaxSize(), 10u);
    EXPECT_EQ(cfg.transferSessionMaxConsumerCount(), 5u);
    EXPECT_EQ(cfg.transferSessionMaxInitialFreezeDuration(), 120u);
}

// 7. Verify bind_port boundary values (0 and 65535).
TEST_F(ConfigTest, BindPortRange) {
    // Test upper bound: 65535
    writeFile(
        "[server]\n"
        "bind_port = 65535\n"
    );

    auto& cfg = Config::instance();
    ASSERT_TRUE(cfg.loadFromFile(tmpPath));
    EXPECT_EQ(cfg.bindPort(), 65535);

    // Test lower bound: 0
    writeFile(
        "[server]\n"
        "bind_port = 0\n"
    );

    ASSERT_TRUE(cfg.loadFromFile(tmpPath));
    EXPECT_EQ(cfg.bindPort(), 0);
}
