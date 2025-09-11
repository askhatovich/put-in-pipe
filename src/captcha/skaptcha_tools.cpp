#include "skaptcha_tools.h"
#include "chacha20_backend/portable8439.h"
#include "sha256_backend/sha256.h"
#include "../crowlib/crow/utility.h" // for base64: you can use another

#include <random>

std::string skaptcha_tools::base64::encode(const std::string &data)
{
    return crow::utility::base64encode(data, data.size());
}

std::string skaptcha_tools::base64::encode(const char *data, size_t length)
{
    return crow::utility::base64encode(data, length);
}

std::string skaptcha_tools::base64::decode(const std::string &data)
{
    return crow::utility::base64decode(data, data.size());
}

std::string skaptcha_tools::base64::encodeUrlsafe(const std::string &data)
{
    return crow::utility::base64encode_urlsafe(data, data.size());
}

std::string skaptcha_tools::base64::encodeUrlsafe(const char *data, size_t length)
{
    return crow::utility::base64encode_urlsafe(data, length);
}

std::vector<std::string> skaptcha_tools::string::split(const std::string &str, char delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    size_t start = 0;
    size_t end = str.find(delimiter);

    while (end != std::string::npos)
    {
        token = str.substr(start, end - start);
        tokens.push_back(token);
        start = end + 1;
        end = str.find(delimiter, start);
    }

    tokens.push_back(str.substr(start));

    return tokens;
}

skaptcha_tools::crypto::ChaCha20 &skaptcha_tools::crypto::ChaCha20::instance()
{
    static ChaCha20 chacha;
    return chacha;
}

std::string skaptcha_tools::crypto::ChaCha20::encrypt(const std::string &data, const std::string &nonce)
{
    if (nonce.size() != RFC_8439_NONCE_SIZE)
    {
        throw std::runtime_error("Nonce must be exactly 12 bytes long RFC 8439");
    }

    std::vector<uint8_t> ciphertext(data.size() + RFC_8439_TAG_SIZE);

    portable_chacha20_poly1305_encrypt(
        ciphertext.data(),
        m_key.data(),
        reinterpret_cast<const uint8_t*>(nonce.data()),
        nullptr,
        0,
        reinterpret_cast<const uint8_t *>(data.data()),
        data.size()
    );

    return {ciphertext.begin(), ciphertext.end()};
}

std::string skaptcha_tools::crypto::ChaCha20::decrypt(const std::string &ciphertext, const std::string &nonce)
{
    if (nonce.size() != RFC_8439_NONCE_SIZE)
    {
        throw std::runtime_error("Nonce must be exactly 12 bytes long RFC 8439");
    }

    if (ciphertext.size() < RFC_8439_TAG_SIZE)
    {
        throw std::runtime_error("Ciphertext is too short to contain authentication tag RFC 8439");
    }

    std::vector<uint8_t> decrypted(ciphertext.size() - RFC_8439_TAG_SIZE);

    const int result = portable_chacha20_poly1305_decrypt(
        decrypted.data(),
        m_key.data(),
        reinterpret_cast<const uint8_t*>(nonce.data()),
        nullptr,
        0,
        reinterpret_cast<const uint8_t *>(ciphertext.data()),
        ciphertext.size()
    );

    if (result < 0)
    {
        throw std::runtime_error("Decryption failed: authentication tag mismatch");
    }

    return {decrypted.begin(), decrypted.end()};
}

uint8_t skaptcha_tools::crypto::ChaCha20::nonceSize()
{
    return RFC_8439_NONCE_SIZE;
}

skaptcha_tools::crypto::ChaCha20::ChaCha20()
{
    const auto random = string::simpleRandom(m_key.size());

    for (size_t i = 0; i < m_key.size(); ++i)
    {
        m_key[i] = random[i];
    }
}

std::string skaptcha_tools::string::simpleRandom(size_t length)
{
    std::string result;
    result.resize( length );

    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<> distribution(0, 255);

    for (int8_t i = 0; i < length; ++i)
    {
        result[i] = distribution(generator);
    }

    return result;
}

skaptcha_tools::crypto::HashSignature &skaptcha_tools::crypto::HashSignature::instance()
{
    static HashSignature hasher;
    return hasher;
}

bool skaptcha_tools::crypto::HashSignature::check(const std::string &data, const std::string &signature)
{
    return sign(data) == signature;
}

std::string skaptcha_tools::crypto::HashSignature::sign(const std::string &data)
{
    tools::SHA256 sha256;
    sha256.update(data);
    sha256.update(m_key);
    const auto digest = sha256.digest();
    std::string result = skaptcha_tools::base64::encodeUrlsafe( reinterpret_cast<const char*>(digest.data()), digest.size() );

    auto eqPos = result.rfind('=');
    while (eqPos != std::string::npos)
    {
        result.erase(eqPos, 1);
        eqPos = result.rfind('=');
    }

    return result;
}

skaptcha_tools::crypto::HashSignature::HashSignature()
{
    const auto random = string::simpleRandom(32);
    m_key.resize(random.size());

    for (size_t i = 0; i < m_key.size(); ++i)
    {
        m_key[i] = random[i];
    }
}
