/*
 *    Copyright (c) 2017, The OpenThread Authors.
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

#include "event_emitter.hpp"

#include <assert.h>

namespace ot {

namespace BorderRouter {

void EventEmitter::On(int aEvent, Callback aCallback, void *aContext)
{
    assert(aCallback);

    Handlers &handlers = mEvents[aEvent];

    handlers.push_back(Handler(aCallback, aContext));
}

void EventEmitter::Off(int aEvent, Callback aCallback, void *aContext)
{
    assert(aCallback);

    if (!mEvents.count(aEvent))
    {
        return;
    }

    Handler   handler  = Handler(aCallback, aContext);
    Handlers &handlers = mEvents[aEvent];

    for (Handlers::iterator it = handlers.begin(); it != handlers.end(); ++it)
    {
        if (*it == handler)
        {
            handlers.erase(it);
            if (handlers.empty())
            {
                mEvents.erase(aEvent);
            }
            break;
        }
    }
}

void EventEmitter::Emit(int aEvent, ...)
{
    if (!mEvents.count(aEvent))
    {
        return;
    }

    va_list args;
    va_start(args, aEvent);
    Handlers &handlers = mEvents[aEvent];

    assert(!handlers.empty());

    for (Handlers::iterator it = handlers.begin(); it != handlers.end(); ++it)
    {
        va_list tmpArgs;
        va_copy(tmpArgs, args);
        it->first(it->second, aEvent, tmpArgs);
        va_end(tmpArgs);
    }

    va_end(args);
}

} // namespace BorderRouter

} // namespace ot
