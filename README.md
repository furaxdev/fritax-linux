# 🐧 Fritax Linux

> La distribution Linux de **Fritax** — construite **de zéro**, pas un remaster.

*Noyau Linux officiel pris tel quel · tout le reste compilé par nous · bureau écrit à la main.*

---

## Ce qui a été construit

### 1. Le bureau (`desktop/`)

| Programme | Description | État |
|---|---|---|
| **`fritax-shell`** (natif) | Le bureau : barre flottante arrondie, lanceur d'applications, horloge. Parle **directement au matériel** (DRM/KMS + evdev) : ni X11, ni Wayland, ni bibliothèque graphique. | ✅ compile et se lie |
| **`fritax-shell`** (X11) | Le même bureau dans une fenêtre, pour tester sur un KDE/GNOME existant. | ✅ testé |
| **`fritax-terminal`** (natif) | Le terminal maison sur DRM/KMS. | ✅ compile et se lie |
| **`fritax-terminal`** (X11) | Le même terminal dans une fenêtre. | ✅ tourne (validé sur un vrai PC) |
| **`fritax-files`** (natif + X11) | Le gestionnaire de fichiers : dossiers, fichiers, tailles, navigation. | ✅ compile et se lie |
| **`fritax-tunnel`** (statique) | Notre tunnel maison (relais / exposition / connexion), authentifié par mot de passe. | ✅ testé de bout en bout |
| **`fritax-relay`** (serveur + client) | Pilotage à distance via un relais HTTPS public : aucun port ouvert, aucune limite de temps. | ✅ utilisé en vrai |

### Le gestionnaire de fenêtres

Le bureau n'est plus une simple barre : c'est un **vrai gestionnaire de fenêtres**,
écrit à la main :

- fenêtres **déplaçables** (on attrape la barre de titre), **empilables** (un clic
  fait passer devant), **réductibles / agrandissables / fermables** (nos trois
  boutons ou `Ctrl+W`) ;
- **toutes les icônes sont dessinées au pixel** par notre code (`appicon.c`) :
  terminal `>_`, dossier, engrenage, feuille — aucun fichier d'icône externe ;
- les fenêtres hébergent de vraies applications :

| Fenêtre | Ce qu'elle fait |
|---|---|
| **Terminal** | un **vrai shell** : `forkpty()` + notre moteur VT100, couleurs, curseur, clavier complet |
| **Fichiers** | parcours du disque, entrées triées (dossiers d'abord), tailles lisibles |
| **Réglages** | couleur d'accent, informations système, heure |
| **Bloc-notes** | saisie de texte au clavier |

Les icônes du bureau ouvrent ces fenêtres ; le lanceur (`F` de la barre) liste les
applications installées.

**Le terminal** est un vrai terminal, écrit de zéro :
- moteur VT100/xterm maison (`vt.c`, 500 lignes) : 16 + 256 couleurs, curseur,
  effacement, **écran alternatif** (vim/htop), **historique 2000 lignes**, molette,
  redimensionnement, réponses DSR (nécessaires aux prompts de shell) ;
- **police bitmap maison 8×14** (ASCII + accents français) — aucune police système requise ;
- rendu logiciel dans un framebuffer (`screen.c`) : rectangles arrondis, dégradés,
  mélange alpha, texte ;
- **22 tests automatiques** du moteur — tous passent.

### 2. La distribution (`distro/`)

Arbre **Buildroot externe** (`BR2_EXTERNAL`) — notre propre distribution :

```
configs/fritax_defconfig        notre OS : architecture, noyau, paquets, ISO
board/fritax/
  kernel.fragment               ce qu'on active dans le noyau (DRM/Radeon, evdev…)
  isolinux.cfg                  menu de démarrage
  rootfs-overlay/               fichiers propres à la distro (init, motd, fond d'écran)
package/fritax-branding/        identité : os-release, fond d'écran, motd
package/fritax-shell/           notre bureau
package/fritax-terminal/        notre terminal
build.sh                        construit l'ISO (1 à 3 h)
```

**Choix techniques assumés :**
- **x86-64 de base, sans AVX2** → tourne sur le AMD A6-3400M (2011) du PC de Fritax,
  là où les binaires modernes plantent (SIGILL) ;
- **noyau Linux officiel 6.12** (pris tel quel) + fragment de config pour les vieux
  GPU AMD (`DRM_RADEON`) et les souris/claviers (`evdev`) ;
- **zéro serveur graphique** : notre bureau est maître du matériel. Sur un vieux
  processeur, ça fait toute la différence (pas de X, pas de Wayland, pas de KDE).
- **ISO bootable BIOS** (le Acer est en BIOS classique) + hybride (clé USB).

---

## Construire l'ISO

### Sur ta machine (Ubuntu)

```sh
sudo apt-get install -y build-essential git bison flex wget cpio unzip rsync bc python3 libncurses-dev
cd distro && ./build.sh
# → fritax-linux-1.0-nova.iso
```

### Dans le cloud (GitHub Actions)

Le workflow `.github/workflows/build-iso.yml` construit l'ISO à chaque envoi
et la publie en artifact. Aucune machine à mobiliser.

## Graver et démarrer

```sh
sudo dd if=fritax-linux-1.0-nova.iso of=/dev/sdX bs=4M status=progress oflag=sync
```
(ou **Rufus** / **balenaEtcher**), puis démarrer sur la clé.

---

## État du projet

- [x] Branding : logo, fond d'écran (3 résolutions), personnage Minecraft avec bandeau
- [x] Terminal maison complet (moteur + police + 2 moteurs d'affichage)
- [x] Bureau maison (barre flottante + lanceur) — 2 moteurs
- [x] Arbre de distribution Buildroot + paquets maison validés
- [ ] Premier boot réel de l'ISO (à construire sur ta machine)
- [ ] Peaufinage : fenêtres flottantes, raccourcis, multi-écrans

*Fait avec beaucoup d'attention. Pour Fritax.* ❤️
