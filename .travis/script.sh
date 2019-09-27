#!/bin/sh
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

die() {
	echo " *** ERROR: " $*
	exit 1
}

case $BUILD_TARGET in
android-check)
    (cd .. && "${TRAVIS_BUILD_DIR}/.travis/check-android-build")
    ;;

mdns-mojo-check)
    ./tests/mdns/test-mojo
    ;;

pretty-check)
    export PATH=$TOOLS_HOME/usr/bin:$PATH
    ./bootstrap && ./configure && make pretty-check || die
    ;;

posix-check)
    CPPFLAGS="$CFLAGS -I$TOOLS_HOME/usr/include" LDFLAGS="$LDFLAGS -L$TOOLS_HOME/usr/lib" .travis/check-posix
    ;;

meshcop)
    if gcc-5 --version; then
      export CC=gcc-5
      export CXX=g++-5
    fi

    ./bootstrap
    ./script/test build

    OT_CLI="ot-cli-mtd" ./script/test meshcop
    OT_CLI="ot-cli-ftd" ./script/test meshcop
    COMMISSIONER_WEB=1 ./script/test meshcop
    NCP_CONTROLLER=openthread ./script/test clean build meshcop
    ;;

scan-build)
    .travis/check-scan-build
    ;;

script-check)
    .travis/check-scripts
    ;;

raspbian-gcc)
    IMAGE_FILE=$TOOLS_HOME/images/$IMAGE_NAME.img .travis/check-raspbian
    ;;

docker-check)
    .travis/check-docker
    ;;

macOS)
    RELEASE=1 ./script/bootstrap
    # Currently only verify otbr-agent
    ./configure --prefix= --exec-prefix=/usr --disable-web-service --with-mdns=none
    make -j$(shell getconf _NPROCESSORS_ONLN)
    ;;
*)
    die
    ;;
esac
