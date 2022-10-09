/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef SOCKMAN_SOCKETCONTEXT_HPP
#define SOCKMAN_SOCKETCONTEXT_HPP

#include "sockman/handler_i.hpp"
#include <cstdint>
#include <memory>

namespace sockman
{

struct socketcontext
{
    uint32_t events;
    std::shared_ptr<handler_i> handler;
};

}

#endif
