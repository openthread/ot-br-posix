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

set(OT_BORDER_AGENT ON CACHE BOOL "enable border agent" FORCE)
set(OT_BORDER_ROUTER ON CACHE BOOL "enable border router feature" FORCE)
set(OT_BUILD_EXECUTABLES OFF CACHE BOOL "disable building executables" FORCE)
set(OT_BUILTIN_MBEDTLS_MANAGEMENT OFF CACHE BOOL "diable mbedTLS management" FORCE)
set(OT_COMMISSIONER ON CACHE BOOL "enable commissioner" FORCE)
set(OT_DAEMON ON CACHE BOOL "enable platform UDP" FORCE)
set(OT_JOINER ON CACHE BOOL "enable joiner" FORCE)
set(OT_MAC_FILTER ON CACHE STRING "enable MAC filter" FORCE)
set(OT_MBEDTLS_CONFIG_FILE "\"${PROJECT_SOURCE_DIR}/third_party/openthread/mbedtls-config.h\"" CACHE STRING "use mbedtls-config.h of this project" FORCE)
set(OT_PLATFORM "posix" CACHE STRING "use posix platform" FORCE)
set(OT_PLATFORM_NETIF ON CACHE BOOL "enable platform netif" FORCE)
set(OT_PLATFORM_UDP ON CACHE BOOL "enable platform UDP" FORCE)
set(OT_UDP_FORWARD OFF CACHE BOOL "diable udp forward" FORCE)

list(APPEND OT_PRIVATE_DEFINES "-DOPENTHREAD_CONFIG_LEGACY_ENABLE=1")
list(APPEND OT_PRIVATE_DEFINES "-DOPENTHREAD_CONFIG_MAX_STATECHANGE_HANDLERS=3")
list(APPEND OT_PRIVATE_DEFINES "-DOPENTHREAD_CONFIG_MLE_STEERING_DATA_SET_OOB_ENABLE=1")

add_subdirectory(repo EXCLUDE_FROM_ALL)

