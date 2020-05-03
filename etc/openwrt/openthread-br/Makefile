#
#  Copyright (c) 2020, The OpenThread Authors.
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

LOCAL_SOURCE_DIR := $(abspath $(realpath $(dir $(lastword $(MAKEFILE_LIST))))/../../..)

include $(TOPDIR)/rules.mk

PKG_NAME := openthread-br
PKG_LICENSE := BSD-3-Clause
PKG_LICENSE_FILES := LICENSE
PKG_RELEASE := 1
PKG_VERSION := 1.0

include $(INCLUDE_DIR)/package.mk

CONFIGURE_CMD = $(LOCAL_SOURCE_DIR)/configure

CONFIGURE_ARGS += \
	--disable-docs \
	--disable-web-service \
	--enable-openwrt \
	--with-ncp=openthread \
	--with-mdns=none \

define Package/openthread-br
	SECTION := base
	CATEGORY := Network
	TITLE := OpenThread Border Router
	DEPENDS := +libstdcpp +libjson-c +libubus +libblobmsg-json
endef

define Package/openthread-br/description
    A Thread border router for POSIX-based platforms.
endef

define Build/Prepare
	cd $(LOCAL_SOURCE_DIR) && ./bootstrap
endef

define Package/openthread-br/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/agent/otbr-agent $(1)/usr/sbin

	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/controller/admin
	$(INSTALL_BIN) $(LOCAL_SOURCE_DIR)/src/openwrt/controller/thread.lua $(1)/usr/lib/lua/luci/controller/admin

	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/view
	$(CP) $(LOCAL_SOURCE_DIR)/src/openwrt/view/admin_thread $(1)/usr/lib/lua/luci/view

	$(INSTALL_DIR) $(1)/www/luci-static/resources
	$(CP) $(LOCAL_SOURCE_DIR)/src/openwrt/handle_error.js $(1)/www/luci-static/resources

	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(LOCAL_SOURCE_DIR)/src/openwrt/otbr-agent $(1)/etc/init.d
endef

$(eval $(call BuildPackage,openthread-br))
