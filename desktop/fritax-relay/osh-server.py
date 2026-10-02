#!/usr/bin/env python3
"""
osh-server - le serveur oSH (OpenConnect Shell)
Lancement : python3 osh-server.py --secret MON_SECRET

Le transport d'origine (relais Supabase) ayant ete supprime, cette version passe
par ntfy.sh : HTTPS sortant des deux cotes, aucun port ouvert, aucune limite de
temps. Le reste est identique au osh d'origine (secret jamais transmis, seul son
SHA-256 sert de nom de canal ; execution puis renvoi de stdout/stderr/code).
Tout passe par ntfy.sh en HTTPS sortant : aucun port ouvert, pas de tunnel,
aucune expiration. Le secret n'est jamais transmis (nom de canal = SHA-256).

Usage : python3 -u server.py --secret MON_SECRET_LONG
"""
import argparse, hashlib, json, subprocess, time, urllib.request

BASE = "https://ntfy.sh"

def topic(secret, suffix):
    return "fritax-relay-" + hashlib.sha256(secret.encode()).hexdigest()[:20] + "-" + suffix

def run_cmd(cmd, timeout):
    try:
        r = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=timeout)
        return r.stdout, r.stderr, r.returncode
    except subprocess.TimeoutExpired:
        return "", f"delai depasse ({timeout} s)", -1
    except Exception as e:
        return "", str(e), -1

def publish(url, body):
    try:
        req = urllib.request.Request(url, data=body.encode(), method="POST")
        urllib.request.urlopen(req, timeout=20).read()
    except Exception as e:
        print("  resultat non envoye:", str(e)[:70], flush=True)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--secret", required=True)
    ap.add_argument("--timeout", type=int, default=180)
    a = ap.parse_args()
    c_url = BASE + "/" + topic(a.secret, "c")
    r_url = BASE + "/" + topic(a.secret, "r")
    print("  Fritax Relay - serveur", flush=True)
    print("  canal :", topic(a.secret, "c"), flush=True)
    print("  connexion continue, en attente de commandes...", flush=True)
    seen = set()
    while True:
        try:
            # connexion continue (streaming) : pas de sondage repete
            req = urllib.request.Request(c_url + "/json?since=10s")
            with urllib.request.urlopen(req, timeout=120) as resp:
                for raw in resp:
                    line = raw.strip()
                    if not line:
                        continue
                    try:
                        msg = json.loads(line.decode())
                    except Exception:
                        continue
                    if msg.get("event") != "message":
                        continue
                    mid = msg.get("id")
                    if not mid or mid in seen:
                        continue
                    seen.add(mid)
                    if len(seen) > 800:
                        seen.clear()
                    try:
                        p = json.loads(msg.get("message", ""))
                    except Exception:
                        continue
                    cmd, rid = p.get("cmd"), p.get("id")
                    if not cmd or not rid:
                        continue
                    ts = time.strftime("%H:%M:%S")
                    print(f"  [{ts}] > {cmd[:90]}", flush=True)
                    out, err, rc = run_cmd(cmd, a.timeout)
                    publish(r_url, json.dumps({"id": rid, "out": out, "err": err, "rc": rc}))
                    print(f"  [{ts}] < exit {rc} ({len(out)} octets)", flush=True)
        except Exception as e:
            print("  reconnexion:", str(e)[:70], flush=True)
            time.sleep(2)

if __name__ == "__main__":
    main()
