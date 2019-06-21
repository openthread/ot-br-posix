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

noinst_HEADERS                                        = \
    code_utils.hpp                                      \
    coap.hpp                                            \
    coap_libcoap.hpp                                    \
    dtls.hpp                                            \
    dtls_mbedtls.hpp                                    \
    event_emitter.hpp                                   \
    libcoap.h                                           \
    mainloop.h                                          \
    time.hpp                                            \
    tlv.hpp                                             \
    types.hpp                                           \
    logging.hpp                                         \
    $(NULL)

noinst_LTLIBRARIES                                    = \
    libotbr-dtls.la                                     \
    libotbr-event-emitter.la                            \
    libotbr-logging.la                                  \
    $(NULL)

if OTBR_ENABLE_COMMISSIONER
noinst_LTLIBRARIES                                   += \
    libotbr-coap.la                                     \
    $(NULL)
endif

libotbr_logging_la_SOURCES =                            \
    logging.cpp                                         \
    $(NULL)

libotbr_coap_la_SOURCES                               = \
    coap_libcoap.cpp                                    \
    $(NULL)

libotbr_coap_la_CPPFLAGS                              = \
    -I$(top_builddir)/third_party/libcoap/repo          \
    -I$(top_srcdir)/third_party/libcoap/repo            \
    -I$(top_srcdir)/third_party/libcoap/repo/include    \
    $(NULL)

libotbr_coap_la_LIBADD                                      = \
    $(top_builddir)/third_party/libcoap/repo/libcoap-1.la     \
    $(NULL)

libotbr_dtls_la_SOURCES                               = \
    dtls_mbedtls.cpp                                    \
    $(NULL)

libotbr_dtls_la_CPPFLAGS                              = \
    $(MBEDTLS_CPPFLAGS)                                 \
    $(NULL)

libotbr_dtls_la_LIBADD                                = \
    $(MBEDTLS_LIBS)                                     \
    $(NULL)

libotbr_event_emitter_la_SOURCES                      = \
    event_emitter.cpp                                   \
    $(NULL)

include $(abs_top_nlbuild_autotools_dir)/automake/post.am
