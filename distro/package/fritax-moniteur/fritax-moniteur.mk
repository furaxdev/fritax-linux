################################################################################
# fritax-moniteur : livre avec Fritax Linux
################################################################################

FRITAX_MONITEUR_VERSION = 1.0
FRITAX_MONITEUR_SITE = $(BR2_EXTERNAL_FRITAX_LINUX_PATH)/../desktop
FRITAX_MONITEUR_SITE_METHOD = local
FRITAX_MONITEUR_LICENSE = GPL-3.0+

define FRITAX_MONITEUR_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/fritax-moniteur CC="$(TARGET_CC)" CFLAGS="$(TARGET_CFLAGS)" native
endef

define FRITAX_MONITEUR_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/fritax-moniteur/fritax-moniteur-native \
		$(TARGET_DIR)/usr/bin/fritax-moniteur
endef

$(eval $(generic-package))
