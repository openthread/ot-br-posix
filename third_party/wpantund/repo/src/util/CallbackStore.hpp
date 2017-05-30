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
 *      Class for handling one-time callbacks. Deprecated.
 *
 */

#ifndef wpantund_callback_store_h
#define wpantund_callback_store_h

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <map>
#include <boost/signals2/signal.hpp>
#include <boost/bind.hpp>
#include <boost/ptr_container/ptr_map.hpp>

template <class Key = std::string>
class CallbackStore {
public:
	typedef boost::signals2::signal<void(int, const uint8_t* data, size_t len)>  signal_type;
	typedef boost::shared_ptr<signal_type>  mapped_type;
	typedef Key key_type;
	typedef std::map<key_type, mapped_type> map_type;

private:
	map_type _map;

public:
	CallbackStore() {
	}

	void
	add(const Key& name, const boost::function<void(int)>& func)
	{
		if(!_map[name])
			_map[name] = mapped_type(new signal_type());

		_map[name]->connect(boost::bind(func, _1));
	}

	void
	add( const Key& name, const boost::function3<void, int, const uint8_t*, size_t>& func)
	{
		if(!_map[name])
			_map[name] = mapped_type(new signal_type());

		_map[name]->connect(func);
	}

	void
	set(const Key& name, const mapped_type& signal)
	{
		if (static_cast<bool>(signal))
			_map[name] = signal;
	}


	size_t
	count(const Key& name)const {
		if (_map.count(name) == 0 || NULL == _map.find(name)->second.get())
			return 0;
		return _map.find(name)->second->num_slots();
	}

	void
	handle(const Key& name, int val, const uint8_t* data, size_t len)
	{
		mapped_type signal;
		if(_map[name]) {
			signal.swap(_map[name]);
			_map.erase(name);
			(*signal)(val, data, len);
		}
	}

	mapped_type
	unhandle(const Key& name)
	{
		mapped_type signal;
		if (_map[name]) {
			signal.swap(_map[name]);
			_map.erase(name);
		}
		return signal;
	}

	void
	handle_all(int val)
	{
		map_type map(_map);
		for (typename map_type::iterator iter=map.begin(); iter!=map.end(); iter++) {
			handle(iter->first, val, NULL, 0);
		}
	}
};

#endif
