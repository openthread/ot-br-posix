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
 *      Implementation of various helper functions related to DBus
 *      funcitonality.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "DBUSHelpers.h"
#include "Data.h"
#include <syslog.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <exception>
#include <stdexcept>
#include "ValueMap.h"

using namespace DBUSHelpers;

nl::ValueMap
DBUSHelpers::value_map_from_dbus_iter(DBusMessageIter *iter)
{
	nl::ValueMap ret;
	DBusMessageIter sub_iter, dict_iter;

	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_ARRAY) {
		throw std::invalid_argument("Wrong type for value map");
	}

	dbus_message_iter_recurse(iter, &sub_iter);

	if (dbus_message_iter_get_arg_type(&sub_iter) != DBUS_TYPE_DICT_ENTRY) {
		throw std::invalid_argument("Wrong type for value map");
	}

	do {
		const char* key_cstr;
		dbus_message_iter_recurse(&sub_iter, &dict_iter);

		if (dbus_message_iter_get_arg_type(&dict_iter) != DBUS_TYPE_STRING) {
			throw std::invalid_argument("Wrong type for value map");
		}

		dbus_message_iter_get_basic(&dict_iter, &key_cstr);
		dbus_message_iter_next(&dict_iter);

		ret[key_cstr] = any_from_dbus_iter(&dict_iter);
	} while (dbus_message_iter_next(&sub_iter));

	return ret;
}

boost::any
DBUSHelpers::any_from_dbus_iter(DBusMessageIter *iter)
{
	boost::any ret;

	switch (dbus_message_iter_get_arg_type(iter)) {
	case DBUS_TYPE_ARRAY: {
		DBusMessageIter sub_iter;
		dbus_message_iter_recurse(iter, &sub_iter);
		if (dbus_message_iter_get_arg_type(&sub_iter) == DBUS_TYPE_BYTE) {
			const uint8_t* value = NULL;
			int nelements = 0;
			dbus_message_iter_get_fixed_array(&sub_iter, &value, &nelements);
			ret = nl::Data(value, nelements);
		} else if (dbus_message_iter_get_arg_type(&sub_iter) == DBUS_TYPE_DICT_ENTRY) {
			ret = value_map_from_dbus_iter(iter);
		} else {
			syslog(LOG_NOTICE,
			       "Unsupported DBUS array type for any: %d",
			       dbus_message_iter_get_arg_type(&sub_iter));
		}
	} break;
	case DBUS_TYPE_VARIANT: {
		DBusMessageIter sub_iter;
		dbus_message_iter_recurse(iter, &sub_iter);
		ret = any_from_dbus_iter(&sub_iter);
	} break;
	case DBUS_TYPE_STRING: {
		const char* v;
		dbus_message_iter_get_basic(iter, &v);
		ret = std::string(v);
	} break;
	case DBUS_TYPE_BOOLEAN: {
		dbus_bool_t v;
		dbus_message_iter_get_basic(iter, &v);
		ret = bool(v);
	} break;
	case DBUS_TYPE_BYTE: {
		uint8_t v;
		dbus_message_iter_get_basic(iter, &v);
		ret = v;
	} break;
	case DBUS_TYPE_DOUBLE: {
		double v;
		dbus_message_iter_get_basic(iter, &v);
		ret = v;
	} break;
	case DBUS_TYPE_UINT16: {
		uint16_t v;
		dbus_message_iter_get_basic(iter, &v);
		ret = v;
	} break;
	case DBUS_TYPE_INT16: {
		int16_t v;
		dbus_message_iter_get_basic(iter, &v);
		ret = v;
	} break;
	case DBUS_TYPE_UINT32: {
		uint32_t v;
		dbus_message_iter_get_basic(iter, &v);
		ret = v;
	} break;
	case DBUS_TYPE_INT32: {
		int32_t v;
		dbus_message_iter_get_basic(iter, &v);
		ret = v;
	} break;
	case DBUS_TYPE_UINT64: {
		uint64_t v;
		dbus_message_iter_get_basic(iter, &v);
		ret = v;
	} break;
	case DBUS_TYPE_INT64: {
		int64_t v;
		dbus_message_iter_get_basic(iter, &v);
		ret = v;
	} break;
	}

	return ret;
}

