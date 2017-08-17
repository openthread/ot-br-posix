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

noinst_PROGRAMS = otbr-commissioner

noinst_HEADERS = commissioner.hpp

otbr_commissioner_SOURCES                             = \
    commissioner_argcargv.cpp                           \
    commissioner_compute.cpp                            \
    commissioner.cpp                                    \
    commissioner_selftest.cpp                           \
    commissioner_utils.cpp                              \
    $(NULL)

otbr_commissioner_CPPFLAGS                            = \
    -DMBEDTLS_CONFIG_FILE='<config-thread.h>'           \
    -I$(top_srcdir)/third_party/mbedtls/repo/configs    \
    -I$(top_srcdir)/third_party/mbedtls/repo/include    \
    -I$(top_srcdir)/src                                 \
    -I$(top_srcdir)/src/agent                           \
    -I$(top_srcdir)/src/web                             \
    $(NULL)

otbr_commissioner_LDADD                               = \
    $(top_builddir)/src/agent/libotbr-agent.la          \
    $(top_builddir)/src/utils/libutils.la               \
    $(top_builddir)/src/web/libotbr-web.la              \
    $(top_builddir)/src/common/libotbr-logging.la       \
    $(NULL)

otbr_commissioner_LDFLAGS                             = \
    -static                                             \
    $(NULL)

EXTRA_DIST                                            = \
    meshcop                                             \
    commissioner.hpp                                    \
    commissioner_argcargv.hpp                           \
    $(NULL)

TESTS_ENVIRONMENT                                     = \
    export                                              \
    MAKE=$(MAKE)                                        \
    top_builddir='$(top_builddir)'                      \
    top_srcdir='$(top_srcdir)'                          \
    VERBOSE=1                                           \
    LD_LIBRARY_PATH=$(top_builddir)/third_party/cppcms/repo \
    ;

TESTS = meshcop

include $(abs_top_nlbuild_autotools_dir)/automake/post.am
