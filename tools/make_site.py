#!/usr/bin/env python3
"""Builds the site that GitHub Pages serves: the web updater (web/) and every published release, with the list the page reads.

usage: make_site.py OUT_DIR                       the real thing, in CI: web/ + every GitHub Release (needs the `gh` command and GH_TOKEN)
       make_site.py OUT_DIR --local FILE.hex TAG  a site with just that file as release TAG, to try the page locally (no network)

What the site contains:
    OUT_DIR/index.html, style.css, js/...    a copy of web/ (without the package.json, which is only for Node's tests)
    OUT_DIR/releases/<tag>/oblio.hex          each release's hex, downloaded from its GitHub Release
    OUT_DIR/releases.json                     {"releases": [{tag, version, date, file, sha256, size, rev}]}, newest first

Why the hex files are copied to the site: a browser cannot download the assets of a GitHub Release (the download redirects to a server that sends
no CORS headers), but it can read files from the same site the page comes from. The Releases stay the source of truth: the site is rebuilt from
them at every release, so nothing binary lives in the repository.
Each release is checked as it is copied: its image must carry the version of its tag (tools/check_version.py), otherwise it is left out with a note.
"""

import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from check_version import version_of  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def add_release(out, tag, date, source, releases):
    """Copy `source` (a hex file) to releases/<tag>/oblio.hex and record it, if its version agrees with the tag."""
    info = version_of(str(source))
    if tag != "v" + info["version"]:
        print("make_site: leaving out {}: the image says version {}".format(tag, info["version"]), file=sys.stderr)
        return
    target = out / "releases" / tag / "oblio.hex"
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, target)
    releases.append({
        "tag": tag,
        "version": info["version"],
        "rev": info["rev"],
        "date": date,
        "file": "releases/{}/oblio.hex".format(tag),
        "sha256": sha256(target),
        "size": target.stat().st_size,
    })


def from_github(out):
    """Every published release of the repository, newest first. Uses the gh command (it is on the CI runners)."""
    listing = subprocess.run(["gh", "release", "list", "--limit", "100", "--json", "tagName,publishedAt,isDraft,isPrerelease"],
                             check=True, capture_output=True, text=True).stdout
    releases = []
    scratch = out / "_download"
    for item in json.loads(listing):
        if item["isDraft"]:
            continue
        tag = item["tagName"]
        folder = scratch / tag
        subprocess.run(["gh", "release", "download", tag, "--pattern", "oblio.hex", "--dir", str(folder)], check=True)
        add_release(out, tag, item["publishedAt"][:10], folder / "oblio.hex", releases)
    shutil.rmtree(scratch, ignore_errors=True)
    return releases


def main():
    args = sys.argv[1:]
    if not args:
        sys.exit(__doc__)
    out = Path(args[0])
    shutil.rmtree(out, ignore_errors=True)
    shutil.copytree(ROOT / "web", out, ignore=shutil.ignore_patterns("package.json", "node_modules"))

    releases = []
    if "--local" in args:
        at = args.index("--local")
        add_release(out, args[at + 2], "local", Path(args[at + 1]), releases)
    else:
        releases = from_github(out)

    releases.sort(key=lambda r: [int(x) for x in r["version"].split(".")], reverse=True)
    (out / "releases.json").write_text(json.dumps({"releases": releases}, indent=2) + "\n")
    print("make_site: {} release(s) in {}".format(len(releases), out))


if __name__ == "__main__":
    main()
