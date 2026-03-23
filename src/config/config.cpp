#include "config.h"
#include "inireader.h"
#include "../log.h"

Config &Config::instance()
{
    static Config cfg;
    return cfg;
}

bool Config::loadFromFile(const std::string &path)
{
    INIReader reader(path);

    if (reader.ParseError() < 0)
    {
        PLOG_ERROR << "Cannot open config file: " << path;
        return false;
    }

    if (reader.ParseError() > 0)
    {
        PLOG_ERROR << "Config parse error on line " << reader.ParseError() << " in " << path;
        return false;
    }

    // [server]
    m_logLevel = reader.GetString("server", "log_level", "info");
    m_address  = reader.GetString("server", "bind_address", "0.0.0.0");
    m_port     = static_cast<uint16_t>(reader.GetUnsigned("server", "bind_port", 2233));

    // [client]
    m_apiMaxClientCount          = reader.GetUnsigned("client", "max_count", 500);
    m_apiWithoutCaptchaThreshold = reader.GetUnsigned("client", "without_captcha_threshold", 500);
    m_apiCaptchaLifetime         = reader.GetUnsigned("client", "captcha_lifetime", 180);
    m_clientTimeout              = reader.GetUnsigned("client", "timeout", 60);

    // [session]
    m_transferSessionCountLimit              = reader.GetUnsigned("session", "count_limit", 100);
    m_transferSessionMaxChunkSize            = reader.GetUnsigned("session", "max_chunk_size", 5242880);
    m_transferSessionChunkQueueMaxSize       = reader.GetUnsigned("session", "chunk_queue_max_size", 10);
    m_transferSessionMaxLifetime             = reader.GetUnsigned("session", "max_lifetime", 7200);
    m_transferSessionMaxConsumerCount        = reader.GetUnsigned("session", "max_consumer_count", 5);
    m_transferSessionMaxInitialFreezeDuration = reader.GetUnsigned("session", "max_initial_freeze_duration", 120);

    return true;
}
