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

include $(top_srcdir)/third_party/openthread/mbedtls.mk
include $(top_srcdir)/third_party/openthread/openthread.mk

# Temporary disable -Werror for the included third party server_http

override CXXFLAGS := $(filter-out -Wshadow -Werror,$(CXXFLAGS))

sbin_PROGRAMS = otbr-web

otbr_web_SOURCES                                           = \
    main.cpp                                                 \
    $(NULL)

otbr_web_LDADD                                             = \
    libotbr-web.la                                           \
    $(NULL)

otbr_web_LDFLAGS                                           = \
    -static                                                  \
    $(NULL)

otbr_web_CPPFLAGS                                          = \
    $(libotbr_web_la_CPPFLAGS)                               \
    $(NULL)

noinst_LTLIBRARIES = libotbr-web.la

libotbr_web_la_LIBADD                                           = \
    $(DBUS_LIBS)                                                  \
    -lboost_filesystem                                            \
    -lboost_system                                                \
    -lpthread                                                     \
    -ljsoncpp                                                     \
    $(top_builddir)/src/commissioner/libotbr-commissioner.la      \
    $(top_builddir)/src/agent/libotbr-agent.la                    \
    $(top_builddir)/src/utils/libutils.la                         \
    $(top_builddir)/src/common/libotbr-logging.la                 \
    $(NULL)

libotbr_web_la_SOURCES                                          = \
    web-service/web_server.cpp                                    \
    web-service/wpan_service.cpp                                  \
    $(NULL)


if OTBR_ENABLE_NCP_WPANTUND
libotbr_web_la_SOURCES                                         += \
    wpan-controller/dbus_base.cpp                                 \
    wpan-controller/dbus_join.cpp                                 \
    wpan-controller/dbus_set.cpp                                  \
    wpan-controller/dbus_gateway.cpp                              \
    wpan-controller/dbus_get.cpp                                  \
    wpan-controller/dbus_leave.cpp                                \
    wpan-controller/dbus_form.cpp                                 \
    wpan-controller/dbus_scan.cpp                                 \
    wpan-controller/dbus_ifname.cpp                               \
    wpan-controller/wpan_controller.cpp                           \
    $(NULL)
endif

if OTBR_ENABLE_NCP_OPENTHREAD
libotbr_web_la_SOURCES                                         += \
    web-service/ot_client.cpp                                     \
    $(NULL)
endif

libotbr_web_la_LDFLAGS                                          = \
    -static                                                       \
    $(NULL)

libotbr_web_la_CPPFLAGS                                         = \
    -I$(top_srcdir)/src                                           \
    -I$(top_srcdir)/src/wpan-controller                           \
    -I$(top_srcdir)/src/web-service                               \
    -I$(top_srcdir)/third_party/Simple-web-server/repo/           \
    -I$(top_srcdir)/third_party/wpantund/repo/src                 \
    -I$(top_srcdir)/third_party/wpantund/repo/src/ipc-dbus        \
    -I$(top_srcdir)/third_party/wpantund/repo/src/wpanctl         \
    -I$(top_srcdir)/third_party/wpantund/repo/src/wpantund        \
    -DWEB_FILE_PATH=\"$(datadir)/border-router/frontend\"         \
    $(DBUS_CFLAGS)                                                \
    $(MBEDTLS_CPPFLAGS)                                           \
    $(OPENTHREAD_CPPFLAGS)                                        \
    $(NULL)

jsdir = $(datadir)/border-router/frontend/res/js
js_DATA                                                         = \
    web-service/frontend/res/js/app.js                            \
    $(NULL)

imgdir = $(datadir)/border-router/frontend/res/img
img_DATA                                                        = \
    web-service/frontend/res/img/android-desktop.png              \
    web-service/frontend/res/img/borderrouter.png                 \
    web-service/frontend/res/img/favicon.png                      \
    web-service/frontend/res/img/ios-desktop.png                  \
    web-service/frontend/res/img/openthread_logo.png              \
    web-service/frontend/res/img/icon-info.png                    \
    $(NULL)

cssdir = $(datadir)/border-router/frontend/res/css
css_DATA                                                        = \
    web-service/frontend/res/css/styles.css                       \
    $(NULL)

htmldir = $(datadir)/border-router/frontend
html_DATA                                                      = \
    web-service/frontend/index.html                              \
    web-service/frontend/join.dialog.html                        \
    $(NULL)

noinst_HEADERS                                                 = \
    utils/encoding.hpp                                           \
    web-service/ot_client.hpp                                    \
    web-service/web_server.hpp                                   \
    web-service/wpan_service.hpp                                 \
    wpan-controller/dbus_base.hpp                                \
    wpan-controller/dbus_form.hpp                                \
    wpan-controller/dbus_gateway.hpp                             \
    wpan-controller/dbus_get.hpp                                 \
    wpan-controller/dbus_ifname.hpp                              \
    wpan-controller/dbus_join.hpp                                \
    wpan-controller/dbus_leave.hpp                               \
    wpan-controller/dbus_scan.hpp                                \
    wpan-controller/dbus_set.hpp                                 \
    wpan-controller/wpan_controller.hpp                          \
    $(NULL)

EXTRA_DIST                                                     = \
    $(js_DATA)                                                   \
    $(css_DATA)                                                  \
    $(img_DATA)                                                  \
    $(html_DATA)                                                 \
    otbr-web.service.in                                          \
    otbr-web.init.in                                             \
    $(NULL)

if HAVE_SYSTEMD
systemdsystemunit_DATA              = \
    otbr-web.service                  \
    $(NULL)

.PHONY: $(systemdsystemunit_DATA)
$(systemdsystemunit_DATA): %: %.in
	$(SED)                                  \
	-e 's,[@]sbindir[@],$(sbindir),g'       \
	-e 's,[@]sysconfdir[@],$(sysconfdir),g' \
	< "$<" > "$@"

else

initddir=$(sysconfdir)/init.d

install-data-local: $(DESTDIR)$(initddir)/otbr-web

uninstall-local:
	-rm $(DESTDIR)$(initddir)/otbr-web

$(DESTDIR)$(initddir)/otbr-web: otbr-web.init
	test -d $(DESTDIR)$(initddir) || mkdir -p $(DESTDIR)$(initddir)
	$(INSTALL) -m 755 ./otbr-web.init $(DESTDIR)$(initddir)/otbr-web

.PHONY: obtr-web.init
otbr-web.init: %: %.in
	pwd
	$(SED)                                  \
	-e 's,[@]sbindir[@],$(sbindir),g'       \
	-e 's,[@]sysconfdir[@],$(sysconfdir),g' \
	< "$<" > "$@"
	ls
	ls $(srcdir)

endif

CLEANFILES                = \
    otbr-web.service        \
    otbr-web.init           \
    $(NULL)

include $(abs_top_nlbuild_autotools_dir)/automake/post.am
