/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "sockman/sockman.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <stdexcept>

using ::testing::_;

class mock_handler
{
public:
    MOCK_METHOD(void, handle, (int, uint32_t));
};

class paired_sockets
{
public:
    paired_sockets()
    {
        int const rc = ::socketpair(AF_LOCAL, SOCK_STREAM, 0, fds);
        if (0 != rc)
        {
            throw std::runtime_error("failec to create socket pair");
        }
    }

    ~paired_sockets()
    {
        ::close(fds[0]);
        ::close(fds[1]);
    }

    int get0() const
    {
        return fds[0];
    }

    int get1() const
    {
        return fds[1];
    }

private:
    int fds[2];
};


TEST(socketmanager, create)
{
    sockman::manager manager;
}

TEST(socketmanager, add_socket)
{
    sockman::manager manager;
    paired_sockets sockets;

    manager.add(sockets.get0(), 0, [](int fd, uint32_t mask){});
}

TEST(socketmanager, add_fails_with_invalid_socket)
{
    sockman::manager manager;

    ASSERT_THROW({
        manager.add(-1,  0, [](int fd, uint32_t mask){});
    }, std::exception);
}


TEST(socketmanager, remove)
{
    sockman::manager manager;
    paired_sockets sockets;

    manager.add(sockets.get0(),  0, [](int fd, uint32_t mask){});
    manager.remove(sockets.get0());
}

TEST(socketmanager, remove_unknown_socket)
{
    sockman::manager manager;
    paired_sockets sockets;

    manager.remove(sockets.get0());
}

TEST(socketmanager, callback_on_writable)
{
    sockman::manager manager;
    mock_handler handler;
    EXPECT_CALL(handler, handle(_, EPOLLOUT)).Times(1);

    paired_sockets sockets;
    manager.add(sockets.get0(), EPOLLOUT, [&handler](int fd, uint32_t event){handler.handle(fd, event);});

    manager.service();
}

TEST(socketmanager, callback_on_closed)
{
    sockman::manager manager;
    mock_handler handler;
    EXPECT_CALL(handler, handle(_, EPOLLHUP)).Times(1);

    paired_sockets sockets;
    manager.add(sockets.get0(), 0, [&handler](int fd, uint32_t event){handler.handle(fd, event);});
    ::close(sockets.get1());

    manager.service();
}

TEST(socketmanager, callback_on_readable)
{
    sockman::manager manager;
    mock_handler handler;
    EXPECT_CALL(handler, handle(_, EPOLLIN)).Times(1);

    paired_sockets sockets;
    manager.add(sockets.get0(), EPOLLIN, [&handler](int fd, uint32_t event){handler.handle(fd, event);});
    char c = 42;
    ::write(sockets.get1(), &c, 1);

    manager.service();
}

TEST(socketmanager, move_assign)
{
    sockman::manager first;
    sockman::manager second;

    first = std::move(second);
}