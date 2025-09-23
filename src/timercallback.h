// Copyright (C) 2025  Roman Lyubimov
// SPDX-License-Identifier: GPL-3.0-or-later
// For full license text, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#pragma once

#include <functional>
#include <chrono>
#include <asio.hpp>
#include <atomic>

class TimerCallback
{
public:
    using Callback = std::function<void()>;
    using Duration = std::chrono::seconds;

    TimerCallback(asio::io_context& ioContext, Callback callback, Duration duration)
                  : m_timer(ioContext)
                  , m_callback(std::move(callback))
                  , m_duration(duration)
                  , m_startTime(std::chrono::steady_clock::now())
                  , m_isRunning(false) {}

    TimerCallback(TimerCallback&& another) noexcept;
    ~TimerCallback();

    void start();
    void stop();
    bool isRunning() const;
    Duration timeRemaining() const;
    void restart(Duration newDuration);
    void restart();

private:
    asio::steady_timer m_timer;
    Callback m_callback;
    Duration m_duration;
    std::chrono::steady_clock::time_point m_startTime;
    std::atomic<bool> m_isRunning;
};
