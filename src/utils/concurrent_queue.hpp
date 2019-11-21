/*
 *    Copyright (c) 2019, The OpenThread Authors.
 *    All rights reserved.
 *
 *    Redistribution and use in source and binary forms, with or without
 *    modification, are permitted provided that the following conditions are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *    3. Neither the name of the copyright holder nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *    POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file includes the tempalte for a concurrent queue
 */

#ifndef OTBR_CONCURRENT_QUEUE_HPP_
#define OTBR_CONCURRENT_QUEUE_HPP_

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

namespace ot {
namespace utils {

template <typename T> class ConcurrentQueue
{
public:
    ConcurrentQueue() = default;

    /**
     * This method pushes an item into the queue
     *
     * @param[in]  aItem  The item to be pushed
     *
     */
    template <typename R> void Push(R &&aItem)
    {
        std::lock_guard<std::mutex> l(mMutex);
        mQueue.push(std::forward<R>(aItem));
        mItemPushedEvent.notify_one();
    }

    /**
     * This method pops an item from the queue
     *
     * @returns The item at the top, if the queue is empty, will block until
     *          one itme is pushed.
     *
     */
    T Pop(void)
    {
        std::unique_lock<std::mutex> lck(mMutex);
        mItemPushedEvent.wait(lck, [this]() { return !mQueue.empty(); });

        T val = std::move(mQueue.front());
        mQueue.pop();
        return val;
    }

    /**
     * This method returns whether the queue is empty
     *
     * @returns Whether the queue is emtpy. Only when you are the only consumer
     *          it is ensured that a call to Pop() won't block if Empty() returns
     *          false.
     *
     */
    bool Empty(void)
    {
        std::lock_guard<std::mutex> l(mMutex);
        return mQueue.empty();
    }

private:
    std::mutex              mMutex;
    std::condition_variable mItemPushedEvent;
    std::queue<T>           mQueue;
};

using TaskQueue = ConcurrentQueue<std::function<void(void)>>;

}; // namespace utils
}; // namespace ot

#endif // OTBR_CONCURRENT_QUEUE_HPP_
