################################################################################
# fritax-tunnel : notre tunnel TCP maison (mot de passe)
################################################################################

FRITAX_TUNNEL_VERSION = 1.0
FRITAX_TUNNEL_SITE = $(BR2_EXTERNAL_FRITAX_LINUX_PATH)/../desktop/fritax-tunnel
FRITAX_TUNNEL_SITE_METHOD = local
FRITAX_TUNNEL_LICENSE = GPL-3.0+

define FRITAX_TUNNEL_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(TARGET_CC) $(TARGET_CFLAGS) -o $(@D)/fritax-tunnel \
		$(@D)/src/fritax-tunnel.c
endef

define FRITAX_TUNNEL_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/fritax-tunnel $(TARGET_DIR)/usr/bin/fritax-tunnel
endef

$(eval $(generic-package))
