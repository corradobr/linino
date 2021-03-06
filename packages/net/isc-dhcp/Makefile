#
# Copyright (C) 2006-2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=isc-dhcp
UPSTREAM_NAME:=dhcp
PKG_VERSION:=4.2.4
PKG_RELEASE:=3

PKG_SOURCE:=$(UPSTREAM_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=ftp://ftp.isc.org/isc/dhcp/$(PKG_VERSION)
PKG_MD5SUM:=c244cefe663d43100af757d8ff625a1f

PKG_FIXUP:=autoreconf
PKG_BUILD_PARALLEL:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(BUILD_VARIANT)/$(UPSTREAM_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/isc-dhcp/Default
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=IP Addresses and Names
  TITLE:=ISC's DHCP
  URL:=https://www.isc.org/software/dhcp
endef

define Package/isc-dhcp-relay-ipv4
  $(call Package/isc-dhcp/Default)
  TITLE+= relay (without IPv6)
  VARIANT:=ipv4
endef

define Package/isc-dhcp-relay-ipv6
  $(call Package/isc-dhcp/Default)
  DEPENDS:= @IPV6
  TITLE+= relay (with IPv6)
  VARIANT:=ipv6
endef

define Package/isc-dhcp-relay/description
 provides a means for relaying DHCP and BOOTP requests from a subnet to which
 no DHCP server is directly connected to one or more DHCP servers on other
 subnets.
endef

define Package/isc-dhcp-relay-ipv4/description
$(call Package/isc-dhcp-relay-ipv6/description)
 This package is compiled with IPv4 support only.
endef

define Package/isc-dhcp-relay-ipv6/description
$(call Package/isc-dhcp-relay/description)
 This package is compiled with IPv4 and IPv6 support.
endef

define Package/isc-dhcp-client-ipv4
  $(call Package/isc-dhcp/Default)
  TITLE+= client (without IPv6)
  VARIANT:=ipv4
endef

define Package/isc-dhcp-client-ipv6
  $(call Package/isc-dhcp/Default)
  DEPENDS:= @IPV6
  TITLE+= client (with IPv6)
  VARIANT:=ipv6
endef

define Package/isc-dhcp-client/description
 provides a means for configuring one or more network interfaces using the
 Dynamic Host Configuration Protocol, BOOTP protocol, or if these protocols
 fail, by statically assigning an address.
endef

define Package/isc-dhcp-client-ipv4/description
$(call Package/isc-dhcp-client/description)
 This package is compiled with IPv4 support only.
endef

define Package/isc-dhcp-client-ipv6/description
$(call Package/isc-dhcp-client/description)
 This package is compiled with IPv4 and IPv6 support.
endef

define Package/isc-dhcp-server-ipv4
  $(call Package/isc-dhcp/Default)
  TITLE+= server (without IPv6)
  VARIANT:=ipv4
endef

define Package/isc-dhcp-server-ipv6
  $(call Package/isc-dhcp/Default)
  DEPENDS:= @IPV6
  TITLE+= server (with IPv6)
  VARIANT:=ipv6
endef

define Package/isc-dhcp-server/description
 implements the Dynamic Host Configuration Protocol (DHCP) and the Internet
 Bootstrap Protocol (BOOTP).
endef

define Package/isc-dhcp-server-ipv4/description
$(call Package/isc-dhcp-server/description)
 This package is compiled with IPv4 support only.
endef

define Package/isc-dhcp-server-ipv6/description
$(call Package/isc-dhcp-server/description)
 This package is compiled with IPv4 and IPv6 support.
endef

define Package/isc-dhcp-omshell-ipv4
  $(call Package/isc-dhcp/Default)
  DEPENDS:= +isc-dhcp-server-ipv4
  TITLE+= omshell (without IPv6)
  VARIANT:=ipv4
endef

define Package/isc-dhcp-omshell-ipv6
  $(call Package/isc-dhcp/Default)
  DEPENDS:= @IPV6 +isc-dhcp-server-ipv6
  TITLE+= omshell (with IPv6)
  VARIANT:=ipv6
endef

define Package/isc-dhcp-omshell/description
 provides an interactive way to connect to, query, and possibly change, the ISC
 DHCP Server's state via OMAPI, the Object Management API.
endef

define Package/isc-dhcp-omshell-ipv4/description
$(call Package/isc-dhcp-omshell/description)
 This package is compiled with IPv4 support only.
endef

define Package/isc-dhcp-omshell-ipv6/description
$(call Package/isc-dhcp-omshell/description)
 This package is compiled with IPv4 and IPv6 support.
endef

CONFIGURE_ARGS += \
	--disable-tracing 		\
	--enable-paranoia		\
	--disable-dependency-tracking	\
	ac_cv_file__dev_random=yes

ifeq ($(BUILD_VARIANT),ipv4)
  CONFIGURE_ARGS += --disable-dhcpv6
endif
ifeq ($(BUILD_VARIANT),ipv6)
  CONFIGURE_ARGS += --enable-dhcpv6
endif

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR)			\
		DESTDIR="$(PKG_INSTALL_DIR)"		\
		BUILD_CC="$(HOSTCC_NOCACHE)"		\
		host_alias="$(GNU_TARGET_NAME)"		\
		target_alias="$(GNU_TARGET_NAME)"	\
		build_alias="$(GNU_HOST_NAME)"		\
		ac_cv_file__dev_random=yes		\
		all install-exec
endef

define Package/isc-dhcp-relay-$(BUILD_VARIANT)/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/sbin/dhcrelay $(1)/usr/sbin
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DATA) ./files/etc/config/dhcrelay $(1)/etc/config
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/dhcrelay4.init $(1)/etc/init.d/dhcrelay4
ifeq ($(BUILD_VARIANT),ipv6)
	$(INSTALL_BIN) ./files/dhcrelay6.init $(1)/etc/init.d/dhcrelay6
