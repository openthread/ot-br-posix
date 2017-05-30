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
 *      Ring-buffer implementation (not thread-safe)
 *
 */

#ifndef wpantund_RingBuffer_h
#define wpantund_RingBuffer_h

#include <stdint.h>
#include <stdexcept>

namespace nl {

// NOTE: The below implementation of RingBuffer<> is NOT thread-safe.

template <typename T = uint8_t, int I = 512>
class RingBuffer
{
public:

	typedef T value_type;
	typedef int size_type;

	static const size_type buffer_size = I;

public:

	RingBuffer()
	{
		clear();
	}

	size_type size() const
	{
		return mCount;
	}

	size_type space_available() const
	{
		return buffer_size - mCount;
	}

	bool empty() const
	{
		return (mCount == 0);
	}

	bool full() const
	{
		return (mCount == buffer_size);
	}

	size_type max_size() const
	{
		return buffer_size;
	}

	const value_type* data_ptr()const {
		return &mBuffer[mReadIdx];
	}

	size_type size_of_data_ptr()const {
		size_type ret;

		if (mWriteIdx >= mReadIdx) {
			ret = mWriteIdx - mReadIdx;
		} else {
			ret = buffer_size - mReadIdx;
		}

		return ret;
	}

	template <typename S> void
	push(const value_type* values, S value_count)
	{
		if (space_available() < value_count) {
			throw std::overflow_error("not enough room in ring buffer");
		}

		while(value_count--) {
			mBuffer[mWriteIdx] = *values++;
			mWriteIdx++;
			mWriteIdx %= buffer_size; // Should be optimized by compiler as a mask
			mCount++;
		}
	}

	template <typename S> size_type
	pop(S value_count)
	{
		size_type bytes_read = size();

		if (bytes_read < value_count) {
			value_count = bytes_read;
		} else {
			bytes_read = static_cast<size_type>(value_count);
		}

		while (value_count--) {
			mReadIdx++;
			mReadIdx %= buffer_size; // Should be optimized by compiler as a mask
			mCount--;
		}

		if (mReadIdx == mWriteIdx) {
			mReadIdx = mWriteIdx = 0;
		}

		return bytes_read;
	}

	template <typename S> size_type
	pull(value_type* values, S value_count)
	{
		size_type bytes_read = size();

		if (bytes_read < value_count) {
			value_count = bytes_read;
		} else {
			bytes_read = static_cast<size_type>(value_count);
		}

		while (value_count--) {
			*values++ = mBuffer[mReadIdx];
			mReadIdx++;
			mReadIdx %= buffer_size; // Should be optimized by compiler as a mask
			mCount--;
		}

		return bytes_read;
	}

	// Returns a pointer to the front (head) element in ring buffer if the ring buffer
	// is not empty, or returns NULL if buffer is empty.
	const value_type *front() const
	{
		return (mCount == 0)? NULL : &mBuffer[mReadIdx];
	}

	// Returns a pointer to the back (tail) element in ring buffer if the ring buffer
	// is not empty, or returns NULL if buffer is empty.
	const value_type *back() const
	{
		if (mCount == 0) {
			return NULL;
		}

		if (mWriteIdx == 0)  {
			return &mBuffer[buffer_size - 1];
		}

		return &mBuffer[mWriteIdx - 1];
	}

	// Attempts to write the new value in the ring buffer, if buffer is full
	// and write fails, returns false, otherwise returns true.
	bool write(const value_type& value)
	{
		if (mCount == buffer_size) {
			return false;
		}

		mBuffer[mWriteIdx] = value;
		mWriteIdx++;
		mWriteIdx %= buffer_size;
		mCount++;

		return true;
	}

	// Force writes the new value in the ring buffer. May overwrite an earlier value and move
	// the read index forward (if buffer is full).
	void force_write(const value_type& value)
	{
		mBuffer[mWriteIdx] = value;
		mWriteIdx++;
		mWriteIdx %= buffer_size;

		if (mCount == buffer_size) {
			mReadIdx = mWriteIdx;
		} else {
			mCount++;
		}
	}

	// Reads one element from the head of ring buffer and removes it. If buffer
	// is empty returns false, otherwise returns true.
	bool read(value_type& value)
	{
		value_type *f = front();

		if (f) {
			value = *f;
		}

		return remove();
	}

	// Removes the element from front of the ring buffer. If buffer is empty
	// and the remove operation fails, returns false, otherwise returns true.
	bool remove()
	{
		if (mCount == 0) {
			return false;
		}

		mReadIdx++;
		mReadIdx %= buffer_size;
		mCount--;

		return true;
	}

