/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef SOCKMAN_MANAGER_HPP
#define SOCKMAN_MANAGER_HPP

#include <sockman/handler_i.hpp>
#include <memory>
namespace sockman
{

class manager
{
    manager(manager const &) = delete;
    manager& operator=(manager const &) = delete;
public:
    manager();
    ~manager();
    manager(manager && other);
    manager& operator=(manager && other);

    void add(int sock, std::shared_ptr<handler_i> handler);
    void remove(int sock);
    void notify_on_readable(int sock, bool enable = true);
    void notify_on_writable(int sock, bool enable = true);

    void service(int timeout = -1);
private:
    class detail;
    detail * d;
};

}

#endif