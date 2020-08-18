/*
 *    Copyright (c) 2020, The OpenThread Authors.
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

#include <string>

// CppUTest does operator overloading which stops the C++ std::function from
// compiling. We need to put the timer headers before CppUTest to resolve this.
// See CppUTest manual ('Resolving conflicts with STL'): https://cpputest.github.io/manual.html.
#include "common/timer.hpp"
#include "common/timer_scheduler.hpp"

#include <CppUTest/TestHarness.h>

TEST_GROUP(TimerScheduler) {};

TEST(TimerScheduler, TestSimpleTimer)
{
    int counter = 0;
    otbr::Timer incTimer([&counter](const otbr::Timer&) {
          ++counter;
        });
    otbr::Timer decTimer([&counter](const otbr::Timer&) {
      --counter;
    });

    incTimer.Start(otbr::Seconds(1));
    CHECK(counter == 0);
    otbr::TimerScheduler::Get().Process(otbr::Clock::now() + otbr::Seconds(1));
    CHECK(counter == 1);
    CHECK(!incTimer.IsRunning());

    incTimer.Start(otbr::Seconds(1));
    CHECK(counter == 1);
    otbr::TimerScheduler::Get().Process(otbr::Clock::now() + otbr::Seconds(1));
    CHECK(counter == 2);
    CHECK(!incTimer.IsRunning());
}

TEST(TimerScheduler, TestTimerOrder)
{
    std::string out;
    otbr::Timer printA([&out](const otbr::Timer&) {
      out.push_back('A');
    });
    otbr::Timer printB([&out](const otbr::Timer&) {
      out.push_back('B');
    });
    otbr::Timer printC([&out](const otbr::Timer&) {
      out.push_back('C');
    });

    printA.Start(otbr::Seconds(2));
    printB.Start(otbr::Seconds(1));
    printC.Start(otbr::Seconds(1));
    CHECK(out.empty());

    otbr::TimerScheduler::Get().Process(otbr::Clock::now() + otbr::Seconds(2));
    CHECK(out == "BCA");
    CHECK(!printA.IsRunning());
    CHECK(!printB.IsRunning());
}
