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


## CMake install layout — force `lib`, never `lib64` (cross-distro)

The CMake-based libs (jansson, mbedtls, pcre2, argp-standalone) are built
in `configure-libs.sh` with `-DCMAKE_INSTALL_LIBDIR=lib`.

CMake's `GNUInstallDirs` derives `CMAKE_INSTALL_LIBDIR` from the host: it
is `lib` on Debian/Ubuntu but **`lib64`** on RHEL/Rocky/Alma. The Yuneta
kernel links its static deps from a single hard-coded path —
`link_directories("${YUNETAS_BASE}/outputs_ext/lib")` in
`tools/cmake/project.cmake` — so on RHEL `libmbedtls.a` / `libpcre2-8.a`
would land in `outputs_ext/lib64/` and the link would fail
(`cannot find -lpcre2-8`). Forcing `lib` keeps one `outputs_ext/lib` on
every distro. It is a no-op on Debian (already `lib`). OpenSSL already
uses `--libdir=lib`; liburing / ncurses / libbacktrace are autotools and
default to `lib`.


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
(defined in `kernel/c/yev_loop/src/static_resolv.c`, declared in
`static_resolv.h`), a minimal UDP resolver that reads `/etc/resolv.conf`
and `/etc/hosts` directly, bypassing glibc's NSS resolver chain.
`#define` macros in `static_resolv.h` redirect all `getaddrinfo` and
`freeaddrinfo` call sites in files that include that header.

> All utils and yunos in the suite (including `emailsender` since 7.4.3)
> build cleanly as fully static GCC/Clang binaries.


## jansson — Yuneta's own (`artgins/jansson`)

jansson is Yuneta's JSON library, and since `configure-libs.sh` 1.26 it is
Yuneta's own: `https://github.com/artgins/jansson`. It is upstream jansson
(`akheron/jansson`) plus a series of Yuneta's commits on a branch named
after the upstream release it sits on (`yuneta/2.15.1`, on tag `v2.15.1`),
linear, one commit per change. Each state pinned here is an annotated tag
(`2.15.1-yuneta.1`), and `TAG_JANSSON` in `repos2clone.sh` names it. On each
upstream release the series is rebased onto it, and upstream's security
fixes are always brought in. Upstream's code is not reformatted; the code of
the series braces every block. The rules of the repo are in its `CLAUDE.md`.

Every change is checked with `test/differential/run.sh <commit>` in the
jansson repo: the same results, errors and positions, and the same bytes of
every dump as the commit pinned before, also under AddressSanitizer and
UndefinedBehaviorSanitizer.

What the series of `2.15.1-yuneta.1` changes:

- **The lexer stops when a save of the token text fails** (it was
  `patches/jansson/0001-load-stop-the-lexer-when-a-save-fails.patch`,
  configure-libs.sh 1.22-1.25). The lexer saves each byte of a token in a
  buffer that grows by doubling, and it ignored a failed save. Yuneta routes
  jansson through `gbmem_*`, which refuses a block larger than
  `MEM_MAX_BLOCK`, so a string token longer than about half of that block
  lost its closing quote and `lex_scan_string()` decoded past the end of its
  buffers: a heap overflow, from json text alone. A failed save now stops
  the lexer with `json_error_out_of_memory` (*"not enough memory"*), and
  every other allocation failure of the parser sets that error too. The
  stop after a number or a literal happens only on a failed save: the patch
  stopped there on any error of the stream, so `"1\xff"` loaded with
  `JSON_DECODE_ANY | JSON_DISABLE_EOF_CHECK` was refused where upstream
  returns the value. Upstream has the defect (v2.15.1 and master).
- **Speed, from the performance round of 7.28.0**: plain ASCII taken eight
  bytes at a time in the dump of a string and in the UTF-8 check (read with
  `memcpy()`, the same on any byte order); the lexer takes runs of plain
  bytes (string text, digits, blanks) and reads a byte without the generic
  stream calls; a seeded hash of 32-bit words in place of lookup3's
  `hashlittle()` (still seeded from `/dev/urandom`, no more resistant to
  flooding than lookup3 was); a short string with its value in its own
  block; a string token decoded without an allocation of its own; a NUL
  looked for only in a string that had escapes; the loop check of a dump,
  a deep copy or a compare without a hashtable for 32 levels; the output
  of a dump gathered in a buffer of 4 KB before its callback.
- **A key that cannot be dumped fails the dump.** A key with bytes that are
  not UTF-8 (set with a `_nocheck` call) was dumped as `{":1}`, invalid
  json, and the dump answered 0. Upstream has the defect.
- **`JANSSON_THREAD_SAFE_REFCOUNT`**, a CMake option of the series (ON by
  default). This script turns it OFF: Yuneta has no threads, and
  `json_incref()`/`json_decref()`, inline in every caller, were a locked
  instruction each. `jansson_config.h` changes, so every yuno is rebuilt,
  not only relinked. **A project that adds threads sharing json must turn
  it ON again**: nothing else warns.

What a caller can see besides the speed: what a custom allocator is asked
(fewer and other blocks), and a dump callback that fails once fails the
dump (upstream went on calling it). Measured and left out of the series:
the first buckets of a table and the first table of an array kept inside
their object (up to 8% more live memory in a treedb-like load, no gain in
time).

The ESP32 copy of jansson (2.14, `kernel/c/root-esp32/components/esp_jansson`)
is not this library: it carries only the lexer fix, by hand.

Check an installed library: `strings outputs_ext/lib/libjansson.a | grep
"not enough memory"` finds the text (upstream's `load.o` has none).
