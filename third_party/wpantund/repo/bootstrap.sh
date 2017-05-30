#!/bin/sh
#
# Copyright (c) 2016 Nest Labs, Inc.
# All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

LOGFILE=`mktemp -q /tmp/bootstrap.log.XXXXXX`
BOOTSTRAP_ANDROID=0
BOOTSTRAP_AUTOTOOLS=1
SOURCE_DIR=$(cd `dirname $0` && pwd)
AUTOANDR=${AUTOANDR-${SOURCE_DIR}/etc/autoandr/autoandr}
AUTOMAKE="automake --foreign"

die() {
	local errcode=$?
	local prog=$1
	shift

	echo " ***************************** "

	[[ $prog == "autoreconf" ]] && cat "$LOGFILE"

	echo ""
	if [[ $# -ge 1 ]]
	then echo " *** $prog failed: \"$*\""
	else echo " *** $prog failed with error code $errcode"
	fi

	exit 1
}

while [ "$#" -ge 1 ]
do
	case $1 in
		--all)
			BOOTSTRAP_ANDROID=1
			BOOTSTRAP_AUTOTOOLS=1
			;;

		--android)
			BOOTSTRAP_ANDROID=1
			BOOTSTRAP_AUTOTOOLS=0
			;;

		--verbose)
			LOGFILE=/dev/stderr
			export V=1
			;;

		*)
			die bootstrap "Unknown argument: $1"
			;;
	esac
	shift
done


if [ "$BOOTSTRAP_AUTOTOOLS" = 1 ]
then
	cd "${SOURCE_DIR}"

	which autoreconf 1>/dev/null 2>&1 || {
		echo " *** error: The 'autoreconf' command was not found."
		echo "Use the appropriate command for your platform to install the package:"
		echo ""
		echo "Homebrew(OS X) ....... brew install libtool autoconf autoconf-archive"
		echo "Debian/Ubuntu ........ apt-get install libtool autoconf autoconf-archive"
		exit 1
	}

	autoreconf --verbose --force --install 2>"$LOGFILE" || die autoreconf

	grep -q AX_CHECK_ configure && {
		echo " *** error: The 'autoconf-archive' package is not installed."
		echo "Use the appropriate command for your platform to install the package:"
		echo ""
		echo "Homebrew(OS X) ....... brew install autoconf-archive"
		echo "Debian/Ubuntu ........ apt-get install autoconf-archive"
		exit 1
	}
fi

if [ "$BOOTSTRAP_ANDROID" = 1 ]
then
	cd "${SOURCE_DIR}"

	AUTOANDR_STDOUT="$LOGFILE" \
	AUTOANDR_MODULE_TAGS=optional \
	$AUTOANDR start \
		--disable-option-checking \
		--enable-static-link-ncp-plugin \
		--disable-shared \
		--prefix=/system \
		--localstatedir=/data/misc \
		--bindir=/system/bin \
		--sbindir=/system/bin \
		--libexecdir=/system/bin \
		--libdir=/system/lib \
		--includedir=/system/include \
		--oldincludedir=/system/include \
		--disable-debug \
		CXXFLAGS="-fexceptions -Wno-non-virtual-dtor -frtti -Wno-c++11-narrowing" \
		CPPFLAGS="-Wno-date-time -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare" \
		DBUS_CFLAGS="-Iexternal/dbus" \
		DBUS_LIBS="-ldbus" \
		TUNNEL_TUNTAP_DEVICE="/dev/tun" \
		SOCKET_UTILS_DEFAULT_SHELL="/system/bin/sh" \
		ac_cv_func_getdtablesize=no \
		ac_cv_func_fgetln=no \
		ac_cv_header_util_h=no \
		--with-boost=internal \
	|| die autoandr
fi

echo
echo Success. Logs in '"'$LOGFILE'"'
