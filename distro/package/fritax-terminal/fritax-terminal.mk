################################################################################
# fritax-terminal : le terminal maison (moteur VT ecrit de zero)
################################################################################

FRITAX_TERMINAL_VERSION = 1.0
FRITAX_TERMINAL_SITE = $(BR2_EXTERNAL_FRITAX_LINUX_PATH)/../desktop/fritax-terminal
FRITAX_TERMINAL_SITE_METHOD = local
FRITAX_TERMINAL_LICENSE = GPL-3.0+

define FRITAX_TERMINAL_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) CC="$(TARGET_CC)" CFLAGS="$(TARGET_CFLAGS)" native
endef

define FRITAX_TERMINAL_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/fritax-terminal-native $(TARGET_DIR)/usr/bin/fritax-terminal
	$(INSTALL) -D -m 0644 $(BR2_EXTERNAL_FRITAX_LINUX_PATH)/board/fritax/rootfs-overlay/usr/share/applications/fritax-terminal.desktop \
		$(TARGET_DIR)/usr/share/applications/fritax-terminal.desktop
endef

$(eval $(generic-package))
