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

# DEP0169 (url.parse) comes from the book-theme's own bundle -- Remix's server
# runtime calls it while rendering, on node 24 -- so there is nothing to fix
# here, and it prints on every build. It is silenced by CODE, not with a blanket
# --no-deprecation: the build log is grepped for "warning" to validate an edit,
# and a permanent false hit there is worse than the warning. Any OTHER
# deprecation still comes through, which is the point.
export NODE_OPTIONS="${NODE_OPTIONS:-} --disable-warning=DEP0169"
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

# Theme hand-off to demo.yuneta.io: localStorage is per ORIGIN, so the demo
# on its own host cannot read the reader's light/dark choice and opens on its
# own default.  Carry it in the url instead.  The landing does this in its own
# script; these are the myst pages, where the link is plain markdown.
#
# Stamped at CLICK time, not at load: this theme is a React app that re-renders
# on navigation and on the theme toggle, so an href written once goes stale or
# disappears with the node that held it.  A delegated listener has neither
# problem and needs no observer.
#
# The theme is the `dark` class the book-theme puts on <html> (Tailwind), NOT a
# data-theme attribute — the landing uses data-theme, the docs use the class,
# and both agree on the localStorage key.
python3 - "$ORIGIN" <<'PYEOF'
import sys, pathlib
root = pathlib.Path(sys.argv[1])
marker = 'yuneta-demo-theme'
script = ('<script>/*yuneta-demo-theme*/(function(){'
          'document.addEventListener("click",function(e){'
          'var a=e.target.closest&&e.target.closest(\'a[href^="https://demo.yuneta.io"]\');'
          'if(!a)return;'
          'var d=document.documentElement.classList.contains("dark");'
          'try{if(!d&&localStorage.getItem("myst:theme")==="dark")d=true;}catch(x){}'
          'a.href="https://demo.yuneta.io/?theme="+(d?"dark":"light");'
          '},true);})();</script>')
count = 0
for f in root.rglob('index.html'):
    html = f.read_text(encoding='utf-8')
    if marker in html or '</body>' not in html:
        continue
    f.write_text(html.replace('</body>', script + '</body>', 1), encoding='utf-8')
    count += 1
print(f"demo theme hand-off injected into {count} pages")
PYEOF

# PWA manifest: makes the docs installable, so the reader gets them in their
# own window with their own icon instead of one more tab.  There is no service
# worker, hence no offline reading -- an installed copy still needs the network.
#
# The book-theme has no hook to add anything to <head>, so the two link tags go
# in here, the same way as the scripts above but before </head>: a <link
# rel="manifest"> in the body is ignored.
#
# The manifest itself is served at /manifest.webmanifest by ONE nginx location,
# in the doc.yuneta.io vhost alone, aliased to the /pwa/ copy installed below.
# The docroot is shared with yuneta.io, yuneta.com, yuneta.es and yunetas.com,
# where "/" is the landing, not these pages -- if the file sat at the root of
# the docroot, all five domains would offer to install a different app under
# the same name.  On those four the link 404s and no install is offered, which
# is the intent.
python3 - "$ORIGIN" <<'PYEOF'
import sys, pathlib
root = pathlib.Path(sys.argv[1])
marker = 'rel="manifest"'
links = ('<link rel="manifest" href="/manifest.webmanifest">'
         '<link rel="apple-touch-icon" href="/pwa/icon-192.png">'
         '<script src="/pwa/register-sw.js" defer></script>')
count = 0
for f in root.rglob('index.html'):
    html = f.read_text(encoding='utf-8')
    if marker in html or '</head>' not in html:
        continue
    f.write_text(html.replace('</head>', links + '</head>', 1), encoding='utf-8')
    count += 1
print(f"pwa manifest link injected into {count} pages")
PYEOF