	// Clears the ring buffer.
	void clear()
	{
		mReadIdx = mWriteIdx = 0;
		mCount = 0;
	}

private:
	class IteratorBase
	{
	protected:
		IteratorBase(const value_type *ptr, const RingBuffer *ring_buffer_ptr)
		{
			mIterPtr = ptr;
			mRingBufferPtr = ring_buffer_ptr;
		}

		IteratorBase(const IteratorBase &it)
		{
			mIterPtr = it.mIterPtr;
			mRingBufferPtr = it.mRingBufferPtr;
		}

	public:
		bool operator==(const IteratorBase &lhs)
		{
			return (mIterPtr == lhs.mIterPtr) && (mRingBufferPtr == lhs.mRingBufferPtr);
		}

		bool operator!=(const IteratorBase &lhs)
		{
			return !((*this) == lhs);
		}

		const value_type *get_ptr()     { return mIterPtr;  }

		const value_type& operator* ()  { return *mIterPtr; }
		const value_type* operator-> () { return mIterPtr;  }

	protected:
		const value_type *mIterPtr;
		const RingBuffer *mRingBufferPtr;
	};

public:
	// An iterator for going through the elements in the ring buffer from front to back.
	class Iterator : public IteratorBase
	{
	public:
		Iterator() : IteratorBase(NULL, NULL) { }
		Iterator(const Iterator &it) : IteratorBase(it) { }

		Iterator& operator++() {
			advance();
			return *this;
		}

		Iterator operator++(int val) {
			(void)val;
			Iterator it(*this);
			advance();
			return it;
		}

		void advance(void)
		{
			const RingBuffer *rb = this->mRingBufferPtr;
			if (rb)
			{
				this->mIterPtr++;
				if (this->mIterPtr == &rb->mBuffer[rb->buffer_size]) {
					this->mIterPtr = &rb->mBuffer[0];
				}

				if (this->mIterPtr == &rb->mBuffer[rb->mWriteIdx]) {
					this->mIterPtr = NULL;
				}
			}
		}

	private:
		Iterator(const value_type *ptr, const RingBuffer *ring_buffer_ptr) :
			IteratorBase(ptr, ring_buffer_ptr) { }

		friend Iterator RingBuffer::begin() const;
		friend Iterator RingBuffer::end() const;
	};

	// A reverse iterator for going through the elements in the ring buffer from back to front.
	class ReverseIterator : public IteratorBase
	{
	public:
		ReverseIterator() : IteratorBase(NULL, NULL) { }
		ReverseIterator(const Iterator &it) : IteratorBase(it) { }

		ReverseIterator& operator++() {
			advance();
			return *this;
		}

		ReverseIterator operator++(int val) {
			(void)val;
			ReverseIterator it(*this);
			advance();
			return it;
		}

		void advance(void)
		{
			const RingBuffer *rb = this->mRingBufferPtr;
			if (rb)
			{
				if (this->mIterPtr == &rb->mBuffer[rb->mReadIdx]) {
					this->mIterPtr = NULL;
				} else {
					if (this->mIterPtr == &rb->mBuffer[0]) {
						this->mIterPtr = &rb->mBuffer[rb->buffer_size - 1];
					} else {
						this->mIterPtr--;
					}
				}
			}
		}

	private:
		ReverseIterator(const value_type *ptr, const RingBuffer *ring_buffer_ptr) :
			IteratorBase(ptr, ring_buffer_ptr) { }

		friend ReverseIterator RingBuffer::rbegin() const;
		friend ReverseIterator RingBuffer::rend() const;
	};

	// Returns a RingBuffer::Iterator to the beginning of the buffer
	Iterator begin() const
	{
		return Iterator(front(), this);
	}

	// Returns a RingBuffer::Iterator marking the end/tail of the buffer
	Iterator end() const
	{
		return Iterator(NULL, this);
	}

	// Returns a RingBuffer::ReverseIterator pointing to back/tail of the buffer.
	ReverseIterator rbegin() const
	{
		return ReverseIterator(back(), this);
	}

	// Returns a RingBuffer::ReverseIterator pointing to end/head of the buffer.
	ReverseIterator rend() const
	{
		return ReverseIterator(NULL, this);
	}

private:
	size_type mReadIdx, mWriteIdx;
	size_type mCount;
	value_type mBuffer[buffer_size];
};

}; // namespace nl

#endif
