// Copyright (C) 2025  Roman Lyubimov
// SPDX-License-Identifier: GPL-3.0-or-later
// For full license text, see <https://www.gnu.org/licenses/gpl-3.0.txt>

// The name of the captcha is derived from Sofia Kopikova,
// who is the author of the original captcha in C (see "skaptcha_backend" directory)

#pragma once

#include <vector>
#include <string>
#include <memory>
#include <chrono>

class Skaptcha
{
public:
    struct View
    {
        std::vector<uint8_t> png;
        std::string token;
    };

    /*
     * The singleton is used for guaranteed initialization of srand(time(NULL)),
     * this is the need for a backend written in C.
     */
    static Skaptcha& instance();
    static unsigned answerLength();

    /*
     * Context is additional information that makes sense to one recipient.
     * For example, the IP address to which the captcha was issued,
     * or the unique user ID that will be assigned to it after completing the captcha.
     */
    std::shared_ptr<View> generate(const std::string& context, std::chrono::seconds lifetime = std::chrono::seconds(60*3));
    bool validate(const std::string& context, const std::string& token, const std::string& answer);

private:
    Skaptcha();
    Skaptcha(const Skaptcha&) = delete;
    Skaptcha(Skaptcha&&) = delete;
    Skaptcha& operator=(const Skaptcha&) = delete;
};
