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
#include <iostream>
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


class connection: public base_handler
{
public:
    explicit connection(sockman::manager & manager)
    : manager_(manager)
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

    void on_readable()
    {
        std::string value = read();
        std::cout << value << std::endl;
    }

    void on_writable()
    {
        write(value);
        manager_.notify_on_writable(fd, false);
    }

    void request_write(std::string data)
    {
        value = data;
        manager_.notify_on_writable(fd);
    }

    void on_hungup()
    {
        std::cerr << "error: connection closed by remote" << std::endl;
        shutdown_requested = true;
    }

private:
    void write(std::string const & value)
    {
        if (value.size() < 256)
        {
            uint8_t length = static_cast<uint8_t>(value.size());
            auto count = ::write(fd, &length, 1);
            if (count != 1)
            {
                std::cerr << "write failed" << std::endl;
                shutdown_requested = true;
                return;
            }

            count = ::write(fd, value.data(), value.size());
            if (count != value.size())
            {
                std::cerr << "write failed" << std::endl;
                shutdown_requested = true;
                return;
            }
        }
        else
        {
            std::cerr << "error: line too long" << std::endl;
        }
    }

    std::string read()
    {
        uint8_t length;
        auto count = ::read(fd, &length, 1);
        if (count != 1)
        {
            std::cerr << "read failed" << std::endl;
            shutdown_requested = true;
            return "";
        }

        char buffer[256];
        count = ::read(fd, buffer, length);
        if (count != length)
        {
            std::cerr << "read failed" << std::endl;
            shutdown_requested = true;
            return "";
        }
        buffer[length] = '\0';

        return buffer;
    }

    sockman::manager & manager_;
    int fd;
    std::string value;
};

class input_handler: public base_handler
{
public:
    explicit input_handler(connection& conn)
    : conn_(conn)
    {

    }

    void on_readable()
    {
        std::string line;
        if (std::getline(std::cin, line))
        {
            if (("exit" != line) && ("quit" != line))
            {
                conn_.request_write(line);
            }
            else
            {
                shutdown_requested = true;
            }
        }
        else
        {
            shutdown_requested = true;
        }
    }
private:
    connection &conn_;
};

}

int main(int argc, char * argv[])
{
    if (argc > 1)
    {
        signal(SIGINT, on_shutdown_requested);
        std::string path = argv[1];

        sockman::manager manager;

        auto conn = std::make_shared<connection>(manager);
        conn->connect(path);

        manager.add(conn->get_fd(), conn);
        manager.notify_on_readable(conn->get_fd());

        manager.add(STDIN_FILENO, std::make_shared<input_handler>(*conn.get()));
        manager.notify_on_readable(STDIN_FILENO);


        while (!shutdown_requested)
        {
            manager.service();
        }

        std::cout << "shutdown" << std::endl;        
    }
    else
    {
        std::cout << "usage: echo_client <path>" << std::endl;
    }

    return EXIT_SUCCESS;
}