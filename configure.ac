#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

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

#
# Declare autoconf version requirements
#
AC_PREREQ([2.68])

#
# Initialize autoconf for the package
#
AC_INIT([OTBR],
        m4_esyscmd([third_party/openthread/repo/third_party/nlbuild-autotools/repo/scripts/mkversion -b `cat .default-version` .]),
        [openthread-devel@googlegroups.com],
        [otbr],
        [https://github.com/openthread/ot-br-posix])

# Tell the rest of the build system the absolute path where the
# nlbuild-autotools repository is rooted at.

AC_SUBST(nlbuild_autotools_stem,[third_party/openthread/repo/third_party/nlbuild-autotools/repo])
AC_SUBST(abs_top_nlbuild_autotools_dir,[\${abs_top_srcdir}/\${nlbuild_autotools_stem}])

#
# OTBR interface current, revision, and age versions.
#
# Maintainters: Please manage these fields as follows:
#
#   Interfaces removed:    CURRENT++, AGE = 0, REVISION = 0
#   Interfaces added:      CURRENT++, AGE++,   REVISION = 0
#   No interfaces changed:                     REVISION++
#
#
AC_SUBST(LIBOTBR_VERSION_CURRENT,  [0])
AC_SUBST(LIBOTBR_VERSION_AGE,      [1])
AC_SUBST(LIBOTBR_VERSION_REVISION, [1])
AC_SUBST(LIBOTBR_VERSION_INFO,     [${LIBOTBR_VERSION_CURRENT}:${LIBOTBR_VERSION_REVISION}:${LIBOTBR_VERSION_AGE}])

#
# Check the sanity of the source directory by checking for the
# presence of a key watch file
#
AC_CONFIG_SRCDIR([src/common/types.hpp])

#
# Tell autoconf where to find auxilliary build tools (e.g. config.guess,
# install-sh, missing, etc.)
#
AC_CONFIG_AUX_DIR([third_party/openthread/repo/third_party/nlbuild-autotools/repo/autoconf])

#
# Tell autoconf where to find auxilliary M4 macros
#
AC_CONFIG_MACRO_DIR([third_party/openthread/repo/third_party/nlbuild-autotools/repo/autoconf/m4])

#
# Tell autoconf what file the package is using to aggregate C preprocessor
# defines.
#
AC_CONFIG_HEADERS([include/otbr-config.h])

#
# Figure out what the canonical build, host and target tuples are.
#
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

#
# Mac OS X / Darwin ends up putting some versioning cruft on the end of its
# tuple that we don't care about in this script. Create "clean" variables
# devoid of it.
#

NL_FILTERED_CANONICAL_BUILD
NL_FILTERED_CANONICAL_HOST
NL_FILTERED_CANONICAL_TARGET

#
# Configure automake with the desired options, indicating that this is not
# a native GNU package, that we want "silent" build rules, and that we want
# objects built in the same subdirectory as their source rather than collapsed
# together at the top-level directory.
#
# Disable silent build rules by either passing --disable-silent-rules to
# configure or passing V=1 to make
#
AM_INIT_AUTOMAKE([1.14 foreign silent-rules subdir-objects tar-pax])

#
# Silent build rules requires at least automake-1.11. Employ
# techniques for not breaking earlier versions of automake.
#
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AM_SILENT_RULES([yes])

#
# Enable maintainer mode to prevent the package from constantly trying
# to rebuild configure, Makefile.in, etc. Rebuilding such files rarely,
# if ever, needs to be done "in the field".
#
# Use the included 'bootstrap' script instead when necessary.
#
AM_MAINTAINER_MODE

#
# Checks for build host programs
#

# If we are cross-compiling and we are on an embedded target that
# doesn't support independent, standalone executables, then all
# compiler tests that attempt to create an executable will fail. In
# such circumstances, set AC_NO_EXECUTABLES (see http://sourceware.org/
# ml/newlib/2006/msg00353.html).

AC_MSG_CHECKING([whether to disable executable checking])
if test "$cross_compiling" = yes; then
    AC_NO_EXECUTABLES
    AC_MSG_RESULT([yes])
else
    AC_MSG_RESULT([no])
fi

# Passing -Werror to GCC-based or -compatible compilers breaks some
# autoconf tests (see
# http://lists.gnu.org/archive/html/autoconf-patches/2008-09/msg00014.html).
#
# If -Werror has been passed transform it into -Wno-error. We'll
# transform it back later with NL_RESTORE_WERROR.

NL_SAVE_WERROR

# Check for compilers.
#
# These should be checked BEFORE we check for and, implicitly,
# initialize libtool such that libtool knows what languages it has to
# work with.

AC_PROG_CPP
AC_PROG_CPP_WERROR

AC_PROG_CC
AC_PROG_CC_C_O

AC_PROG_CXXCPP

AC_PROG_CXX
AC_PROG_CXX_C_O

# Check for other compiler toolchain tools.

AC_CHECK_TOOL(AR, ar)
AC_CHECK_TOOL(RANLIB, ranlib)
AC_CHECK_TOOL(OBJCOPY, objcopy)
AC_CHECK_TOOL(STRIP, strip)

# Check for other host tools.

AC_PROG_INSTALL
AC_PROG_LN_S

AC_PATH_PROG(CMP, cmp)
AC_PATH_PROG(PERL, perl)

#
# Checks for specific compiler characteristics
#

#
# Common compiler flags we would like to have.
#
#   -Wall                        CC, CXX
#

PROSPECTIVE_CFLAGS="-Wall -Wextra -Wshadow -Werror -std=c99 -pedantic-errors"
PROSPECTIVE_CXXFLAGS="-Wall -Wextra -Wshadow -Werror -std=c++11 -Wno-c++14-compat"

AC_CACHE_CHECK([whether $CC is Clang],
    [nl_cv_clang],
    [nl_cv_clang=no
    if test "x${GCC}" = "xyes"; then
        AC_EGREP_CPP([NL_CC_IS_CLANG],
            [/* Note: Clang 2.7 lacks __clang_[a-z]+__ */
#            if defined(__clang__) && defined(__llvm__)
             NL_CC_IS_CLANG
#            endif
            ],
            [nl_cv_clang=yes])
    fi
    ])

if test "${nl_cv_clang}" = "yes"; then
    PROSPECTIVE_CFLAGS="${PROSPECTIVE_CFLAGS} -Wconversion"
    PROSPECTIVE_CXXFLAGS="${PROSPECTIVE_CXXFLAGS} -Wconversion"
fi

AX_CHECK_COMPILER_OPTIONS([C],   ${PROSPECTIVE_CFLAGS})
AX_CHECK_COMPILER_OPTIONS([C++], ${PROSPECTIVE_CXXFLAGS})

# Check for and initialize libtool

LT_INIT

#
# Debug instances
#
AC_MSG_NOTICE([checking whether to build debug instances])

# Debug

NL_ENABLE_DEBUG([no])

AM_CONDITIONAL([OTBR_BUILD_DEBUG], [test "${nl_cv_build_debug}" = "yes"])

#
# Code coverage and compiler optimization
#

# Coverage

NL_ENABLE_COVERAGE([no])

AM_CONDITIONAL([OTBR_BUILD_COVERAGE], [test "${nl_cv_build_coverage}" = "yes"])

NL_ENABLE_COVERAGE_REPORTS([auto])

AM_CONDITIONAL([OTBR_BUILD_COVERAGE_REPORTS], [test "${nl_cv_build_coverage_reports}" = "yes"])

# Optimization

NL_ENABLE_OPTIMIZATION([yes])

AM_CONDITIONAL([OTBR_BUILD_OPTIMIZED], [test "${nl_cv_build_optimized}" = "yes"])

#
# Code style
#

AC_SUBST(PRETTY, ["\${abs_top_srcdir}/.script/clang-format.sh"])
AC_SUBST(PRETTY_ARGS, ["-style=file -i"])
AC_SUBST(PRETTY_CHECK, ["\${abs_top_srcdir}/.script/clang-format-check.sh"])
AC_SUBST(PRETTY_CHECK_ARGS, [""])

#
# Tests
#
AC_MSG_NOTICE([checking whether to build tests])

# Tests

NL_ENABLE_TESTS([yes])

AM_CONDITIONAL([OTBR_BUILD_TESTS], [test "${nl_cv_build_tests}" = "yes"])

#
# Documentation
#

# Determine whether or not documentation (via Doxygen) should be built
# or not, with 'auto' as the default and establish a default support
# value for GraphViz 'dot' support.

NL_ENABLE_DOCS([auto],[NO])

AM_CONDITIONAL(OTBR_BUILD_DOCS, [test "${nl_cv_build_docs}" = "yes"])

#
# Commissioner
#

AC_ARG_ENABLE(commissioner,
    [AS_HELP_STRING([--enable-commissioner],[Enable commissioner @<:@default=yes@:>@.])],
    [
        case "${enableval}" in

        no|yes)
            enable_commissioner=${enableval}
            ;;

        *)
            AC_MSG_ERROR([Invalid value ${enable_commissioner} for --enable-commissioner])
            ;;
        esac
    ],
    [enable_commissioner=yes])

