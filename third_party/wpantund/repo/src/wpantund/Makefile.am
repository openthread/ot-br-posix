#
# Copyright (c) 2016 Nest Labs, Inc.
# All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

AM_CPPFLAGS = \
	$(CPPFLAGS) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/util \
	-I$(top_srcdir)/src/ipc-dbus \
	-I$(top_srcdir)/third_party/fgetln \
	-I$(top_srcdir)/third_party/pt \
	-I$(top_srcdir)/third_party/assert-macros \
	$(MISSING_CPPFLAGS) \
	$(NULL)

@CODE_COVERAGE_RULES@

DISTCLEANFILES = \
	.deps \
	Makefile \
	$(NULL)

EXTRA_DIST = \
	wpantund.conf \
	$(NULL)

pkginclude_HEADERS = \
	wpan-properties.h \
	wpan-error.h \
	$(NULL)

sysconf_DATA = wpantund.conf

sbin_PROGRAMS = wpantund

SOURCES = \
	wpantund.h \
	IPCServer.h \
	NCPConstants.h \
	NCPControlInterface.cpp \
	NCPControlInterface.h \
	NCPInstance.cpp \
	NCPInstance.h \
	NCPInstanceBase.cpp \
	NCPInstanceBase.h \
	NCPMfgInterface_v0.h \
	NCPMfgInterface_v1.h \
	NetworkInstance.h \
	NCPConstants.h \
	FirmwareUpgrade.h \
	FirmwareUpgrade.cpp \
	StatCollector.h \
	StatCollector.cpp \
	RunawayResetBackoffManager.cpp \
	RunawayResetBackoffManager.h \
	NCPInstanceBase-NetInterface.cpp \
	NCPInstanceBase-Addresses.cpp \
	NCPInstanceBase-Prefixes.cpp \
	NCPInstanceBase-AsyncIO.cpp \
	NCPTypes.h \
	NCPTypes.cpp \
	NetworkRetain.h \
	NetworkRetain.cpp \
	Pcap.h \
	Pcap.cpp \
	wpan-error.c \
	../util/IPv6PacketMatcher.cpp \
	../util/IPv6Helpers.cpp \
	../util/tunnel.c \
	../util/netif-mgmt.c \
	../util/config-file.c \
	../util/socket-utils.c \
	../util/any-to.cpp \
	../util/string-utils.c \
	../util/time-utils.c \
	../util/nlpt-select.c \
	../util/Data.cpp \
	../util/SocketWrapper.cpp \
	../util/SocketAdapter.cpp \
	../util/UnixSocket.cpp \
	../util/SuperSocket.cpp \
	../util/EventHandler.cpp \
	../util/TunnelIPv6Interface.cpp \
	../util/ValueMap.cpp \
	../util/Timer.cpp \
	../util/sec-random.c \
	$(NULL)

SOURCE_VERSION=$(shell                                            \
	git describe --dirty --always --match "[0-9].*" 2> /dev/null  \
)

BUILT_SOURCES  = $(top_builddir)/$(subdir)/version.c
CLEANFILES     = $(top_builddir)/$(subdir)/version.c
.INTERMEDIATE:   wpantund-version.$(OBJEXT)

$(top_builddir)/$(subdir)/version.c: ../version.c.in Makefile
	sed 's/SOURCE_VERSION/"$(SOURCE_VERSION)"/' < $< > $@

SOURCES += $(top_builddir)/$(subdir)/version.c


wpantund_SOURCES = wpantund.cpp $(SOURCES)

wpantund_LDADD = $(MISSING_LIBADD) $(DBUS_LIBS)
wpantund_LDADD += ../ipc-dbus/libwpantund-dbus.la

if STATIC_LINK_NCP_PLUGIN
wpantund_LDADD += ../ncp-@default_ncp_plugin@/libncp-@default_ncp_plugin@.la
else
wpantund_LDADD += $(LIBDL_LIBS)
wpantund_LDFLAGS = @EXPORT_DYNAMIC_LDFLAGS@
endif

wpantund_CPPFLAGS = $(AM_CPPFLAGS) $(DBUS_CFLAGS) $(CODE_COVERAGE_CPPFLAGS)
wpantund_CXXFLAGS = $(AM_CXXFLAGS) $(BOOST_CXXFLAGS) $(CODE_COVERAGE_CXXFLAGS)
wpantund_CFLAGS = $(AM_CFLAGS)

wpantund_CPPFLAGS += $(CODE_COVERAGE_CPPFLAGS)
wpantund_CXXFLAGS += $(CODE_COVERAGE_CXXFLAGS)
wpantund_CFLAGS += $(CODE_COVERAGE_CFLAGS)
wpantund_LDADD += $(CODE_COVERAGE_LIBS)

if ENABLE_FUZZ_TARGETS
noinst_PROGRAMS = wpantund-fuzz
endif

wpantund_fuzz_SOURCES = wpantund-fuzz.cpp $(SOURCES)

wpantund_fuzz_LDADD = $(MISSING_LIBADD) $(DBUS_LIBS)
wpantund_fuzz_LDADD += ../ipc-dbus/libwpantund-dbus-fuzz.la
wpantund_fuzz_LDADD += ../ncp-@default_ncp_plugin@/libncp-@default_ncp_plugin@-fuzz.la
wpantund_fuzz_CPPFLAGS = $(AM_CPPFLAGS) $(DBUS_CFLAGS) $(FUZZ_CPPFLAGS) $(CODE_COVERAGE_CPPFLAGS)
wpantund_fuzz_CXXFLAGS = $(BOOST_CXXFLAGS) $(FUZZ_CXXFLAGS) $(CODE_COVERAGE_CXXFLAGS)
wpantund_fuzz_CFLAGS = $(FUZZ_CFLAGS) $(CODE_COVERAGE_CFLAGS)
wpantund_fuzz_LDADD += $(CODE_COVERAGE_LIBS) $(FUZZ_LIBS)
wpantund_fuzz_LDFLAGS = $(AM_LDFLAGS) $(FUZZ_LDFLAGS)


