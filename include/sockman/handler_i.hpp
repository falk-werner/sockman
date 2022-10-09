/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef SOCKMAN_HANDLER_I_HPP
#define SOCKMAN_HANDLER_I_HPP

namespace sockman
{

class handler_i
{
public:
    virtual ~handler_i() = default;

    virtual void on_readable() = 0;
    virtual void on_writable() = 0;
    virtual void on_hungup() = 0;
    virtual void on_error() = 0;
};

}

#endif