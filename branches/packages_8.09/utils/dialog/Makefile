#
# Copyright (C) 2006 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
# $Id$

include $(TOPDIR)/rules.mk

PKG_NAME:=dialog
PKG_VERSION:=1.1-20080819
PKG_RELEASE:=1

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tgz
PKG_SOURCE_URL:=ftp://invisible-island.net/dialog
PKG_MD5SUM:=3caebd641a9f337b980becb4444336c5

include $(INCLUDE_DIR)/package.mk

define Package/dialog
	SECTION:=utils
	CATEGORY:=Utilities
	DEPENDS:=+libncurses
	TITLE:=Dialog
	URL:=http://invisible-island.net/dialog/
endef

define Package/dialog/description
		A script-interpreter which provides a set of curses widgets.
endef

define Build/Configure
	$(call Build/Configure/Default)
endef

define Package/dialog/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/dialog $(1)/usr/bin
	$(STRIP) $(1)/usr/bin/dialog
endef

$(eval $(call BuildPackage,dialog))

