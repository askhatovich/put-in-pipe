// Copyright (C) 2025  Roman Lyubimov
// SPDX-License-Identifier: GPL-3.0-or-later
// For full license text, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#include "timercallback.h"

TimerCallback::TimerCallback(TimerCallback &&another) noexcept
    : m_timer(std::move(another.m_timer))
    , m_callback(std::move(another.m_callback))
    , m_duration(std::move(another.m_duration))
    , m_startTime(std::move(another.m_startTime))
    , m_isRunning(another.m_isRunning.load())
{
}

TimerCallback::~TimerCallback()
{
    stop();
}

void TimerCallback::start()
{
    if (m_isRunning.exchange(true))
    {
        return;
    }

    m_startTime = std::chrono::steady_clock::now();
    m_timer.expires_after(m_duration);

    m_timer.async_wait([this](const std::error_code& ec) {
        if (!ec && m_isRunning.load())
        {
            m_isRunning.store(false);
            if (m_callback)
            {
                m_callback();
            }
        }
    });
}

void TimerCallback::stop()
{
    if (m_isRunning.exchange(false))
    {
        m_timer.cancel();
    }
}

bool TimerCallback::isRunning() const
{
    return m_isRunning.load();
}

void TimerCallback::restart(Duration newDuration)
{
    stop();
    m_duration = newDuration;
    start();
}

void TimerCallback::restart()
{
    stop();
    start();
}

TimerCallback::Duration TimerCallback::timeRemaining() const
{
    if (not m_isRunning.load())
    {
        return Duration(0);
    }

    const auto elapsed = std::chrono::steady_clock::now() - m_startTime;
    const auto remaining = m_duration - std::chrono::duration_cast<Duration>(elapsed);

    return std::max(Duration(0), remaining);
}
