dnl #
dnl # Copyright (c) 2016 Nest Labs, Inc.
dnl # All rights reserved.
dnl #
dnl # Licensed under the Apache License, Version 2.0 (the "License");
dnl # you may not use this file except in compliance with the License.
dnl # You may obtain a copy of the License at
dnl #
dnl #    http://www.apache.org/licenses/LICENSE-2.0
dnl #
dnl # Unless required by applicable law or agreed to in writing, software
dnl # distributed under the License is distributed on an "AS IS" BASIS,
dnl # WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
dnl # See the License for the specific language governing permissions and
dnl # limitations under the License.
dnl #

AC_PREREQ(2.59)

dnl This is the project name and release verison number.
dnl The release version is stored in the file `.default-version`.
dnl Update that file *after* every release to indicate the version
dnl of the *next* anticipated release.
AC_INIT(
	[wpantund],
	m4_esyscmd([printf "%s" `cat .default-version`]),
	[wpantund-devel@googlegroups.com],
	[wpantund],
	[https://github.com/openthread/wpantund/]dnl
)

dnl These are the list of plugins that have usage restrictions
dnl associated with their license. These will be automatically
dnl disabled by default and print out a warning when enabled.
m4_define(RESTRICTED_USE_NCP_PLUGINS, [  dnl
])

dnl This is the list of plugins which are included with wpantund.
dnl As plugins are added, they should be appended to this list.
m4_define(AVAILABLE_NCP_PLUGINS, [       dnl
	RESTRICTED_USE_NCP_PLUGINS           dnl
	spinel                               dnl
	dummy                                dnl
])

dnl These are the list of plugins are disabled by default.
dnl This list automatically includes the plugins from
dnl RESTRICTED_USE_NCP_PLUGINS.
m4_define(DISABLED_NCP_PLUGINS, [RESTRICTED_USE_NCP_PLUGINS])

dnl Attempt to include a reasonable git tag description.
SOURCE_VERSION=m4_esyscmd([
	configver="$PACKAGE_VERSION"
	(test -d .git && gitver=`git describe --dirty --match "[0-9].*"` && printf "$gitver") ||
	(test -d .git && gitver=`git describe --always` && printf "$configver-g$gitver") ||
	printf "$configver"
])

AC_CONFIG_AUX_DIR([m4])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_FILES(Makefile doxygen.cfg src/Makefile doc/Makefile src/connman-plugin/Makefile src/ipc-dbus/Makefile src/wpanctl/Makefile src/wpantund/Makefile src/util/Makefile src/scripts/Makefile src/missing/Makefile src/missing/strlcat/Makefile src/missing/strlcpy/Makefile third_party/Makefile)

# Set up AC_CONFIG_FILES for all of the available plugins.
AC_CONFIG_FILES(m4_normalize(m4_foreach_w(THIS_PLUGIN,AVAILABLE_NCP_PLUGINS,[src/ncp-[]THIS_PLUGIN/Makefile ])))


PLUGIN_SUBDIRS="[]m4_foreach_w(THIS_PLUGIN,AVAILABLE_NCP_PLUGINS,ncp-[]THIS_PLUGIN )"
AC_SUBST(PLUGIN_SUBDIRS)

AM_INIT_AUTOMAKE()

AM_MAINTAINER_MODE

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

m4_ifdef([AX_CODE_COVERAGE], [AX_CODE_COVERAGE], [AC_SUBST([CODE_COVERAGE_RULES])])

LT_INIT

AC_LIBTOOL_DLOPEN_SELF

AC_PROG_LIBTOOL

AC_CONFIG_SRCDIR([src/version.h])
AC_CONFIG_HEADERS([src/config.h])

# This varable is for defining the default NCP plugin.
# This is the default value, which can be overridden below.
default_ncp_plugin=none

dnl Add --enable/--disable configure arguments for all the available plugins.
m4_foreach_w(THIS_PLUGIN,AVAILABLE_NCP_PLUGINS,[
	m4_define([THIS_PLUGIN_DISABLED],m4_foreach_w(THAT_PLUGIN,DISABLED_NCP_PLUGINS,[m4_if(THIS_PLUGIN,THAT_PLUGIN,[disabled])]))

	AC_ARG_ENABLE(
		ncp-[]m4_translit(THIS_PLUGIN,[_],[-]),
		AC_HELP_STRING(
			[--[]m4_if(THIS_PLUGIN_DISABLED,[disabled],[enable],[disable])-ncp-[]m4_translit(THIS_PLUGIN,[_],[-])],
			m4_do([[]m4_if(THIS_PLUGIN_DISABLED,[disabled],[enable],[disable]) the NCP plugin "],[THIS_PLUGIN],["])
		),
		[
			if test "x$enableval" = "xdefault"
			then
				default_ncp_plugin=[]THIS_PLUGIN
				enable_ncp_[]THIS_PLUGIN=yes
			fi
		],
		[m4_if(THIS_PLUGIN_DISABLED,[disabled],[true],[enable_ncp_[]THIS_PLUGIN=yes])],
	)
])

AC_ARG_ENABLE(
	all-restricted-plugins,
	AC_HELP_STRING(
		[--enable-all-restricted-plugins],
		[Enable all plugins with restrictive licenses]
	)
)

AC_ARG_ENABLE(
	static-link-ncp-plugin,
	AC_HELP_STRING(
		[--enable-static-link-ncp-plugin=[plugin]],
		[Statically link against a single NCP plugin]
	),
	[],
	[
		if test "x$enable_shared" == "xno"
		then enable_static_link_ncp_plugin=yes
		fi
	]
)

if test "x$enable_all_restricted_plugins" = "xyes"
then {
true
m4_foreach_w(THIS_PLUGIN,RESTRICTED_USE_NCP_PLUGINS,
[	if test "x$enable_ncp_[]m4_translit(THIS_PLUGIN,[-],[_])" = "x"
	then enable_ncp_[]m4_translit(THIS_PLUGIN,[-],[_])="yes"
	fi
])}
fi

test "x$prefix" = xNONE && prefix=$ac_default_prefix
test "x$exec_prefix" = xNONE && exec_prefix='${prefix}'
test "x$pkglibexecdir" = x && pkglibexecdir='${libexecdir}/${PACKAGE_TARNAME}'

AC_DEFINE_UNQUOTED([PREFIX], ["`eval echo "$prefix"`"], [Define to the install prefix])
AC_DEFINE_UNQUOTED([SYSCONFDIR], ["`eval echo "$sysconfdir"`"], [Define to the sub-directory for system settings.])
AC_DEFINE_UNQUOTED([PKGLIBEXECDIR], ["`eval eval eval echo "$pkglibexecdir"`"], [Define to the sub-directory for plugins.])

AC_DEFINE_UNQUOTED([SOURCE_VERSION], ["`eval echo "$SOURCE_VERSION"`"], [Source version])

if test "${TUNNEL_TUNTAP_DEVICE+set}" = "set"
then AC_DEFINE_UNQUOTED([TUNNEL_TUNTAP_DEVICE], ["$TUNNEL_TUNTAP_DEVICE"], [Path to tuntap device])
fi
if test "${SOCKET_UTILS_DEFAULT_SHELL+set}" = "set"
then AC_DEFINE_UNQUOTED([SOCKET_UTILS_DEFAULT_SHELL], ["$SOCKET_UTILS_DEFAULT_SHELL"], [Default path to a shell])
fi

AC_PROG_CC([],AC_MSG_ERROR(["A working C compiler is required"]))
AC_PROG_CXX([],AC_MSG_ERROR(["A working C++ compiler is required"]))

AC_HEADER_STDC
AC_HEADER_TIME

AC_CHECK_HEADERS([unistd.h errno.h stdbool.h], [], AC_MSG_ERROR(["Missing a required header."]))

AC_CHECK_HEADERS([sys/un.h sys/wait.h pty.h pwd.h execinfo.h asm/sigcontext.h sys/prctl.h])

AC_C_CONST
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

AC_C_INLINE
AC_C_VOLATILE

dnl Raspberry Pi's clock_gettime() function is in librt.
dnl This should only pick it up if necessary.
AC_SEARCH_LIBS([clock_gettime], [rt])
AC_CHECK_FUNCS([clock_gettime])

CHECK_MISSING_FUNC([strlcpy])
CHECK_MISSING_FUNC([strlcat])

AC_CHECK_FUNCS([alloca fgetln memcmp memset strtol strdup strndup strlcpy strlcat stpncpy vsnprintf vsprintf snprintf getdtablesize getloadavg])

NL_DEBUG
NL_CHECK_DBUS
NL_CHECK_BOOST_SIGNALS2([],AC_MSG_ERROR([Unable to find a usable implementation of boost::signals2 (not even our internal copy)]))
NL_CHECK_READLINE
NL_CHECK_PTS

AC_DEFINE_UNQUOTED([__STDC_LIMIT_MACROS], [1], [Needed by C++])
AC_DEFINE_UNQUOTED([__STDC_CONSTANT_MACROS], [1], [Needed by C++])

AC_DEFINE_UNQUOTED([ASSERT_MACROS_USE_SYSLOG], 1, [Define to 1 to have assertmacros.h use syslog])
AC_DEFINE_UNQUOTED([ASSERT_MACROS_SQUELCH], 0, [Define to 0 to explicitly prevent squelching assert printouts])

NL_CHECK_CONNMAN
AM_CONDITIONAL([BUILD_CONNMAN_PLUGIN],[test "x${with_connman}" != "xno"])


AM_CONDITIONAL([HOST_IS_LINUX],[(case "${host_os}" in *linux*) true ;; *) false ;; esac)])

