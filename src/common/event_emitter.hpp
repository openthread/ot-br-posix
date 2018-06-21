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

#ifndef EVENT_EMITTER_HPP_
#define EVENT_EMITTER_HPP_

#include <list>
#include <map>

#include <stdarg.h>

namespace ot {

namespace BorderRouter {

/**
 * This class implements the basic functionality of an event emitter.
 *
 */
class EventEmitter
{
    /**
     * This function pointer will be called when the @p aEvent is emitted.
     *
     * @param[in]   aContext    A pointer to application-specific context.
     * @param[in]   aEvent      The emitted event id.
     * @param[in]   aArguments  The arguments associated with this event.
     *
     */
    typedef void (*Callback)(void *aContext, int aEvent, va_list aArguments);

public:
    /**
     * This method register an event handler for @p aEvent.
     *
     * @param[in]   aEvent      The event id.
     * @param[in]   aCallback   The function poiner to be called.
     * @param[in]   aContext    A pointer to application-specific context.
     *
     */
    void On(int aEvent, Callback aCallback, void *aContext);

    /**
     * This method deregister an event handler for @p aEvent.
     *
     * @param[in]   aEvent      The event id.
     * @param[in]   aCallback   The function poiner to be called.
     * @param[in]   aContext    A pointer to application-specific context.
     *
     */
    void Off(int aEvent, Callback aCallback, void *aContext);

    /**
     * This method emits an event.
     *
     * @param[in]   aEvent      The event id.
     *
     */
    void Emit(int aEvent, ...);

private:
    typedef std::pair<Callback, void *> Handler;
    typedef std::list<Handler>          Handlers;
    typedef std::map<int, Handlers>     Events;
    Events                              mEvents;
};

} // namespace BorderRouter

} // namespace ot

#endif // EVENT_EMITTER_HPP_
