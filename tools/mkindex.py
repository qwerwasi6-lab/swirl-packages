#!/usr/bin/env python3
"""mkindex.py - build index.json for an spm registry.

Run from the repository root after adding or changing a package:

    python3 tools/mkindex.py

For every packages/<name>/package.json it hashes the listed source files
(SHA-256 + size) and writes the combined index.json that `spm update`
downloads. Commit and push index.json together with the package files.

package.json:
  { "name": "foo", "version": "1.0.0", "description": "...",
    "author": "you", "deps": ["other-pkg"],
    "files": [ { "src": "foo.json", "dest": "/system/gterm/foo.json" } ] }
  - src  = file inside packages/<name>/ (sub-folders are fine)
  - dest = absolute install path on SwirlOS: under /system/ or /home/user/
           (spm refuses /system/kernel/ and /system/spm/)
"""
import hashlib, json, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PKG_DIR = os.path.join(ROOT, "packages")
NAME_RE = re.compile(r"^[a-z0-9_-][a-z0-9._-]{0,31}$")
RPATH_RE = re.compile(r"^[A-Za-z0-9._/+@~-]+$")


def die(msg):
    print("mkindex: error:", msg, file=sys.stderr)
    sys.exit(1)


def dest_ok(d):
    if not d.startswith("/") or len(d) > 160 or d.endswith("/") or ".." in d or "//" in d:
        return False
    if not (d.startswith("/system/") or d.startswith("/home/user/")):
        return False
    return not (d.startswith("/system/kernel/") or d.startswith("/system/spm/"))


def main():
    packages = {}
    for name in sorted(os.listdir(PKG_DIR)):
        pdir = os.path.join(PKG_DIR, name)
        meta_path = os.path.join(pdir, "package.json")
        if not os.path.isfile(meta_path):
            continue
        with open(meta_path) as f:
            meta = json.load(f)
        if meta.get("name") != name:
            die(f"{meta_path}: 'name' must equal the folder name '{name}'")
        if not NAME_RE.match(name):
            die(f"bad package name '{name}' (a-z 0-9 . _ - , max 32)")
        ver = meta.get("version")
        if not ver or not isinstance(ver, str):
            die(f"{name}: missing 'version'")
        files = []
        for ent in meta.get("files", []):
            src, dest = ent.get("src"), ent.get("dest")
            if not src or not dest:
                die(f"{name}: every file needs 'src' and 'dest'")
            rel = f"packages/{name}/{src}"
            if not RPATH_RE.match(rel) or ".." in rel:
                die(f"{name}: bad src path '{src}'")
            if not dest_ok(dest):
                die(f"{name}: bad dest '{dest}'")
            fpath = os.path.join(ROOT, rel)
            if not os.path.isfile(fpath):
                die(f"{name}: missing file {rel}")
            data = open(fpath, "rb").read()
            files.append({"path": rel, "dest": dest,
                          "sha256": hashlib.sha256(data).hexdigest(), "size": len(data)})
        if not files:
            die(f"{name}: no files listed")
        entry = {"version": ver, "description": meta.get("description", "")}
        if meta.get("author"):
            entry["author"] = meta["author"]
        if meta.get("deps"):
            entry["deps"] = list(meta["deps"])
        entry["files"] = files
        packages[name] = entry
    for name, e in packages.items():
        for d in e.get("deps", []):
            if d not in packages:
                die(f"{name}: depends on '{d}' which is not in this registry")
    index = {"spm": 1, "packages": packages}
    out = os.path.join(ROOT, "index.json")
    with open(out, "w") as f:
        json.dump(index, f, indent=2)
        f.write("\n")
    print(f"mkindex: wrote index.json with {len(packages)} package(s)")


if __name__ == "__main__":
    main()
