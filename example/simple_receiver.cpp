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
#include <iostream>
 
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

    int const rc = connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address));
    if (rc != 0)
    {
        perror("connect");
        close(fd);
        return EXIT_FAILURE;
    }
 
    bool shutdown_requested = false;
    sockman::manager manager;
    manager.add(fd, sockman::readable, [&shutdown_requested](int sock, auto events) {
        if (events.error() || events.hungup())
        {
            shutdown_requested = true;
        }
        if (events.readable())
        {
            char buffer[80];
            auto count = ::read(sock, buffer, 79);
            if (count > 0)
            {
                buffer[count] = '\0';
                std::cout << buffer << std::endl;
            }
        }
    });
 
    while (!shutdown_requested)
    {
        manager.service();
    }
 
    manager.remove(fd);
    ::close(fd);
 
    return 0;
}