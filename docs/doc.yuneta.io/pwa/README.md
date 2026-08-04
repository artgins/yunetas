# PWA manifest for doc.yuneta.io

These files make the documentation installable. The reader gets the docs in
their own window, with their own icon, instead of one more browser tab.

There is **no service worker**, thus there is no offline reading: an installed
copy still needs the network. Add one only with a cache strategy for the
mystmd build (content-hashed assets, client-side navigation).

## How it is served

`deploy.sh` does two things:

- It installs this directory into the site as `/pwa/`. mystmd copies no raw
  files, and the deploy rsync deletes what it does not carry.
- It injects `<link rel="manifest" href="/manifest.webmanifest">` and
  `<link rel="apple-touch-icon">` into the `<head>` of each built page. The
  book-theme has no hook to add tags to the head.

nginx maps the manifest URL to this copy, in the `doc.yuneta.io` server block
only (artgins ops repo, `tasks/nginx/yuneta.io/nginx.conf`):

```nginx
location = /manifest.webmanifest {
    alias /yuneta/gui/doc.yuneta.io/pwa/manifest.webmanifest;
    default_type application/manifest+json;
    expires -1;
}
```

Two reasons for that location:

1. `.webmanifest` is not in the standard `mime.types` of nginx. Without
   `default_type` the manifest is sent as `application/octet-stream` and the
   browser ignores it.
2. `yuneta.io`, `yuneta.com`, `yuneta.es` and `yunetas.com` share this docroot,
   but `/` on those domains is the landing page, not the documentation. A
   manifest at the root of the docroot would make all five domains offer to
   install a different application under the same name. On those four the link
   gets a 404 and no install is offered.

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
