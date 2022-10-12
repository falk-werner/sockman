#include <sockman/sockman.hpp>

#include <getopt.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <csignal>

#include <string>
#include <iostream>
#include <stdexcept>

namespace
{

constexpr size_t const max_message_size = 80;

class context
{
public:
    context(int argc, char * argv[]);

    std::string name;
    std::string path;
    int exit_code;
    bool show_help;
};

class connection
{
    connection(connection const &) = delete;
    connection& operator=(connection const &) = delete;
    connection(connection &&) = delete;
    connection& operator=(connection &&) = delete;
public:
    connection();
    ~connection();
    int get_fd() const;
    void connect(std::string const & path);
    void send(std::string const & message);
    std::string receive();
private:
    int fd;
};

class chat_client
{
    chat_client(chat_client const &) = delete;
    chat_client& operator=(chat_client const &) = delete;
    chat_client(chat_client &&) = delete;
    chat_client& operator=(chat_client &&) = delete;
public:
    chat_client(std::string const & name, std::string const & path);
    ~chat_client() = default;
    void run();
private:
    std::string name_;
    connection conn;

};

bool shutdown_requested = false;
void on_shutdown_requested(int)
{
    shutdown_requested = true;
}

void print_usage()
{
    std::cout << R"(chat_client
A sockman example.

Usage:
    chat_client [-n <name>] [-s <path>]

Arguments:
    -n, --name <name>     Sets the name of the chat user (default: <user>)
    -s, --socket <path>   Path of the server socket to connect (default: /tmp/sockman_chat.sock)
    -h, --help            Prints this message

Example:
    chat_client Bob /path/to/server.sock
)";
}

context::context(int argc, char * argv[])
: name("<user>")
, path("/tmp/sockman_chat.sock")
, exit_code(EXIT_SUCCESS)
, show_help(false)
{
    static option const long_options[] =
    {
        {"help", no_argument, nullptr, 'h'},
        {"name", required_argument, nullptr, 'n'},
        {"socket", required_argument, nullptr, 's'},
        {nullptr, 0, nullptr, 0}
    };

    opterr = 0;
    optind = 0;

    bool finished = false;
    while (!finished)
    {
        int option_index = 0;
        int const c = getopt_long(argc, argv, "n:s:h", long_options, &option_index);
        switch (c)
        {
            case -1:
                finished = true;
                break;
            case 'h':
                show_help = true;
                finished = true;
                break;
            case 'n':
                name = optarg;
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

connection::connection()
{
    fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (0 > fd)
    {
        throw std::runtime_error("failed to create socket");
    }    
}

connection::~connection()
{
    ::close(fd);
}

int connection::get_fd() const
{
    return fd;
}

void connection::connect(std::string const & path)
{
    sockaddr_un address;

    if (path.size() > (sizeof(address.sun_path) - 1))
    {
        throw std::runtime_error("socket path too long");
    }

    memset(reinterpret_cast<void*>(&address), 0, sizeof(address));
    address.sun_family = AF_LOCAL;
    strcpy(address.sun_path, path.c_str());

    int const rc = ::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address));
    if (rc != 0)
    {
        throw std::runtime_error("failed to connect");
    }
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


chat_client::chat_client(std::string const & name, std::string const & path)
: name_(name)
{
    conn.connect(path);
    conn.send(name_);
}

void chat_client::run()
{
    sockman::manager manager;

    manager.add(STDIN_FILENO, sockman::readable, [this](int, auto events) {
        if (events.readable())
        {
            std::string message;
            if (std::getline(std::cin, message))
            {
                conn.send(message);
            }
        }
    });

    manager.add(conn.get_fd(), sockman::readable, [this](int, auto events) {
        if (events.readable())
        {
            std::string message = conn.receive();
            if (!message.empty())
            {
                std::cout << message << std::endl;
            }
        }

        if ((events.error()) || (events.hungup()))
        {
            std::cerr << "error: connection lost" << std::endl;
            shutdown_requested = true;
        }
    });

    while (!shutdown_requested)
    {
        manager.service();
    }
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
            chat_client client(ctx.name, ctx.path);
            client.run();
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