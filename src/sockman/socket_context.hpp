/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef SOCKMAN_SOCKETCONTEXT_HPP
#define SOCKMAN_SOCKETCONTEXT_HPP

#include "sockman/sockman.hpp"
#include <memory>

namespace sockman
{

struct socket_context
{
    int fd;
    uint32_t events;
    socket_callback callback;
};

}

#endif
