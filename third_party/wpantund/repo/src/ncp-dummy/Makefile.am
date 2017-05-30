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
	-I$(top_srcdir)/src/util \
	-I$(top_srcdir)/src/ncp-dummy \
	-I$(top_srcdir)/src/wpantund \
	-I$(top_srcdir)/third_party/pt \
	-I$(top_srcdir)/third_party/assert-macros \
	$(NULL)

DISTCLEANFILES = \
	.deps \
	Makefile \
	$(NULL)

if BUILD_PLUGIN_NCP_DUMMY

# Work-around the omnipotent automake nanny-state.
mypkglibexecdir = $(pkglibexecdir)

NCP_SOURCES = \
	DummyNCPControlInterface.cpp \
	DummyNCPControlInterface.h \
	DummyNCPInstance.cpp \
	DummyNCPInstance.h \
	$(NULL)

if STATIC_LINK_NCP_PLUGIN
noinst_LTLIBRARIES = libncp-dummy.la
libncp_dummy_la_SOURCES = $(NCP_SOURCES)
libncp_dummy_la_CXXFLAGS = @BOOST_CXXFLAGS@
else
mypkglibexec_LTLIBRARIES = ncp-dummy.la
ncp_dummy_la_LDFLAGS = \
	-module \
	-avoid-version \
	-shared \
	$(NULL)
ncp_dummy_la_SOURCES = $(NCP_SOURCES)
ncp_dummy_la_CXXFLAGS = @BOOST_CXXFLAGS@
endif

endif #BUILD_PLUGIN_NCP_dummy
