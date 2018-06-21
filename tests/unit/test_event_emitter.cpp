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

#include <CppUTest/TestHarness.h>

#include <stdarg.h>

#include "common/event_emitter.hpp"

static int   sCounter = 0;
static int   sEvent   = 0;
static void *sContext = NULL;

static void HandleSingleEvent(void *aContext, int aEvent, va_list aArguments)
{
    sCounter++;

    CHECK_EQUAL(sContext, aContext);
    CHECK_EQUAL(sEvent, aEvent);
    (void)aArguments;
}

static void HandleTestDifferentContextEvent(void *aContext, int aEvent, va_list aArguments)
{
    void *context1 = va_arg(aArguments, void *);
    void *context2 = va_arg(aArguments, void *);

    CHECK_EQUAL(sEvent, aEvent);

    int id = *static_cast<int *>(aContext);
    if (id == 1)
    {
        CHECK_EQUAL(context1, aContext);
    }
    else if (id == 2)
    {
        CHECK_EQUAL(context2, aContext);
    }
    else
    {
        FAIL("Unexpected id value!");
    }

    sCounter++;
}

static void HandleTestCallSequenceEvent(void *aContext, int aEvent, va_list aArguments)
{
    CHECK_EQUAL(sEvent, aEvent);

    int id = *static_cast<int *>(aContext);

    ++sCounter;

    CHECK_EQUAL(sCounter, id);

    (void)aArguments;
}

TEST_GROUP(EventEmitter){};

TEST(EventEmitter, TestSingleHandler)
{
    ot::BorderRouter::EventEmitter ee;
    int                            event = 1;
    ee.On(event, HandleSingleEvent, NULL);

    sContext = NULL;
    sEvent   = event;
    sCounter = 0;

    ee.Emit(event);

    CHECK_EQUAL(1, sCounter);
}

TEST(EventEmitter, TestDoubleHandler)
{
    ot::BorderRouter::EventEmitter ee;
    int                            event = 1;
    ee.On(event, HandleSingleEvent, NULL);
    ee.On(event, HandleSingleEvent, NULL);

    sContext = NULL;
    sEvent   = event;
    sCounter = 0;

    ee.Emit(event);

    CHECK_EQUAL(2, sCounter);
}

TEST(EventEmitter, TestDifferentContext)
{
    ot::BorderRouter::EventEmitter ee;
    int                            event = 2;

    int context1 = 1;
    int context2 = 2;

    ee.On(event, HandleTestDifferentContextEvent, &context1);
    ee.On(event, HandleTestDifferentContextEvent, &context2);

    sContext = NULL;
    sEvent   = event;
    sCounter = 0;

    ee.Emit(event, &context1, &context2);

    CHECK_EQUAL(2, sCounter);
}

TEST(EventEmitter, TestCallSequence)
{
    ot::BorderRouter::EventEmitter ee;
    int                            event = 3;

    int context1 = 1;
    int context2 = 2;

    ee.On(event, HandleTestCallSequenceEvent, &context1);
    ee.On(event, HandleTestCallSequenceEvent, &context2);

    sContext = NULL;
    sEvent   = event;
    sCounter = 0;

    ee.Emit(event);

    CHECK_EQUAL(2, sCounter);
}

TEST(EventEmitter, TestRemoveHandler)
{
    ot::BorderRouter::EventEmitter ee;
    int                            event = 3;

    ee.On(event, HandleSingleEvent, NULL);
    ee.On(event, HandleSingleEvent, NULL);

    sContext = NULL;
    sEvent   = event;
    sCounter = 0;

    ee.Emit(event);
    CHECK_EQUAL(2, sCounter);

    ee.Off(event, HandleSingleEvent, NULL);
    ee.Emit(event);
    CHECK_EQUAL(3, sCounter);

    ee.Off(event, HandleSingleEvent, NULL);
    ee.Emit(event);
    CHECK_EQUAL(3, sCounter);
}
