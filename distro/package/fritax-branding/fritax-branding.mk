################################################################################
# fritax-branding : l'identite de Fritax Linux
################################################################################

FRITAX_BRANDING_VERSION = 1.0
FRITAX_BRANDING_SITE = $(BR2_EXTERNAL_FRITAX_LINUX_PATH)/package/fritax-branding/files
FRITAX_BRANDING_SITE_METHOD = local
FRITAX_BRANDING_LICENSE = GPL-3.0+

define FRITAX_BRANDING_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/os-release $(TARGET_DIR)/etc/os-release
	$(INSTALL) -D -m 0644 $(@D)/motd $(TARGET_DIR)/etc/motd
	$(INSTALL) -D -m 0644 $(@D)/wallpaper.png $(TARGET_DIR)/usr/share/fritax/wallpaper.png
	echo "Fritax Linux 1.0 (Nova)" > $(TARGET_DIR)/etc/fritax-release
endef

$(eval $(generic-package))
