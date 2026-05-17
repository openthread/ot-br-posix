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

LOCAL_SOURCE_DIR:=$(abspath $(realpath $(dir $(lastword $(MAKEFILE_LIST))))/../../..)

include $(TOPDIR)/rules.mk

PKG_NAME:=openthread-br
PKG_LICENSE:=BSD-3-Clause
PKG_LICENSE_FILES:=LICENSE
PKG_RELEASE:=2
PKG_VERSION:=1.0

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

CMAKE_SOURCE_DIR=$(LOCAL_SOURCE_DIR)
CMAKE_OPTIONS+= \
	-DBUILD_TESTING=OFF \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DOTBR_BORDER_AGENT=ON \
	-DOTBR_INFRA_IF_NAME=\"br-lan\" \
	-DOTBR_MDNS="avahi" \
	-DOTBR_OPENWRT=ON \
	-DOTBR_VENDOR_NAME="OpenWrt" \
	-DOTBR_PRODUCT_NAME="BorderRouter" \
	-DOT_FIREWALL=ON \
	-DOT_POSIX_SETTINGS_PATH=\"/etc/openthread\" \
	-DOT_READLINE=OFF \
	-DOTBR_NAT64=OFF

TARGET_CFLAGS += -DOPENTHREAD_POSIX_CONFIG_DAEMON_SOCKET_BASENAME=\\\"/var/run/openthread-%s\\\"

define Package/openthread-br
	SECTION:=base
	CATEGORY:=Network
	TITLE:=OpenThread Border Router
	DEPENDS:=+libstdcpp +libjson-c +libubus +libblobmsg-json +libavahi-client +avahi-daemon +iptables-mod-extra +ipset +luci-base +rpcd-mod-ucode
endef

define Package/openthread-br/description
    A Thread border router for POSIX-based platforms.
endef

define Package/openthread-br/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/sbin/otbr-agent $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/sbin/ot-ctl $(1)/usr/sbin

	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/etc/init.d/* $(1)/etc/init.d

	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) $(PKG_INSTALL_DIR)/etc/config/* $(1)/etc/config

	$(INSTALL_DIR) $(1)/www/luci-static/resources/view/openthread
	$(CP) $(LOCAL_SOURCE_DIR)/src/openwrt/luci-app-openthread/htdocs/luci-static/resources/view/openthread/* \
	    $(1)/www/luci-static/resources/view/openthread/

	$(INSTALL_DIR) $(1)/www/luci-static/resources/openthread
	$(CP) $(LOCAL_SOURCE_DIR)/src/openwrt/luci-app-openthread/htdocs/luci-static/resources/openthread/* \
	    $(1)/www/luci-static/resources/openthread/

	$(INSTALL_DIR) $(1)/usr/share/luci/menu.d
	$(INSTALL_DATA) $(LOCAL_SOURCE_DIR)/src/openwrt/luci-app-openthread/root/usr/share/luci/menu.d/luci-app-openthread.json \
	    $(1)/usr/share/luci/menu.d/

	$(INSTALL_DIR) $(1)/usr/share/rpcd/acl.d
	$(INSTALL_DATA) $(LOCAL_SOURCE_DIR)/src/openwrt/luci-app-openthread/root/usr/share/rpcd/acl.d/luci-app-openthread.json \
	    $(1)/usr/share/rpcd/acl.d/

	$(INSTALL_DIR) $(1)/usr/share/rpcd/ucode
	$(INSTALL_DATA) $(LOCAL_SOURCE_DIR)/src/openwrt/luci-app-openthread/root/usr/share/rpcd/ucode/luci.openthread \
	    $(1)/usr/share/rpcd/ucode/
endef

$(eval $(call BuildPackage,openthread-br))
