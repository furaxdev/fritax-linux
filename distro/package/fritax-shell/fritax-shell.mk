################################################################################
# fritax-shell : le bureau de Fritax Linux (DRM/KMS, sans serveur graphique)
################################################################################

FRITAX_SHELL_VERSION = 1.0
FRITAX_SHELL_SITE = $(BR2_EXTERNAL_FRITAX_LINUX_PATH)/../desktop
FRITAX_SHELL_SITE_METHOD = local
FRITAX_SHELL_LICENSE = GPL-3.0+

define FRITAX_SHELL_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/fritax-shell CC="$(TARGET_CC)" CFLAGS="$(TARGET_CFLAGS)" native
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/fritax-open CC="$(TARGET_CC)" CFLAGS="$(TARGET_CFLAGS)"
endef

define FRITAX_SHELL_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/fritax-shell/fritax-shell-native \
		$(TARGET_DIR)/usr/bin/fritax-shell
	$(INSTALL) -D -m 0755 $(@D)/fritax-open/fritax-open \
		$(TARGET_DIR)/usr/bin/fritax-open
	$(INSTALL) -D -m 0755 $(BR2_EXTERNAL_FRITAX_LINUX_PATH)/board/fritax/rootfs-overlay/etc/init.d/S99fritax-shell \
		$(TARGET_DIR)/etc/init.d/S99fritax-shell
endef

$(eval $(generic-package))
