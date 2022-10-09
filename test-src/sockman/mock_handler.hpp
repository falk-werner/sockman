/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "sockman/handler_i.hpp"
#include <gmock/gmock.h>

namespace sockman
{

class mock_handler: public handler_i
{
public:
    MOCK_METHOD(void, on_readable, (), (override));
    MOCK_METHOD(void, on_writable, (), (override));
    MOCK_METHOD(void, on_hungup, (), (override));
    MOCK_METHOD(void, on_error, (), (override));
};

}