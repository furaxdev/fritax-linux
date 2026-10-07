################################################################################
# fritax-antivirus : livre avec Fritax Linux
################################################################################

FRITAX_ANTIVIRUS_VERSION = 1.0
FRITAX_ANTIVIRUS_SITE = $(BR2_EXTERNAL_FRITAX_LINUX_PATH)/../desktop
FRITAX_ANTIVIRUS_SITE_METHOD = local
FRITAX_ANTIVIRUS_LICENSE = GPL-3.0+

define FRITAX_ANTIVIRUS_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/fritax-antivirus CC="$(TARGET_CC)" CFLAGS="$(TARGET_CFLAGS)"
endef

define FRITAX_ANTIVIRUS_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/fritax-antivirus/fritax-av \
		$(TARGET_DIR)/usr/bin/fritax-av
endef

$(eval $(generic-package))