if test "$enable_commissioner" = "yes"; then
    OTBR_ENABLE_COMMISSIONER=1
else
    OTBR_ENABLE_COMMISSIONER=0
fi

AC_MSG_RESULT(${enable_commissioner})
AC_SUBST(OTBR_ENABLE_COMMISSIONER)
AM_CONDITIONAL([OTBR_ENABLE_COMMISSIONER], [test "${enable_commissioner}" = "yes"])

#
# Web Service
#

AC_ARG_ENABLE(web-service,
    [AS_HELP_STRING([--enable-web-service],[Enable web service @<:@default=yes@:>@.])],
    [
        case "${enableval}" in

        no|yes)
            enable_web_service=${enableval}
            ;;

        *)
            AC_MSG_ERROR([Invalid value ${enable_web_service} for --enable-web-service])
            ;;
        esac
    ],
    [enable_web_service=yes])

if test "$enable_web_service" = "yes"; then
    OTBR_ENABLE_WEB_SERVICE=1
else
    OTBR_ENABLE_WEB_SERVICE=0
fi

AC_MSG_RESULT(${enable_web_service})
AC_SUBST(OTBR_ENABLE_WEB_SERVICE)
AM_CONDITIONAL([OTBR_ENABLE_WEB_SERVICE], [test "${enable_web_service}" = "yes"])

