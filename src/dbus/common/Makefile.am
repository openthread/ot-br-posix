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
include $(top_srcdir)/third_party/openthread/openthread.mk

noinst_LTLIBRARIES = libotbr-dbus-common.la

libotbr_dbus_common_la_SOURCES       = \
    dbus_message_helper.cpp            \
    error.cpp                          \
    error_helper.cpp                   \
    dbus_message_helper_openthread.cpp \
    $(NULL)

libotbr_dbus_common_la_LIBADD                              = \
    $(top_builddir)/src/utils/libutils.la                    \
    $(DBUS_LIBS)                                             \
    $(NULL)

libotbr_dbus_common_la_LDFLAGS  = \
    -static                       \
    $(NULL)

libotbr_dbus_common_la_CPPFLAGS                           = \
    -I$(top_srcdir)/include                                 \
    -I$(top_srcdir)/src                                     \
    -I$(top_srcdir)/third_party/openthread/repo/include/    \
    -I$(top_srcdir)/third_party/openthread/repo/src/        \
    $(DBUS_CFLAGS)                                          \
		$(OPENTHREAD_CPPFLAGS)                                  \
    $(NULL)

libotbr_dbus_common_la_CXXFLAGS  = \
    -fno-exceptions                \
    $(NULL)

noinst_HEADERS            = \
    constants.hpp           \
    dbus_message_helper.hpp \
    dbus_resources.hpp      \
    error.hpp               \
    error_helper.hpp        \
    types.hpp               \
    $(NULL)

include $(abs_top_nlbuild_autotools_dir)/automake/post.am
