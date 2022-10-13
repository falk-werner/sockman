/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <sockman/sockman.hpp>

#include <getopt.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <csignal>

#include <string>
#include <iostream>
#include <unordered_map>
#include <memory>
#include <stdexcept>

namespace
{

constexpr size_t const max_message_size = 250;

class context
{
public:
    context(int argc, char* argv[]);

    std::string path;
    int exit_code;
    bool show_help;
};

class connection
{
    connection(connection const &) = delete;
    connection& operator=(connection const &) = delete;
public:
    explicit connection(int sock);
    connection(connection && other);
    connection& operator=(connection && other);
    ~connection();
    std::string const & get_name();
    void send(std::string const & message);
    std::string receive();
private:
    int fd;
    std::string name;
};

class chat_server
{
    chat_server(chat_server const &) = delete;
    chat_server& operator=(chat_server const &) = delete;
    chat_server(chat_server &&) = delete;
    chat_server& operator=(chat_server &&) = delete;
public:
    explicit chat_server(std::string const & path);
    ~chat_server();
    void run();
private:
    int fd;
    std::string path_;
};

bool shutdown_requested = false;
void on_shutdown_requested(int)
{
    shutdown_requested = true;
}

void print_usage()
{
    std::cout << R"(chat_server
A sockman example.

Usage:
    chat_server [-n <name>] [-s <path>]

Arguments:
    -s, --socket <path>   Path of the server socket to connect (default: /tmp/sockman_chat.sock)
    -h, --help            Prints this message

Example:
    chat_server /path/to/server.sock
)";
}

context::context(int argc, char* argv[])
: path("/tmp/sockman_chat.sock")
, exit_code(EXIT_SUCCESS)
, show_help(false)
{
    static option const long_options[] =
    {
        {"help", no_argument, nullptr, 'h'},
        {"socket", required_argument, nullptr, 's'},
        {nullptr, 0, nullptr, 0}
    };

    opterr = 0;
    optind = 0;

    bool finished = false;
    while (!finished)
    {
        int option_index = 0;
        int const c = getopt_long(argc, argv, "n:h", long_options, &option_index);
        switch (c)
        {
            case -1:
                finished = true;
                break;
            case 'h':
                show_help = true;
                finished = true;
                break;
            case 's':
                path = optarg;
                break;
            default:
                std::cerr << "error: invalid arguments" << std::endl;
                show_help = true;
                finished = true;
                break;
        }
    }
}

connection::connection(int sock)
: fd(sock)
, name(receive())
{

}

connection::connection(connection && other)
{
    if (this != &other)
    {
        this->fd = other.fd;
        other.fd = -1;
        this->name = std::move(other.name);
    }
}

connection& connection::operator=(connection && other)
{
    if (this != &other)
    {
        this->fd = other.fd;
        other.fd = -1;
        this->name = std::move(other.name);
    }

    return *this;
}

connection::~connection()
{
    if (0 <= fd)
    {
        ::close(fd);
    }
}

std::string const & connection::get_name()
{
    return name;
}

void connection::send(std::string const & message)
{
    if (message.size() <= max_message_size)
    {
        uint8_t size = static_cast<uint8_t>(message.size());
        auto count = ::write(fd, reinterpret_cast<void const*>(&size), 1);
        if (count != 1)
        {
            std::cerr << "error: failed to send message" << std::endl;
            shutdown_requested = true;
            return;
        }

        count = ::write(fd, reinterpret_cast<void const *>(message.data()), size);
        if (count != size)
        {
            std::cerr << "error: failed to send message" << std::endl;
            shutdown_requested = true;
            return;
        }
    }
    else
    {
        std::cerr << "error: message too long" << std::endl;
    }
}

std::string connection::receive()
{
    uint8_t size;
    auto count = ::read(fd, reinterpret_cast<void*>(&size), 1);
    if (count == 0)
    {
        // connection closed
        return "";
    }
    else if (count != 1)
    {
        std::cerr << "error: failed to read message" << std::endl;
        shutdown_requested = true;
        return "";
    }

    if (0 == size)
    {
        return "";
    }

    char buffer[256];
    count = ::read(fd, reinterpret_cast<void*>(buffer), size);
    if (count != size)
    {
        std::cerr << "error: failed to read message" << std::endl;
        shutdown_requested = true;
        return "";
    }

    return std::string(buffer, size);
}


chat_server::chat_server(std::string const & path)
: path_(path)
{
    sockaddr_un address;

    if (path.size() > (sizeof(address.sun_path) - 1))
    {
        throw std::runtime_error("socket path too long");
    }

    fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (0 > fd)
    {
        throw std::runtime_error("failed to create socket");
    }    

    memset(reinterpret_cast<void*>(&address), 0, sizeof(address));
    address.sun_family = AF_LOCAL;
    strcpy(address.sun_path, path.c_str());

    ::unlink(path.c_str());
    int rc = ::bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address));
    if (rc != 0)
    {
        throw std::runtime_error("failed to bind");
    }

    constexpr int const backlog = 5;
    rc = ::listen(fd, backlog);
    if (rc != 0)
    {
        throw std::runtime_error("failed to listen");
    }
}

chat_server::~chat_server()
{
    ::close(fd);
    ::unlink(path_.c_str());
}

void chat_server::run()
{
    sockman::manager manager;
    std::unordered_map<int, std::shared_ptr<connection>> connections;

    manager.add(fd, sockman::readable, [&connections, &manager](int sock, auto events){
        if (events.readable())
        {
            int client_fd = ::accept(sock, nullptr, nullptr);
            if (0 <= client_fd)
            {
                auto conn = std::make_shared<connection>(client_fd);

                std::string const info = conn->get_name() + " has entered the chat";
                std::cout << info << std::endl;
                for(auto const & entry: connections)
                {
                    entry.second->send(info);
                }
                connections.insert({client_fd, conn});
                conn->send("Hi there, " + conn->get_name());

                manager.add(client_fd, sockman::readable, [&connections, &manager, conn](int fd, auto events) {
                    if (events.readable())
                    {
                        std::string message = conn->receive();
                        if (!message.empty())
                        {
                            std::string full_message = conn->get_name() + ": " + message;
                            std::cout << full_message << std::endl;

                            for(auto const &entry: connections)
                            {
                                if (entry.first != fd)
                                {
                                    entry.second->send(full_message);
                                }
                            }
                        }
                    }

                    if ((events.error()) || (events.hungup()))
                    {
                        std::string const info = conn->get_name() + " left the chat";
                        std::cout << info << std::endl;
                        for(auto const &entry: connections)
                        {
                            if (entry.first != fd)
                            {
                                entry.second->send(info);
                            }
                        }

                        connections.erase(fd);
                        manager.remove(fd);
                    }
                });
            }
        }
    });

    while (!shutdown_requested)
    {
        manager.service();
    }

    for(auto const &entry: connections)
    {
        manager.remove(entry.first);
    }
    manager.remove(fd);
}


}

int main(int argc, char* argv[])
{
    context ctx(argc, argv);

    if (!ctx.show_help)
    {
        signal(SIGINT, on_shutdown_requested);

        try
        {
            chat_server server(ctx.path);
            server.run();
        }
        catch (std::exception const & ex)
        {
            std::cerr << "error: " << ex.what() << std::endl;
            ctx.exit_code = EXIT_FAILURE;
        }
        catch (...)
        {
            std::cerr << "error: fatal error occured" << std::endl;
            ctx.exit_code = EXIT_FAILURE;
        }
    }
    else
    {
        print_usage();
    }

    return ctx.exit_code;
}