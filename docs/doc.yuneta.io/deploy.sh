#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"
REPO_ROOT="$(git rev-parse --show-toplevel)"

# Stamp current version from YUNETA_VERSION into index.md
VERSION=$(sed 's/YUNETA_VERSION=//' "$REPO_ROOT/YUNETA_VERSION")
TODAY=$(date +%Y-%m-%d)
sed -i "s|\*\*Current version: \[.*\](.*)\*\*|\*\*Current version: [${VERSION}](https://github.com/artgins/yunetas/tree/${VERSION})\*\*|" index.md
sed -i "s|\*Documentation updated: .*\*|\*Documentation updated: ${TODAY}\*|" index.md

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

# --delete mirrors the build onto the server: pages and content-hashed assets
# dropped from the build (renamed/moved TOC nodes, stale assets) are removed on
# the server too. --delete-after defers removals until the transfer succeeds.
rsync -auvzL --delete-after -e ssh \
    "$ORIGIN" "$DESTINE"
