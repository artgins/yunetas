# doc.yuneta.io — official documentation

Built with [mystmd / Jupyter Book 2](https://mystmd.org).

## How to build

```bash
npm install -g mystmd
cd docs/doc.yuneta.io

# Live-reloading dev server (usually http://localhost:3000)
myst start

# Validate an edit. `myst build --html` does NOT exit and writes no
# _build/html: it boots a dev server, so it is a check, not a build.
# timeout is the whole cleanup; do not chain a pkill (see CLAUDE.md).
NODE_OPTIONS=--disable-warning=DEP0169 timeout 30 myst build --html 2>&1 |
    grep -iE 'warning|error|⚠|fail'
```

For any executable `{code-cell}` pages you additionally need a Python
kernel and whatever libraries those cells import.

## How to deploy

`./deploy.sh` builds the site and mirrors it to the server. It is the
real build, and it does more than mystmd does:

- Stamps the current `YUNETA_VERSION` and the date into `index.md`, and
  the `yunetas` CLI version into `installation.md`.
- Injects into every built page what the book-theme has no hook for: the
  anchor-scroll fix, the diagram lightbox, the theme hand-off to
  demo.yuneta.io, and the PWA tags.
- Installs the files mystmd does not copy — the landing page, the
  standalone pages, and `pwa/` — then checks that every standalone page
  is carded on the landing.
- Mirrors the result with `rsync --delete`, so anything dropped straight
  into the docroot is erased by the next deploy.

The site is a PWA: installable, and it reads offline what the reader
already opened. See [`pwa/README.md`](pwa/README.md), which also covers
its nginx side and how to test the offline behaviour.

## API coverage verifier

[`scripts/verify_api_coverage.py`](https://github.com/artgins/yunetas/blob/7.9.12/scripts/verify_api_coverage.py) compares every `PUBLIC`
function declared in the kernel C headers against the `(funcname)=`
anchors present in the documentation landing pages. It reports
per-header MISSING (exported but not documented) and EXTRA (documented
but not exported) symbols.

- Local run: `python3 scripts/verify_api_coverage.py`.
