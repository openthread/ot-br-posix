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

include $(abs_top_nlbuild_autotools_dir)/automake/pre.am

include $(top_srcdir)/third_party/openthread/mbedtls.mk

noinst_LTLIBRARIES = libutils.la

libutils_la_SOURCES = \
    crc16.cpp         \
    hex.cpp           \
    pskc.cpp          \
    steering_data.cpp \
    strcpy_utils.cpp  \
    $(NULL)

libutils_la_CPPFLAGS                                    = \
    -I$(top_srcdir)/src                                   \
    -I$(top_srcdir)/third_party/mbedtls/repo/include      \
    $(MBEDTLS_CPPFLAGS)                                   \
    $(NULL)

libutils_la_LIBADD                                      = \
    $(MBEDTLS_LIBS)                                       \
    $(top_builddir)/src/common/libotbr-logging.la         \
    $(NULL)

noinst_HEADERS      = \
    crc16.hpp         \
    hex.hpp           \
    pskc.hpp          \
    steering_data.hpp \
    strcpy_utils.hpp  \
    $(NULL)

include $(abs_top_nlbuild_autotools_dir)/automake/post.am
