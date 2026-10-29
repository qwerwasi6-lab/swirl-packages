#!/usr/bin/env python3
"""publish_os.py - publish a SwirlOS system release into this registry.

    python3 tools/publish_os.py --stage <swirlos>/build/content_stage \\
            --notes "What changed, in a sentence or two."
    python3 tools/mkindex.py          # embeds the release into index.json
    git add -A && git commit && git push

Reads the manifest the SwirlOS build wrote (system/spm/os.tsv inside the
stage folder: version, kernel ABI, every system file with SHA-256) and:
  * copies each file into os/<aa>/<sha256> (content-addressed: a file that did
    not change between releases is stored once and never re-downloaded),
  * writes os/release.json, which mkindex.py puts under "os" in index.json.

Old blobs are kept so a device a few releases behind can still update.
Use --prune to drop blobs the newest release no longer references.
"""
import argparse, datetime, hashlib, json, os, shutil, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--stage", required=True)
    ap.add_argument("--notes", default="")
    ap.add_argument("--prune", action="store_true")
    ap.add_argument("--out", default=ROOT, help="registry root (default: this repo)")
    a = ap.parse_args()

    tsv = os.path.join(a.stage, "system", "spm", "os.tsv")
    if not os.path.isfile(tsv):
        sys.exit(f"publish_os: {tsv} not found - build SwirlOS first")
    lines = open(tsv).read().splitlines()
    head = lines[0].split("\t")
    if head[0] != "#swirlos":
        sys.exit("publish_os: bad manifest header")
    version = head[1]
    abi = int(head[2].split("=")[1])

    files, used = [], set()
    blobdir = os.path.join(a.out, "os")
    new = 0
    for ln in lines[1:]:
        dest, sha, size, flags = ln.split("\t")
        src = os.path.join(a.stage, dest.lstrip("/"))
        data = open(src, "rb").read()
        if hashlib.sha256(data).hexdigest() != sha or len(data) != int(size):
            sys.exit(f"publish_os: {dest} changed since the manifest was written - rebuild")
        rel = f"os/{sha[:2]}/{sha[:40]}"   # 40 hex: fits SwirlFS names too (a registry can live on a SwirlFS volume)
        used.add(rel)
        dst = os.path.join(a.out, rel)
        if not os.path.exists(dst):
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copyfile(src, dst)
            new += 1
        e = {"path": rel, "dest": dest, "sha256": sha, "size": int(size)}
        if "k" in flags:
            e["keep"] = 1
        files.append(e)

    rel = {"name": "SwirlOS", "version": version, "abi": abi,
           "date": datetime.date.today().isoformat(), "notes": a.notes, "files": files}
    os.makedirs(blobdir, exist_ok=True)
    with open(os.path.join(blobdir, "release.json"), "w") as f:
        json.dump(rel, f, indent=1)
        f.write("\n")

    pruned = 0
    if a.prune:
        for dp, _, fn in os.walk(blobdir):
            for f in fn:
                full = os.path.join(dp, f)
                r = os.path.relpath(full, a.out).replace(os.sep, "/")
                if r != "os/release.json" and r not in used:
                    os.remove(full)
                    pruned += 1
    total = sum(e["size"] for e in files)
    print(f"publish_os: {version} (abi {abi}): {len(files)} files, {total/1048576:.1f} MB total, "
          f"{new} new blob(s){', %d pruned' % pruned if a.prune else ''}")


if __name__ == "__main__":
    main()
