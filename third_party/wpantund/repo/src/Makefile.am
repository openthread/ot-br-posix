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

SUBDIRS =                \
	missing              \
	util                 \
	wpanctl              \
	ipc-dbus             \
	connman-plugin       \
	@PLUGIN_SUBDIRS@     \
	wpantund             \
	scripts              \
	$(NULL)

DISTCLEANFILES =         \
	.deps                \
	Makefile             \
	$(NULL)

EXTRA_DIST =             \
	version.c.in         \
	version.h            \
	$(NULL)

SOURCE_VERSION=$(shell                                            \
	git describe --dirty --always --match "[0-9].*" 2> /dev/null  \
)

CLEANFILES    = $(top_builddir)/$(subdir)/version.c
BUILT_SOURCES = $(top_builddir)/$(subdir)/version.c
.INTERMEDIATE:  $(top_builddir)/$(subdir)/version.c

$(top_builddir)/$(subdir)/version.c: version.c.in Makefile
	sed 's/SOURCE_VERSION/"$(SOURCE_VERSION)"/' < $< > $@
