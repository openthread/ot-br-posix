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
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/ipc-dbus \
	-I$(top_srcdir)/src/wpantund \
	-I$(top_srcdir)/src/util \
	-I$(top_srcdir)/third_party/assert-macros \
	$(NULL)

DISTCLEANFILES = .deps Makefile

if BUILD_CONNMAN_PLUGIN

connman_plugindir = ${libdir}/connman/plugins

connman_plugin_LTLIBRARIES = wpan-tunnel-plugin.la

wpan_tunnel_plugin_la_SOURCES = \
	wpan-connman-plugin.c \
	../util/string-utils.c \
	$(NULL)

wpan_tunnel_plugin_la_CFLAGS = \
	$(CONNMAN_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(NULL)

wpan_tunnel_plugin_la_LDFLAGS = \
	$(CONNMAN_LIBS) \
	-shared \
	-module \
	-avoid-version \
	-Wl,--allow-shlib-undefined \
	$(NULL)

endif