void
DBUSHelpers::append_any_to_dbus_iter(
    DBusMessageIter *iter, const boost::any &value
    )
{
	if (value.type() == typeid(std::string)) {
		std::string v = boost::any_cast<std::string>(value);
		const char* cstr = v.c_str();
		dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &cstr);
	} else if (value.type() == typeid(char*)) {
		std::string v = boost::any_cast<char*>(value);
		const char* cstr = v.c_str();
		dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &cstr);
	} else if (value.type() == typeid(bool)) {
		dbus_bool_t v = boost::any_cast<bool>(value);
		dbus_message_iter_append_basic(iter, DBUS_TYPE_BOOLEAN, &v);
	} else if (value.type() == typeid(uint8_t)) {
		uint8_t v = boost::any_cast<uint8_t>(value);
		dbus_message_iter_append_basic(iter, DBUS_TYPE_BYTE, &v);
	} else if (value.type() == typeid(int8_t)) {
		int16_t v = boost::any_cast<int8_t>(value);
		dbus_message_iter_append_basic(iter, DBUS_TYPE_INT16, &v);
	} else if (value.type() == typeid(uint16_t)) {
		uint16_t v = boost::any_cast<uint16_t>(value);
		dbus_message_iter_append_basic(iter, DBUS_TYPE_UINT16, &v);
	} else if (value.type() == typeid(int16_t)) {
		int16_t v = boost::any_cast<int16_t>(value);
		dbus_message_iter_append_basic(iter, DBUS_TYPE_INT16, &v);
	} else if (value.type() == typeid(uint32_t)) {
		uint32_t v = boost::any_cast<uint32_t>(value);
		dbus_message_iter_append_basic(iter, DBUS_TYPE_UINT32, &v);
	} else if (value.type() == typeid(int32_t)) {
		int32_t v = boost::any_cast<int32_t>(value);
		dbus_message_iter_append_basic(iter, DBUS_TYPE_INT32, &v);
	} else if (value.type() == typeid(uint64_t)) {
		uint64_t v = boost::any_cast<uint64_t>(value);
		dbus_message_iter_append_basic(iter, DBUS_TYPE_UINT64, &v);
	} else if (value.type() == typeid(int64_t)) {
		int64_t v = boost::any_cast<int64_t>(value);
		dbus_message_iter_append_basic(iter, DBUS_TYPE_INT64, &v);
	} else if (value.type() == typeid(double)) {
		double v = boost::any_cast<double>(value);
		dbus_message_iter_append_basic(iter, DBUS_TYPE_DOUBLE, &v);
	} else if (value.type() == typeid(float)) {
		double v = boost::any_cast<float>(value);
		dbus_message_iter_append_basic(iter, DBUS_TYPE_DOUBLE, &v);
	} else if (value.type() == typeid(std::list<std::string>)) {
		DBusMessageIter array_iter;
		const std::list<std::string>& list_of_strings =
		    boost::any_cast< std::list<std::string> >(value);
		std::list<std::string>::const_iterator list_iter;
		dbus_message_iter_open_container(
		    iter,
		    DBUS_TYPE_ARRAY,
		    DBUS_TYPE_STRING_AS_STRING,
		    &array_iter
		    );

		for (list_iter = list_of_strings.begin();
		     list_iter != list_of_strings.end();
		     list_iter++) {
			const char* cstr = list_iter->c_str();
			dbus_message_iter_append_basic(&array_iter, DBUS_TYPE_STRING,
			                               &cstr);
		}

		dbus_message_iter_close_container(iter, &array_iter);

	} else if (value.type() == typeid(std::set<std::string>)) {
		DBusMessageIter array_iter;
		const std::set<std::string>& set_of_strings =
		    boost::any_cast< std::set<std::string> >(value);
		std::set<std::string>::const_iterator set_iter;
		dbus_message_iter_open_container(
		    iter,
		    DBUS_TYPE_ARRAY,
		    DBUS_TYPE_STRING_AS_STRING,
		    &array_iter
		    );

		for (set_iter = set_of_strings.begin();
		     set_iter != set_of_strings.end();
		     set_iter++) {
			const char* cstr = set_iter->c_str();
			dbus_message_iter_append_basic(&array_iter, DBUS_TYPE_STRING,
			                               &cstr);
		}

		dbus_message_iter_close_container(iter, &array_iter);
	} else if (value.type() == typeid(nl::Data)) {
		DBusMessageIter array_iter;
		const nl::Data& vector = boost::any_cast< nl::Data >(value);
		nl::Data::const_iterator vector_iter;
		dbus_message_iter_open_container(
		    iter,
		    DBUS_TYPE_ARRAY,
		    DBUS_TYPE_BYTE_AS_STRING,
		    &array_iter
		    );

		for (vector_iter = vector.begin();
		     vector_iter != vector.end();
		     vector_iter++) {
			dbus_message_iter_append_basic(&array_iter,
			                               DBUS_TYPE_BYTE,
			                               &*vector_iter);
		}

		dbus_message_iter_close_container(iter, &array_iter);
	} else if (value.type() == typeid(std::vector<uint8_t>)) {
		DBusMessageIter array_iter;
		const std::vector<uint8_t>& vector =
		    boost::any_cast< std::vector<uint8_t> >(value);
		std::vector<uint8_t>::const_iterator vector_iter;
		dbus_message_iter_open_container(
		    iter,
		    DBUS_TYPE_ARRAY,
		    DBUS_TYPE_BYTE_AS_STRING,
		    &array_iter
		    );

		for (vector_iter = vector.begin();
		     vector_iter != vector.end();
		     vector_iter++) {
			dbus_message_iter_append_basic(&array_iter,
			                               DBUS_TYPE_BYTE,
			                               &*vector_iter);
		}

		dbus_message_iter_close_container(iter, &array_iter);
	} else if (value.type() == typeid(std::set<int>)) {
		DBusMessageIter array_iter;
		const std::set<int>& container =
		    boost::any_cast< std::set<int> >(value);
		std::set<int>::const_iterator container_iter;
		dbus_message_iter_open_container(
		    iter,
		    DBUS_TYPE_ARRAY,
		    DBUS_TYPE_INT32_AS_STRING,
		    &array_iter
		    );

		for (container_iter = container.begin();
		     container_iter != container.end();
		     container_iter++) {
			dbus_message_iter_append_basic(&array_iter,
			                               DBUS_TYPE_INT32,
			                               &*container_iter);
		}

		dbus_message_iter_close_container(iter, &array_iter);
	} else if (value.type() == typeid(nl::ValueMap)) {
		DBusMessageIter array_iter;
		const nl::ValueMap& value_map = boost::any_cast<nl::ValueMap>(value);
		nl::ValueMap::const_iterator value_map_iter;

		// Open a container as "Dictionary/Array of Strings to Variants" (dbus type "a{sv}")
		dbus_message_iter_open_container(
			iter,
			DBUS_TYPE_ARRAY,
			DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
				DBUS_TYPE_STRING_AS_STRING
				DBUS_TYPE_VARIANT_AS_STRING
			DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
			&array_iter
			);

		for (value_map_iter = value_map.begin(); value_map_iter != value_map.end(); ++value_map_iter) {
			append_dict_entry(&array_iter, value_map_iter->first.c_str(), value_map_iter->second);
		}

		dbus_message_iter_close_container(iter, &array_iter);
	} else if (value.type() == typeid(std::list<nl::ValueMap>)) {
		DBusMessageIter array_iter;
		const std::list<nl::ValueMap>& value_map_list = boost::any_cast< std::list<nl::ValueMap> >(value);
		std::list<nl::ValueMap>::const_iterator list_iter;

		// Open a container as "Array of Dictionaries/Arrays of Strings to Variants" (dbus type "aa{sv}")
		dbus_message_iter_open_container(
			iter,
			DBUS_TYPE_ARRAY,
			DBUS_TYPE_ARRAY_AS_STRING
				DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
					DBUS_TYPE_STRING_AS_STRING
					DBUS_TYPE_VARIANT_AS_STRING
				DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
			&array_iter
			);

		for (list_iter = value_map_list.begin(); list_iter != value_map_list.end(); ++list_iter) {
			append_any_to_dbus_iter(&array_iter, *list_iter);
		}

		dbus_message_iter_close_container(iter, &array_iter);
	} else {
		throw std::invalid_argument("Unsupported type");
	}
}

