#
#  Copyright (c) 2021, The OpenThread Authors.
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

include(CMakeDependentOption)
find_package(PkgConfig)

option(OTBR_DOC "Build documentation" OFF)

option(OTBR_BORDER_AGENT "Enable Border Agent" ON)
if (OTBR_BORDER_AGENT)
    target_compile_definitions(otbr-config INTERFACE OTBR_ENABLE_BORDER_AGENT=1)
endif()

option(OTBR_BACKBONE_ROUTER "Enable Backbone Router" OFF)
if (OTBR_BACKBONE_ROUTER)
    target_compile_definitions(otbr-config INTERFACE OTBR_ENABLE_BACKBONE_ROUTER=1)
endif()

option(OTBR_BORDER_ROUTING "Enable Border Routing Manager" OFF)
if (OTBR_BORDER_ROUTING)
    target_compile_definitions(otbr-config INTERFACE OTBR_ENABLE_BORDER_ROUTING=1)
endif()

option(OTBR_BORDER_ROUTING_COUNTERS "Enable Border Routing Counters" ON)
if (OTBR_BORDER_ROUTING_COUNTERS)
    target_compile_definitions(otbr-config INTERFACE OTBR_ENABLE_BORDER_ROUTING_COUNTERS=1)
endif()

option(OTBR_DBUS "Enable DBus support" OFF)
if(OTBR_DBUS)
    pkg_check_modules(DBUS REQUIRED dbus-1)
    pkg_get_variable(OTBR_DBUS_SYSTEM_BUS_SERVICES_DIR dbus-1 system_bus_services_dir)
    target_compile_definitions(otbr-config INTERFACE OTBR_ENABLE_DBUS_SERVER=1)
endif()

option(OTBR_FEATURE_FLAGS "Enable feature flags support" OFF)
if (OTBR_FEATURE_FLAGS)
    target_compile_definitions(otbr-config INTERFACE OTBR_ENABLE_FEATURE_FLAGS=1)
endif()

option(OTBR_TELEMETRY_DATA_API "Enable telemetry data API support" OFF)
if (OTBR_TELEMETRY_DATA_API)
    target_compile_definitions(otbr-config INTERFACE OTBR_ENABLE_TELEMETRY_DATA_API=1)
endif()

option(OTBR_DUA_ROUTING "Enable Backbone Router DUA Routing" OFF)
if (OTBR_DUA_ROUTING)
    target_compile_definitions(otbr-config INTERFACE OTBR_ENABLE_DUA_ROUTING=1)
endif()

option(OTBR_OPENWRT "Enable OpenWrt support" OFF)
if(OTBR_OPENWRT)
    target_compile_definitions(otbr-config INTERFACE OTBR_ENABLE_OPENWRT=1)
endif()

option(OTBR_REST "Enable Rest Server" OFF)
if(OTBR_REST)
    target_compile_definitions(otbr-config INTERFACE OTBR_ENABLE_REST_SERVER=1)
endif()

option(OTBR_SRP_ADVERTISING_PROXY "Enable Advertising Proxy" OFF)
if (OTBR_SRP_ADVERTISING_PROXY)
    target_compile_definitions(otbr-config INTERFACE OTBR_ENABLE_SRP_ADVERTISING_PROXY=1)
endif()

cmake_dependent_option(OTBR_SRP_SERVER_AUTO_ENABLE "Enable SRP server auto enable mode" ON "OTBR_SRP_ADVERTISING_PROXY;OTBR_BORDER_ROUTING" OFF)
if (OTBR_SRP_SERVER_AUTO_ENABLE)
    target_compile_definitions(otbr-config INTERFACE OTBR_ENABLE_SRP_SERVER_AUTO_ENABLE_MODE=1)
endif()

option(OTBR_DNSSD_DISCOVERY_PROXY   "Enable DNS-SD Discovery Proxy support" OFF)
if (OTBR_DNSSD_DISCOVERY_PROXY)
    target_compile_definitions(otbr-config INTERFACE OTBR_ENABLE_DNSSD_DISCOVERY_PROXY=1)
endif()

