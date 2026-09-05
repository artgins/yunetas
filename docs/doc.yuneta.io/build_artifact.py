#!/usr/bin/env python3
#
#   Turn a standalone page of this site into an artifact body.
#
#   The pages under docs/doc.yuneta.io/<slug>/index.html are whole HTML
#   documents served by this site. An artifact on claude.ai is not: the
#   host supplies <!doctype>, <html>, <head> and <body>, so the page must
#   ship as the CONTENT of that body plus its own <title> and <style>.
#
#   Three differences, and each one is a real bug if it is skipped:
#
#     1. The site chrome goes. The top bar links to "/" and carries a
#        theme toggle; on an artifact both point at nothing.
#     2. The theme init script goes. It stamps data-theme from
#        localStorage["myst:theme"], which is this SITE's key -- on an
#        artifact the host stamps data-theme itself, and a second writer
#        makes the page argue with the reader's own choice.
#     3. And because nothing stamps data-theme any more in the default
#        "system" state, the dark tokens have to be reachable through
#        prefers-color-scheme too. The :root[data-theme="dark"] block is
#        copied into a media query guarded with :not([data-theme="light"]),
#        so an explicit light choice still wins over a dark OS.
#
#   Usage:
#       python3 build_artifact.py <slug> [out.html]
#

import os
import re
import sys


#
#   Cut points
#
def slice_between(text, start_pat, end_pat, what):
    start = re.search(start_pat, text)
    end = re.search(end_pat, text)
    if not start or not end:
        raise SystemExit("cannot find the %s of the page" % what)
    return text[start.start():end.end()]


#
#   The dark block, reachable without a stamp
#
def add_prefers_dark(css):
    m = re.search(
        r'  :root\[data-theme="dark"\] \{\n(.*?)\n  \}\n',
        css,
        re.S
    )
    if not m:
        raise SystemExit("cannot find the :root[data-theme=\"dark\"] block")
    body = m.group(1)
    media = (
        '\n  /*  No stamp in the default "system" state: the same tokens,\n'
        '      reached through the media query, with an explicit light\n'
        '      choice still winning.  */\n'
        '  @media (prefers-color-scheme: dark) {\n'
        '    :root:not([data-theme="light"]) {\n'
        + "\n".join(("  " + ln) if ln.strip() else ln for ln in body.split("\n"))
        + "\n    }\n  }\n"
    )
    return css[:m.end()] + media + css[m.end():]


def main():
    if len(sys.argv) < 2:
        raise SystemExit("usage: build_artifact.py <slug> [out.html]")
    slug = sys.argv[1].strip("/")
    here = os.path.dirname(os.path.abspath(__file__))
    src = os.path.join(here, slug, "index.html")
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join("/tmp", slug + ".html")

    with open(src, encoding="utf-8") as fd:
        page = fd.read()

    title = slice_between(page, r"<title>", r"</title>\n", "title")
    css = slice_between(page, r"<style>", r"</style>\n", "stylesheet")
    css = add_prefers_dark(css)

    body = slice_between(page, r"<body>\n", r"\n</body>", "body")
    body = body[len("<body>\n"):-len("\n</body>")]

    #
    #   The site chrome: the back link and the theme toggle
    #
    body, n = re.subn(r'<nav class="topbar">.*?</nav>\n\n', "", body, flags=re.S)
    if n != 1:
        raise SystemExit("cannot find the top bar to remove")

    #
    #   The toggle's own handler goes with the button it drove
    #
    body, n = re.subn(
        r'  var STORE_KEY = "myst:theme";\n'
        r'  function applyTheme.*?'
        r'applyTheme\(e\.matches \? "dark" : "light"\);\n    \}\n  \}\);\n',
        "",
        body,
        flags=re.S
    )
    if n != 1:
        raise SystemExit("cannot find the theme handlers to remove")

    #
    #   A script block that held nothing else goes with them: an empty
    #   IIFE is not wrong, it is just litter in a published page.
    #
    body = re.sub(
        r'\n*<script>\n\(function \(\) \{\n  "use strict";\n\s*\}\(\)\);\n</script>(?:\n|$)',
        "",
        body
    )

    #
    #   An artifact is served from its own origin, so a root-relative
    #   href points at nothing. The pages of this site are the ones being
    #   named, so they take their real address.
    #
    body = re.sub(r'href="/([a-z0-9-]+)"', r'href="https://doc.yuneta.io/\1"', body)

    for leftover in ("myst:theme", "theme-toggle", "STORE_KEY", "applyTheme"):
        if leftover in body:
            raise SystemExit("site theme machinery survived the strip: " + leftover)

    with open(out, "w", encoding="utf-8") as fd:
        fd.write(title + "\n" + css + "\n" + body + "\n")

    print("%s -> %s (%d bytes)" % (src, out, os.path.getsize(out)))


main()
