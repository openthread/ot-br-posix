#!/bin/bash
#
#  Copyright (c) 2017, The OpenThread Authors.
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

set -e
set -x

TOOLS_HOME=$HOME/.cache/tools
export PATH=/usr/local/bin:$PATH

case $BUILD_TARGET in
    meshcop)
        ./script/bootstrap
        ./script/test build

        OT_CLI="ot-cli-mtd" ./script/test meshcop
        OT_CLI="ot-cli-ftd" ./script/test meshcop
        OTBR_USE_WEB_COMMISSIONER=1 ./script/test meshcop
        ;;

    openwrt-check)
        ./script/test openwrt
        ;;

    docker-check)
        .travis/check-docker
        ;;

    otbr-dbus-check)
        .travis/check-otbr-dbus
        ;;

    macOS)
        # On Travis, brew install fails when a package is already installed, so use reinstall here instead of ./script/bootstrap
        brew reinstall boost cmake cpputest dbus jsoncpp ninja
        OTBR_OPTIONS='-DOTBR_MDNS=OFF' ./script/test build
        ;;
    *)
        false
        ;;
esac