endif
endef

define Package/isc-dhcp-server-$(BUILD_VARIANT)/install
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/etc/init.d
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/sbin/dhcpd $(1)/usr/sbin
	$(INSTALL_BIN) ./files/dhcpd.init $(1)/etc/init.d/dhcpd
	$(INSTALL_BIN) ./files/dhcpd.conf $(1)/etc
ifeq ($(BUILD_VARIANT),ipv6)
	$(INSTALL_BIN) ./files/dhcpd6.init $(1)/etc/init.d/dhcpd6
	$(INSTALL_BIN) ./files/dhcpd6.conf $(1)/etc
endif
endef

define Package/isc-dhcp-client-$(BUILD_VARIANT)/install
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/etc/init.d
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/sbin/dhclient $(1)/usr/sbin
	$(INSTALL_BIN) ./files/dhclient-script $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/dhclient.init $(1)/etc/init.d/dhclient
ifeq ($(BUILD_VARIANT),ipv6)
	$(INSTALL_BIN) ./files/dhclient6.init $(1)/etc/init.d/dhclient6
	$(INSTALL_BIN) ./files/dhclient6.conf $(1)/etc
endif
endef

define Package/isc-dhcp-omshell-$(BUILD_VARIANT)/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/omshell $(1)/usr/bin
endef

$(eval $(call BuildPackage,isc-dhcp-relay-ipv4))
$(eval $(call BuildPackage,isc-dhcp-server-ipv4))
$(eval $(call BuildPackage,isc-dhcp-client-ipv4))
$(eval $(call BuildPackage,isc-dhcp-omshell-ipv4))
$(eval $(call BuildPackage,isc-dhcp-relay-ipv6))
$(eval $(call BuildPackage,isc-dhcp-server-ipv6))
$(eval $(call BuildPackage,isc-dhcp-client-ipv6))
$(eval $(call BuildPackage,isc-dhcp-omshell-ipv6))
