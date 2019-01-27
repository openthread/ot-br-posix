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

OPENTHREAD_CPPFLAGS	= \
    -DOPENTHREAD_PROJECT_CORE_CONFIG_FILE=\"openthread-core-posix-config.h\" \
    $(NULL)

PRIVATE_OPENTHREAD_CPPFLAGS = \
	$(CPPFLAGS) \
    -I$(top_srcdir)/third_party/openthread \
    $(NULL)

OPENTHREAD_CONFIG_FILE = build/posix/otbr/include/openthread-config-generic.h

$(OPENTHREAD_CONFIG_FILE):
	CPPFLAGS="$(PRIVATE_OPENTHREAD_CPPFLAGS)" make -f $(srcdir)/repo/src/posix/Makefile-posix BORDER_AGENT=1 BORDER_ROUTER=1 DISABLE_BUILTIN_MBEDTLS=1 DISABLE_EXECUTABLE=0 JOINER=1 PLATFORM_NETIF=1 PLATFORM_UDP=1 TargetTuple=otbr configure

clean-local-openthread:
	-rm -rf build output
