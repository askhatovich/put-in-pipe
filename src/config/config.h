#pragma once

#include <string>

class Config
{
public:
    static Config& instance();

    void setBindAddress(const std::string& address)        { m_address = address; }
    void setBindPort(uint16_t port)                        { m_port = port; }
    void setApiMaxClientCount(size_t value)                { m_apiMaxClientCount = value; }
    void setApiWithoutCaptchaThreshold(size_t value)       { m_apiWithoutCaptchaThreshold = value; }
    void setApiCaptchaLifetime(size_t value)               { m_apiCaptchaLifetime = value; }
    void setClientTimeout(size_t value)                    { m_clientTimeout = value; }
    void setTransferSessionCountLimit(size_t value)        { m_transferSessionCountLimit = value; }
    void setTransferSessionMaxChunkSize(size_t value)      { m_transferSessionMaxChunkSize = value; }
    void setTransferSessionChunkQueueMaxSize(size_t value) { m_transferSessionChunkQueueMaxSize = value; }
    void setTransferSessionMaxLifetime(size_t value)       { m_transferSessionMaxLifetime = value; }
    void setTransferSessionMaxConsumerCount(size_t value)  { m_transferSessionMaxConsumerCount = value; }
    void setTransferSessionMaxInitialFreezeDuration(size_t value) { m_transferSessionMaxInitialFreezeDuration = value; }

    std::string bindAddress() const                 { return m_address; }
    uint16_t bindPort() const                       { return m_port; }
    size_t clientTimeout() const                    { return m_clientTimeout; }
    size_t apiMaxClientCount() const                { return m_apiMaxClientCount; }
    size_t apiWithoutCaptchaThreshold() const       { return m_apiWithoutCaptchaThreshold; }
    size_t apiCaptchaLifetime() const               { return m_apiCaptchaLifetime; }
    size_t transferSessionCountLimit() const        { return m_transferSessionCountLimit; }
    size_t transferSessionMaxChunkSize() const      { return m_transferSessionMaxChunkSize; }
    size_t transferSessionMaxLifetime() const       { return m_transferSessionMaxLifetime; }
    size_t transferSessionChunkQueueMaxSize() const { return m_transferSessionChunkQueueMaxSize; }
    size_t transferSessionMaxConsumerCount() const  { return m_transferSessionMaxConsumerCount; }
    size_t transferSessionMaxInitialFreezeDuration() const { return m_transferSessionMaxInitialFreezeDuration; }

private:
    Config() = default;
    Config(const Config&) = delete;
    Config(Config&&) = delete;
    Config& operator=(const Config&) = delete;

    std::string m_address;
    uint16_t m_port = 0;
    size_t m_apiMaxClientCount = 0;
    size_t m_apiWithoutCaptchaThreshold = 0;
    size_t m_apiCaptchaLifetime = 0;
    size_t m_clientTimeout = 0;
    size_t m_transferSessionCountLimit = 0;
    size_t m_transferSessionMaxChunkSize = 0;
    size_t m_transferSessionMaxInitialFreezeDuration = 0;
    size_t m_transferSessionChunkQueueMaxSize = 0;
    size_t m_transferSessionMaxConsumerCount = 0;
    size_t m_transferSessionMaxLifetime = 0;
};