#
# Checks for libraries and packages.
#
# At minimum, the following packages are optional, depending on
# configuration:
#
#   * TBD
#
AC_MSG_NOTICE([checking required package dependencies])

# NL_WITH_PACKAGE(...)

AC_ARG_WITH(ncp,
  AC_HELP_STRING([--with-ncp=NCP_TYPE], [specify the type of NCP implementation: wpantund, openthread @<:@default=wpantund@:>@.]),
  [with_ncp=${withval}],
  [with_ncp=wpantund]
)

case "${with_ncp}" in
openthread)
  AC_DEFINE_UNQUOTED([OTBR_ENABLE_NCP_OPENTHREAD], [1], [Define to 1 if you want to choose wpantund to manage NCP])
  ;;
wpantund)
  AC_DEFINE_UNQUOTED([OTBR_ENABLE_NCP_WPANTUND], [1], [Define to 1 if you want to choose openthread to manage NCP])
  ;;
*)
  AC_MSG_ERROR([Invalid value ${with_ncp} for --with-ncp])
esac

AM_CONDITIONAL([OTBR_ENABLE_NCP_OPENTHREAD], [test "${with_ncp}" = "openthread"])
AM_CONDITIONAL([OTBR_ENABLE_NCP_WPANTUND], [test "${with_ncp}" = "wpantund"])

AC_ARG_WITH(mdns,
  AC_HELP_STRING([--with-mdns=MDNS_TYPE], [specify the type of mdns implementation: avahi, mDNSResponder, none @<:@default=avahi@:>@.]),
  [with_mdns=${withval}],
  [with_mdns=avahi]
)

case "${with_mdns}" in
avahi)
  AC_DEFINE_UNQUOTED([OTBR_ENABLE_MDNS_AVAHI], [1], [Define to 1 if you want to choose avahi to provide mdns])
  ;;
mDNSResponder)
  AC_DEFINE_UNQUOTED([OTBR_ENABLE_MDNS_MDNSSD], [1], [Define to 1 if you want to choose mDNSResponder to provide mdns])
  ;;
none)
  ;;
*)
  AC_MSG_ERROR([Invalid value ${with_mdns} for --with-mdns])
esac
AM_CONDITIONAL([OTBR_ENABLE_MDNS_AVAHI], [test "${with_mdns}" = "avahi"])
AM_CONDITIONAL([OTBR_ENABLE_MDNS_MDNSSD], [test "${with_mdns}" = "mDNSResponder"])

