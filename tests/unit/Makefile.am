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

check_PROGRAMS = unittest

unittest_SOURCES           = \
    main.cpp                 \
    test_coap.cpp            \
    test_event_emitter.cpp   \
    test_pskc.cpp            \
    test_logging.cpp         \
    $(NULL)

if OTBR_ENABLE_MDNS_MDNSSD
unittest_SOURCES          += \
    test_mdns_mdnssd.cpp     \
    $(NULL)
endif

unittest_CPPFLAGS                                             = \
    -I$(top_srcdir)/src                                         \
    -I$(top_srcdir)/src/agent                                   \
    -I$(top_srcdir)/src/web                                     \
    -I$(top_srcdir)/third_party/mbedtls/repo/include            \
    $(MBEDTLS_CPPFLAGS)                                                      \
    $(NULL)

unittest_LDADD                                                = \
    $(top_builddir)/src/agent/libotbr-agent.la                  \
    $(top_builddir)/src/common/libotbr-event-emitter.la         \
    $(top_builddir)/src/common/libotbr-logging.la               \
    $(top_builddir)/src/web/libotbr-web.la                      \
    $(NULL)

unittest_LDFLAGS             = \
    -lCppUTest                 \
    -lCppUTestExt              \
    -static                    \
    $(NULL)

TESTS = unittest

include $(abs_top_nlbuild_autotools_dir)/automake/post.am
