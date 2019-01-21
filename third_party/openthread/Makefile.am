#
#  Copyright (c) 2019, The OpenThread Authors.
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
include $(top_srcdir)/third_party/openthread/openthread.mk

lib_LTLIBRARIES=libmbedcrypto.la
override CFLAGS:=$(filter-out -Wconversion,$(CFLAGS)) -Wno-overlength-strings

libmbedcrypto_la_SOURCES                                         = \
    repo/third_party/mbedtls/repo/library/aes.c                    \
    repo/third_party/mbedtls/repo/library/asn1parse.c              \
    repo/third_party/mbedtls/repo/library/asn1write.c              \
    repo/third_party/mbedtls/repo/library/base64.c                 \
    repo/third_party/mbedtls/repo/library/bignum.c                 \
    repo/third_party/mbedtls/repo/library/ccm.c                    \
    repo/third_party/mbedtls/repo/library/cipher.c                 \
    repo/third_party/mbedtls/repo/library/cipher_wrap.c            \
    repo/third_party/mbedtls/repo/library/cmac.c                   \
    repo/third_party/mbedtls/repo/library/ctr_drbg.c               \
    repo/third_party/mbedtls/repo/library/debug.c                  \
    repo/third_party/mbedtls/repo/library/ecdh.c                   \
    repo/third_party/mbedtls/repo/library/ecdsa.c                  \
    repo/third_party/mbedtls/repo/library/ecjpake.c                \
    repo/third_party/mbedtls/repo/library/ecp.c                    \
    repo/third_party/mbedtls/repo/library/ecp_curves.c             \
    repo/third_party/mbedtls/repo/library/entropy.c                \
    repo/third_party/mbedtls/repo/library/entropy_poll.c           \
    repo/third_party/mbedtls/repo/library/md.c                     \
    repo/third_party/mbedtls/repo/library/md_wrap.c                \
    repo/third_party/mbedtls/repo/library/memory_buffer_alloc.c    \
    repo/third_party/mbedtls/repo/library/oid.c                    \
    repo/third_party/mbedtls/repo/library/pem.c                    \
    repo/third_party/mbedtls/repo/library/pk.c                     \
    repo/third_party/mbedtls/repo/library/pk_wrap.c                \
    repo/third_party/mbedtls/repo/library/pkparse.c                \
    repo/third_party/mbedtls/repo/library/platform.c               \
    repo/third_party/mbedtls/repo/library/platform_util.c          \
    repo/third_party/mbedtls/repo/library/sha256.c                 \
    repo/third_party/mbedtls/repo/library/ssl_cookie.c             \
    repo/third_party/mbedtls/repo/library/ssl_ciphersuites.c       \
    repo/third_party/mbedtls/repo/library/ssl_cli.c                \
    repo/third_party/mbedtls/repo/library/ssl_srv.c                \
    repo/third_party/mbedtls/repo/library/ssl_ticket.c             \
    repo/third_party/mbedtls/repo/library/ssl_tls.c                \
    repo/third_party/mbedtls/repo/library/net_sockets.c            \
    repo/third_party/mbedtls/repo/library/timing.c                 \
    repo/third_party/mbedtls/repo/library/x509.c                   \
    repo/third_party/mbedtls/repo/library/x509_crt.c               \
    $(NULL)

libmbedcrypto_la_CPPFLAGS                                        = \
    $(MBEDTLS_CPPFLAGS)                                            \
    $(NULL)

EXTRA_DIST                                                       = \
    mbedtls-config.h                                               \
    mbedtls.mk                                                     \
    openthread.mk                                                  \
    repo                                                           \
    $(NULL)

BUILT_SOURCES                                                    = \
    $(OPENTHREAD_CONFIG_FILE)                                      \
    $(NULL)

CLEANFILES                                                       = \
    $(OPENTHREAD_CONFIG_FILE)                                      \
    $(NULL)

clean-local: clean-local-openthread

include $(abs_top_nlbuild_autotools_dir)/automake/post.am
