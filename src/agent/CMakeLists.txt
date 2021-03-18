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

add_executable(otbr-agent
    advertising_proxy.cpp
    advertising_proxy.hpp
    agent_instance.cpp
    agent_instance.hpp
    border_agent.cpp
    border_agent.hpp
    main.cpp
    uris.hpp
    ncp_openthread.cpp
    ncp_openthread.hpp
    thread_helper.cpp
    thread_helper.hpp
    instance_params.cpp
    instance_params.hpp
)

target_link_libraries(otbr-agent PRIVATE
    $<$<BOOL:${OTBR_DBUS}>:otbr-dbus-server>
    $<$<BOOL:${OTBR_MDNS}>:otbr-mdns>
    $<$<BOOL:${OTBR_OPENWRT}>:otbr-ubus>
    $<$<BOOL:${OTBR_REST}>:otbr-rest>
    $<$<BOOL:${OTBR_BACKBONE_ROUTER}>:otbr-backbone-router>
    openthread-cli-ftd
    openthread-ftd
    openthread-posix
    openthread-spinel-rcp
    openthread-hdlc
    otbr-common
    otbr-utils
)

add_dependencies(otbr-agent ot-ctl print-ot-config)
install(TARGETS otbr-agent DESTINATION sbin)

if(CMAKE_VERSION VERSION_LESS 3.13)
    install(PROGRAMS $<TARGET_FILE:ot-ctl> DESTINATION sbin)
else()
    install(TARGETS ot-ctl DESTINATION sbin)
endif()

set(OTBR_AGENT_USER "root" CACHE STRING "set the username running otbr-agent service")
set(OTBR_AGENT_GROUP "root" CACHE STRING "set the group using otbr-agent client")

if(OTBR_DBUS)
    configure_file(otbr-agent.conf.in otbr-agent.conf)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/otbr-agent.conf
        DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/dbus-1/system.d)
endif()

if(OTBR_SYSTEMD_UNIT_DIR)
    configure_file(otbr-agent.service.in otbr-agent.service)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/otbr-agent.service
        DESTINATION ${OTBR_SYSTEMD_UNIT_DIR}
    )
elseif(NOT OTBR_OPENWRT)
    configure_file(otbr-agent.init.in otbr-agent.init)
    install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/otbr-agent.init
        DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/init.d
        RENAME otbr-agent)
endif()

configure_file(otbr-agent.default.in otbr-agent.default)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/otbr-agent.default
    DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/default
    RENAME otbr-agent
)
