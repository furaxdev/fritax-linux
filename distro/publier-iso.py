#!/usr/bin/env python3
"""Publie l'ISO Fritax Linux en Release GitHub (lien de telechargement direct).

Usage : python3 publier-iso.py
- telecharge l'artefact de la derniere compilation reussie
- cree la Release v1.0 (Nova) et y depose l'ISO
- affiche le lien public
"""
import json, os, subprocess, sys

TOK = open("/home/furax/.github-token").read().strip()
REPO = "furaxdev/fritax-linux"
API = "https://api.github.com"

def api(chemin, methode="GET", donnees=None, binaire=False):
    cmd = ["curl", "-s", "-X", methode, "-H", "Authorization: token " + TOK,
           "-H", "Accept: application/vnd.github+json", API + chemin]
    if donnees is not None:
        cmd += ["-d", json.dumps(donnees)]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    if binaire:
        return r.stdout.encode()
    try:
        return json.loads(r.stdout)
    except Exception:
        return {"_brut": r.stdout[:200]}

def main():
    runs = api(f"/repos/{REPO}/actions/runs?per_page=5").get("workflow_runs", [])
    run = next((r for r in runs if r.get("conclusion") == "success"), None)
    if not run:
        print("aucune compilation reussie pour l'instant"); return 1
    print(f"compilation retenue : #{run['run_number']} ({run['created_at']})")

    arts = api(f"/repos/{REPO}/actions/runs/{run['id']}/artifacts").get("artifacts", [])
    if not arts:
        print("pas d'artefact"); return 1
    art = arts[0]
    print(f"artefact : {art['name']} ({art['size_in_bytes'] // 1048576} Mo)")

    # telechargement (l'API renvoie une redirection vers un stockage signe)
    os.makedirs("/tmp/fritax-release", exist_ok=True)
    zip_path = "/tmp/fritax-release/iso.zip"
    subprocess.run(["curl", "-sL", "-H", "Authorization: token " + TOK,
                    "-o", zip_path, f"{API}/repos/{REPO}/actions/artifacts/{art['id']}/zip"], timeout=3000)
    print("archive :", os.path.getsize(zip_path) // 1048576, "Mo")
    subprocess.run(["unzip", "-o", "-q", zip_path, "-d", "/tmp/fritax-release/"], timeout=600)
    iso = None
    for racine, _, fichiers in os.walk("/tmp/fritax-release"):
        for f in fichiers:
            if f.endswith(".iso"):
                iso = os.path.join(racine, f)
    if not iso:
        print("ISO introuvable dans l'archive"); return 1
    print("ISO :", iso, os.path.getsize(iso) // 1048576, "Mo")

    # Release + depot de l'ISO
    rel = api(f"/repos/{REPO}/releases", "POST", {
        "tag_name": "v1.0-nova",
        "name": "Fritax Linux 1.0 (Nova)",
        "body": "Premiere version de Fritax Linux : noyau Linux 6.12 officiel, "
                "systeme et bureau compiles de zero.\n\n"
                "- Bureau maison (DRM/KMS) : fenetres, icones dessinees, lanceur, barre des taches\n"
                "- Terminal maison (vrai shell PTY) + Fichiers + Reglages + Bloc-notes\n"
                "- 3 fonds d'ecran, osh, Fritax Tunnel\n"
                "- ISO hybride : demarrage BIOS classique et cle USB (x86-64 sans AVX2)",
        "draft": False, "prerelease": False})
    if "id" not in rel:
        print("creation de la Release impossible :", rel); return 1
    print("Release :", rel.get("html_url"))

    nom = os.path.basename(iso)
    up = subprocess.run(["curl", "-s", "-X", "POST",
                         "-H", "Authorization: token " + TOK,
                         "-H", "Content-Type: application/octet-stream",
                         "-H", "Accept: application/vnd.github+json",
                         "--data-binary", "@" + iso,
                         f"https://uploads.github.com/repos/{REPO}/releases/{rel['id']}/assets?name={nom}"],
                        capture_output=True, text=True, timeout=3000)
    try:
        d = json.loads(up.stdout)
    except Exception:
        print("reponse inattendue de l'envoi"); return 1
    print("\nLIEN DE TELECHARGEMENT DIRECT :")
    print("  " + d.get("browser_download_url", up.stdout[:200]))
    return 0

if __name__ == "__main__":
    sys.exit(main())
