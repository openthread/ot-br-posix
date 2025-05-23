#!/bin/bash
#
#  Copyright (c) 2025, The OpenThread Authors.
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

set -euxo pipefail

INFRA_IF_NAME="${INFRA_IF_NAME:-wlan0}"
readonly INFRA_IF_NAME

SYSCTL_ACCEPT_RA_FILE="/etc/sysctl.d/60-otbr-accept-ra.conf"
readonly SYSCTL_ACCEPT_RA_FILE

SYSCTL_IP_FORWARD_FILE="/etc/sysctl.d/60-otbr-ip-forward.conf"
readonly SYSCTL_IP_FORWARD_FILE

accept_ra_install()
{
    sudo tee $SYSCTL_ACCEPT_RA_FILE <<EOF
net.ipv6.conf.${INFRA_IF_NAME}.accept_ra = 2
net.ipv6.conf.${INFRA_IF_NAME}.accept_ra_rt_info_max_plen = 64
EOF
}

accept_ra_enable()
{
    echo 2 | sudo tee /proc/sys/net/ipv6/conf/"${INFRA_IF_NAME}"/accept_ra || die 'Failed to enable IPv6 RA!'
    echo 64 | sudo tee /proc/sys/net/ipv6/conf/"${INFRA_IF_NAME}"/accept_ra_rt_info_max_plen || die 'Failed to enable IPv6 RIO!'
}

ipforward_install()
{
    sudo tee $SYSCTL_IP_FORWARD_FILE <<EOF
net.ipv6.conf.all.forwarding = 1
net.ipv4.ip_forward = 1
EOF
}

ipforward_enable()
{
    echo 1 | sudo tee /proc/sys/net/ipv6/conf/all/forwarding || die 'Failed to enable IPv6 forwarding!'
    echo 1 | sudo tee /proc/sys/net/ipv4/ip_forward || die 'Failed to enable IPv4 forwarding!'
}

main()
{
    accept_ra_install
    accept_ra_enable
    ipforward_install
    ipforward_enable
}

main
