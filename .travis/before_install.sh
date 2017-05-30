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

die() {
	echo " *** ERROR: " $*
	exit 1
}

set -x

case $TRAVIS_OS_NAME in
linux)
    # For wpantund
    sudo apt-get install libdbus-1-dev

    # This is for configuring wpantund
    sudo apt-get install autoconf-archive

    # For libcoap generate doc
    sudo apt-get install doxygen

    # For libcoap to generate .sym and .map
    sudo apt-get install ctags

    # Dependencies
    sudo apt-get install libboost-dev libboost-filesystem-dev libboost-system-dev libavahi-core-dev

    # npm bower
    curl -sL https://deb.nodesource.com/setup_7.x | sudo -E bash -

    sudo apt-get install -y nodejs

    sudo npm -g install bower

    # For functional test
    sudo apt-get install expect

    # Uncrustify
    [ $BUILD_TARGET != pretty-check ] || (cd /tmp &&
        wget https://github.com/uncrustify/uncrustify/archive/uncrustify-0.64.tar.gz &&
        tar xzf uncrustify-0.64.tar.gz &&
        cd uncrustify-uncrustify-0.64 &&
        mkdir build &&
        cd build &&
        cmake -DCMAKE_BUILD_TYPE=Release .. &&
        make &&
        export PATH=/tmp/uncrustify-uncrustify-0.64/build:$PATH &&
        uncrustify --version) || die

    # Unittest
    [ $BUILD_TARGET != posix-check ] || (cd /tmp &&
        wget https://github.com/cpputest/cpputest/archive/v3.8.tar.gz &&
        tar xzf v3.8.tar.gz &&
        cd cpputest-3.8 &&
        ./autogen.sh &&
        ./configure --prefix=/usr &&
        make install DESTDIR=../cpputest) || die

    # Enable IPv6
    [ $BUILD_TARGET != posix-check ] || (echo 0 | sudo tee /proc/sys/net/ipv6/conf/all/disable_ipv6) || die
    ;;
*)
    die
    ;;
esac

./bootstrap
