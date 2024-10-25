#
#  Copyright (c) 2020, The OpenThread Authors.
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#  3. Neither the name of the copyright holder nor the
#     names of its contributors may be used to endorse or promote products
#     derived from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
#  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.
#

cmake_minimum_required(VERSION 3.14)
project(openthread-br-gtest)

# GoogleTest requires at least C++14
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(otbr-gtest-unit
    test_async_task.cpp
    test_common_types.cpp
    test_dns_utils.cpp
    test_logging.cpp
    test_once_callback.cpp
    test_pskc.cpp
    test_task_runner.cpp
)
target_link_libraries(otbr-gtest-unit
    mbedtls
    otbr-common
    otbr-ncp
    otbr-utils
    GTest::gmock_main
)
gtest_discover_tests(otbr-gtest-unit)

if(OTBR_MDNS)
    add_executable(otbr-gtest-mdns-subscribe
        test_mdns_subscribe.cpp
    )
    target_link_libraries(otbr-gtest-mdns-subscribe
        otbr-common
        otbr-mdns
        GTest::gmock_main
    )
    gtest_discover_tests(otbr-gtest-mdns-subscribe)
endif()

add_executable(otbr-posix-gtest-unit
    test_infra_if.cpp
    test_netif.cpp
)
target_link_libraries(otbr-posix-gtest-unit
    otbr-posix
    GTest::gmock_main
)
gtest_discover_tests(otbr-posix-gtest-unit PROPERTIES LABELS "sudo")

add_executable(otbr-gtest-host-api
    ${OTBR_PROJECT_DIRECTORY}/src/ncp/rcp_host.cpp
    ${OPENTHREAD_PROJECT_DIRECTORY}/tests/gtest/fake_platform.cpp
    fake_posix_platform.cpp
    test_rcp_host_api.cpp
)
target_include_directories(otbr-gtest-host-api
    PRIVATE
        ${OTBR_PROJECT_DIRECTORY}/src
        ${OPENTHREAD_PROJECT_DIRECTORY}/src/core
        ${OPENTHREAD_PROJECT_DIRECTORY}/tests/gtest
)
target_link_libraries(otbr-gtest-host-api
    mbedtls
    otbr-common
    otbr-utils
    GTest::gmock_main
)
gtest_discover_tests(otbr-gtest-host-api)
