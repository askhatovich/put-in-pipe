// Copyright (C) 2025  Roman Lyubimov
// SPDX-License-Identifier: GPL-3.0-or-later
// For full license text, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#pragma once

#include <memory>
#include <vector>
#include <algorithm>
#include <mutex>
#include <shared_mutex>
#include <any>

template<typename EventType>
class Subscriber : public std::enable_shared_from_this<Subscriber<EventType>>
{
public:
    virtual ~Subscriber() = default;
    virtual void update(EventType type, std::any any) = 0;

protected:
    Subscriber() = default;
    Subscriber(const Subscriber&) = delete;
    Subscriber& operator=(const Subscriber&) = delete;
};

template<typename SubscriberType, typename... Args>
std::shared_ptr<SubscriberType> createSubscriber(Args&&... args)
{
    struct MakeSharedEnabler : public SubscriberType
    {
        MakeSharedEnabler(Args&&... args)
            : SubscriberType(std::forward<Args>(args)...) {}
    };
    return std::make_shared<MakeSharedEnabler>(std::forward<Args>(args)...);
}



template<typename EventType>
class Publisher
{
public:
    virtual ~Publisher()
    {
        std::unique_lock lock(m_mutex);
        m_subscribers.clear();
    }

    void addSubscriber(std::shared_ptr<Subscriber<EventType>> subscriber)
    {
        if (subscriber == nullptr)
        {
            return;
        }

        std::unique_lock lock(m_mutex);

        if (!hasSubscriber(subscriber))
        {
            m_subscribers.push_back(subscriber);
        }
    }

    void removeSubscriber(std::shared_ptr<Subscriber<EventType>> subscriber)
    {
        std::unique_lock lock(m_mutex);

        auto it = std::find_if(m_subscribers.begin(), m_subscribers.end(),
                               [&subscriber](const std::weak_ptr<Subscriber<EventType>>& wp) {
                                   return !wp.expired() && wp.lock() == subscriber;
                               });

        if (it != m_subscribers.end())
        {
            m_subscribers.erase(it);
        }
    }

    void notifySubscribers(const EventType& event, std::any data)
    {
        std::vector<std::shared_ptr<Subscriber<EventType>>> validSubcribers;

        {
            std::unique_lock lock(m_mutex);

            m_subscribers.erase(
                std::remove_if(m_subscribers.begin(), m_subscribers.end(),
                               [&validSubcribers](const std::weak_ptr<Subscriber<EventType>>& wp) {
                                   if (auto sp = wp.lock()) {
                                       validSubcribers.push_back(sp);
                                       return false;
                                   }
                                   return true;
                               }),
                m_subscribers.end()
                );
        }

        for (auto& subscriber : validSubcribers)
        {
            subscriber->update(event, data);
        }
    }

    size_t countObservers() const
    {
        std::shared_lock lock(m_mutex);

        return std::count_if(m_subscribers.begin(), m_subscribers.end(),
                             [](const std::weak_ptr<Subscriber<EventType>>& wp) {
                                 return !wp.expired();
                             });
    }

protected:
    Publisher() = default;

private:
    mutable std::shared_mutex m_mutex;
    std::vector<std::weak_ptr<Subscriber<EventType>>> m_subscribers;

    bool hasSubscriber(std::shared_ptr<Subscriber<EventType>> observer) const
    {
        return std::any_of(m_subscribers.begin(), m_subscribers.end(),
                           [&observer](const std::weak_ptr<Subscriber<EventType>>& wp) {
                               return !wp.expired() && wp.lock() == observer;
                           });
    }
};