building_ncp_plugins=""

m4_foreach_w(THIS_PLUGIN,AVAILABLE_NCP_PLUGINS,[
	if test "x${enable_ncp_[]m4_translit(THIS_PLUGIN,[-],[_])}" = "xyes"
	then building_ncp_plugins=`echo ${building_ncp_plugins} []THIS_PLUGIN`
	fi
])

if test "x$building_ncp_plugins" != "x"
then
	if test "x$default_ncp_plugin" = "xnone"
	then default_ncp_plugin=$(IFS=" " ; set -- $building_ncp_plugins ; echo $1)
	fi
fi

if test "x${enable_static_link_ncp_plugin-no}" != "xno"
then {
	if test "x${enable_static_link_ncp_plugin-no}" != "xyes"
	then {
		# TODO: Extract statically linked plugin here, make sure it is enabled.
		default_ncp_plugin=${enable_static_link_ncp_plugin}
		enable_static_link_ncp_plugin=yes
	} fi

	if test "x${default_ncp_plugin-none}" = "xnone"
	then AC_MSG_ERROR(["You must enable one NCP plugin when statically linking"])
	fi

	building_ncp_plugins=${default_ncp_plugin}

	m4_foreach_w(THIS_PLUGIN,AVAILABLE_NCP_PLUGINS,[
		if test "[]THIS_PLUGIN" != "${default_ncp_plugin}"
		then enable_ncp_[]m4_translit(THIS_PLUGIN,[-],[_])=no
		fi
	])

	AC_DEFINE_UNQUOTED([WPANTUND_PLUGIN_STATICLY_LINKED], [1], [Set to 1 if we are statically linking the plugin/])
}
else {
	NL_CHECK_LIBDL
	NL_EXPORT_DYNAMIC
}
fi

