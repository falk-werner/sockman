/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "sockman/manager.hpp"
#include "sockman/mock_handler.hpp"

#include <gtest/gtest.h>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <stdexcept>

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
    auto handler = std::make_shared<sockman::mock_handler>();
    paired_sockets sockets;

    manager.add(sockets.get0(), handler);
}

TEST(socketmanager, add_fails_with_invalid_socket)
{
    sockman::manager manager;
    auto handler = std::make_shared<sockman::mock_handler>();

    ASSERT_THROW({
        manager.add(-1, handler);
    }, std::exception);
}


TEST(socketmanager, remove)
{
    sockman::manager manager;
    auto handler = std::make_shared<sockman::mock_handler>();
    paired_sockets sockets;

    manager.add(sockets.get0(), handler);
    manager.remove(sockets.get0());
}

TEST(socketmanager, remove_unknown_socket)
{
    sockman::manager manager;
    auto handler = std::make_shared<sockman::mock_handler>();
    paired_sockets sockets;

    manager.remove(sockets.get0());
}

TEST(socketmanager, callback_on_writable)
{
    sockman::manager manager;
    auto handler = std::make_shared<sockman::mock_handler>();
    EXPECT_CALL(*handler, on_writable()).Times(1);

    paired_sockets sockets;
    manager.add(sockets.get0(), handler);
    manager.notify_on_writable(sockets.get0());

    manager.service();
}

TEST(socketmanager, callback_on_closed)
{
    sockman::manager manager;
    auto handler = std::make_shared<sockman::mock_handler>();
    EXPECT_CALL(*handler, on_hungup()).Times(1);

    paired_sockets sockets;
    manager.add(sockets.get0(), handler);
    ::close(sockets.get1());

    manager.service();
}

TEST(socketmanager, callback_on_readable)
{
    sockman::manager manager;
    auto handler = std::make_shared<sockman::mock_handler>();
    EXPECT_CALL(*handler, on_readable()).Times(1);

    paired_sockets sockets;
    manager.add(sockets.get0(), handler);
    manager.notify_on_readable(sockets.get0());
    char c = 42;
    ::write(sockets.get1(), &c, 1);

    manager.service();
}
