#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"
REPO_ROOT="$(git rev-parse --show-toplevel)"

# Stamp current version from YUNETA_VERSION into index.md
VERSION=$(sed 's/YUNETA_VERSION=//' "$REPO_ROOT/YUNETA_VERSION")
TODAY=$(date +%Y-%m-%d)
sed -i "s|\*\*Current version: \[.*\](.*)\*\*|\*\*Current version: [${VERSION}](https://github.com/artgins/yunetas/tree/${VERSION})\*\*|" index.md
sed -i "s|\*Documentation updated: .*\*|\*Documentation updated: ${TODAY}\*|" index.md

# Stamp the yunetas CLI (PyPI) version into installation.md from its single
# source of truth (the package's __version__.py). Pattern matches the whole
# "(currently …)" parenthetical, so it is idempotent across re-runs.
CLI_VERSION=$(sed -n 's/^__version__ = "\(.*\)"/\1/p' "$REPO_ROOT/utils/python/tui_yunetas/yunetas/__version__.py")
sed -i "s|management/build CLI (currently [^)]*)|management/build CLI (currently ${CLI_VERSION})|" installation.md

# Build static HTML with empty BASE_URL so the site works at the domain root
# myst build --html spawns a temporary node server that doesn't always exit;
# snapshot node PIDs before and kill only the new ones after the build.
export BASE_URL=""
BEFORE=$(pgrep -x node 2>/dev/null | sort || true)
myst build --html
AFTER=$(pgrep -x node 2>/dev/null | sort || true)
NEW_PIDS=$(comm -13 <(echo "$BEFORE") <(echo "$AFTER"))
if [ -n "$NEW_PIDS" ]; then
    kill $NEW_PIDS 2>/dev/null || true
fi

ORIGIN="./_build/html/"
DESTINE="yuneta@yuneta.io:/yuneta/gui/doc.yuneta.io/"

# Safety guard: a failed/empty build must never reach the --delete rsync, or it
# would wipe the live site. Require the built entry page before syncing.
if [ ! -f "${ORIGIN}index.html" ]; then
    echo "ERROR: ${ORIGIN}index.html missing; build failed or incomplete — aborting deploy." >&2
    exit 1
fi

# Anchor-scroll fix: the mystmd React theme resets scroll to the top on
# hydration, so a deep link (`/page#fragment`) lands at the page top instead of
# the anchor — even though the SSR HTML already has the heading id and nginx
# does no redirect (verified: try_files, no 301). Inject a tiny script into
# every built page that re-applies the #fragment scroll for ~2.5s after load,
# beating the theme's reset, and stops as soon as the user scrolls.
python3 - "$ORIGIN" <<'PYEOF'
import sys, pathlib
root = pathlib.Path(sys.argv[1])
marker = 'location.hash.slice(1)'
script = ('<script>(function(){if(!location.hash)return;'
          'var id=decodeURIComponent(location.hash.slice(1)),n=0;'
          'var iv=setInterval(function(){var el=document.getElementById(id);'
          'if(el)el.scrollIntoView();if(++n>25)clearInterval(iv);},100);'
          '["wheel","touchmove","keydown","pointerdown"].forEach(function(e){'
          'window.addEventListener(e,function(){clearInterval(iv);},'
          '{passive:true,once:true});});})();</script>')
count = 0
for f in root.rglob('index.html'):
    html = f.read_text(encoding='utf-8')
    if marker in html or '</body>' not in html:
        continue
    f.write_text(html.replace('</body>', script + '</body>', 1), encoding='utf-8')
    count += 1
print(f"anchor-scroll fix injected into {count} pages")
PYEOF

# Diagram lightbox: clicking any content image (the markdown-embedded diagrams,
# identified by data-canonical-url — theme chrome lacks it) opens it full-screen
# over a dark backdrop, with a caption taken from the image's alt text (falling
# back to title, then a sibling <figcaption>). Click anywhere or press Esc to
# close. Styling lives in _static/custom.css; this only wires the behaviour.
# mystmd's book-theme has no native custom-JS hook, so inject it the same way as
# the anchor-scroll fix.
python3 - "$ORIGIN" <<'PYEOF'
import sys, pathlib
root = pathlib.Path(sys.argv[1])
marker = 'yuneta-lightbox'
script = ('<script>(function(){'
          'function close(){var o=document.getElementById("yuneta-lightbox");'
          'if(o){o.classList.remove("open");o.firstChild.removeAttribute("src");}'
          'document.documentElement.style.overflow="";}'
          'function open(src,cap){var o=document.getElementById("yuneta-lightbox");'
          'if(!o){o=document.createElement("div");o.id="yuneta-lightbox";'
          'o.appendChild(document.createElement("img"));'
          'var fc=document.createElement("figcaption");'
          'fc.id="yuneta-lightbox-cap";o.appendChild(fc);'
          'o.addEventListener("click",close);document.body.appendChild(o);}'
          'o.firstChild.src=src;'
          'var c=document.getElementById("yuneta-lightbox-cap");'
          'c.textContent=cap||"";c.style.display=cap?"block":"none";'
          'o.classList.add("open");'
          'document.documentElement.style.overflow="hidden";}'
          'document.addEventListener("click",function(e){'
          'var img=e.target.closest&&e.target.closest("img[data-canonical-url]");'
          'if(!img)return;e.preventDefault();'
          'var cap=img.getAttribute("alt")||img.getAttribute("title")||"";'
          'if(!cap){var fig=img.closest("figure");'
          'var fc=fig&&fig.querySelector("figcaption");'
          'if(fc){cap=fc.textContent.trim();}}'
          'open(img.currentSrc||img.src,cap);});'
          'document.addEventListener("keydown",function(e){'
          'if(e.key==="Escape")close();});})();</script>')
count = 0
for f in root.rglob('index.html'):
    html = f.read_text(encoding='utf-8')
    if marker in html or '</body>' not in html:
        continue
    f.write_text(html.replace('</body>', script + '</body>', 1), encoding='utf-8')
    count += 1
print(f"diagram lightbox injected into {count} pages")
PYEOF

# --delete mirrors the build onto the server: pages and content-hashed assets
# dropped from the build (renamed/moved TOC nodes, stale assets) are removed on
# the server too. --delete-after defers removals until the transfer succeeds.
rsync -auvzL --delete-after -e ssh \
    "$ORIGIN" "$DESTINE"
