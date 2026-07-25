# nginx configuration for the Yuneta domains

The node at **37.187.89.46** serves several unrelated products from one nginx.
This directory owns the **root configuration** of that nginx; every other
product contributes a drop-in.

## Who owns what

| Layer | Lives in | Covers |
|---|---|---|
| Root `nginx.conf` | **here** (`yuneta.io/nginx.conf`) | `http {}` globals (TLS, gzip, security headers), the port-80 `default_server`, and `yuneta.io`, `doc.yuneta.io`, `yunetas.com`, `yuneta.es`, `yuneta.com` |
| `conf.d/niyamaka.com.conf` | **here** (`yuneta.io/conf.d/`) | `niyamaka.com` — the gobj-ui test-app demo |
| `conf.d/app.wattyzer.com.conf` | `wattyzer` repo | `wattyzer.com`, `app.wattyzer.com` |
| `conf.d/demo.estadodelaire.com.conf` | `estadodelaire` repo | `estadodelaire.com`, `demo.estadodelaire.com` |

A product never edits the root config: it ships a drop-in carrying its own
`server` blocks, including its own http→https redirect. The root config's
`include conf.d/*.conf;` picks them up.

## Deploying

```bash
./yuneta.io/deploy-conf.sh          # root config + the conf.d files owned here
```

The script backs up the live `nginx.conf` on the node, uploads, runs
`nginx -t`, and reloads only if the test passes.

## The server is plain nginx, not openresty

The node runs **`/yuneta/bin/nginx`** (nginx 1.31.2). An `openresty` tree also
exists at `/yuneta/bin/openresty` but serves nothing here — this node has no
authentication to do, unlike `artgins`, which needs openresty's Lua. Anything
pointing a deploy at `/yuneta/bin/openresty/...` is deploying to a dead tree.

## ⚠️ Reinstalling nginx wipes this config

A fresh nginx install overwrites `conf/nginx.conf` with the stock default and
leaves no `conf.d/`. The running master keeps the old config **in memory**, so
nothing appears to break — until the next restart, when every HTTPS host on
the node goes down at once and the only surviving copy is this repo.

This happened on 2026-07-22 (nginx upgraded to 1.31.2 for a CVE) and surfaced
on 2026-07-25, when a restart took down nine hostnames.

**After any nginx upgrade or reinstall, re-run every deploy script**: this one,
plus the drop-ins in the `wattyzer` and `estadodelaire` repos. Then verify:

```bash
ssh yuneta@yuneta.io 'ss -ltn | grep :443'      # must be listening
for h in doc.yuneta.io yuneta.io yunetas.com yuneta.es yuneta.com \
         niyamaka.com app.wattyzer.com demo.estadodelaire.com; do
    curl -s -o /dev/null -w "$h %{http_code}\n" --resolve "$h:443:<node-ip>" "https://$h/"
done
```

## The `try_files` order matters

Static pages use `try_files $uri $uri/index.html $uri/ =404;`. With the bare
`$uri/` form nginx 301-redirects `/page` → `/page/`, and that extra hop breaks
cross-page `#anchor` scrolling. Browsers cache the 301 indefinitely, so the fix
looks ineffective until you clear the cache — verify server-side with
`curl -sI https://doc.yuneta.io/<page>` (must be 200, not 301).
