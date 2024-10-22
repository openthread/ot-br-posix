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

set(OT_15_4 ON CACHE STRING "enable 802.15.4 radio link" FORCE)
set(OT_ANYCAST_LOCATOR ON CACHE STRING "enable anycast locator" FORCE)
set(OT_BACKBONE_ROUTER ${OTBR_BACKBONE_ROUTER} CACHE STRING "Enable Backbone Router feature in OpenThread" FORCE)
set(OT_BACKBONE_ROUTER_DUA_NDPROXYING ${OTBR_DUA_ROUTING} CACHE STRING "Configure DUA ND Proxy feature in OpenThread" FORCE)
set(OT_BORDER_AGENT ON CACHE STRING "enable border agent" FORCE)
set(OT_BORDER_AGENT_EPSKC ON CACHE STRING "enable border agent ephemeral PSKc" FORCE)
set(OT_BORDER_AGENT_ID ON CACHE STRING "enable border agent ID" FORCE)
set(OT_BORDER_ROUTER ON CACHE STRING "enable border router feature" FORCE)
set(OT_BORDER_ROUTING ${OTBR_BORDER_ROUTING} CACHE STRING "enable border routing feature" FORCE)
set(OT_BORDER_ROUTING_COUNTERS ${OTBR_BORDER_ROUTING_COUNTERS} CACHE STRING "enable border routing counters feature" FORCE)
set(OT_BORDER_ROUTING_DHCP6_PD ${OTBR_DHCP6_PD} CACHE STRING "enable dhcpv6 pd support in border routing" FORCE)
set(OT_BUILD_EXECUTABLES OFF CACHE STRING "disable building executables" FORCE)
set(OT_BUILTIN_MBEDTLS_MANAGEMENT OFF CACHE STRING "diable mbedTLS management" FORCE)
set(OT_CHILD_SUPERVISION ON CACHE STRING "enable child supervision" FORCE)
set(OT_COAP ON CACHE STRING "Enable CoAP in OpenThread")
set(OT_COAPS ON CACHE STRING "Enable secure CoAP in OpenThread")
set(OT_COMMISSIONER ON CACHE STRING "enable commissioner")
set(OT_DAEMON ON CACHE STRING "enable daemon mode" FORCE)
set(OT_DATASET_UPDATER ON CACHE STRING "enable dataset updater" FORCE)
set(OT_DNS_CLIENT ON CACHE STRING "enable DNS client" FORCE)
set(OT_DNS_CLIENT_OVER_TCP OFF CACHE STRING "disable DNS query over TCP")
set(OT_DNS_UPSTREAM_QUERY ${OTBR_DNS_UPSTREAM_QUERY} CACHE STRING "enable sending DNS queries to upstream" FORCE)
set(OT_DNSSD_SERVER ${OTBR_DNSSD_DISCOVERY_PROXY} CACHE STRING "enable DNS-SD server support" FORCE)
set(OT_ECDSA ON CACHE STRING "enable ECDSA" FORCE)
set(OT_FIREWALL ON CACHE STRING "enable firewall feature")
set(OT_HISTORY_TRACKER ON CACHE STRING "enable history tracker" FORCE)
set(OT_JOINER ON CACHE STRING "enable joiner" FORCE)
set(OT_LINK_METRICS_INITIATOR ${OTBR_LINK_METRICS_TELEMETRY} CACHE STRING "enable link metrics initiator" FORCE)
set(OT_LINK_METRICS_MANAGER ${OTBR_LINK_METRICS_TELEMETRY} CACHE STRING "enable link metrics manager" FORCE)
set(OT_LOG_LEVEL_DYNAMIC ON CACHE STRING "enable dynamic log level control" FORCE)
set(OT_MAC_FILTER ON CACHE STRING "enable MAC filter" FORCE)
set(OT_NAT64_BORDER_ROUTING ${OTBR_NAT64} CACHE STRING "enable NAT64 in border routing manager" FORCE)
set(OT_NAT64_TRANSLATOR ${OTBR_NAT64} CACHE STRING "enable NAT64 translator" FORCE)
set(OT_NETDATA_PUBLISHER ON CACHE STRING "enable netdata publisher" FORCE)
set(OT_NETDIAG_CLIENT ON CACHE STRING "enable Network Diagnostic client" FORCE)
set(OT_PLATFORM "posix" CACHE STRING "use posix platform" FORCE)
set(OT_PLATFORM_NETIF ON CACHE STRING "enable platform netif" FORCE)
set(OT_PLATFORM_UDP ON CACHE STRING "enable platform UDP" FORCE)
set(OT_SERVICE ON CACHE STRING "enable service" FORCE)
set(OT_SLAAC ON CACHE STRING "enable SLAAC" FORCE)
set(OT_SRP_CLIENT ON CACHE STRING "enable SRP client" FORCE)
set(OT_TARGET_OPENWRT ${OTBR_OPENWRT} CACHE STRING "target on OpenWRT" FORCE)
set(OT_TCP OFF CACHE STRING "disable TCP")
set(OT_TREL ${OTBR_TREL} CACHE STRING "enable TREL" FORCE)
set(OT_UDP_FORWARD OFF CACHE STRING "disable udp forward" FORCE)
set(OT_UPTIME ON CACHE STRING "enable uptime" FORCE)

