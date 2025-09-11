// Copyright (C) 2025  Roman Lyubimov
// SPDX-License-Identifier: GPL-3.0-or-later
// For full license text, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#pragma once

#include <string>
#include <vector>
#include <array>

namespace skaptcha_tools {

namespace base64 {

std::string encode(const std::string& data);
std::string encode(const char* data, size_t length);
std::string decode(const std::string& data);
std::string encodeUrlsafe(const std::string& data);
std::string encodeUrlsafe(const char* data, size_t length);

} // namespace base64

namespace string {

std::vector<std::string> split(const std::string &str, char delimiter);
std::string simpleRandom(size_t length);

} // namespace string

namespace crypto {

class ChaCha20
{
public:
    static uint8_t nonceSize();
    static ChaCha20& instance();

    std::string encrypt(const std::string& data, const std::string& nonce);
    std::string decrypt(const std::string& ciphertext, const std::string& nonce);

private:
    ChaCha20();
    ChaCha20(const ChaCha20&) = delete;
    ChaCha20(ChaCha20&&) = delete;
    ChaCha20& operator=(const ChaCha20&) = delete;

    /*
     * A unique key, a new one is generated each time the application is started.
     * It is used to encrypt the correct response to the captcha
     * (it can also be used for other internal application data that is not transmitted for processing outside).
     */
    std::array<uint8_t, 32> m_key;
};

class HashSignature
{
public:
    static HashSignature& instance();

    bool check(const std::string& data, const std::string& signature);
    std::string sign(const std::string& data);

private:
    HashSignature();
    HashSignature(const HashSignature&) = delete;
    HashSignature(HashSignature&&) = delete;
    HashSignature& operator=(const HashSignature&) = delete;

    /*
     * A unique key, a new one is generated each time the application is started.
     * It is used in hashing to obtain a unique result that cannot be obtained without knowing the key.
     */
    std::vector<uint8_t> m_key;
};

} // namespace crypto

} // namespace tools
