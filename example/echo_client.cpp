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
#include <csignal>

#include <string>
#include <queue>
#include <iostream>
#include <stdexcept>

namespace
{

bool shutdown_requested = false;
void on_shutdown_requested(int)
{
    shutdown_requested = true;
}

class connection
{
public:
    connection()
    {
        fd = socket(AF_LOCAL, SOCK_STREAM, 0);
        if (0 > fd)
        {
            throw std::runtime_error("failed to create socket");            
        }
    }

    ~connection()
    {
        ::close(fd);
    }

    int get_fd() const
    {
        return fd;
    }

    void connect(std::string const & path)
    {
        if (path.size() > 107)
        {
            throw std::runtime_error("path too long");
        }

        sockaddr_un address;
        memset(&address, 0, sizeof(address));
        address.sun_family = AF_LOCAL;
        strcpy(address.sun_path, path.c_str());

        int rc = ::connect(fd, (sockaddr*) &address, sizeof(address));
        if (0 != rc)
        {
            ::close(fd);
            throw std::runtime_error("connect failed");
        }
    }

    void write(std::string const & value)
    {
        if (value.size() < 256)
        {
            uint8_t length = static_cast<uint8_t>(value.size());
            write_exact(reinterpret_cast<void*>(&length), 1);
            write_exact(reinterpret_cast<void const*>(value.data()), value.size());
        }
        else
        {
            std::cerr << "error: line too long" << std::endl;
        }
    }

    std::string read()
    {
        uint8_t length;
        read_exact(reinterpret_cast<void*>(&length), 1);

        char buffer[256];
        read_exact(reinterpret_cast<void*>(buffer), length);
        buffer[length] = '\0';

        return buffer;
    }

private:
    void read_exact(void * buffer, size_t length)
    {
        auto const count = ::read(fd, buffer, length);
        if (count != length)
        {
            throw std::runtime_error("read failed");
        }
    }

    void write_exact(void const * buffer, size_t length)
    {
        auto const count = ::write(fd, buffer, length);
        if (count != length)
        {
            throw std::runtime_error("write failed");
        }
    }

    int fd;
};

}

int main(int argc, char * argv[])
{
    if (argc > 1)
    {
        signal(SIGINT, on_shutdown_requested);
        std::string path = argv[1];

        sockman::manager manager;
        std::queue<std::string> messages;

        connection conn;
        conn.connect(path);

        manager.add(conn.get_fd(), sockman::readable, [&manager, &conn, &messages](int, auto events){
            if ((events.error()) || (events.hungup()))
            {
                shutdown_requested = true;
            }
            else if (events.readable())
            {
                std::cout << conn.read() << std::endl;
            }
            else if (events.writable())
            {
                if (!messages.empty())
                {
                    conn.write(messages.front());
                    messages.pop();
                }

                manager.notify_on_writable(conn.get_fd(), !messages.empty());
            }

        });

        manager.add(STDIN_FILENO, sockman::readable, [&manager, &conn, &messages](int, auto events){
            if (events.readable())
            {
                std::string line;
                if (std::getline(std::cin, line))
                {
                    messages.push(line);
                    manager.notify_on_writable(conn.get_fd());
                }
            }
        });


        while (!shutdown_requested)
        {
            try
            {
                manager.service();
            }
            catch (std::exception const & ex)
            {
                std::cerr << "error: " << ex.what() << std::endl;
                shutdown_requested = true;
            }
        }

        std::cout << "shutdown" << std::endl;        
    }
    else
    {
        std::cout << "usage: echo_client <path>" << std::endl;
    }

    return EXIT_SUCCESS;
}