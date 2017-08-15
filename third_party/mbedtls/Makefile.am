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

lib_LTLIBRARIES=libmbedtls.la
override CFLAGS:=$(filter-out -Wconversion,$(CFLAGS)) -Wno-overlength-strings

libmbedtls_la_SOURCES                   = \
    repo/library/aes.c                    \
    repo/library/md.c                     \
    repo/library/md_wrap.c                \
    repo/library/memory_buffer_alloc.c    \
    repo/library/platform.c               \
    repo/library/sha256.c                 \
    repo/library/bignum.c                 \
    repo/library/ccm.c                    \
    repo/library/cipher.c                 \
    repo/library/cipher_wrap.c            \
    repo/library/cmac.c                   \
    repo/library/ctr_drbg.c               \
    repo/library/debug.c                  \
    repo/library/ecjpake.c                \
    repo/library/ecp.c                    \
    repo/library/ecp_curves.c             \
    repo/library/entropy.c                \
    repo/library/entropy_poll.c           \
    repo/library/ssl_cookie.c             \
    repo/library/ssl_ciphersuites.c       \
    repo/library/ssl_cli.c                \
    repo/library/ssl_srv.c                \
    repo/library/ssl_ticket.c             \
    repo/library/ssl_tls.c                \
    repo/library/net_sockets.c            \
    repo/library/timing.c                 \
    $(NULL)

libmbedtls_la_CPPFLAGS                        = \
    -I$(srcdir)/repo/include                    \
    -I$(srcdir)/repo/configs                    \
    -D_GNU_SOURCE                               \
    -DMBEDTLS_CONFIG_FILE='<config-thread.h>'   \
    -DMBEDTLS_DEBUG_C                           \
    $(NULL)

noinst_HEADERS                      = \
    repo/configs/config-thread.h      \
    repo/include                      \
    $(NULL)

include $(abs_top_nlbuild_autotools_dir)/automake/post.am
