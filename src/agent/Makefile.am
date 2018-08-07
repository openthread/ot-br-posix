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

sbin_PROGRAMS=otbr-agent

otbr_agent_SOURCES = \
    main.cpp         \
    $(NULL)

otbr_agent_LDADD     = \
    libotbr-agent.la   \
    $(NULL)

otbr_agent_LDFLAGS = \
    -static          \
    $(NULL)

otbr_agent_CPPFLAGS    = \
    -I$(top_srcdir)/src  \
    $(NULL)

noinst_LTLIBRARIES = libotbr-agent.la

libotbr_agent_la_CXXFLAGS                                     = \
    -fno-exceptions
    $(NULL)

libotbr_agent_la_SOURCES                                      = \
    agent_instance.cpp                                          \
    border_agent.cpp                                            \
    coap_libcoap.cpp                                            \
    dtls_mbedtls.cpp                                            \
    ncp_wpantund.cpp                                            \
    $(NULL)

libotbr_agent_la_LIBADD                                       = \
    $(top_builddir)/third_party/libcoap/repo/libcoap-1.la       \
    $(top_builddir)/third_party/mbedtls/libmbedtls.la           \
    $(top_builddir)/third_party/wpantund/libwpanctl.la          \
    $(top_builddir)/src/common/libotbr-logging.la               \
    $(top_builddir)/src/common/libotbr-event-emitter.la         \
    $(top_builddir)/src/utils/libutils.la                       \
    $(DBUS_LIBS)                                                \
    $(NULL)

libotbr_agent_la_LDFLAGS = \
    -static                \
    $(NULL)

libotbr_agent_la_CPPFLAGS                                                  = \
    -DMBEDTLS_CONFIG_FILE='<config-thread.h>'                                \
    -I$(top_srcdir)/third_party/mbedtls/repo/configs                         \
    -I$(top_srcdir)/third_party/mbedtls/repo/include                         \
    -I$(top_builddir)/third_party/libcoap/repo                               \
    -I$(top_srcdir)/src                                                      \
    -I$(top_srcdir)/third_party/libcoap/repo                                 \
    -I$(top_srcdir)/third_party/libcoap/repo/include                         \
    -I$(top_srcdir)/third_party/wpantund/repo/src                            \
    -I$(top_srcdir)/third_party/wpantund/repo/src                            \
    -I$(top_srcdir)/third_party/wpantund/repo/src/ipc-dbus                   \
    -I$(top_srcdir)/third_party/wpantund/repo/src/wpanctl                    \
    -I$(top_srcdir)/third_party/wpantund/repo/src/wpantund                   \
    -I$(top_srcdir)/third_party/wpantund/repo/third_party/openthread/src/ncp \
    $(DBUS_CFLAGS)                                                           \
    $(NULL)

if OTBR_ENABLE_MDNS_AVAHI
libotbr_agent_la_SOURCES                                     += \
    mdns_avahi.cpp                                              \
    $(NULL)

libotbr_agent_la_LDFLAGS                                     += \
    -lavahi-common                                              \
    -lavahi-client                                              \
    $(NULL)
endif

if OTBR_ENABLE_MDNS_MDNSSD
libotbr_agent_la_SOURCES                                     += \
    mdns_mdnssd.cpp                                             \
    $(NULL)

libotbr_agent_la_LDFLAGS                                     += \
    -ldns_sd                                                    \
    $(NULL)
endif

noinst_HEADERS        = \
    agent_instance.hpp  \
    border_agent.hpp    \
    coap.hpp            \
    coap_libcoap.hpp    \
    dtls.hpp            \
    dtls_mbedtls.hpp    \
    mdns.hpp            \
    mdns_avahi.hpp      \
    mdns_mdnssd.hpp     \
    ncp.hpp             \
    ncp_wpantund.hpp    \
    libcoap.h           \
    uris.hpp            \
    $(NULL)

EXTRA_DIST                = \
    otbr-agent.conf         \
    otbr-agent.default      \
    otbr-agent.service.in   \
    $(NULL)

dbusconfdir = $(DBUS_CONFDIR)
dbusconf_DATA = otbr-agent.conf

systemddir=$(sysconfdir)/systemd/system
systemd_DATA                        = \
    otbr-agent.service                \
    $(NULL)

defaultdir=$(sysconfdir)/default

install-data-local: $(DESTDIR)$(defaultdir)/otbr-agent

# Do not overwrite existing configuration file
$(DESTDIR)$(defaultdir)/otbr-agent: otbr-agent.default
	test -d $(DESTDIR)$(defaultdir) || mkdir -p $(DESTDIR)$(defaultdir)
	$(INSTALL_DATA) $(srcdir)/otbr-agent.default $(DESTDIR)$(defaultdir)/otbr-agent

uninstall-local:
	-rm $(DESTDIR)$(defaultdir)/otbr-agent

.PHONY: $(systemd_DATA)
$(systemd_DATA): %: %.in
	$(SED)                                  \
	-e 's,[@]sbindir[@],$(sbindir),g'       \
	-e 's,[@]sysconfdir[@],$(sysconfdir),g' \
	< "$<" > "$@"

CLEANFILES                = \
    otbr-agent.service      \
    $(NULL)

include $(abs_top_nlbuild_autotools_dir)/automake/post.am
