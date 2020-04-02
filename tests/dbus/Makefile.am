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

include $(abs_top_nlbuild_autotools_dir)/automake/pre.am

if OTBR_ENABLE_DBUS_SERVER
check_PROGRAMS = otbr-test-dbus-server

if OTBR_ENABLE_NCP_OPENTHREAD
check_PROGRAMS += otbr-test-dbus-client
endif

otbr_test_dbus_server_SOURCES = \
    test_dbus_server.cpp \
    $(NULL)

otbr_test_dbus_server_CXXFLAGS = \
    -fno-exceptions \
    $(NULL)

otbr_test_dbus_server_CPPFLAGS = \
    -I$(top_srcdir) \
    -I$(top_srcdir)/include \
    -I$(top_srcdir)/src \
    -I$(top_srcdir)/third_party/openthread/repo/src/ \
    -I$(top_srcdir)/third_party/openthread/repo/include/ \
    $(DBUS_CFLAGS) \
    $(NULL)

otbr_test_dbus_server_LDADD = \
    $(top_builddir)/src/dbus/server/libotbr-dbus-server.la \
    $(top_builddir)/src/common/libotbr-logging.la \
    $(NULL)

otbr_test_dbus_server_LDFLAGS = \
    -static \
    $(NULL)

if OTBR_ENABLE_NCP_OPENTHREAD
otbr_test_dbus_client_SOURCES = \
    test_dbus_client.cpp \
    $(NULL)

otbr_test_dbus_client_CXXFLAGS = \
    -fno-exceptions \
    $(NULL)

otbr_test_dbus_client_CPPFLAGS = \
    -I$(top_srcdir) \
    -I$(top_srcdir)/include \
    -I$(top_srcdir)/src \
    $(DBUS_CFLAGS) \
    $(NULL)

otbr_test_dbus_client_LDADD = \
    $(top_builddir)/src/dbus/client/libotbr-dbus-client.la \
    $(NULL)

otbr_test_dbus_client_LDFLAGS = \
    -static \
    $(NULL)
endif# if OTBR_ENABLE_NCP_OPENTHREAD

dist_check_SCRIPTS = \
    test-server \
    $(NULL)

EXTRA_DIST                = \
    otbr-test-dbus-server.conf \
    $(NULL)

TESTS = \
    test-server \
    $(NULL)

TESTS_ENVIRONMENT = \
    export \
    srcdir='$(srcdir)' \
    VERBOSE=1; \
    $(NULL)
endif# if OTBR_ENABLE_DBUS_SERVER
include $(abs_top_nlbuild_autotools_dir)/automake/post.am
