# PWA manifest for doc.yuneta.io

These files make the documentation installable. The reader gets the docs in
their own window, with their own icon, instead of one more browser tab.

`sw.js` adds offline reading: every page the reader opened stays readable with
no network. It does **not** download the site in advance — the build is 28 MB,
which is not a cost to put on a mobile connection for pages nobody asked for.
Pages are cached as they are read.

## How it is served

`deploy.sh` does three things:

- It installs this directory into the site as `/pwa/`. mystmd copies no raw
  files, and the deploy rsync deletes what it does not carry.
- It stamps the deploy version into `sw.js`, which names its caches after it
  and deletes every cache that does not carry it. A byte-identical `sw.js`
  would be taken by the browser as unchanged, and no reader with the old build
  would ever see the new one.
- It injects `<link rel="manifest">`, `<link rel="apple-touch-icon">` and
  `<script src="/pwa/register-sw.js">` into the `<head>` of each built page.
  The book-theme has no hook to add tags to the head.

The standalone pages (`landing/`, `high-semantics/`, `login-flow/`,
`navigation/`, `package-transition/`) are copied into the build after that
injection, so each one carries the three tags in its own `<head>`. A new
standalone page must do the same, or the reader who lands there gets no offer
to install and no offline copy.

nginx maps two URLs to this directory, in the `doc.yuneta.io` server block only
(artgins ops repo, `tasks/nginx/yuneta.io/nginx.conf`):

```nginx
location = /manifest.webmanifest {
    alias /yuneta/gui/doc.yuneta.io/pwa/manifest.webmanifest;
    default_type application/manifest+json;
    expires -1;
}

location = /sw.js {
    alias /yuneta/gui/doc.yuneta.io/pwa/sw.js;
    expires -1;
}
```

Three reasons for those locations:

1. `.webmanifest` is not in the standard `mime.types` of nginx. Without
   `default_type` the manifest is sent as `application/octet-stream` and the
   browser ignores it.
2. A service worker only controls the path it is served from, so one that
   answers for the whole site has to answer at `/sw.js`. Both are served
   `no-cache`: a cached service worker is the one mistake with no remote cure.
3. `yuneta.io`, `yuneta.com`, `yuneta.es` and `yunetas.com` share this docroot,
   but `/` on those domains is the landing page, not the documentation. A
   manifest at the root of the docroot would make all five domains offer to
   install a different application under the same name. On those four both URLs
   answer 404, and `register-sw.js` registers nothing anyway.

## Offline reading

`sw.js` keeps two caches, both named after the deploy stamp:

| Cache | Holds | Rule |
|---|---|---|
| `assets` | `/build/**` | Cache first. Every name carries a content hash, so a cached copy is never the wrong one. |
| `pages` | the documents and their data urls | Network first. A redeployed page must not read stale; the cache answers when the network fails. |

A page arrives under two URLs: `/<slug>` on a cold load, and
`/<slug>?_data=<route>` when the reader clicks a link and the theme routes on
the client. This host is static nginx, which ignores the query and answers the
**same bytes** to both, so `sw.js` drops that query from the cache key. One
entry then serves both: a page read by a cold load opens on a click, and a page
reached by a click survives a reload.

A page that was never read falls back to `offline.html`.

### Testing it

`offline-test.mjs` drives the deployed site with Playwright and asserts the
whole set. Run it after any deploy that touches `sw.js`:

```bash
cd /yuneta/development/projects/wattyzer/gui   # for its node_modules
node /yuneta/development/yunetas/docs/doc.yuneta.io/pwa/offline-test.mjs
```

**`context.setOffline()` is a no-op in Playwright's Firefox.** With it on, a
page that had never been fetched still loaded from the network — every
assertion passed and proved nothing. The test goes offline by relaunching the
same browser profile behind a dead proxy.

## Icons

The mark is `_static/yuneta-y.svg`, on the dark background of the landing page
(`#10191A`). The maskable icon keeps the mark at 59% of the canvas, inside the
80% safe zone that launchers can crop to a circle.

```bash
inkscape ../_static/yuneta-y.svg --export-filename=/tmp/m165.png -w 165 -h 165
inkscape ../_static/yuneta-y.svg --export-filename=/tmp/m440.png -w 440 -h 440
inkscape ../_static/yuneta-y.svg --export-filename=/tmp/m300.png -w 300 -h 300
magick -size 192x192 xc:'#10191A' /tmp/m165.png -gravity center -composite icon-192.png
magick -size 512x512 xc:'#10191A' /tmp/m440.png -gravity center -composite icon-512.png
magick -size 512x512 xc:'#10191A' /tmp/m300.png -gravity center -composite icon-maskable-512.png
```
