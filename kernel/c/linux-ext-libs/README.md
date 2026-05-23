# External libraries

Third-party libraries used by Yuneta. They are cloned, built and
installed into **`outputs_ext/`** and linked **statically** into the
Yuneta kernel, so they don't conflict with any system-wide version.

## Libraries (statically linked into the Yuneta kernel)

| Library         | Upstream                                            |
|-----------------|-----------------------------------------------------|
| jansson-artgins | https://github.com/akheron/jansson.git              |
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
| openresty | https://github.com/openresty/openresty.git|

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
their own rebuild (see memory `project_nginx_cve_2026_42945` —
hidraulia is production).

### nginx CVE-2026-9256

Buffer overflow in `ngx_http_rewrite_module`.

Resolved in two pin bumps:
- nginx pin → `release-1.30.2` (configure-libs.sh v1.13).
- openresty pin → `1.29.2.5` (configure-libs.sh v1.14), which
  backports the CVE-2026-9256 patch into the openresty-bundled
  nginx core and also fixes a proxy_protocol v2 overflow/over-read.

Same caveat as the previous CVE: each deployed project must
rebuild its own nginx / openresty copy.

> If you change the version of any library, update
> `VERSION_INSTALLED.txt` and the corresponding version in
> `configure-libs.sh`.

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
