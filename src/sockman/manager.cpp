/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "sockman/manager.hpp"
#include "sockman/socketcontext.hpp"

#include <unistd.h>
#include <sys/epoll.h>

#include <cstring>

#include <unordered_map>
#include <stdexcept>

namespace sockman
{

class manager::detail
{
    detail(detail const &) = delete;
    detail& operator=(detail const &) = delete;
    detail(detail &&) = delete;
    detail& operator=(detail &&) = delete;
public:
    detail(int epfd)
    : fd(epfd)
    {
    }

    ~detail()
    {
        ::close(fd);
    }

    void modify(int sock, uint32_t mast, bool enable);

    int fd;
    std::unordered_map<int, socketcontext> sockets;
};

manager::manager()
{
    int fd = epoll_create1(0);
    if (0 > fd)
    {
        throw std::runtime_error("failed to create epoll socket");
    }

    d = new detail(fd);
}

manager::~manager()
{
    delete d;
}

manager::manager(manager && other)
{
    if (this != &other)
    {
        this->d = other.d;
        other.d = nullptr;
    }
}

manager& manager::operator=(manager && other)
{
    if (this != &other)
    {
        this->d = other.d;
        other.d = nullptr;
    }

    return *this;
}

void manager::add(int sock, std::shared_ptr<handler_i> handler)
{
    remove(sock);

    epoll_event event;
    memset(&event, 0, sizeof(event));
    event.data.ptr = reinterpret_cast<void*>(handler.get());
    event.events = 0;

    int const rc = epoll_ctl(d->fd, EPOLL_CTL_ADD, sock, &event);
    if (0 != rc)
    {
        throw std::runtime_error("epoll_ctl: failed to add socket");
    }

    d->sockets.insert({sock, {0, handler}});
}

void manager::remove(int sock)
{
    auto it = d->sockets.find(sock);
    if (it!= d->sockets.end())
    {
        epoll_ctl(d->fd, EPOLL_CTL_DEL, sock, nullptr);
        d->sockets.erase(it);
    }
}

void manager::notify_on_readable(int sock, bool enable)
{
    d->modify(sock, EPOLLIN, enable);
}

void manager::notify_on_writable(int sock, bool enable)
{
    d->modify(sock, EPOLLOUT, enable);
}

void manager::service(int timeout)
{
    epoll_event event;
    int const rc = epoll_wait(d->fd, &event, 1, -1);
    if (1 == rc)
    {
        auto * const handler = reinterpret_cast<handler_i*>(event.data.ptr);
        if (0 != (event.events & EPOLLERR))
        {
            handler->on_error();            
        }
        else if (0 != (event.events & EPOLLHUP))
        {
            handler->on_hungup();            
        }
        else if (0 != (event.events & EPOLLIN))
        {
            handler->on_readable();
        }
        else if (0 != (event.events & EPOLLOUT))
        {
            handler->on_writable();
        }
    }
}

void manager::detail::modify(int sock, uint32_t mask, bool enable)
{
    auto it = sockets.find(sock);
    if (it != sockets.end())
    {
        epoll_event event;
        memset(&event, 0, sizeof(event));
        event.data.ptr = reinterpret_cast<void*>(it->second.handler.get());
        if (enable)
        {
            event.events = it->second.events | mask;
        }
        else
        {
            event.events = it->second.events & (~mask);
        }

        if (event.events != it->second.events)
        {
            it->second.events = event.events;
            int const rc = epoll_ctl(fd, EPOLL_CTL_MOD, sock, &event);
            if (0 != rc)
            {
                throw std::runtime_error("epoll_ctl: failed to modify socket");
            }
        }
    }
    else
    {
        throw std::runtime_error("socket not found");
    }
}


}