if (OTBR_SRP_ADVERTISING_PROXY)
    set(OT_SRP_SERVER ON CACHE STRING "enable SRP server" FORCE)
    set(OT_EXTERNAL_HEAP ON CACHE STRING "enable external heap" FORCE)
endif()

if (NOT OT_THREAD_VERSION STREQUAL "1.1")
    if (OT_REFERENCE_DEVICE)
        set(OT_DUA ON CACHE STRING "Enable Thread 1.2 DUA for reference devices")
    endif()
    set(OT_MLR ON CACHE STRING "Enable Thread 1.2 MLR by default")
    set(OT_LINK_METRICS_SUBJECT ON CACHE STRING "enable link metrics subject" FORCE)
endif()

if (NOT OT_LOG_LEVEL)
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(OT_LOG_LEVEL "DEBG" CACHE STRING "set OpenThread log level to DEBG" FORCE)
    else()
        set(OT_LOG_LEVEL "INFO" CACHE STRING "set OpenThread log level to INFO" FORCE)
    endif()
endif()

if (NOT OT_POSIX_SETTINGS_PATH)
    set(OT_POSIX_SETTINGS_PATH "\"/var/lib/thread\"" CACHE STRING "set the directory to store Thread data" FORCE)
endif()

set(OT_MBEDTLS_CONFIG_FILE "\"${PROJECT_SOURCE_DIR}/third_party/openthread/mbedtls-config.h\"" CACHE STRING "use mbedtls-config.h of this project" FORCE)

add_subdirectory(repo EXCLUDE_FROM_ALL)

target_compile_definitions(ot-config INTERFACE
    "-DOPENTHREAD_CONFIG_LOG_CLI=1"
    "-DOPENTHREAD_CONFIG_MAX_STATECHANGE_HANDLERS=3"
    "-DOPENTHREAD_CONFIG_MLE_STEERING_DATA_SET_OOB_ENABLE=1"
    "-DOPENTHREAD_POSIX_CONFIG_FILE=\"${PROJECT_BINARY_DIR}/src/agent/openthread-otbr-posix-config.h\""
)

if (NOT OT_THREAD_VERSION STREQUAL "1.1")
    target_compile_definitions(ot-config INTERFACE
        "-DOPENTHREAD_CONFIG_TMF_PROXY_DUA_ENABLE=1"
        "-DOPENTHREAD_CONFIG_TMF_PROXY_MLR_ENABLE=1"
    )
endif()
