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
	# Volontairement AUCUN script de demarrage : l'ecran de connexion ne doit
	# pas se lancer tout seul. Il ouvrait la carte graphique avant le bureau, et
	# la carte n'accepte qu'un seul maitre a la fois : le bureau recevait alors
	# "SETCRTC: Permission denied" en boucle, sans rien afficher.
	#
	# Le script existe toujours, range par l'overlay dans
	# /usr/share/fritax/desactive/ : il est disponible a la main si besoin, mais
	# rien ne l'appelle. Attention, l'overlay (BR2_ROOTFS_OVERLAY) copie son
	# contenu TEL QUEL dans le systeme : il ne suffit pas de retirer la ligne ici,
	# il faut aussi que le fichier ne soit plus dans rootfs-overlay/etc/init.d/.
endef

$(eval $(generic-package))
