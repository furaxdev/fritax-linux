################################################################################
# fritax-login : ecran de connexion de Fritax Linux (DRM/KMS)
################################################################################

FRITAX_LOGIN_VERSION = 1.0
FRITAX_LOGIN_SITE = $(BR2_EXTERNAL_FRITAX_LINUX_PATH)/../desktop
FRITAX_LOGIN_SITE_METHOD = local
FRITAX_LOGIN_LICENSE = GPL-3.0+
# l'ecran de connexion commence par le noyau et l'outil : ordre de construction
FRITAX_LOGIN_DEPENDENCIES = fritax-shell

define FRITAX_LOGIN_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/fritax-login CC="$(TARGET_CC)" CFLAGS="$(TARGET_CFLAGS)" native
endef

define FRITAX_LOGIN_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/fritax-login/fritax-login-native \
		$(TARGET_DIR)/usr/bin/fritax-login
	$(INSTALL) -D -m 0755 $(BR2_EXTERNAL_FRITAX_LINUX_PATH)/board/fritax/rootfs-overlay/etc/init.d/S30fritax-login \
		$(TARGET_DIR)/etc/init.d/S30fritax-login
endef

$(eval $(generic-package))
