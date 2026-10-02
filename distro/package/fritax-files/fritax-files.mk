################################################################################
# fritax-files : le gestionnaire de fichiers de Fritax Linux
################################################################################

FRITAX_FILES_VERSION = 1.0
FRITAX_FILES_SITE = $(BR2_EXTERNAL_FRITAX_LINUX_PATH)/../desktop
FRITAX_FILES_SITE_METHOD = local
FRITAX_FILES_LICENSE = GPL-3.0+

define FRITAX_FILES_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/fritax-files CC="$(TARGET_CC)" CFLAGS="$(TARGET_CFLAGS)" native
endef

define FRITAX_FILES_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/fritax-files/fritax-files-native \
		$(TARGET_DIR)/usr/bin/fritax-files
endef

$(eval $(generic-package))
