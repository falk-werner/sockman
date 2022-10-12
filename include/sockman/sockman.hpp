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

/// @brief readable event
///
/// This flag can be used at @ref manager::add to tell
/// the manager to callback when the socket is readable.
///
/// @see manager::add
constexpr uint32_t const readable = EPOLLIN;

/// @brief writable event
///
/// This flag can be used at @ref manager::add to tell
/// the manager to callback when the socket is writable.
///
/// @see manager::add
constexpr uint32_t const writable = EPOLLOUT;

/// @brief Wrapper to encapsulate socket events.
class socket_events
{
public:
    /// @brief Wraps epoll events.
    /// @param events events as defined by epoll
    explicit inline socket_events(uint32_t events)
    : events_(events)
    {
    }

    inline ~socket_events() = default;

    /// @brief Returns the plain epoll events.
    /// @return plain epoll events.
    inline operator uint32_t() const
    {
        return events_;
    }

    /// @brief Returns true, if events contains the readable event.
    /// @return True, if events contains readable event, false otherwise.
    inline bool readable() const
    {
        return (0 != (events_ & EPOLLIN));
    }

    /// @brief Returns true, if events contains the writable event.
    /// @return True, if events contains writable event, false otherwise.
    inline bool writable() const
    {
        return (0 != (events_ & EPOLLOUT));
    }

    /// @brief Returns true, if events contains the hungup event.
    /// @return True, if events contains hungup event, false otherwise.
    inline bool hungup() const
    {
        return (0 != (events_ & EPOLLHUP));
    }

    /// @brief Returns true, if events contains the error event.
    /// @return True, if events contains error event, false otherwise.
    inline bool error() const
    {
        return (0 != (events_ & EPOLLERR));
    }

private:
    uint32_t const events_;
};

/// @brief socket callback
///
/// Defines the callback the \ref manager will call
/// whenever an event is detected.
///
/// @param fd socket which raises the event
/// @param events collection of raised events
using socket_callback = ::std::function<void(int fd, socket_events events)>;

/// @brief socket event manager
class manager
{
    manager(manager const &) = delete;
    manager& operator=(manager const &) = delete;
public:
    /// @brief initialized a socket event manager
    manager();

    /// @brief cleans up the instance
    ~manager();

    /// @brief move constructor
    /// @param other instance, that should be moved
    manager(manager && other);

    /// @brief move assign operator
    /// @param other instance that should be moved
    /// @return refernce to actual instance
    manager& operator=(manager && other);

    /// @brief adds a socket to the manager
    ///
    /// @note it is not allowed to add the same socket twice
    ///       (an exception is thrown in this case)
    ///
    /// @param sock socket to add
    /// @param events events to listen (0, or any comination of \ref readable an \ref writable)
    /// @param callback callback to invoke on event
    void add(int sock, uint32_t events, socket_callback callback);

    /// @brief removed a socket from the manager
    /// @param sock socket to remove
    void remove(int sock);

    /// @brief enables or disabled notification of readable events
    ///
    /// @note it is not allowed to configure an unmanaged socket
    ///       (an exception is thrown in this case)
    ///
    /// @param sock socket to configure
    /// @param enable enables notifacation if true, otherwise notifiactions are disabled
    void notify_on_readable(int sock, bool enable = true);

    /// @brief enables or disabled notification of writable events
    ///
    /// @note it is not allowed to configure an unmanaged socket
    ///       (an exception is thrown in this case)
    ///
    /// @param sock socket to configure
    /// @param enable 
    void notify_on_writable(int sock, bool enable = true);

    /// @brief waits for the next socket event or timeout
    ///
    /// @note the timeout is measured against CLOCK_MONOTONIC
    ///
    /// @param timeout timeout in milliseconds, 0 means poll, -1 means to block until next event
    void service(int timeout = -1);
private:
    class detail;
    detail * d;
};

}

#endif