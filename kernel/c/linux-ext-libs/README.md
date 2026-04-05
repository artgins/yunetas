# External libraries

Third-party libraries used by Yunetas. They are cloned, built and installed into **`outputs_ext/`** and linked **statically** into the Yuneta kernel, so they don't conflict with any system-wide version.

## Libraries

| Library | Upstream |
|---|---|
| jansson-artgins | https://github.com/akheron/jansson.git |
| liburing | https://github.com/axboe/liburing |
| mbedtls | https://github.com/Mbed-TLS/mbedtls |
| openssl | https://github.com/openssl/openssl |
| pcre2 | https://github.com/PCRE2Project/pcre2 |
| libbacktrace | https://github.com/ianlancetaylor/libbacktrace |
| argp-standalone | https://github.com/artgins/argp-standalone.git |
| ncurses | https://github.com/mirror/ncurses.git |
| http-parser | https://github.com/nodejs/http-parser |
| linenoise | https://github.com/antirez/linenoise |

## Additional utilities

| Tool | Upstream |
|---|---|
| nginx | https://github.com/nginx/nginx.git |
| openresty | https://github.com/openresty/openresty.git |

> If you change the version of any library, update `VERSION_INSTALLED.txt` and the corresponding version in `configure-libs.sh`.

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
