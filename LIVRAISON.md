# 🐧 Fritax Linux 1.0 « Nova » — le guide

> Ta distribution, construite **de zéro** : le noyau Linux officiel est pris tel
> quel, **tout le reste est compilé par nous** (compilateur, système, bureau,
> applications). Ce n'est pas un remaster d'Ubuntu ou d'Arch.

---

## 1. Ce qu'il y a dedans

| Élément | Ce que c'est |
|---|---|
| **Noyau** | Linux 6.12.9 (officiel), configuré pour les vieux GPU AMD + souris/claviers |
| **Fenêtres** | gestionnaire maison : déplacer, réduire, agrandir, redimensionner (coin), empiler, barre des tâches |
| **Compilateur** | gcc compilé par nous, ciblé **x86-64 sans AVX2** → tourne sur le AMD A6-3400M |
| **Bureau** | **Fritax Shell** : écrit à la main, parle **directement au matériel** (DRM/KMS). Ni X11, ni Wayland, ni Qt |
| **Fenêtres** | déplaçables, réductibles, agrandissables, fermables, empilables (vrais bonshommes) |
| **Terminal** | **Fritax Terminal** : moteur VT100 maison + police bitmap maison + vrai shell |
| **Fichiers** | **Fritax Fichiers** : parcours du disque, tailles, navigation clavier/souris |
| **Réglages** | **3 fonds d'écran** (Nuit / Aurore / Graphite) applicables immédiatement, couleur d'accent, informations du système, heure |
| **Lanceur** | bouton **F** de la barre : liste les applications installées, recherche au clavier |
| **`osh`** | l'outil de furaxdev : piloter la machine à distance sans ouvrir de port (relais HTTPS) |
| **Bloc-notes** | saisie de texte |
| **Icônes** | toutes **dessinées au pixel** par notre code (terminal `>_`, dossier, engrenage, feuille, extinction) |
| **Tunnel** | **Fritax Tunnel** : notre propre tunnel (relais / exposition / connexion), authentifié par mot de passe |
| **Outils** | bash, coreutils, nano, htop, git, util-linux, usbutils, e2fsprogs, dosfstools |

---

## 2. Comment fabriquer l'ISO

### ⭐ Le plus simple : dans le cloud (recommandé)

Le dépôt **github.com/furaxdev/fritax-linux** compile l'ISO tout seul sur les
serveurs de GitHub à chaque envoi de code. C'est **beaucoup plus rapide** que sur
un PC :

```
GitHub → ton dépôt → onglet « Actions » → la dernière exécution
   → section « Artifacts » → télécharge « fritax-linux-1.0-nova »
```

*(Tu peux aussi la lancer à la main : onglet Actions → « Construire l'ISO Fritax
Linux » → bouton « Run workflow ».)*

### Sur ta machine (Ubuntu)

```sh
cd distro
./build.sh          # 1 h à 3 h la première fois (il compile le compilateur)
```

Sur un petit PC, prévois de la patience : la première construction compile le
compilateur lui-même. Les suivantes ne prennent que quelques minutes.

À la fin tu obtiens :

```
distro/fritax-linux-1.0-nova.iso
```

> La première construction est longue (gcc, le noyau, tout le système).
> Les suivantes ne prennent que quelques minutes : tout est déjà compilé.

⚠️ Sur une machine où `/tmp` est un disque en mémoire, il faut dire au système
d'écrire les fichiers temporaires sur le disque (déjà fait dans `build.sh`) :

```sh
export TMPDIR="$HOME/build-tmp"
```

---

## 3. Mettre l'ISO sur une clé USB

**Le plus simple, le script maison** :

```sh
./Fritax-USB.sh          # (double-clic) il liste les clés et demande confirmation
```

**Ou avec un logiciel graphique** : **Rufus** (Windows) ou **balenaEtcher**.
Choisis l'ISO, choisis la clé, écris.

**Ou en ligne de commande** (⚠️ remplace `sdX` par TA clé, jamais le disque du PC) :

