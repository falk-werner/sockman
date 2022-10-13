/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <sockman/sockman.hpp>

#include <map>
#include <iostream>
#include <chrono>

namespace
{

struct timeout_id
{
    int64_t timeout;
    int id;

    bool operator<(timeout_id const & other) const
    {
        if (timeout != other.timeout)
        {
            return (timeout < other.timeout);
        }
        else
        {
            return (id < other.id);
        }
    }
};

class timeout_manager
{
public:

    timeout_manager();
    timeout_id add(int millis, std::function<void()> handler);
    void remove(timeout_id id);
    int poll();
private:
    int64_t now() const;
    int id_;
    std::map<timeout_id, std::function<void()>> timeouts;
};

timeout_manager::timeout_manager()
: id_(0)
{

}

timeout_id timeout_manager::add(int millis, std::function<void()> handler)
{
    timeout_id id = {now() + millis, ++id_};
    timeouts.emplace(id, handler);    
    return id;
}

void timeout_manager::remove(timeout_id id)
{
    timeouts.erase(id);
}

int timeout_manager::poll()
{
    auto now_ = now();
    auto entry = timeouts.begin();
    while ((entry != timeouts.end()) && (entry->first.timeout < now_))
    {
        auto handler = entry->second;
        timeouts.erase(entry);

        handler();
        entry = timeouts.begin();
    }

    int result = -1;
    if (entry != timeouts.end())
    {
        result = entry->first.timeout - now_;
    }

    return result;
}

int64_t timeout_manager::now() const
{
    auto now_ = std::chrono::steady_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now_);
    return now_ms.time_since_epoch().count();
}

}


int main(int argc, char * argv[])
{
    bool shutdown_requested = false;

    sockman::manager manager;

    timeout_manager timeouts;
    auto id = timeouts.add(3000, [](){
        std::cout << "this should not be shown" << std::endl;
    });

    timeouts.add(2000, [id, &timeouts](){
        std::cout << "1st timeout" << std::endl;
        timeouts.remove(id);
    });

    timeouts.add(5000, [&shutdown_requested](){
        std::cout << "final timeout" << std::endl;
        shutdown_requested = true;
    });

    int timeout = timeouts.poll();
    while (!shutdown_requested)
    {
        std::cout << "loop" << std::endl;
        manager.service(timeout);
        timeout = timeouts.poll();
    }

    return EXIT_SUCCESS;
}