std::string
DBUSHelpers::any_to_dbus_type_string(const boost::any &value)
{
	if (value.type() == typeid(std::string)) {
		return DBUS_TYPE_STRING_AS_STRING;
	} else if (value.type() == typeid(bool)) {
		return DBUS_TYPE_BOOLEAN_AS_STRING;
	} else if (value.type() == typeid(uint8_t)) {
		return DBUS_TYPE_BYTE_AS_STRING;
	} else if (value.type() == typeid(int8_t)) {
		return DBUS_TYPE_INT16_AS_STRING;
	} else if (value.type() == typeid(uint16_t)) {
		return DBUS_TYPE_UINT16_AS_STRING;
	} else if (value.type() == typeid(int16_t)) {
		return DBUS_TYPE_INT16_AS_STRING;
	} else if (value.type() == typeid(uint32_t)) {
		return DBUS_TYPE_UINT32_AS_STRING;
	} else if (value.type() == typeid(int32_t)) {
		return DBUS_TYPE_INT32_AS_STRING;
	} else if (value.type() == typeid(uint64_t)) {
		return DBUS_TYPE_UINT64_AS_STRING;
	} else if (value.type() == typeid(int64_t)) {
		return DBUS_TYPE_INT64_AS_STRING;
	} else if (value.type() == typeid(double)) {
		return DBUS_TYPE_DOUBLE_AS_STRING;
	} else if (value.type() == typeid(float)) {
		return DBUS_TYPE_DOUBLE_AS_STRING;
	} else if (value.type() == typeid(nl::Data)) {
		return DBUS_TYPE_ARRAY_AS_STRING DBUS_TYPE_BYTE_AS_STRING;
	} else if (value.type() == typeid(std::vector<uint8_t>)) {
		return DBUS_TYPE_ARRAY_AS_STRING DBUS_TYPE_BYTE_AS_STRING;
	} else if (value.type() == typeid(std::list<std::string>)) {
		return DBUS_TYPE_ARRAY_AS_STRING DBUS_TYPE_STRING_AS_STRING;
	} else if (value.type() == typeid(std::set<std::string>)) {
		return DBUS_TYPE_ARRAY_AS_STRING DBUS_TYPE_STRING_AS_STRING;
	} else if (value.type() == typeid(nl::ValueMap)) {
		return std::string(DBUS_TYPE_ARRAY_AS_STRING) +
					DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING +
						DBUS_TYPE_STRING_AS_STRING +
						DBUS_TYPE_VARIANT_AS_STRING +
					DBUS_DICT_ENTRY_END_CHAR_AS_STRING;
	} else if (value.type() == typeid(std::list<nl::ValueMap>)) {
		return  std::string(DBUS_TYPE_ARRAY_AS_STRING) +
					DBUS_TYPE_ARRAY_AS_STRING +
						DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING +
							DBUS_TYPE_STRING_AS_STRING +
							DBUS_TYPE_VARIANT_AS_STRING +
						DBUS_DICT_ENTRY_END_CHAR_AS_STRING;
	}

	return "";
}

void
DBUSHelpers::append_dict_entry(
    DBusMessageIter *dict, const char *key, const boost::any& value
    )
{
	DBusMessageIter entry;
	DBusMessageIter value_iter;
	std::string sig = any_to_dbus_type_string(value);

	dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);

	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);

	dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT,
	                                 sig.c_str(), &value_iter);

	append_any_to_dbus_iter(&value_iter, value);

	dbus_message_iter_close_container(&entry, &value_iter);

	dbus_message_iter_close_container(dict, &entry);
}

void
DBUSHelpers::append_dict_entry(
    DBusMessageIter *dict, const char *key, char type, void *val
    )
{
	DBusMessageIter entry;
	DBusMessageIter value_iter;
	char sig[2] = { type, '\0' };

	if (type == DBUS_TYPE_STRING) {
		const char *str = *((const char**)val);
		if (str == NULL)
			return;
	}

	dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);

	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);

	dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, sig,
	                                 &value_iter);

	dbus_message_iter_append_basic(&value_iter, type, val);

	dbus_message_iter_close_container(&entry, &value_iter);

	dbus_message_iter_close_container(dict, &entry);
}