# Landing page: a standalone HTML document (its own inlined fonts and CSS, no
# external requests) served at /landing.  It shares the theme with the site
# through localStorage["myst:theme"], same origin.  myst copies no raw files,
# so it is placed into the build output here — after the two injections above,
# so it is left pristine, and before the --delete rsync, so the mirror keeps it.
mkdir -p "${ORIGIN}landing"
# The version lives in the source as __YUNETA_VERSION__ and is stamped
# here, the same way index.md is above: written by hand in four places it
# went stale at every release, and this page is the front door served at
# yuneta.io.
sed "s|__YUNETA_VERSION__|${VERSION}|g" landing/index.html > "${ORIGIN}landing/index.html"
if grep -q "__YUNETA_VERSION__" "${ORIGIN}landing/index.html"; then
    echo "ERROR: landing page still carries an unsubstituted __YUNETA_VERSION__" >&2
    exit 1
fi
echo "landing page installed at /landing (version ${VERSION})"

# The 404 page, for the `error_page 404 /404.html` of every server block that
# serves this docroot -- five hostnames share it.  Installed here for the same
# reason as the landing: myst copies no raw files, and the --delete rsync below
# erases anything dropped straight into the docroot on the server.
#
# Until this shipped, nginx failed to open the file on every 404 and wrote a
# second error line for it, so a missing page cost two log lines and the reader
# still got the built-in page of nginx.
cp errors/404.html "${ORIGIN}404.html"
echo "404 page installed at /404.html"

# robots.txt, overwriting the one myst just built.  myst has no absolute
# address for the site, so its Sitemap line named its own development server
# (http://localhost:3000/sitemap.xml) and every crawler had to discard it --
# which left the sitemap myst does build reachable by nobody.  Ours names the
# real one and refuses the backlink crawlers.  Copied AFTER the build for that
# reason, and before the --delete rsync so the mirror keeps it.
cp robots.txt "${ORIGIN}robots.txt"
# Only the Sitemap line, not the whole file: the comments in robots.txt name
# the localhost address on purpose, to say what this replaced.
if grep -E '^[[:space:]]*Sitemap:' "${ORIGIN}robots.txt" | grep -q localhost; then
    echo "ERROR: the Sitemap line of robots.txt still points a crawler at localhost" >&2
    exit 1
fi
if ! grep -qE '^[[:space:]]*Sitemap:' "${ORIGIN}robots.txt"; then
    echo "ERROR: robots.txt carries no Sitemap line" >&2
    exit 1
fi
echo "robots.txt installed at /robots.txt"

# The manifest, its icons and the service worker, for the tags injected above.
# myst copies no raw files, and the rsync below deletes what it does not carry,
# so they are installed here like the landing.  See pwa/README.md.
# --exclude: README.md and offline-test.mjs are source for this directory, not
# content of the site. Nothing links them, but anything under the docroot is
# fetchable by anyone who guesses -- and by a crawler.
rsync -a --delete --exclude 'README.md' --exclude 'offline-test.mjs' \
    pwa/ "${ORIGIN}pwa/"

# The service worker names its caches after this stamp, and drops every cache
# that does not carry it. So the stamp is what makes a deploy reach a reader
# who already has the old build: a byte-identical sw.js would be considered
# unchanged by the browser and nothing would be invalidated.
SW_VERSION="${VERSION}-$(date +%Y%m%d%H%M%S)"
sed -i "s|__CACHE_VERSION__|${SW_VERSION}|" "${ORIGIN}pwa/sw.js"
if grep -q "__CACHE_VERSION__" "${ORIGIN}pwa/sw.js"; then
    echo "ERROR: sw.js still carries an unsubstituted __CACHE_VERSION__" >&2
    exit 1
fi
echo "pwa manifest, icons and service worker installed at /pwa (cache ${SW_VERSION})"