```sh
sudo dd if=fritax-linux-1.0-nova.iso of=/dev/sdX bs=4M status=progress oflag=sync
```

---

## 4. Démarrer sur Fritax

1. Branche la clé, **redémarre**.
2. Au démarrage, tapote **F12** (ou **F2**, **Échap** selon le PC) → menu de démarrage → choisis la clé USB.
3. Tu arrives sur le **menu Fritax** → choisis « Fritax Linux ».
4. Le bureau apparaît avec son fond d'écran, ses icônes et sa barre en bas.

**Se connecter en texte** : utilisateur `root`, mot de passe `fritax` (console tty1).

---

## 5. Se servir du bureau

| Action | Comment |
|---|---|
| Ouvrir une appli | **clic** sur une icône du bureau, ou le bouton **F** de la barre (lanceur) |
| Déplacer une fenêtre | attrape sa **barre de titre** et glisse |
| Réduire / Agrandir / Fermer | les **trois boutons** en haut à droite, ou **Ctrl+W** pour fermer |
| Passer une fenêtre devant | clique dessus |
| Éteindre la machine | lanceur (**F**) → **Eteindre** |
| Redémarrer la machine | lanceur (**F**) → **Redemarrer** |
| Redémarrer le bureau | touche **Échap** (le bureau revient tout seul) |

---

## 6. Ce qui marche / ce qui viendra

**Marche** : le bureau complet, les fenêtres, le terminal (vrai shell), le
gestionnaire de fichiers, les réglages, le bloc-notes, les outils en ligne de
commande, la connexion Ethernet en DHCP, notre tunnel.

**Pas encore là** : un navigateur web, le son, le WiFi, un magasin
d'applications, le glisser-déposer de fichiers. Chacun est un chantier à part —
dis-moi ce qui te manque en premier.

---

## 7. Caractéristiques techniques

- Architecture : **x86-64 de base, sans AVX2** (compatible 2011 et après)
- Système : glibc compilée par nous, bash, busybox
- Graphismes : **DRM/KMS** directement (aucun serveur graphique)
- Entrées : **evdev** directement (clavier, souris, molette)
- Format : ISO **hybride** (démarrage BIOS classique + clé USB amorçable)
- Construction : arbre **Buildroot externe** (`distro/`), 100 % reproductible

---

*Fait avec beaucoup d'attention, pour Fritax.* ❤️

---

## Ecran de connexion (nouveau)

Fritax Linux demarre maintenant sur un **ecran de connexion** maison
(`fritax-login`, DRM/KMS, aucun serveur graphique), avant d'ouvrir le bureau.
Le trio « connexion -> bureau -> retour a la connexion » forme un vrai
gestionnaire de session.

**Compte de la distribution** : utilisateur `furax` (ou `root`), mot de passe `fritax`.
Le pseudo est pre-rempli : on tape juste le mot de passe puis Entree.

- `Entree` = valider (ou passer au champ suivant)
- `Tab` = changer de champ, `Echap` = effacer le pseudo
- `Retour arriere` = effacer une lettre

## Verifier AVANT d'envoyer (ne plus perdre 1 h de compilation)

```sh
distro/valider-tout.sh        # controle complet : code, config, paquets, scripts
```

Ce script compile **tout** le C en local, fait tourner les **3 suites de tests**,
valide chaque option de la config Buildroot contre l'arbre reel, et verifie
qu'aucun des pieges deja payes n'est revenu. S'il est vert, la compilation
cloud ne peut plus echouer sur une erreur bete.

## Iterer vite : le cache

La cle du cache GitHub porte **uniquement sur la configuration du compilateur**
(defconfig + board). Modifier le C des applis ne l'invalide plus : seuls nos
petits paquets sont recompiles, le compilateur reste en cache.
`build.sh` efface de lui-meme les tampons de nos paquets a chaque run, pour
qu'une modification du C soit toujours reprise.
