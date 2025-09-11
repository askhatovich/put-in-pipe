// Copyright (C) 2025  Roman Lyubimov
// SPDX-License-Identifier: GPL-3.0-or-later
// For full license text, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#include "token.h"
#include "skaptcha_tools.h"

#include <iostream>

namespace skaptcha_tools {

std::shared_ptr<Token> Token::fromString(const std::string &token)
{
    const std::vector<std::string> tokenVector = skaptcha_tools::string::split(token, m_delimiter);
    if (tokenVector.size() != 3) // chacha20 nonce, encrypted payload, signature
    {
        return nullptr;
    }

    const auto newSignature = sign(tokenVector[0] + m_delimiter + tokenVector[1]);
    if (newSignature != tokenVector[2])
    {
        return nullptr;
    }

    const std::string nonce = skaptcha_tools::base64::decode(tokenVector[0]);
    if (nonce.length() != skaptcha_tools::crypto::ChaCha20::nonceSize())
    {
        return nullptr;
    }

    const auto encryptedPayload = skaptcha_tools::base64::decode(tokenVector[1]);

    std::string payload;
    try {
        payload = skaptcha_tools::crypto::ChaCha20::instance().decrypt(encryptedPayload, nonce);
    } catch (std::exception& e) {
        return nullptr;
    }

    auto sharedToken = std::shared_ptr<Token>(new Token);
    sharedToken->m_nonce = nonce;
    sharedToken->m_payload = payload;
    return sharedToken;
}

std::shared_ptr<Token> Token::generate(const std::string &payload)
{
    auto token = std::shared_ptr<Token>(new Token);
    token->m_nonce = skaptcha_tools::string::simpleRandom(skaptcha_tools::crypto::ChaCha20::nonceSize());
    token->m_payload = payload;
    return token;
}

std::string Token::sign(const std::string &string)
{
    return skaptcha_tools::crypto::HashSignature::instance().sign(string);
}

std::string Token::dump() const
{
    try {

        const std::string encryptedPayload = skaptcha_tools::base64::encode(
                skaptcha_tools::crypto::ChaCha20::instance().encrypt(m_payload, m_nonce)
            );
        const std::string tokenPayload = skaptcha_tools::base64::encode(m_nonce) + m_delimiter + encryptedPayload;
        const std::string signature = sign(tokenPayload);
        return tokenPayload + m_delimiter + signature;

    } catch (std::exception& e) {
        std::cerr << __PRETTY_FUNCTION__ << " exception: " << e.what() << std::endl;
        return std::string("500");
    }
}

std::string ExpiringToken::PrivatePayload::dump() const
{
    std::ostringstream oss;
    oss << usersPayload << "|" << created << "|" << lifetime.count();
    return oss.str();
}

bool ExpiringToken::PrivatePayload::fromString(const std::string &string)
{
    /*
     * We parse from the end to avoid errors if there is a separator character in the user data.
     */

    if (string.empty())
    {
        return false;
    }

    const size_t lastSep = string.rfind(delimiter);
    if (lastSep == std::string::npos or lastSep == string.length() - 1)
    {
        return false;
    }

    const size_t prevSep = string.rfind(delimiter, lastSep - 1);
    if (prevSep == std::string::npos)
    {
        return false;
    }

    usersPayload = string.substr(0, prevSep);

    const std::string createdStr = string.substr(prevSep + 1, lastSep - prevSep - 1);
    const std::string lifetimeStr = string.substr(lastSep + 1);

    try {
        created = std::stoll(createdStr);
        lifetime = std::chrono::seconds(std::stoll(lifetimeStr));
    } catch (...) {
        return false;
    }

    return true;
}

bool ExpiringToken::PrivatePayload::isExpired() const
{
    const auto now = std::time(nullptr);
    return now > (created + lifetime.count());
}

std::shared_ptr<ExpiringToken> ExpiringToken::fromString(const std::string &token)
{
    const auto baseToken = Token::fromString(token);
    if (baseToken == nullptr)
    {
        return nullptr;
    }

    PrivatePayload ppayload;
    if (not ppayload.fromString(baseToken->payload()))
    {
        return nullptr;
    }

    if (ppayload.isExpired())
    {
        return nullptr;
    }

    auto expiringToken = std::shared_ptr<ExpiringToken>(new ExpiringToken(ppayload.lifetime));
    expiringToken->m_ppayload = ppayload;
    return expiringToken;
}

std::shared_ptr<ExpiringToken> ExpiringToken::generate(const std::string &payload, std::chrono::seconds secondsLifetime)
{
    auto token = std::shared_ptr<ExpiringToken>(new ExpiringToken(secondsLifetime));
    token->m_ppayload.usersPayload = payload;
    return token;
}

std::string ExpiringToken::dump()
{
    m_ppayload.created = std::chrono::system_clock::to_time_t( std::chrono::system_clock::now() );
    const auto baseToken = Token::generate(m_ppayload.dump());
    if (baseToken == nullptr)
    {
        return nullptr;
    }
    return baseToken->dump();
}

ExpiringToken::ExpiringToken(std::chrono::seconds lifetime)
{
    m_ppayload.lifetime = lifetime;
}

} // namespace skaptcha_tools
