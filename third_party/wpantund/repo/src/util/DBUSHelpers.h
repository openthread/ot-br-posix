/*
 *
 * Copyright (c) 2016 Nest Labs, Inc.
 * All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *    Description:
 *      Declaration of various helper functions related to DBus
 *      funcitonality.
 *
 */

#ifndef __wpantund__DBUSHelpers__
#define __wpantund__DBUSHelpers__

#include <boost/any.hpp>
#include <dbus/dbus.h>
#include <string>
#include "ValueMap.h"

namespace DBUSHelpers {
nl::ValueMap value_map_from_dbus_iter(DBusMessageIter *iter);
boost::any any_from_dbus_iter(DBusMessageIter *iter);
void append_any_to_dbus_iter(DBusMessageIter *iter, const boost::any &value);
std::string any_to_dbus_type_string(const boost::any &value);
void append_dict_entry(DBusMessageIter *dict, const char *key, const boost::any& value);
void append_dict_entry(DBusMessageIter *dict, const char *key, char type, void *val);
};

#endif /* defined(__wpantund__DBUSHelpers__) */
