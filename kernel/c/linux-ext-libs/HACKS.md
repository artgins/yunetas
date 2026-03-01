# Hacks of External libraries


## OpenSSL — static build options

OpenSSL is built with `no-dso` and `no-sock` in `configure-libs.sh`
to produce a clean `libcrypto.a` for fully static GCC/Clang binaries.

**no-dso**
: Removes the DSO (Dynamic Shared Object) engine loader, which normally
  calls `dlopen`/`dlsym` to load hardware crypto engine plugins at
  runtime.  In a fully static binary there are no `.so` files to load,
  so this code is dead weight and triggers a linker warning.

**no-sock**
: Removes the socket BIO layer (`bio_addr.c`, `bio_sock.c`), which
  provides `BIO_lookup_ex` (→ `getaddrinfo`) and `BIO_gethostbyname`.
  Yuneta never uses OpenSSL's built-in socket BIOs — all network I/O
  goes through `yev_loop` (io_uring) and `ytls` passes raw bytes to
  OpenSSL via custom memory BIOs.  Without this flag, the glibc resolver
  stubs end up in every binary that links `libcrypto.a` and the linker
  warns that they require glibc shared libraries at runtime even in an
  otherwise-static binary.


## glibc NSS bypass for CONFIG_FULLY_STATIC

Fully static glibc binaries cannot use glibc's Name Service Switch (NSS)
because NSS loads `libnss_*.so` plugins at runtime via `dlopen`.
When `CONFIG_FULLY_STATIC=y` the following call sites are redirected to
pure-file implementations (`/etc/passwd`, `/etc/group`) defined in
`kernel/c/gobj-c/src/helpers.c`:

- `getpwuid`     → `static_getpwuid`
- `getpwnam`     → `static_getpwnam`
- `getgrnam`     → `static_getgrnam`
- `getgrouplist` → `static_getgrouplist`

DNS resolution is handled by `yuneta_getaddrinfo` / `yuneta_freeaddrinfo`
(defined in `kernel/c/yev_loop/src/yev_loop.c`), a minimal UDP resolver
that reads `/etc/resolv.conf` and `/etc/hosts` directly, bypassing
glibc's NSS resolver chain.  A `#define` macro redirects all
`getaddrinfo` and `freeaddrinfo` call sites within `yev_loop.c`
automatically.

> **Note:** `emailsender` cannot be fully static because it depends on
> `libcurl`, which requires shared library support.  All other utils and
> yunos build cleanly as fully static GCC/Clang binaries.
