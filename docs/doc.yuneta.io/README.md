# doc.yuneta.io — official documentation

Built with [mystmd / Jupyter Book 2](https://mystmd.org).

## How to build

```bash
npm install -g mystmd
cd docs/doc.yuneta.io

# Live-reloading dev server (usually http://localhost:3000)
myst start

# Static HTML build
myst build --html
```

For any executable `{code-cell}` pages you additionally need a Python
kernel and whatever libraries those cells import.

## API coverage verifier

`tools/docs-migration/verify_api_coverage.py` compares every `PUBLIC`
function declared in the kernel C headers against the `(funcname)=`
anchors present in the documentation landing pages. It reports
per-header MISSING (exported but not documented) and EXTRA (documented
but not exported) symbols.

- CI enforcement: `.github/workflows/docs-api-coverage.yml` runs the
  verifier on every push / PR that touches kernel headers, doc sources
  or the script itself, and fails the build on any issue.
- Local run: `python3 tools/docs-migration/verify_api_coverage.py`.
