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

cmake_minimum_required(VERSION 3.11.4)
project(openthread-br VERSION 0.1.0)


option(OTBR_DBUS    "Build DBus support" OFF)
option(OTBR_OPENWRT "Build OpenWrt support" OFF)
option(OTBR_WEB     "Build Web GUI" OFF)


if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

set(CMAKE_CXX_EXTENSIONS OFF)

if (OTBR_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(STATUS "Coverage: ON")
    add_compile_options(-g -O0 --coverage)
    add_link_options(--coverage)
endif()


execute_process(
    COMMAND git describe --dirty --always
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE OTBR_GIT_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(OTBR_GIT_VERSION)
    set(OTBR_VERSION "${PROJECT_VERSION}-${OTBR_GIT_VERSION}")
elseif(OTBR_GIT_VERSION)
    set(OTBR_VERSION "${PROJECT_VERSION}")
endif()

message(STATUS "Version: ${OTBR_VERSION}")


add_library(otbr-config INTERFACE)
target_include_directories(otbr-config INTERFACE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
target_compile_definitions(otbr-config INTERFACE
    "PACKAGE_NAME=\"OPENTHREAD_BR\""
    "PACKAGE_VERSION=\"${OTBR_VERSION}\""
)


find_package(PkgConfig)
include(GNUInstallDirs)

if(OTBR_DBUS)
    pkg_check_modules(DBUS REQUIRED dbus-1)
    pkg_get_variable(OTBR_DBUS_SYSTEM_BUS_SERVICES_DIR dbus-1 system_bus_services_dir)
    target_compile_definitions(otbr-config INTERFACE
        OTBR_ENABLE_DBUS_SERVER=1
    )
endif()

if(OTBR_WEB)
    pkg_check_modules(JSONCPP jsoncpp REQUIRED)
    find_package(Boost REQUIRED
        COMPONENTS filesystem system)
    set(OTBR_WEB_DATADIR ${CMAKE_INSTALL_DATADIR}/otbr-web)
endif()

if(OTBR_OPENWRT)
    target_compile_definitions(otbr-config INTERFACE
        OTBR_ENABLE_OPENWRT=1
    )
endif()

set(OTBR_MDNS "avahi" CACHE STRING "MDNS service provider")
set_property(CACHE OTBR_MDNS PROPERTY STRINGS "avahi" "mDNSResponder" "mojo")

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