# Check if ctags is present.

AC_MSG_CHECKING([checking if Exuberant Ctags is available])
AC_PATH_PROGS_FEATURE_CHECK([CTAGS], [ctags],
                            [$ac_path_CTAGS --version | grep -q Exuberant &&
                             ac_cv_path_CTAGS=$ac_path_CTAGS],
                            [
                                if test "$enable_commissioner" = "yes"; then
                                    AC_MSG_ERROR([could not find ctags])
                                fi
                            ])
AC_MSG_RESULT([$ac_cv_path_CTAGS])

# Check if the DBus module

if test "${with_mdns}" == "avahi" -o "${with_ncp}" == "wpantund"; then
  PKG_CHECK_MODULES(DBUS, [dbus-1 >= 1.4], , [AC_MSG_ERROR([could not find dbus(>= 1.4)])])
fi
AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)

AC_ARG_WITH(dbusconfdir,
  AC_HELP_STRING([--with-dbusconfdir=PATH], [path to D-Bus config directory]),
  [path_dbusconf=${withval}],
  [
    if test "$prefix" = "`$PKG_CONFIG --variable=prefix dbus-1`"
    then path_dbusconf="`$PKG_CONFIG --variable=sysconfdir dbus-1`"
    fi
  ]
)
if (test -z "${path_dbusconf}"); then
  if test "${prefix}" = "/usr/local" && test "${sysconfdir}" = '${prefix}/etc' && test -d /etc/dbus-1/system.d
  then DBUS_CONFDIR='/etc/dbus-1/system.d'
  else DBUS_CONFDIR='${sysconfdir}/dbus-1/system.d'
  fi
else
  [path_dbusconf="$(echo ${path_dbusconf} | sed 's:^'"${prefix}"':${prefix}:')" ; ]
  [path_dbusconf="$(echo ${path_dbusconf} | sed 's:^'"${sysconfdir}"':${sysconfdir}:')" ; ]
  DBUS_CONFDIR="${path_dbusconf}/dbus-1/system.d"
fi
AC_SUBST(DBUS_CONFDIR)

AC_ARG_WITH(dbusdatadir, AC_HELP_STRING([--with-dbusdatadir=PATH],
  [path to D-Bus data directory]), [path_dbusdata=${withval}],
  [
    if test "$prefix" = "`$PKG_CONFIG --variable=prefix dbus-1`"
    then path_dbusdata="`$PKG_CONFIG --variable=datadir dbus-1`"
    fi
  ]
)
if (test -z "${path_dbusdata}"); then
  DBUS_DATADIR='${datadir}/dbus-1/system-services'
else
  [path_dbusconf="$(echo ${path_dbusdata} | sed 's:^'"${prefix}"':${prefix}:')" ; ]
  [path_dbusconf="$(echo ${path_dbusdata} | sed 's:^'"${datadir}"':${datadir}:')" ; ]
  DBUS_DATADIR="${path_dbusdata}/dbus-1/system-services"
fi
AC_SUBST(DBUS_DATADIR)

#
# Check for headers
#
AC_HEADER_STDBOOL
AC_HEADER_STDC

AC_DEFUN([OTBR_REQUIRE_HEADER],
         [AC_CHECK_HEADER([$1], , [AC_MSG_ERROR([could not find $1])])])

OTBR_REQUIRE_HEADER([stdint.h])
OTBR_REQUIRE_HEADER([string.h])

#
# Check for types and structures
#
AC_TYPE_INT8_T
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

#
# Checks for library functions
#

if test "${ac_no_link}" != "yes"; then
    AC_CHECK_FUNCS([memcpy])
fi

# Add any code coverage CPPFLAGS and LDFLAGS

CPPFLAGS="${CPPFLAGS} ${NL_COVERAGE_CPPFLAGS}"
LIBS="${LIBS} ${NL_COVERAGE_LIBS}"
LDFLAGS="${LDFLAGS} ${NL_COVERAGE_LDFLAGS}"

# At this point, we can restore the compiler flags to whatever the
# user passed in, now that we're clear of an -Werror issues by
# transforming -Wno-error back to -Werror.

NL_RESTORE_WERROR

#
# Agent user
#

