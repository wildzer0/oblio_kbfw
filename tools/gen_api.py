#!/usr/bin/env python3
"""Writes docs/api.md: the reference of every header of hal/, core/ and board/, taken from the headers themselves.

Why not Doxygen: the headers are documented with plain `//` comments written as prose above each declaration, and Doxygen only
reads `///` or `/** */`. Rewriting 55 headers to feed a tool would add noise to the code; this script reads what is already there, so
the reference cannot drift from the code (`make api` regenerates it, `make check-api` fails if the committed copy is stale).

What it takes from a header:
  * the first comment block after the includes is the module's description;
  * a comment directly above a declaration (no blank line between) is that declaration's documentation;
  * a comment block set apart by blank lines is a note that belongs to the section it is in;
  * a `// ...` after a declaration on the same line is added to its documentation.

usage: gen_api.py [--check]        (writes docs/api.md; with --check only compares and exits 1 if it differs)
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "docs", "api.md")

LAYERS = [
    ("hal", "Hardware abstraction (`hal/`)", "The chip: registers, the timer, USB controller, flash, PWM, power. Nothing here knows what a keyboard is."),
    ("core", "Core logic (`core/`)", "Hardware-independent C, tested on the host with gcc. It reaches the hardware only through `*_hook_*` functions that the board (or a test) provides."),
    ("board", "Board (`board/`)", "The B68 itself: pins, the matrix, LEDs, the radio's serial port, sleep, the keycodes of the board, and the glue that implements the hooks of `core/`."),
]

SKIP = {"sh68f90.h"}  # the register map: a list of names, not an API


def clean(comment_lines):
    text = []
    for line in comment_lines:
        line = re.sub(r"^\s*//\s?", "", line.rstrip())
        text.append(line)
    # keep the author's line breaks as paragraphs: a line break inside a sentence is only wrapping
    paragraphs, current = [], []
    for line in text:
        if line.strip() == "":
            if current:
                paragraphs.append(" ".join(current))
                current = []
        elif re.match(r"^\s*([-*]|\d+\.)\s", line) or line.startswith("    "):
            if current:
                paragraphs.append(" ".join(current))
                current = []
            paragraphs.append(line.strip())
        else:
            current.append(line.strip())
    if current:
        paragraphs.append(" ".join(current))
    return paragraphs


def render_paragraphs(paragraphs):
    out = []
    for p in paragraphs:
        if re.match(r"^([-*]|\d+\.)\s", p):
            out.append(p)
        else:
            out.append(p)
            out.append("")
    return "\n".join(out).rstrip() + "\n" if out else ""


def split_trailing(line):
    """(code, comment) of a line with a `// ...` at its end."""
    m = re.match(r"^(.*?\S)\s*//\s?(.*)$", line)
    if m and '"' not in m.group(1)[m.group(1).rfind("//") + 2:]:
        return m.group(1), m.group(2).strip()
    return line.rstrip(), ""


def decl_name(code):
    first = code.strip().splitlines()[0].strip()
    m = re.match(r"#\s*define\s+(\w+)", first)
    if m:
        return m.group(1)
    flat = " ".join(l.strip() for l in code.splitlines())
    m = re.search(r"}\s*(\w+)\s*;", flat)
    if m:
        return m.group(1)
    m = re.search(r"(\w+)\s*\([^()]*\)\s*(?:__\w+\([^)]*\))?\s*(?:\{|;)", flat)
    if m and not flat.startswith("typedef"):
        return m.group(1)
    m = re.search(r"(\w+)\s*(?:\[[^\]]*\])?\s*;", flat)
    if m:
        return m.group(1)
    return first[:40]


def parse(path):
    lines = open(path, encoding="utf-8").read().splitlines()
    items = []          # ("note", paragraphs) | ("decl", name, code, paragraphs)
    pending = []        # comment lines directly above the next declaration
    module_doc = None
    i = 0
    seen_decl = False

    def flush_pending_as_note():
        nonlocal pending, module_doc
        if pending:
            paragraphs = clean(pending)
            if module_doc is None and not seen_decl:
                module_doc = paragraphs
            else:
                items.append(("note", paragraphs))
            pending = []

    while i < len(lines):
        line = lines[i]
        stripped = line.strip()

        if stripped.startswith("//"):
            pending.append(line)
            i += 1
            continue

        if stripped == "":
            flush_pending_as_note()
            i += 1
            continue

        if re.match(r"#\s*(pragma|include|if|ifdef|ifndef|else|elif|endif|undef)\b", stripped):
            if pending and not re.match(r"#\s*include", stripped):
                flush_pending_as_note()
            i += 1
            continue

        if stripped.startswith("_Static_assert"):
            pending = []
            while i < len(lines) and ";" not in lines[i]:
                i += 1
            i += 1
            continue

        # a declaration: a #define (with continuations) or a statement up to its ';' / closing brace
        block = []
        if stripped.startswith("#"):
            while True:
                block.append(lines[i])
                if not lines[i].rstrip().endswith("\\"):
                    break
                i += 1
            i += 1
        else:
            depth = 0
            while i < len(lines):
                block.append(lines[i])
                code, _ = split_trailing(lines[i])
                depth += code.count("{") - code.count("}")
                done = depth <= 0 and (code.rstrip().endswith(";") or (code.rstrip().endswith("}") and "(" in " ".join(block)))
                i += 1
                if done:
                    break

        trailing = ""
        if len(block) == 1:
            code, trailing = split_trailing(block[0])
            code_text = code
        else:
            code_text = "\n".join(block)

        # a function with a body (a static inline) is shown as its signature only
        if "{" in code_text and re.search(r"\)\s*\{", code_text) and not code_text.lstrip().startswith("typedef"):
            code_text = code_text.split("{", 1)[0].rstrip() + ";"

        paragraphs = clean(pending)
        pending = []
        seen_decl = True
        items.append(("decl", decl_name(code_text), code_text, paragraphs, trailing))

    flush_pending_as_note()
    return module_doc or [], items


def module_blurb(paragraphs):
    return paragraphs[0] if paragraphs else ""


def anchor(path):
    return re.sub(r"[^a-z0-9]+", "-", path.lower()).strip("-")


def main():
    out = []
    out.append("# Oblio — API reference\n")
    out.append("*Generated by `tools/gen_api.py` from the comments in the headers: do not edit by hand (`make api`). "
               "How the modules fit together is in [design.md](design.md).*\n")

    toc = []
    body = []

    for directory, title, intro in LAYERS:
        headers = sorted(f for f in os.listdir(os.path.join(ROOT, directory)) if f.endswith(".h") and f not in SKIP)
        body.append("\n---\n\n## %s\n\n%s\n" % (title, intro))
        toc.append("- **%s**" % title)

        for header in headers:
            path = "%s/%s" % (directory, header)
            module_doc, items = parse(os.path.join(ROOT, path))
            blurb = module_blurb(module_doc).split(". ")[0].rstrip(".")[:110]
            toc.append("  - [`%s`](#%s)%s" % (path, anchor(path), (" — " + blurb) if blurb else ""))

            body.append("\n### `%s`\n" % path)
            if module_doc:
                body.append(render_paragraphs(module_doc))

            constants = []   # consecutive #defines without a comment of their own go in one block

            def flush_constants():
                if constants:
                    body.append("\n```c\n%s\n```\n" % "\n".join(constants))
                    constants.clear()

            for item in items:
                if item[0] == "note":
                    flush_constants()
                    body.append(render_paragraphs(item[1]))
                    continue
                _, name, code, paragraphs, trailing = item
                if code.lstrip().startswith("#") and not paragraphs and "\n" not in code:
                    constants.append(code + ("  // " + trailing if trailing else ""))
                    continue
                flush_constants()
                if trailing:
                    paragraphs = paragraphs + [trailing]
                body.append("\n#### `%s`\n" % name)
                body.append("```c\n%s\n```\n" % code.strip("\n"))
                if paragraphs:
                    body.append(render_paragraphs(paragraphs))

            flush_constants()

    out.append("\n".join(toc))
    out.append("".join("\n" + b if not b.startswith("\n") else b for b in body))
    text = "\n".join(out).rstrip() + "\n"
    text = re.sub(r"\n{3,}", "\n\n", text)

    if "--check" in sys.argv:
        current = open(OUT, encoding="utf-8").read() if os.path.exists(OUT) else ""
        if current != text:
            sys.exit("docs/api.md is out of date: run `make api`")
        print("docs/api.md is up to date")
        return

    with open(OUT, "w", encoding="utf-8") as f:
        f.write(text)
    print("wrote %s (%d lines)" % (OUT, text.count("\n")))


if __name__ == "__main__":
    main()