option(OTBR_UNSECURE_JOIN "Enable unsecure joining" OFF)
if(OTBR_UNSECURE_JOIN)
    target_compile_definitions(otbr-config INTERFACE OTBR_ENABLE_UNSECURE_JOIN=1)
endif()

option(OTBR_TREL "Enable TREL link support." OFF)
if(OTBR_TREL)
    target_compile_definitions(otbr-config INTERFACE OTBR_ENABLE_TREL=1)
endif()

option(OTBR_EPSKC "Enable ephemeral PSKc" ON)
if (OTBR_EPSKC)
    target_compile_definitions(otbr-config INTERFACE OTBR_ENABLE_EPSKC=1)
else()
    target_compile_definitions(otbr-config INTERFACE OTBR_ENABLE_EPSKC=0)
endif()

option(OTBR_WEB "Enable Web GUI" OFF)

option(OTBR_NOTIFY_UPSTART "Notify upstart when ready." ON)
if(OTBR_NOTIFY_UPSTART)
    target_compile_definitions(otbr-config INTERFACE OTBR_ENABLE_NOTIFY_UPSTART=1)
endif()

option(OTBR_NAT64 "Enable NAT64 support" OFF)
if(OTBR_NAT64)
    target_compile_definitions(otbr-config INTERFACE OTBR_ENABLE_NAT64=1)
else()
    target_compile_definitions(otbr-config INTERFACE OTBR_ENABLE_NAT64=0)
endif()

option(OTBR_VENDOR_INFRA_LINK_SELECT "Enable Vendor-specific infrastructure link selection rules" OFF)
if(OTBR_VENDOR_INFRA_LINK_SELECT)
    target_compile_definitions(otbr-config INTERFACE OTBR_ENABLE_VENDOR_INFRA_LINK_SELECT=1)
else()
    target_compile_definitions(otbr-config INTERFACE OTBR_ENABLE_VENDOR_INFRA_LINK_SELECT=0)
endif()

option(OTBR_DNS_UPSTREAM_QUERY "Allow sending DNS queries to upstream" OFF)
if (OTBR_DNS_UPSTREAM_QUERY)
    target_compile_definitions(otbr-config INTERFACE OTBR_ENABLE_DNS_UPSTREAM_QUERY=1)
endif()

option(OTBR_PUBLISH_MESHCOP_BA_ID "Publish the MeshCoP mDNS 'id' TXT entry, enable this feature only when 'id' is not set via dbus API" ON)
if (OTBR_PUBLISH_MESHCOP_BA_ID)
    target_compile_definitions(otbr-config INTERFACE OTBR_ENABLE_PUBLISH_MESHCOP_BA_ID=1)
endif()

option(OTBR_DHCP6_PD "Prefix delegation support" OFF)
if (OTBR_DHCP6_PD)
    target_compile_definitions(otbr-config INTERFACE OTBR_ENABLE_DHCP6_PD=1)
else()
    target_compile_definitions(otbr-config INTERFACE OTBR_ENABLE_DHCP6_PD=0)
endif()

option(OTBR_VENDOR_SERVER "Enable vendor server" OFF)
if (OTBR_VENDOR_SERVER)
    target_compile_definitions(otbr-config INTERFACE OTBR_ENABLE_VENDOR_SERVER=1)
endif()

option(OTBR_LINK_METRICS_TELEMETRY "Enable Link Metrics Telemetry Upload" OFF)
if (OTBR_LINK_METRICS_TELEMETRY)
    target_compile_definitions(otbr-config INTERFACE OTBR_ENABLE_LINK_METRICS_TELEMETRY=1)
else()
    target_compile_definitions(otbr-config INTERFACE OTBR_ENABLE_LINK_METRICS_TELEMETRY=0)
endif()

option(OTBR_POWER_CALIBRATION "Enable Power Calibration" ON)
if (OTBR_POWER_CALIBRATION)
    target_compile_definitions(otbr-config INTERFACE OTBR_ENABLE_POWER_CALIBRATION=1)
else()
    target_compile_definitions(otbr-config INTERFACE OTBR_ENABLE_POWER_CALIBRATION=0)
endif()
