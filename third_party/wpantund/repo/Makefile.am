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

AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4
DIST_TARGETS = dist-gzip dist-bzip2
DIST_ARCHIVES = $(distdir).tar.gz $(distdir).tar.bz2
AM_DISTCHECK_CONFIGURE_FLAGS = --enable-all-restricted-plugins

@CODE_COVERAGE_RULES@

SUBDIRS =                              \
	src                                \
	doc                                \
	third_party                        \
	$(NULL)

DISTCLEANFILES =                       \
	config.log                         \
	config.status                      \
	Makefile                           \
	libtool                            \
	make.out                           \
	$(distdir).tar.*                   \
	$(NULL)

EXTRA_DIST =                           \
	.default-version                   \
	.gitignore                         \
	.gitattributes                     \
	.uncrustify.cfg                    \
	bootstrap.sh                       \
	README.md                          \
	TODO.md                            \
	HACKING.md                         \
	LICENSE                            \
	CHANGELOG                          \
	CONTRIBUTING.md                    \
	INSTALL.md                         \
	doxygen.cfg.in                     \
	etc/Dockerfile                     \
	etc/build-in-docker.sh             \
	etc/run-in-docker.sh               \
	etc/wpantund.rb                    \
	etc/autoandr/autoandr              \
	m4/nl.m4                           \
	$(NULL)



HASH_VERSION = $(shell                                                   \
	git describe --dirty --match __poison__ --always 2> /dev/null        \
)

EXTRA_VERSION = $(shell                                                  \
	git describe --all --contains 54e116add1f093663e047f88dac18b4851589483 1> /dev/null 2> /dev/null \
		&& (test "`git describe 2> /dev/null | sed 's:^full/::'`" = $(VERSION) || echo -g$(HASH_VERSION))  \
)

distdir = $(PACKAGE)-$(VERSION)$(EXTRA_VERSION)
