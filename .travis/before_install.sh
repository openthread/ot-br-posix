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

TOOLS_HOME=$HOME/.cache/tools
[ -d $TOOLS_HOME ] || mkdir -p $TOOLS_HOME

die() {
	echo " *** ERROR: " $*
	exit 1
}

set -e

case $TRAVIS_OS_NAME in
linux)
    # Uncrustify
    [ $BUILD_TARGET != pretty-check ] || [ "$($TOOLS_HOME/usr/bin/uncrustify --version)" = 'Uncrustify-0.65_f' ] || (cd /tmp &&
        wget https://github.com/uncrustify/uncrustify/archive/uncrustify-0.65.tar.gz &&
        tar xzf uncrustify-0.65.tar.gz &&
        cd uncrustify-uncrustify-0.65 &&
        mkdir build &&
        cd build &&
        cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr .. &&
        make && make install DESTDIR=$TOOLS_HOME) || die

    # Unittest
    [ $BUILD_TARGET != posix-check ] || [ -f $TOOLS_HOME/usr/lib/libCppUTest.a ] || (cd /tmp &&
        wget https://github.com/cpputest/cpputest/archive/v3.8.tar.gz &&
        tar xzf v3.8.tar.gz &&
        cd cpputest-3.8 &&
        ./autogen.sh &&
        ./configure --prefix=/usr &&
        make && make install DESTDIR=$TOOLS_HOME) || die

    # Enable IPv6
    [ $BUILD_TARGET != posix-check ] || (echo 0 | sudo tee /proc/sys/net/ipv6/conf/all/disable_ipv6) || die
    ;;

*)
    die
    ;;
esac

./bootstrap
