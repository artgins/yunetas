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


## jansson — from `artgins/jansson`

jansson comes from Yuneta's fork, `https://github.com/artgins/jansson`,
which is Yuneta's official jansson (`repos2clone.sh`, `TAG_JANSSON`): a
commit of its `master`, which is upstream (`akheron/jansson`) and one commit
per change Yuneta carries on it, the five below (the last one in two
commits). A new fix or change is a commit there, merged into `master` once
it is checked, and `TAG_JANSSON` moves to that commit of `master` (with a
bump of `VERSION` in `configure-libs.sh`). Since `configure-libs.sh` 1.29
`TAG_JANSSON` is a commit of `master`; from 1.25 to 1.28 it was a commit of
a work branch. Up to 1.24 the changes were `patches/jansson/*.patch`,
applied after a checkout of upstream.

## jansson — the lexer stops when a save fails

The first commit of the fork on v2.15.1 (it was
`patches/jansson/0001-load-stop-the-lexer-when-a-save-fails.patch`,
configure-libs.sh 1.22-1.24). Upstream has the defect too (v2.15.1 and
master, 2026-09-23).

The lexer saves each byte of a token in a buffer that grows by doubling, and
it ignored a failed save. Yuneta routes jansson through `gbmem_*`, which
refuses a block larger than `MEM_MAX_BLOCK`. So a string token longer than
about half of that block lost its closing quote, and `lex_scan_string()`
decoded past the end of its buffers: a heap overflow, from json text alone.
It crashed the reads of timeranger2 (`SIGSEGV`, *"corrupted size vs.
prev_size"*), and any other `json_load*()` of such a text. With the patch, a
failed save stops the lexer with the error `json_error_out_of_memory`
(*"not enough memory"*), and every other allocation failure of the parser
sets that error too (before, they set none, and a caller could not tell "no
memory" from "not json").

The same fix is applied by hand to the ESP32 copy of jansson (2.14),
`kernel/c/root-esp32/components/esp_jansson/jansson/src/load.c`.

Check an installed library: `strings outputs_ext/lib/libjansson.a | grep
"not enough memory"` finds the text (the unpatched `load.o` has none).

## jansson — fast paths for ASCII

The second commit of the fork (configure-libs.sh 1.25), for speed, not a
fix, from the performance audit of the C SDK
(`performance/audit/round-2-timeranger2-treedb.md`).
jansson walks a string a byte at a time through a function call and a
UTF-8 decode, and its lexer saves a string token a byte per callback. For
ASCII, which is most of what Yuneta stores and sends, none of that is
needed:

- `utf8_check_string()` skips a byte below `0x80` before the decode, which
  answers 1 for it;
- `dump_string()` takes a byte below `0x80` as its own codepoint, what
  `utf8_iterate()` answers;
- `lex_scan_string()`, reading from the buffer of `json_loadb()`, saves the
  run of plain ASCII that follows (`0x20..0x7F` but `"` and `\`) at once,
  grown by the same doublings, so a token that fails for memory fails where
  it failed. From another source, or in the middle of a multi-byte
  character, the bytes are read one by one, as before.

The same results, bytes and errors: jansson's 215 tests pass, and the
patched and unpatched libraries, under the same driver, gave the same output
for random documents (valid, mutated, invalid UTF-8, escapes, control
characters; three sets of load flags, five of dump flags; and the allocator
capped to force "not enough memory", with the same positions). Not applied
to the ESP32 copy of jansson.

## jansson — a faster seeded hash of the keys

The third commit of the fork (configure-libs.sh 1.25), for speed, from the
performance audit of the C SDK (`performance/audit/round-1-gobj-core.md`).
lookup3's `hashlittle()` was the most expensive function of a lookup (5% of
an inter-yuno round trip, 11% of an MQTT publication, 10-12% of the TCP
echo). `hash_str()` now takes 64-bit words of the key, xored with the seed
and with constants, multiplied 64x64 -> 128 and folded to 64 bits: the
construction of wyhash, the one Go's runtime uses for its maps where it has
no AES. It is seeded with `hashtable_seed`, as before (`json_object_seed()`).

Nothing that jansson answers depends on the hash: an object is walked in the
order its keys were inserted, never by its buckets. Checked: jansson's 215
tests, a differential fuzz against the previous library (the same bytes),
and the buckets as evenly filled as with `hashlittle()`. Per hash, -10% to
-25% for the keys of a Yuneta message, x3.5 faster for a key of 100 bytes.
Not applied to the ESP32 copy of jansson.

## jansson — the key of the loop check without `snprintf()`

The fourth commit of the fork (configure-libs.sh 1.26), for speed, from the
performance audit of the C SDK (`performance/audit/round-5-subscriptions-treedb.md`).
Every object and array dumped, deep-copied or compared is first put in a
table of its parents, under a key made with `snprintf(key, size, "%p",
json)`: 15 printf calls per inter-yuno message, 1.8% of its round trip.
`loop_key()` writes the same text glibc's `"%p"` writes ("0x" and the
hexadecimal digits of the address, without leading zeros); a buffer too
short for it, or a NULL json, still goes to `snprintf()`. Checked against
`snprintf()` on 20 million addresses and buffer sizes (the same bytes and
length), and jansson's 215 tests. Not applied to the ESP32 copy of jansson.

## jansson — the runs of the lexer for `json_loads()`, digits and blanks

The fifth and sixth commits of the fork (configure-libs.sh 1.27 and 1.28), for speed, from the
performance audit of the C SDK (`performance/audit/round-6-jansson-parse-treedb-reads.md`).
The run of plain ASCII of a string token (the second commit) was saved at
once only from the source of `json_loadb()`; `gbuf2json()` parses every
message Yuneta receives with `json_loads()`, whose source went a byte at a
time through `stream_get()` and `string_get()`. The runs now read either
source (`json_loads()`'s up to its NUL), and two more kinds are taken at
once: the digits of a number, and the blanks before a token (a newline
still goes a byte at a time, for the line count). Each run does what the
byte-by-byte reads did: the same bytes saved, the same position and column,
the saved text grown where it was grown. Checked: jansson's 215 tests, and a
differential fuzz against the previous library (140 000 documents, both
loaders, five sets of flags, a capped allocator): the same results, errors,
positions and allocations. The sixth commit takes a run only after a byte of
its kind (the blanks after the first one, the digits of a number after its
second), so that a compact document with short numbers pays nothing for
them. Not applied to the ESP32 copy of jansson.
