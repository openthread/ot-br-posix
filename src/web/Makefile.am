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

# Temporary disable -Werror for the included third party server_http

override CXXFLAGS := $(filter-out -std=gnu++98 -Wshadow -Werror,$(CXXFLAGS))

sbin_PROGRAMS = otbr-web

otbr_web_SOURCES                                           = \
    main.cpp                                                 \
	$(NULL)

otbr_web_LDADD                                             = \
    libotbr-web.la                                           \
	$(NULL)

otbr_web_CPPFLAGS                                          = \
    $(libotbr_web_la_CPPFLAGS)                               \
	$(NULL)

noinst_LTLIBRARIES = libotbr-web.la

libotbr_web_la_LIBADD                                           = \
    $(DBUS_LIBS)                                                  \
    -lavahi-common                                                \
    -lavahi-client                                                \
    -lboost_filesystem                                            \
    -lboost_system                                                \
    -lpthread                                                     \
    -ljsoncpp                                                     \
    $(top_builddir)/third_party/mbedtls/libmbedtls.la             \
    $(top_builddir)/src/utils/libutils.la                         \
    $(top_builddir)/src/common/libotbr-logging.la                 \
    $(NULL)

libotbr_web_la_SOURCES                                          = \
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
    mdns-publisher/mdns_publisher.cpp                             \
    pskc-generator/pskc.cpp                                       \
    web-service/mdns_service.cpp                                  \
    web-service/web_server.cpp                                    \
    web-service/wpan_service.cpp                                  \
    $(NULL)

libotbr_web_la_LDFLAGS                                          = \
    -static                                                       \
    $(NULL)

libotbr_web_la_CPPFLAGS                                         = \
    $(DBUS_CFLAGS)                                                \
    -I$(top_srcdir)/src                                           \
    -I$(top_srcdir)/src/wpan-controller                           \
    -I$(top_srcdir)/src/mdns-publisher                            \
    -I$(top_srcdir)/src/web-service                               \
    -I$(top_srcdir)/src/pskc-generator                            \
    -I$(top_srcdir)/third_party/Simple-web-server/repo/           \
    -I$(top_srcdir)/third_party/mbedtls/repo/configs              \
    -I$(top_srcdir)/third_party/mbedtls/repo/include              \
    -I$(top_srcdir)/third_party/wpantund/repo/src                 \
    -I$(top_srcdir)/third_party/wpantund/repo/src/ipc-dbus        \
    -I$(top_srcdir)/third_party/wpantund/repo/src/wpanctl         \
    -I$(top_srcdir)/third_party/wpantund/repo/src/wpantund        \
    -DMBEDTLS_CONFIG_FILE='<config-thread.h>'                     \
    -DWEB_FILE_PATH=\"$(datadir)/border-router/frontend\"         \
    -std=c++11                                                    \
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
    mdns-publisher/mdns_publisher.hpp                            \
    pskc-generator/pskc.hpp                                      \
    utils/encoding.hpp                                           \
    web-service/web_server.hpp                                   \
    web-service/wpan_service.hpp                                 \
    web-service/mdns_service.hpp                                 \
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
    $(NULL)

systemddir=$(sysconfdir)/systemd/system
systemd_DATA                        = \
    otbr-web.service                  \
    $(NULL)

.PHONY: $(systemd_DATA)
$(systemd_DATA): %: %.in
	$(SED)                                  \
	-e 's,[@]sbindir[@],$(sbindir),g'       \
	-e 's,[@]sysconfdir[@],$(sysconfdir),g' \
	< "$<" > "$@"

CLEANFILES                = \
    otbr-web.service        \
    $(NULL)

include $(abs_top_nlbuild_autotools_dir)/automake/post.am
