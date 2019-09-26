include $(TOPDIR)/rules.mk

# Name, version and release number
# The name and version of your package are used to define the variable to point to the build directory of your package: $(PKG_BUILD_DIR)
PKG_NAME:=openthread
PKG_VERSION:=1.0
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/openthread/ot-br-posix.git
PKG_SOURCE_VERSION:=460a31137648dd18124a5c271f64d9d1fe3c5517

include $(INCLUDE_DIR)/package.mk

# Package definition; instructs on how and where our package will appear in the overall configuration menu ('make menuconfig')
define Package/openthread
	SECTION:=base
	CATEGORY:=Network
	TITLE:=OpenThread border router
	DEPENDS:=+libjson-c +libubox +libubus +libblobmsg-json
endef

# Package description; a more verbose description on what our package does
define Package/openthread/description
	A mesh network border router implementation -application.
endef

define Build/Configure
	@echo "Build/Configure"
	cd $(PKG_BUILD_DIR) && ./bootstrap
	cd $(PKG_BUILD_DIR) && HOST=mips-openwrt-linux \
		./configure --with-ncp=openthread \
		--prefix= --exec-prefix=/usr --disable-docs \
		--disable-web-service --host=mips-openwrt-linux \
		--with-mdns=none --with-openwrt=openwrt
endef

define Build/Compile
	@echo "Build/Compile"
	cd $(PKG_BUILD_DIR) && HOST=mips-openwrt-linux $(MAKE)
endef

# Package install instructions; create a directory inside the package to hold our executable, and then copy the executable we built previously into the folder
define Package/openthread/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/controller/admin
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/view
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci-static/resources
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/agent/otbr-agent $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/openwrt/controller/thread.lua $(1)/usr/lib/lua/luci/controller/admin
	$(CP) $(PKG_BUILD_DIR)/src/openwrt/view/admin_thread $(1)/usr/lib/lua/luci/view
	$(CP) $(PKG_BUILD_DIR)/src/openwrt/handle_error.js $(1)/www/luci-static/resources
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/openwrt/otbr $(1)/etc/init.d
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_DATA) $(TOOLCHAIN_DIR)/lib/libstdc++.so.6 $(1)/usr/lib
endef

# This command is always the last, it uses the definitions and variables we give above in order to get the job done
$(eval $(call BuildPackage,openthread))
