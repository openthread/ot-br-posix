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

EXTRA_DIST                         = \
    wpantund.service.in              \
    wpantund.init                    \
    $(NULL)

if OTBR_ENABLE_NCP_WPANTUND
noinst_LTLIBRARIES = libwpanctl.la
endif

# Disable Werror for third_party

override CFLAGS                   := $(filter-out -pedantic-errors -Werror,$(CFLAGS))
override CXXFLAGS                 := $(filter-out -Werror,$(CXXFLAGS))

libwpanctl_la_SOURCES              = \
    repo/src/util/string-utils.c     \
    repo/src/wpanctl/wpanctl-utils.c \
    $(NULL)

libwpanctl_la_LIBADD               = \
    $(DBUS_LIBS)                     \
    $(NULL)

libwpanctl_la_CPPFLAGS                                                     = \
    -I$(srcdir)                                                              \
    -I$(top_srcdir)/third_party/wpantund/repo/src                            \
    -I$(top_srcdir)/third_party/wpantund/repo/src/ipc-dbus                   \
    -I$(top_srcdir)/third_party/wpantund/repo/src/util                       \
    -I$(top_srcdir)/third_party/wpantund/repo/src/wpantund                   \
    -I$(top_srcdir)/third_party/wpantund/repo/third_party/assert-macros      \
    -I$(top_srcdir)/third_party/wpantund/repo/third_party/openthread/src/ncp \
    $(DBUS_CFLAGS)                                                           \
    $(NULL)

noinst_HEADERS                                     = \
    config.h                                         \
	repo/src/ipc-dbus/wpan-dbus-v0.h                 \
	repo/src/ipc-dbus/wpan-dbus-v1.h                 \
	repo/src/util/string-utils.h                     \
	repo/src/wpanctl/wpanctl-utils.h                 \
	repo/src/wpantund/wpan-error.h                   \
	repo/src/wpantund/wpan-properties.h              \
    repo/third_party/assert-macros/assert-macros.h   \
    repo/third_party/openthread/src/ncp/spinel.h     \
    $(NULL)

if HAVE_SYSTEMD
systemdsystemunit_DATA              = \
    wpantund.service                  \
    $(NULL)

.PHONY: $(systemdsystemunit_DATA)
$(systemdsystemunit_DATA): %: %.in
	$(SED)                                  \
	-e 's,[@]sbindir[@],$(sbindir),g'       \
	-e 's,[@]sysconfdir[@],$(sysconfdir),g' \
	< "$<" > "$@"

else

initddir=$(sysconfdir)/init.d

install-data-local: $(DESTDIR)$(initddir)/wpantund

uninstall-local:
	-rm $(DESTDIR)$(initddir)/wpantund

$(DESTDIR)$(initddir)/wpantund:
	test -d $(DESTDIR)$(initddir) || mkdir -p $(DESTDIR)$(initddir)
	$(INSTALL) -m 755 $(srcdir)/wpantund.init $(DESTDIR)$(initddir)/wpantund

endif

CLEANFILES                = \
    wpantund.service        \
    $(NULL)

include $(abs_top_nlbuild_autotools_dir)/automake/post.am
