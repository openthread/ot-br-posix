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

cmake_minimum_required(VERSION 3.10.2)
project(openthread-br VERSION 0.3.0)


add_library(otbr-config INTERFACE)

set(OTBR_INFRA_IF_NAME "wlan0" CACHE STRING "The infrastructure interface name")

option(OTBR_BACKBONE_ROUTER         "Enable Backbone Router" OFF)
option(OTBR_BORDER_ROUTING          "Enable Border Routing Manager" OFF)
option(OTBR_DBUS                    "Enable DBus support" OFF)
option(OTBR_DOC                     "Build documentation" OFF)
option(OTBR_DUA_ROUTING             "Enable Backbone Router DUA Routing" OFF)
option(OTBR_OPENWRT                 "Enable OpenWrt support" OFF)
option(OTBR_REST                    "Enable Rest Server" OFF)
option(OTBR_SRP_ADVERTISING_PROXY   "Enable Advertising Proxy" OFF)
option(OTBR_UNSECURE_JOIN           "Enable unsecure joining" OFF)
option(OTBR_WEB                     "Enable Web GUI" OFF)

if(NOT CMAKE_C_STANDARD)
    set(CMAKE_C_STANDARD 99)
    set(CMAKE_C_STANDARD_REQUIRED ON)
endif()

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

set(CMAKE_CXX_EXTENSIONS OFF)

if (OTBR_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(STATUS "Coverage: ON")
    target_compile_options(otbr-config INTERFACE -g -O0 --coverage)
    target_link_libraries(otbr-config INTERFACE --coverage)
endif()

add_compile_options(-Wall -Wextra -Werror -Wfatal-errors -Wno-missing-braces)


execute_process(
    COMMAND git describe --dirty --always
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE OTBR_GIT_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(OTBR_GIT_VERSION)
    set(OTBR_VERSION "${PROJECT_VERSION}-${OTBR_GIT_VERSION}")
else()
    set(OTBR_VERSION "${PROJECT_VERSION}")
endif()

message(STATUS "Version: ${OTBR_VERSION}")


target_include_directories(otbr-config INTERFACE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
target_compile_definitions(otbr-config INTERFACE
    "OTBR_PACKAGE_NAME=\"OPENTHREAD_BR\""
    "OTBR_PACKAGE_VERSION=\"${OTBR_VERSION}\""
)

if(BUILD_SHARED_LIBS)
    target_link_libraries(otbr-config INTERFACE -Wl,--unresolved-symbols=ignore-in-shared-libs)
endif()

find_package(PkgConfig)
include(GNUInstallDirs)

if (OTBR_SRP_ADVERTISING_PROXY)
    target_compile_definitions(otbr-config INTERFACE
            OTBR_ENABLE_SRP_ADVERTISING_PROXY=1
    )
endif()

if (OTBR_BACKBONE_ROUTER)
    target_compile_definitions(otbr-config INTERFACE
            OTBR_ENABLE_BACKBONE_ROUTER=1
    )
endif()

if (OTBR_DUA_ROUTING)
    target_compile_definitions(otbr-config INTERFACE
            OTBR_ENABLE_DUA_ROUTING=1
    )
endif()

if(OTBR_DBUS)
    pkg_check_modules(DBUS REQUIRED dbus-1)
    pkg_get_variable(OTBR_DBUS_SYSTEM_BUS_SERVICES_DIR dbus-1 system_bus_services_dir)
    target_compile_definitions(otbr-config INTERFACE
        OTBR_ENABLE_DBUS_SERVER=1
    )
endif()

if(OTBR_REST)
    target_compile_definitions(otbr-config INTERFACE
        OTBR_ENABLE_REST_SERVER=1
    )
endif()

if(OTBR_WEB)
    pkg_check_modules(JSONCPP jsoncpp REQUIRED)
    set(Boost_USE_STATIC_LIBS ON)
    set(Boost_USE_MULTITHREADED ON)
    set(Boost_USE_STATIC_RUNTIME OFF)
    find_package(Boost REQUIRED
        COMPONENTS filesystem system)
    set(OTBR_WEB_DATADIR ${CMAKE_INSTALL_FULL_DATADIR}/otbr-web)
endif()

if(OTBR_OPENWRT)
    target_compile_definitions(otbr-config INTERFACE
        OTBR_ENABLE_OPENWRT=1
    )
endif()

if(OTBR_UNSECURE_JOIN)
    target_compile_definitions(otbr-config INTERFACE
        OTBR_ENABLE_UNSECURE_JOIN=1
    )
endif()

set(OTBR_MDNS "avahi" CACHE STRING "MDNS service provider")
set_property(CACHE OTBR_MDNS PROPERTY STRINGS "avahi" "mDNSResponder")

pkg_check_modules(SYSTEMD systemd)

if(SYSTEMD_FOUND)
    pkg_get_variable(OTBR_SYSTEMD_UNIT_DIR systemd systemdsystemunitdir)
endif()


add_subdirectory(third_party EXCLUDE_FROM_ALL)
add_subdirectory(src)
add_subdirectory(tools)


if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)

    if(BUILD_TESTING)
        pkg_check_modules(CPPUTEST cpputest REQUIRED)
        add_subdirectory(tests)
    endif()

    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "OpenThread Authors <openthread-users@googlegroups.com")
    set(CPACK_PACKAGE_CONTACT "OpenThread Authors <openthread-users@googlegroups.com")
    include(CPack)
endif()

if (OTBR_DOC)
    add_subdirectory(doc)
endif()
