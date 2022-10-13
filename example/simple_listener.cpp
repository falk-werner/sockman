/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */


#include <sockman/sockman.hpp>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <cstring>
#include <csignal>
#include <iostream>

namespace
{

bool shutdown_requested = false;
void on_shutdown_requested(int)
{
    shutdown_requested = true;
}

}

int main()
{
    constexpr char const path[] = "/tmp/sockman_simple.sock";
    int const fd = socket(AF_LOCAL, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (0 > fd)
    {
        perror("socket");
        return EXIT_FAILURE;
    }

    sockaddr_un address;
    memset(reinterpret_cast<void*>(&address), 0, sizeof(address));
    address.sun_family = AF_LOCAL;
    strcpy(address.sun_path, path);

    unlink(path);
    int rc = bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address));
    if (rc != 0)
    {
        perror("bind");
        close(fd);
        return EXIT_FAILURE;
    }

    constexpr int const backlog = 5;
    rc = listen(fd, backlog);
    if (rc != 0)
    {
        perror("listen");
        close(fd);
        return EXIT_FAILURE;
    }

    sockman::manager manager;
    manager.add(fd, sockman::readable, [](int sock, auto events) {
        if (events.error() || events.hungup())
        {
            std::cout << "shutdown..." << std::endl;
            shutdown_requested = true;
        }
        else if (events.readable())
        {
            int const client_fd = accept(sock, nullptr, nullptr);
            if (0 <= client_fd)
            {
                std::cout << "new connection: send hello message" << std::endl;
                
                char const message[] = "Hello";
                write(client_fd, reinterpret_cast<void const*>(message), strlen(message));
                close(client_fd);
            }
        }
    });
 
    std::cout << "waiting for incoming connections on " << path << std::endl;
    while (!shutdown_requested)
    {
        manager.service();
    }
 
    manager.remove(fd);
    close(fd);
    unlink(path);

    return 0;
}