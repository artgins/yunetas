# External libraries

Third-party libraries used by Yuneta. They are cloned, built and
installed into **`outputs_ext/`** and linked **statically** into the
Yuneta kernel, so they don't conflict with any system-wide version.

## Libraries (statically linked into the Yuneta kernel)

| Library         | Upstream                                            |
|-----------------|-----------------------------------------------------|
| jansson-artgins | https://github.com/artgins/jansson.git (a fork of https://github.com/akheron/jansson.git) |
| liburing        | https://github.com/axboe/liburing                   |
| mbedtls         | https://github.com/Mbed-TLS/mbedtls                 |
| openssl         | https://github.com/openssl/openssl                  |
| pcre2           | https://github.com/PCRE2Project/pcre2               |
| libbacktrace    | https://github.com/ianlancetaylor/libbacktrace      |
| argp-standalone | https://github.com/artgins/argp-standalone.git      |
| ncurses         | https://github.com/mirror/ncurses.git               |
| llhttp          | https://github.com/nodejs/llhttp                    |
| linenoise       | https://github.com/antirez/linenoise                |

> Yuneta migrated its HTTP parser from `http-parser` (Joyent, archived)
> to **`llhttp`** (Node.js). See memory `project_llhttp_integration` —
> the `ghttp_parser` wrapper has been buggy in the past, treat it as
> suspect first when investigating HTTP issues.

## Additional utilities (linked **dynamically** against system libs)

| Tool      | Upstream                                  |
|-----------|-------------------------------------------|
| nginx     | https://github.com/nginx/nginx.git        |
| openresty | https://openresty.org/download/ (release tarball, sha256 pinned in `repos2clone.sh`) |

> **Linking policy is NOT uniform across this directory** (see memory
> `project_ext_libs_vendoring_policy`).
>
> - **Yuneta kernel** links static against the `.a` files in
>   `outputs_ext/`. This is what gives `CONFIG_FULLY_STATIC=y` its
>   self-contained binaries.
> - **`nginx` / `openresty`** must link **dynamic** against system
>   `libssl` / `libpcre` / `libz`. Do not unify the two — past attempts
>   broke nginx in subtle ways. Each consumer of an ext-lib has to
>   declare its linking mode.
>
> Companion rule (`project_ext_libs_consumer_migration`): when
> `configure-libs.sh` flips a flag, every header/CMakeLists consumer
> migration must ship in the **same commit**, else the next maintainer
> reverts it (v1.9 → v1.10 → v1.11 cycle).

### nginx CVE-2026-42945

Resolved in this repo at commit `322d4cb03` by rebuilding the openresty
+ pcre + zlib + libssl stack. Private deployed projects still need
their own rebuild.

### nginx CVE-2026-9256

Buffer overflow in `ngx_http_rewrite_module`.

Resolved in two pin bumps:
- nginx pin → `release-1.30.2` (configure-libs.sh v1.13).
- openresty pin → `1.29.2.5` (configure-libs.sh v1.14), which
  backports the CVE-2026-9256 patch into the openresty-bundled
  nginx core and also fixes a proxy_protocol v2 overflow/over-read.

Same caveat as the previous CVE: each deployed project must
rebuild its own nginx / openresty copy.

### nginx CVE-2026-42530 / CVE-2026-42055 / CVE-2026-48142

- CVE-2026-42530: use-after-free in `ngx_http_v3_module`.
- CVE-2026-42055: buffer overflow in the HTTP/2 paths of
  `ngx_http_proxy_module` / `ngx_http_grpc_module`.
- CVE-2026-48142: buffer overread in `ngx_http_charset_module`.

Resolved for the **standalone nginx** binary by a pin bump:
- nginx pin → `release-1.31.2` (configure-libs.sh v1.17).

Note: `1.31.x` is the nginx **mainline** branch (odd minor), not the
`1.30.x` stable line we were on — chosen because the fixes landed there.

⚠️ **openresty is NOT yet covered.** The openresty pin was advanced
`1.29.2.5` → `1.31.1.1` (configure-libs.sh v1.18), which moves its
bundled nginx core from `1.29.2` to **`1.31.1`**. But these three CVEs
were fixed in nginx **`1.31.2`** (2026-06-17), and openresty `1.31.1.1`
was tagged 2026-05-29 — *before* 1.31.2. So the openresty binary (the
one that actually fronts the SPAs) remains exposed to CVE-2026-42530 /
42055 / 48142 until upstream ships a release based on ≥ nginx 1.31.2 (or
backports the patches). Track upstream openresty.

Same caveat as the previous CVEs: each deployed project must rebuild its
own nginx / openresty copy.

