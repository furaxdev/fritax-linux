################################################################################
# fritax-calculatrice : livre avec Fritax Linux
################################################################################

FRITAX_CALCULATRICE_VERSION = 1.0
FRITAX_CALCULATRICE_SITE = $(BR2_EXTERNAL_FRITAX_LINUX_PATH)/../desktop
FRITAX_CALCULATRICE_SITE_METHOD = local
FRITAX_CALCULATRICE_LICENSE = GPL-3.0+

define FRITAX_CALCULATRICE_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/fritax-calculatrice CC="$(TARGET_CC)" CFLAGS="$(TARGET_CFLAGS)" native
endef

define FRITAX_CALCULATRICE_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/fritax-calculatrice/fritax-calculatrice-native \
		$(TARGET_DIR)/usr/bin/fritax-calculatrice
endef

$(eval $(generic-package))