AC_DEFINE_UNQUOTED([WPANTUND_DEFAULT_NCP_PLUGIN], ["${default_ncp_plugin}"], [Set to the name of the default NCP plugin])

AM_CONDITIONAL(STATIC_LINK_NCP_PLUGIN,[test "x${enable_static_link_ncp_plugin}" = "xyes"])

building_ncp_plugins=""


m4_foreach_w(THIS_PLUGIN,AVAILABLE_NCP_PLUGINS,[
	if test "x${enable_ncp_[]m4_translit(THIS_PLUGIN,[-],[_])}" = "xyes"
	then building_ncp_plugins=`echo ${building_ncp_plugins} []THIS_PLUGIN`
	fi

	m4_ifdef([m4_do(WPANTUND_PLUGIN_NCP_[]m4_toupper(m4_translit(THIS_PLUGIN,[-],[_]))],
		[WPANTUND_PLUGIN_NCP_[]m4_toupper(m4_translit(THIS_PLUGIN,[-],[_])]
	)

	AM_CONDITIONAL(BUILD_PLUGIN_NCP_[]m4_toupper(m4_translit(THIS_PLUGIN,[-],[_])),[test "x${enable_ncp_[]m4_translit(THIS_PLUGIN,[-],[_])}" = "xyes"])
])

m4_foreach_w(THIS_PLUGIN,RESTRICTED_USE_NCP_PLUGINS,[
	if test "x${enable_ncp_[]m4_translit(THIS_PLUGIN,[-],[_])}" = "xyes"
	then restricted_use_ncp_plugins=`echo ${restricted_use_ncp_plugins} []THIS_PLUGIN`
	fi
])

AC_SUBST(default_ncp_plugin)

AC_OUTPUT

echo ""
echo "= Summary ====================================================================="
echo ""
echo "ConnMan Plugin ................. ${with_connman-no}"
echo "Using libreadline .............. ${with_readline-???}"
echo "Boost .......................... ${with_boost-???}"
echo "NCP plugins to build ........... ${building_ncp_plugins-none}"
echo "Default NCP Plugin ............. ${default_ncp_plugin}"
echo "Static link NCP plugin ......... ${enable_static_link_ncp_plugin-no}"
echo ""

if test "x${with_readline}" != "xyes"
then {
	echo "warning: libreadline was disabled or not found, interactive"
	echo "         wpanctl command line interface will not be enabled!"
	echo ""
}
fi

if test "x$restricted_use_ncp_plugins" != "x"
then {
	echo "ATTENTION: Some of the plugins you are building have proprietary licenses which"
	echo "           are more restrictive than the Apache 2.0 license that covers wpantund"
	echo "           in general. If you are reading this message, it means you have manually"
	echo "           enabled at least one plug-in that has a proprietary license. Please"
	echo "           make sure that you understand the usage restrictions before using"
	echo "           wpantund with these plugins: "
	echo ""
	echo "           $restricted_use_ncp_plugins"
	echo ""
}
fi

if test "x$building_ncp_plugins" = "x"
then {
	echo " *** WARNING: The current configuration is NOT BUILDING ANY PLUGINS! wpantund won't"
	echo "              be very useful in this state. Note that plugins with proprietary"
	echo "              licenses are no longer built by default: you must either enable them"
	echo '              individually (using `--enable-ncp-*` configuration options) or by using'
	echo '              the `--enable-all-restricted-plugins` configuration option.'
	echo "              In any case, you can still make and install wpantund without plugins"
	echo "              if you really know what you are doing."
	echo ""
}
fi

if test "x$building_ncp_plugins" = "xdummy"
then {
	echo " *** WARNING: The current configuration is only building the dummy plugin! wpantund"
	echo "              won't be very useful in this state. Note that plugins with proprietary"
	echo "              licenses are no longer built by default: you must either enable them"
	echo '              individually (using `--enable-ncp-*` configuration options) or by using'
	echo '              the `--enable-all-restricted-plugins` configuration option.'
	echo ""
}
fi
