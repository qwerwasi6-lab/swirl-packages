# swirl-packages

Package registry for **spm**, the SwirlOS package manager.

    spm update
    spm search
    spm install welcome

## Layout

    index.json                 generated - what `spm update` downloads
    packages/<name>/package.json   metadata + file list
    packages/<name>/...            the files themselves
    tools/mkindex.py           regenerates index.json (hashes + sizes)

## package.json

    {
      "name": "welcome",
      "version": "1.0.0",
      "description": "one line",
      "author": "you",
      "deps": ["other-package"],
      "files": [ { "src": "welcome.json", "dest": "/system/gterm/welcome.json" } ]
    }

`dest` must be an absolute path under `/system/` or `/home/user/`
(never `/system/kernel/` or `/system/spm/`). Terminal commands are just
JSON files in `/system/gterm/` (see `/system/gterm/guide.md` on SwirlOS).

## Publishing

1. Add `packages/<name>/` with `package.json` and the files.
2. Run `python3 tools/mkindex.py`.
3. Commit `index.json` together with the package and push to `main`.

Bump `version` when a package changes; `spm upgrade` compares versions.

## System releases (SwirlOS updates)

Besides packages, this registry carries the SwirlOS system itself, so installed
machines can update with `spm os upgrade` (or Settings > Software Update).

    # in the SwirlOS source tree:  make            (writes build/content_stage + os.tsv)
    # here:
    python3 tools/publish_os.py --stage <swirlos>/build/content_stage --notes "What changed."
    python3 tools/mkindex.py
    git add -A && git commit -m "SwirlOS <version>" && git push

`publish_os.py` stores every system file once as `os/<aa>/<sha40>` (unchanged files are
never stored or downloaded again) and writes `os/release.json`; `mkindex.py` embeds it
under `"os"` in `index.json`. The version comes from the SwirlOS `VERSION` file
(bump it for each release). A release that needs a newer kernel sets `abi` higher than
the booted kernel's `SWIRL_ABI` and is refused on older kernels.

## Updating installs from before r63 (r61, r62)

Those installs have no system manifest and an old spm, so `spm upgrade`/`spm os`
cannot update them. Boot a SwirlOS r63+ ISO (the kernel is on the boot medium) with your
disk attached, then:

    spm install swirl-upgrade -f
    swirl-upgrade            # or: swirl-upgrade check
    # restart

`packages/swirl-upgrade/swirl-upgrade.elf` is the spm built from the same release as
`os/` (copy `build/spm.elf` there when you publish a release). It compares every system
file by content, downloads only what differs and installs under the usual journal.
