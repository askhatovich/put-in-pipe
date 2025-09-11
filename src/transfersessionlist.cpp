#include "transfersessionlist.h"
#include "client.h"
#include "observerpattern.h"
#include "transfersession.h"
#include "config/config.h"

#include <mutex>

TransferSessionList::~TransferSessionList()
{
    m_ioContext.stop();
    m_ioContextThreadPtr->join();
    delete m_ioContextThreadPtr;
}

TransferSessionList &TransferSessionList::instanse()
{
    static TransferSessionList list;
    return list;
}

TransferSessionList::SessionAndTimeout TransferSessionList::create(std::shared_ptr<Client> creator)
{
    // checked in webapi
    // if (not possibleToCreateNew()) return nullptr;

    if (creator == nullptr) return {nullptr, 0};

    std::unique_lock lock (m_mutex);

    /*
     * The session identifier is equal to the public identifier of its creator,
     * in order to avoid difficulties in detecting attempts to create multiple
     * sessions by one client.
     */
    const std::string id = creator->publicId();
    while (m_map.contains(id))
    {
        return {nullptr, 0};
    }

    auto session = createSubscriber<TransferSession>(creator, id, m_ioContext);

    session->Publisher<Event::TransferSession>::addSubscriber(creator);
    session->Publisher<Event::TransferSessionForSender>::addSubscriber(creator);
    creator->Publisher<Event::ClientInternal>::addSubscriber(session);

    TimerCallback timer(m_ioContext,
                        [this, id](){ removeDueTimeout(id); },
                        TimerCallback::Duration(Config::instance().transferSessionMaxLifetime()));
    timer.start();

    // returned value ignored because id already is checked to be unique
    m_map.try_emplace(
        id,
        SessionWithTimer {
            session,
            std::move(timer)
        }
    );

    return {session, timer.timeRemaining().count()};
}

TransferSessionList::SessionAndTimeout TransferSessionList::get(const std::string &id)
{
    std::shared_lock lock (m_mutex);

    const auto iter = m_map.find(id);
    if (iter == m_map.end())
    {
        return {nullptr, 0};
    }

    return {iter->second.session, iter->second.timer.timeRemaining().count()};
}

size_t TransferSessionList::count() const
{
    std::shared_lock lock (m_mutex);
    return m_map.size();
}

bool TransferSessionList::possibleToCreateNew()
{
    std::shared_lock lock (m_mutex);

    return Config::instance().transferSessionCountLimit() > m_map.size();
}

void TransferSessionList::remove(const std::string &id)
{   
    /*
     * A design to avoid deadlocks on recursive calls to this function
     */

    std::shared_ptr<TransferSession> session = nullptr;
    {
        std::unique_lock lock (m_mutex);
        auto iter = m_map.find(id);
        if (iter == m_map.end())
        {
            return;
        }
        session = iter->second.session;
        m_map.erase(iter);
    }
}

TransferSessionList::TransferSessionList()
{
    m_ioContextThreadPtr = new std::thread([&]() {
        asio::executor_work_guard<asio::io_context::executor_type> work = asio::make_work_guard(m_ioContext);
        m_ioContext.run();
    });
}

void TransferSessionList::removeDueTimeout(const std::string &id)
{
    std::unique_lock lock (m_mutex);

    auto iter = m_map.find(id);
    if (iter == m_map.end()) return;

    iter->second.session->setTimedout();

    m_map.erase(iter);
}

