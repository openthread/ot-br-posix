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

include $(top_srcdir)/third_party/openthread/openthread.mk
include $(top_srcdir)/third_party/openthread/mbedtls.mk

sbin_PROGRAMS=otbr-agent

otbr_agent_SOURCES = \
    main.cpp         \
    $(NULL)

otbr_agent_LDADD     = \
    libotbr-agent.la   \
    $(NULL)

if OTBR_ENABLE_NCP_OPENTHREAD
otbr_agent_LDADD    += \
    $(OPENTHREAD_LIBS) \
    $(MBEDTLS_LIBS)    \
    -lutil             \
    $(NULL)
endif

otbr_agent_LDADD    += \
    $(NULL)

otbr_agent_LDFLAGS = \
    -static          \
    $(NULL)

otbr_agent_CPPFLAGS         = \
    -I$(top_srcdir)/src       \
    $(OPENTHREAD_CPPFLAGS)    \
    $(NULL)

noinst_LTLIBRARIES = libotbr-agent.la

libotbr_agent_la_CXXFLAGS                                     = \
    -fno-exceptions
    $(NULL)

libotbr_agent_la_SOURCES                                      = \
    agent_instance.cpp                                          \
    border_agent.cpp                                            \
    $(NULL)

if OTBR_ENABLE_NCP_OPENTHREAD
libotbr_agent_la_SOURCES                                     += \
    ncp_openthread.cpp                                          \
    $(NULL)
endif

if OTBR_ENABLE_NCP_WPANTUND
libotbr_agent_la_SOURCES                                     += \
    ncp_wpantund.cpp                                            \
    $(NULL)
endif

libotbr_agent_la_LIBADD                                       = \
    $(top_builddir)/src/common/libotbr-logging.la               \
    $(top_builddir)/src/common/libotbr-event-emitter.la         \
    $(top_builddir)/src/utils/libutils.la                       \
    $(NULL)

if OTBR_ENABLE_NCP_WPANTUND
libotbr_agent_la_LIBADD                                      += \
    $(top_builddir)/third_party/wpantund/libwpanctl.la          \
    $(DBUS_LIBS)                                                \
    $(NULL)
endif

libotbr_agent_la_LDFLAGS = \
    -static                \
    $(NULL)

libotbr_agent_la_CPPFLAGS                                                  = \
    -I$(top_srcdir)/src                                                      \
    -I$(top_srcdir)/third_party/wpantund/repo/src                            \
    -I$(top_srcdir)/third_party/wpantund/repo/src                            \
    -I$(top_srcdir)/third_party/wpantund/repo/src/ipc-dbus                   \
    -I$(top_srcdir)/third_party/wpantund/repo/src/wpanctl                    \
    -I$(top_srcdir)/third_party/wpantund/repo/src/wpantund                   \
    -I$(top_srcdir)/third_party/wpantund/repo/third_party/openthread/src/ncp \
    $(DBUS_CFLAGS)                                                           \
    $(OPENTHREAD_CPPFLAGS)                                                   \
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
    mdns.hpp            \
    mdns_avahi.hpp      \
    mdns_mdnssd.hpp     \
    ncp.hpp             \
    ncp_openthread.hpp  \
    ncp_wpantund.hpp    \
    uris.hpp            \
    $(NULL)

EXTRA_DIST                = \
    otbr-agent.conf         \
    otbr-agent.default      \
    otbr-agent.service.in   \
    otbr-agent.init.in      \
    $(NULL)

dbusconfdir = $(DBUS_CONFDIR)
dbusconf_DATA = otbr-agent.conf

if HAVE_SYSTEMD
systemdsystemunit_DATA              = \
    otbr-agent.service                \
    $(NULL)
endif

initddir=$(sysconfdir)/init.d

defaultdir=$(sysconfdir)/default

install-data-local: $(DESTDIR)$(defaultdir)/otbr-agent $(DESTDIR)$(initddir)/otbr-agent

if HAVE_SYSTEMD
uninstall-local:
	-rm $(DESTDIR)$(defaultdir)/otbr-agent
else
uninstall-local:
	-rm $(DESTDIR)$(defaultdir)/otbr-agent
	-rm $(DESTDIR)$(initddir)/otbr-agent
endif

# Do not overwrite existing configuration file
$(DESTDIR)$(defaultdir)/otbr-agent: otbr-agent.default
	test -d $(DESTDIR)$(defaultdir) || mkdir -p $(DESTDIR)$(defaultdir)
	$(INSTALL_DATA) $(srcdir)/otbr-agent.default $(DESTDIR)$(defaultdir)/otbr-agent

if HAVE_SYSTEMD
$(DESTDIR)$(initddir)/otbr-agent: ;
else
$(DESTDIR)$(initddir)/otbr-agent: otbr-agent.init
	test -d $(DESTDIR)$(initddir) || mkdir -p $(DESTDIR)$(initddir)
	$(INSTALL) -m 755 ./otbr-agent.init $(DESTDIR)$(initddir)/otbr-agent
endif

if HAVE_SYSTEMD
.PHONY: $(systemdsystemunit_DATA)
$(systemdsystemunit_DATA): %: %.in
	$(SED)                                  \
	-e 's,[@]sbindir[@],$(sbindir),g'       \
	-e 's,[@]sysconfdir[@],$(sysconfdir),g' \
	< "$<" > "$@"
else
.PHONY: otbr-agent.init
otbr-agent.init: %: %.in
	$(SED)                                  \
	-e 's,[@]sbindir[@],$(sbindir),g'       \
	-e 's,[@]sysconfdir[@],$(sysconfdir),g' \
	< "$<" > "$@"
endif

CLEANFILES                = \
    otbr-agent.service      \
    otbr-agent.init         \
    $(NULL)

include $(abs_top_nlbuild_autotools_dir)/automake/post.am