### OpenSSL 3.6.5, PCRE2 10.49, nginx 1.31.7 (configure-libs.sh v1.24)

Security bumps made before the deploy of SDK 7.27.x (2026-10-15):

- **OpenSSL** `openssl-3.6.3` -> `openssl-3.6.5`: 3.6.4 and 3.6.5 are
  security releases (~25 CVEs, the worst "High": an out-of-bounds access
  after `SSL_set_SSL_CTX()` during a handshake, DTLS 1.2 DoS, EC/SM2 timing
  side channels, CMS, CMP, QUIC). Linked statically into every yuno.
- **PCRE2** `pcre2-10.47` -> `pcre2-10.49`: seven GHSA advisories
  (out-of-bounds reads and writes). Linked statically (timeranger2,
  `c_tranger`, `json_replace_vars`).
- **nginx** `release-1.31.2` -> `release-1.31.7`: CVE-2026-42533,
  CVE-2026-60005, CVE-2026-56434 (1.31.3), CVE-2026-90439 (1.31.6),
  CVE-2026-15176, CVE-2026-93479, CVE-2026-95101, CVE-2026-97629,
  CVE-2026-97205 (1.31.7).

⚠️ **openresty is still NOT covered**: there is no release after
`1.31.1.1` (checked 2026-10-15), so its nginx 1.31.1 core is exposed to the
CVEs of 1.31.2 to 1.31.7. Track upstream openresty.

### jansson: heap overflow in the lexer (patched)

jansson (v2.15.1, and upstream master) ignored a failed save of a token
byte, and then wrote past its buffers: a string longer than about half of
`MEM_MAX_BLOCK` crashed the process that parsed it. Fixed in configure-libs.sh
v1.22 by a patch, which is a commit of `artgins/jansson` since v1.25. See
`HACKS.md`. Every node that builds from source must run
`./extrae.sh && ./configure-libs.sh` again, and then rebuild the SDK and
its yunos.

### jansson: from `artgins/jansson` (configure-libs.sh v1.25 to v1.27)

jansson is cloned from Yuneta's fork, `artgins/jansson`, at `TAG_JANSSON`:
v2.15.1 and one commit per change, the lexer fix above and four from the
performance audit of the C SDK (`performance/audit/`): fast paths for ASCII
(a record of timeranger2 read twice as fast and appended 25% faster), a
faster seeded hash of the keys (v1.25), the key of the loop check without
`snprintf()` (v1.26), and the runs of the lexer for `json_loads()`, digits
and blanks (v1.27). The same results, bytes and errors. See
`HACKS.md`. Every node that builds from source must run `./extrae.sh &&
./configure-libs.sh` again, and then rebuild the SDK and its yunos.

### openresty: release tarball (configure-libs.sh v1.23)

openresty comes from its release tarball on openresty.org (sha256 pinned in
`repos2clone.sh`), not from a git clone: building from the git tag fetches
about 45 modules as tarballs from github.com. Same sources, same binary.
Every node that builds from source must run `./extrae.sh &&
./configure-libs.sh` again.

> **Any change to the scripts of this directory bumps `VERSION` in
> `configure-libs.sh`**, with its entry in the header — a library version, a
> patch, a flag, or where a source comes from. The `yunetas` CLI compares it
> with `VERSION_INSTALLED.txt` and refuses to build until the libraries are
> reinstalled, which is how every node that builds from source learns that
> its external libraries are stale.
>
> A fix to a library that upstream has not released goes in Yuneta's fork of
> it under `github.com/artgins` (as `artgins/jansson`), as a commit whose
> message says why, and `repos2clone.sh` points at that commit.

## Build flow

1. (Optional) Select the compiler in `menuconfig`, then run `./set_compiler.sh` in the repo root so this directory uses the same compiler as the rest of Yuneta.
2. Then, from this directory:

```bash
./extrae.sh             # clone the libraries
./configure-libs.sh     # configure, build and install into outputs_ext/
./re-install-libs.sh    # re-install without re-configuring
```

## Static builds (`CONFIG_FULLY_STATIC`)

`configure-libs.sh` already includes the extra OpenSSL flags needed for fully-static glibc binaries:

| Flag | Effect |
|---|---|
| `no-dso` | Drop the DSO/dlopen engine loader (removes `dlopen`/`dlsym` warnings) |
| `no-sock` | Drop socket BIO (`bio_addr.c` / `bio_sock.c`) — removes `getaddrinfo`/`BIO_gethostbyname` warnings. Safe because Yuneta routes all I/O through `ytls` + `yev_loop`, never through OpenSSL socket BIOs. |

See the top-level `CLAUDE.md` for the full static-build notes and the NSS/resolver replacements used at runtime.
