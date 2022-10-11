/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef SOCKMAN_HPP
#define SOCKMAN_HPP

#include <sys/epoll.h>
#include <functional>

namespace sockman
{

constexpr uint32_t const readable = EPOLLIN;
constexpr uint32_t const writable = EPOLLOUT;

class socket_events
{
public:
    explicit inline socket_events(uint32_t events)
    : events_(events)
    {
    }

    inline ~socket_events() = default;

    inline operator uint32_t() const
    {
        return events_;
    }

    inline bool readable() const
    {
        return (0 != (events_ & EPOLLIN));
    }

    inline bool writable() const
    {
        return (0 != (events_ & EPOLLOUT));
    }

    inline bool hungup() const
    {
        return (0 != (events_ & EPOLLHUP));
    }

    inline bool error() const
    {
        return (0 != (events_ & EPOLLERR));
    }

private:
    uint32_t const events_;
};

using socket_callback = ::std::function<void(int fd, socket_events events)>;


class manager
{
    manager(manager const &) = delete;
    manager& operator=(manager const &) = delete;
public:
    manager();
    ~manager();
    manager(manager && other);
    manager& operator=(manager && other);

    void add(int sock, uint32_t events, socket_callback callback);
    void remove(int sock);
    void notify_on_readable(int sock, bool enable = true);
    void notify_on_writable(int sock, bool enable = true);

    void service(int timeout = -1);
private:
    class detail;
    detail * d;
};

}

#endif