AC_ARG_WITH(
    [agent-user],
    [AS_HELP_STRING([--with-agent-user=<USER>],[Specify the user to run otbr-agent service @<:@default=root@:>@.])],
    [
	    OTBR_AGENT_USER="${withval}"
    ],
    [
	    OTBR_AGENT_USER=root
    ])

AC_MSG_CHECKING([Agent user])
AC_MSG_RESULT(${OTBR_AGENT_USER})
AC_SUBST(OTBR_AGENT_USER)
AC_DEFINE_UNQUOTED([OTBR_AGENT_USER],[${OTBR_AGENT_USER}],[Define the user to run otbr-agent service])

#
# systemd
#

AC_ARG_WITH([systemdsystemunitdir],
            AS_HELP_STRING([--with-systemdsystemunitdir=DIR], [Path to install systemd unit files.]),
            [], [with_systemdsystemunitdir=$($PKG_CONFIG --variable=systemdsystemunitdir systemd)])
if test -n "${with_systemdsystemunitdir}"; then
  AC_SUBST([systemdsystemunitdir], [${with_systemdsystemunitdir}])
fi
AM_CONDITIONAL(HAVE_SYSTEMD, [test -n "${with_systemdsystemunitdir}"])

#
# Identify the various makefiles and auto-generated files for the package
#
AC_CONFIG_FILES([
Makefile
third_party/Makefile
third_party/Simple-web-server/Makefile
third_party/libcoap/Makefile
third_party/openthread/Makefile
third_party/wpantund/Makefile
third_party/mdl/Makefile
third_party/angular/Makefile
third_party/angular-material/Makefile
include/Makefile
src/Makefile
src/agent/Makefile
src/agent/otbr-agent.conf
src/commissioner/Makefile
src/common/Makefile
src/utils/Makefile
src/web/Makefile
tests/Makefile
tests/mdns/Makefile
tests/tools/Makefile
tests/unit/Makefile
tools/Makefile
doc/Makefile
])

if test "$enable_commissioner" = "yes"; then
    ac_configure_args="$ac_configure_args --disable-examples --disable-documentation"
    AC_CONFIG_SUBDIRS([third_party/libcoap/repo])
fi

#
# Generate the auto-generated files for the package
#
AC_OUTPUT

#
# Summarize the package configuration
#

AC_MSG_NOTICE([

  Configuration Summary
  ---------------------
  Package                                   : ${PACKAGE_NAME}
  Version                                   : ${PACKAGE_VERSION}
  Interface                                 : ${LIBOTBR_VERSION_INFO//:/.}
  Build system                              : ${build}
  Host system                               : ${host}
  Target system                             : ${target}
  Target architecture                       : ${target_cpu}
  Target OS                                 : ${target_os}
  Cross compiling                           : ${cross_compiling}
  Build shared libraries                    : ${enable_shared}
  Build static libraries                    : ${enable_static}
  Build debug libraries                     : ${nl_cv_build_debug}
  Build optimized libraries                 : ${nl_cv_build_optimized}
  Build coverage libraries                  : ${nl_cv_build_coverage}
  Build coverage reports                    : ${nl_cv_build_coverage_reports}
  MDNS implementation                       : ${with_mdns}
  Commissioner                              : ${enable_commissioner}
  Web service                               : ${enable_web_service}
  Lcov                                      : ${LCOV:--}
  Genhtml                                   : ${GENHTML:--}
  Build tests                               : ${nl_cv_build_tests}
  Prefix                                    : ${prefix}
  Shadow directory program                  : ${LNDIR}
  Documentation support                     : ${nl_cv_build_docs}
  Doxygen                                   : ${DOXYGEN:--}
  GraphViz dot                              : ${DOT:--}
  PERL                                      : ${PERL:--}
  C Preprocessor                            : ${CPP}
  C Compiler                                : ${CC}
  C++ Preprocessor                          : ${CXXCPP}
  C++ Compiler                              : ${CXX}
  Archiver                                  : ${AR}
  Archive Indexer                           : ${RANLIB}
  Symbol Stripper                           : ${STRIP}
  Object Copier                             : ${OBJCOPY}
  C Preprocessor flags                      : ${CPPFLAGS:--}
  C Compile flags                           : ${CFLAGS:--}
  C++ Compile flags                         : ${CXXFLAGS:--}
  Link flags                                : ${LDFLAGS:--}
  Link libraries                            : ${LIBS}

])
