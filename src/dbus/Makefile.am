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

noinst_LTLIBRARIES = libotbr-dbus.la

bin_PROGRAMS = dbus-server-test

dbus_server_test_SOURCES  = \
    dbus_server_test.cpp    \
    $(NULL)

dbus_server_test_CXXFLAGS = \
    -fno-exceptions         \
    $(NULL)

dbus_server_test_CPPFLAGS = \
    -I$(top_srcdir)/src     \
    -I$(top_srcdir)         \
    $(DBUS_CFLAGS)          \
    $(NULL)

dbus_server_test_LDADD    = \
    libotbr-dbus.la         \
    $(NULL)

dbus_server_test_LDFLAGS  = \
    -static                 \
    $(NULL)

libotbr_dbus_la_CXXFLAGS          = \
    -fno-exceptions                 \
    -Wno-missing-field-initializers \
    -Wno-unused-parameter           \
    $(NULL)

libotbr_dbus_la_SOURCES   = \
    dbus_object.cpp         \
    dbus_message_helper.cpp \
    $(NULL)

libotbr_dbus_la_CPPFLAGS  = \
    -I$(top_srcdir)/src     \
    -I$(top_srcdir)         \
    $(DBUS_CFLAGS)          \
    $(NULL)

libotbr_dbus_la_LIBADD    = \
    $(DBUS_LIBS)            \
    $(NULL)

noinst_HEADERS            = \
    dbus_object.hpp         \
    dbus_request.hpp        \
    dbus_resources.hpp      \
    dbus_message_helper.hpp \
    $(NULL)

include $(abs_top_nlbuild_autotools_dir)/automake/post.am