#
#   Standalone documents with no external requests, sharing the theme
#   through localStorage["myst:theme"].  They do things no markdown page
#   can, so they ship as raw HTML — myst copies no raw files.
#
#   Three kinds, and the differences are real enough to have their own
#   band on the landing:
#
#     WALKTHROUGHS  are stepped through — a graph that runs, a harness
#                   that proves something.  Band "Pages that run",
#                   carded with class="run-card".
#     REFERENCES    are read — one decision followed down to the API
#                   names.  Band "Field guides", class="ref-card".
#     ESSAYS        argue — what the framework is and what each decision
#                   costs, for a reader who has not adopted it yet.  Band
#                   "The idea", class="essay-card".
#
#   Adding one: put it at docs/doc.yuneta.io/<slug>/index.html, add its
#   slug to the right list below, and card it in the matching band.  The
#   check after the loop enforces that last step, per band.
#
WALKTHROUGHS="login-flow three-layers treedb-files"
REFERENCES="package-transition navigation treedb-system-topics"
ESSAYS="high-semantics"

for _walkthrough in ${WALKTHROUGHS} ${REFERENCES} ${ESSAYS}; do
    # The whole directory: a page may ship files of its own (a script to
    # download, an image, a runtime), and index.html alone would leave
    # them 404ing.
    #
    # Except `artifact/`, which is SOURCE for a page published elsewhere
    # (see navigation/artifact/README.md). Copying it would put a
    # half-assembled document at /<page>/artifact/content.es.html for
    # anyone — and a crawler — to find.
    if [ ! -f "${_walkthrough}/index.html" ]; then
        echo "ERROR: page '${_walkthrough}' has no index.html" >&2
        exit 1
    fi
    rm -rf "${ORIGIN:?}${_walkthrough}"
    rsync -a --exclude 'artifact/' "${_walkthrough}/" "${ORIGIN}${_walkthrough}/"
    echo "page installed at /${_walkthrough}"
done

#
#   The landing is the only index these pages have, so each list must
#   agree with its own band. Installed-but-unlisted is a page nobody can
#   find; listed-but-not-installed is a 404 on the front page. And a page
#   carded in the WRONG band is found by nobody looking for it, which is
#   why the check is per band and not against the union. None of the
#   three shows up until somebody complains, so they are caught here —
#   before the rsync, so a mismatch never reaches the server.
#
check_band()   # <card-class> <band-name> <slug>...
{
    _class="$1"
    _band="$2"
    shift 2

    #
    #   Both sides can legitimately be EMPTY — a band with no cards yet,
    #   a list emptied while a page is retired — and under `set -o
    #   pipefail` an empty grep would kill the deploy with no message at
    #   all. Say "nothing" explicitly instead of exiting silently.
    #
    _carded=$(grep -oE "class=\"${_class}\" href=\"/[a-z0-9-]+\"" landing/index.html |
              sed 's|.*href="/||; s|"$||' | sort || true)
    if [ "$#" -gt 0 ]; then
        _installed=$(printf '%s\n' "$@" | sort)
    else
        _installed=""
    fi

    _unlisted=$(comm -23 <(printf '%s\n' "${_installed}") <(printf '%s\n' "${_carded}"))
    _orphan=$(comm -13 <(printf '%s\n' "${_installed}") <(printf '%s\n' "${_carded}"))

    if [ -n "${_unlisted}" ]; then
        echo "ERROR: page(s) installed but not carded in the landing's" >&2
        echo "       \"${_band}\" band — nobody would find them:" >&2
        printf '         /%s\n' ${_unlisted} >&2
        exit 1
    fi
    if [ -n "${_orphan}" ]; then
        echo "ERROR: the landing's \"${_band}\" band cards page(s) that are" >&2
        echo "       not installed there — those links would 404:" >&2
        printf '         /%s\n' ${_orphan} >&2
        exit 1
    fi
    echo "\"${_band}\" cards match the installed set"
}

check_band run-card   "Pages that run" ${WALKTHROUGHS}
check_band ref-card   "Field guides"   ${REFERENCES}
check_band essay-card "The idea"       ${ESSAYS}

# --delete mirrors the build onto the server: pages and content-hashed assets
# dropped from the build (renamed/moved TOC nodes, stale assets) are removed on
# the server too. --delete-after defers removals until the transfer succeeds.
rsync -auvzL --delete-after -e ssh \
    "$ORIGIN" "$DESTINE"
