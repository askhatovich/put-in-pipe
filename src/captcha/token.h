// Copyright (C) 2025  Roman Lyubimov
// SPDX-License-Identifier: GPL-3.0-or-later
// For full license text, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#pragma once

#include <string>
#include <memory>
#include <chrono>

/*
 * A text token that allows you to put sensitive data into it with
 * a simple data extraction interface. The encryption key is stored only in memory,
 * the tokens are valid until the application is restarted.
 */

namespace skaptcha_tools {

class Token
{
public:
    static std::shared_ptr<Token> fromString(const std::string& token);
    static std::shared_ptr<Token> generate(const std::string& payload);

    std::string dump() const;
    std::string payload() const { return m_payload; }

private:
    Token() = default;
    static std::string sign(const std::string& string);

    std::string m_payload;
    std::string m_nonce;

    const static char m_delimiter = '.';
};

class ExpiringToken
{
public:
    static std::shared_ptr<ExpiringToken> fromString(const std::string& token);
    static std::shared_ptr<ExpiringToken> generate(const std::string& payload, std::chrono::seconds secondsLifetime);

    std::string dump();
    std::string payload() const { return m_ppayload.usersPayload; }

private:
    ExpiringToken(std::chrono::seconds lifetime);

    struct PrivatePayload
    {
        std::string usersPayload;
        std::time_t created = 0;
        std::chrono::seconds lifetime = std::chrono::seconds(0);

        static const char delimiter = '|';

        std::string dump() const;
        bool fromString(const std::string& string);
        bool isExpired() const;
    };
    PrivatePayload m_ppayload;
};


} // namespace Skaptcha

