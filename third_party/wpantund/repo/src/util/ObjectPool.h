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
 *      A simple object pool implementation
 *
 */

#ifndef wpantund_ObjectPool_h
#define wpantund_ObjectPool_h

#include <list>

namespace nl {

template <typename T, int I = 64>
class ObjectPool
{
public:
	typedef T element_type;
	typedef int size_type;

	static const size_type pool_size = I;

public:
	ObjectPool(): mFreeElementList()
	{
		free_all();
	}

	// Free all elements in the object pool
	void free_all(void)
	{
		size_type count = pool_size;
		element_type *element_ptr;

		mFreeElementList.clear();

		element_ptr = &mElementPool[pool_size - 1];
		while(count--) {
			mFreeElementList.push_back(element_ptr);
			element_ptr--;
		}
	}

	// Attempts to allocate a new object from pool, returns NULL if no object available.
	element_type *alloc(void)
	{
		element_type *element_ptr = NULL;
		if (!mFreeElementList.empty()) {
			element_ptr = mFreeElementList.front();
			mFreeElementList.pop_front();
		}
		return element_ptr;
	}

	// Frees a previously allocated pool object.
	void free(element_type *element_ptr)
	{
		if (is_ptr_in_pool(element_ptr)) {
			mFreeElementList.push_back(element_ptr);
		}
	}

private:
	element_type mElementPool[pool_size];
	std::list<element_type *>mFreeElementList;

	bool is_ptr_in_pool(const element_type *ptr) const
	{
		return ((ptr >= mElementPool) && (ptr < mElementPool + pool_size));
	}

};

}; // namespace nl

#endif // wpantund_ObjectPool_h
