/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <sockman/sockman.hpp>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <cstdlib>
#include <cstring>
#include <csignal>

#include <string>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace
{

bool shutdown_requested = false;
void on_shutdown_requested(int)
{
    shutdown_requested = true;
}


class base_handler: public sockman::handler_i
{
public:
    void on_readable() override { }
    void on_writable() override { }
    void on_hungup() override { }
    void on_error() override { }
};

class connection
: public base_handler
, std::enable_shared_from_this<connection>
{
    connection(sockman::manager& manager, int sock, int id)
    : manager_(manager)
    , fd(sock)
    , id_(id)
    {
        std::cout << "connection #" << id_ << ": connected" << std::endl;
    }

public:
    static std::shared_ptr<connection> create(sockman::manager& manager, int sock, int id)
    {
        auto instance = std::shared_ptr<connection>(new connection(manager, sock, id));
        manager.add(instance->fd, instance);
        manager.notify_on_readable(instance->fd);

        return instance;
    }

    ~connection() 
    {
        ::close(fd);
        std::cout << "connection #" << id_ << ": closed" << std::endl;
    }

    void on_readable() override
    {
        uint8_t buffer[257];
        auto count = ::read(fd, buffer, 1);
        if (count != 1)
        {
            std::cerr << "connection #" << id_ << ": error: failed to read length" << std::endl;
            manager_.remove(fd);
            return;
        }

        count = ::read(fd, &(buffer[1]), buffer[0]);
        if (count != buffer[0])
        {
            std::cerr << "connection #" << id_ << ": error: failed to value (length: " << (int) buffer[0] << ")" << std::endl;
            manager_.remove(fd);
            return;            
        }
        buffer[buffer[0] + 1] = '\0';
        std::cout << "connection #" << id_ << ": received: " << buffer << std::endl;

        count = ::write(fd, buffer, buffer[0] + 1);
        if (count != (buffer[0] + 1))
        {
            manager_.remove(fd);
            return;
        }
    }

    void on_hungup() override
    {
        std::cout << "connection #" << id_ << ": hung up" << std::endl;
        manager_.remove(fd);
    }

    void on_error() override
    {
        std::cout << "connection #" << id_ << ": error" << std::endl;
        manager_.remove(fd);
    }


public:
    sockman::manager & manager_;
    int fd;
    int id_;
};

class listener
: public base_handler
, std::enable_shared_from_this<listener>
{
    listener(sockman::manager& manager, std::string const & path)
    : manager_(manager)
    , path_(path)
    , connection_id(0)
    {
        if (path.size() > 107)
        {
            throw std::runtime_error("path too long");
        }

        fd = socket(AF_LOCAL, SOCK_STREAM, 0);
        if (0 > fd)
        {
            throw std::runtime_error("failed to create socket");            
        }

        ::unlink(path.c_str());

        sockaddr_un address;
        memset(&address, 0, sizeof(address));
        address.sun_family = AF_LOCAL;
        strcpy(address.sun_path, path.c_str());

        int rc = ::bind(fd, (sockaddr*) &address, sizeof(address));
        if (0 != rc)
        {
            ::close(fd);
            throw std::runtime_error("bind failed");
        }

        rc = ::listen(fd, 5);
        if (0 != rc)
        {
            ::close(fd);
            throw std::runtime_error("bind failed");
        }

    }
public:
    static std::shared_ptr<listener> create(sockman::manager& manager, std::string path)
    {
        auto instance = std::shared_ptr<listener>(new listener(manager, path));

        manager.add(instance->fd, instance);
        manager.notify_on_readable(instance->fd);

        return instance;
    }

    ~listener()
    {
        ::close(fd);
        ::unlink(path_.c_str());
    }

    void on_readable() override
    {
        int client_fd = accept(fd, nullptr, nullptr);
        if (0 < client_fd)
        {
            connection::create(manager_, client_fd, ++connection_id);
        }
    }

private:
    sockman::manager& manager_;
    std::string path_;
    int fd;
    int connection_id;
};

}

int main(int argc, char* argv[])
{
    if (argc > 1)
    {
        signal(SIGINT, on_shutdown_requested);
        std::string path = argv[1];

        sockman::manager manager;
        auto server = listener::create(manager, path);

        while (!shutdown_requested)
        {
            manager.service();
        }
    }
    else
    {
        std::cout << "usage: echo_server <path>" << std::endl;
    }

    return EXIT_SUCCESS;
}