#!/usr/bin/env python3
"""Verifie que CHAQUE option de notre defconfig existe dans l'arbre Buildroot.
Une option inconnue est ignoree en silence par Buildroot (c'est ce qui a fait
tomber notre outil en uclibc au lieu de glibc)."""
import os, re, subprocess, sys

BR = sys.argv[1] if len(sys.argv) > 1 else "/tmp/buildroot-2026.02.3"
DEF = sys.argv[2] if len(sys.argv) > 2 else "/home/furax/workspace/fritax-linux/distro/configs/fritax_defconfig"

# recenser tous les symboles definis dans Buildroot
chemins = [BR, BR + "/../distro/package" if not BR.endswith("distro") else BR]
corpus = ""
for c in chemins:
    if os.path.isdir(c):
        corpus += subprocess.run(["grep", "-rhoE", "^[[:space:]]*(config|menuconfig)[[:space:]]+BR2_[A-Z0-9_]+", c],
                                 capture_output=True, text=True).stdout
notre = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(DEF))), "package")
corpus += subprocess.run(["grep", "-rhoE", "^[[:space:]]*(config|menuconfig)[[:space:]]+BR2_[A-Z0-9_]+", notre],
                         capture_output=True, text=True).stdout
connus = set(m.split()[1] for m in corpus.splitlines() if len(m.split()) > 1)
print(f"symboles BR2_* definis dans Buildroot : {len(connus)}")

inconnus, verifies = [], 0
for ligne in open(DEF):
    m = re.match(r'^\s*(BR2_[A-Z0-9_]+)=', ligne)
    if not m:
        continue
    nom = m.group(1)
    verifies += 1
    if nom not in connus:
        inconnus.append(nom)

print(f"options testees dans notre defconfig : {verifies}")
if inconnus:
    print("\nOPTIONS INCONNUES (ignorees en silence !) :")
    for n in inconnus:
        print("   ✗", n)
    sys.exit(1)
print("\nToutes nos options existent dans cet arbre Buildroot ✅")
