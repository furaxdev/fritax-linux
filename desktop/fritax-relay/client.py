#!/usr/bin/env python3
"""Fritax Relay - client : envoyer une commande, attendre la reponse (streaming)."""
import argparse, hashlib, json, socket, sys, time, urllib.request, uuid

BASE = "https://ntfy.sh"

def topic(secret, suffix):
    return "fritax-relay-" + hashlib.sha256(secret.encode()).hexdigest()[:20] + "-" + suffix

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--secret", required=True)
    ap.add_argument("--timeout", type=int, default=120)
    ap.add_argument("cmd")
    a = ap.parse_args()
    rid = uuid.uuid4().hex[:12]
    c_url = BASE + "/" + topic(a.secret, "c")
    r_url = BASE + "/" + topic(a.secret, "r")
    try:
        req = urllib.request.Request(c_url, data=json.dumps({"id": rid, "cmd": a.cmd}).encode(), method="POST")
        urllib.request.urlopen(req, timeout=20).read()
    except Exception as e:
        print("envoi impossible:", str(e)[:90], file=sys.stderr)
        return 2
    deadline = time.time() + a.timeout
    # on ecoute le canal resultat en continu (since=10s pour ne rien rater)
    while time.time() < deadline:
        try:
            req = urllib.request.Request(r_url + "/json?since=10s")
            with urllib.request.urlopen(req, timeout=25) as resp:
                for raw in resp:
                    line = raw.strip()
                    if not line:
                        continue
                    try:
                        m = json.loads(line.decode())
                    except Exception:
                        continue
                    if m.get("event") != "message":
                        continue
                    try:
                        p = json.loads(m.get("message", ""))
                    except Exception:
                        continue
                    if p.get("id") != rid:
                        continue
                    sys.stdout.write(p.get("out", ""))
                    if p.get("err"):
                        sys.stderr.write(p["err"])
                    return int(p.get("rc", 0))
        except socket.timeout:
            continue
        except Exception:
            time.sleep(1)
    print("delai d'attente depasse", file=sys.stderr)
    return 3

if __name__ == "__main__":
    sys.exit(main())
