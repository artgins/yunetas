# **Changelog**

## Unreleased
    - **API change(ghttp_parser)**: `ghttp_parser_reset()` is **removed** from
      the public API.  It was a foot-gun: calling it from inside an llhttp
      callback (as `on_message_complete` used to do) corrupted llhttp's state
      machine and silently swallowed pipelined messages.  Callers that need a
      pristine parser for a new connection now use the destroy+create cycle
      (see `c_prot_http_sr::ac_connected`, `c_prot_http_cl::ac_connected`,
      `c_websocket::ac_connected`).  The llhttp settings vtable is now
      initialised once, lazily, via `llhttp_settings_init()` in
      `ensure_settings_initialized()`.
    - **feat(ghttp_parser)**: new `ghttp_parser_finish()` that signals
      end-of-stream (`llhttp_finish()`) to the parser.  Fixes a latent bug
      where HTTP/1.0 responses (or HTTP/1.1 `Connection: close` responses
      without `Content-Length` / `Transfer-Encoding: chunked`) never fired
      `on_message_complete` because the peer's socket close was the only
      message terminator.  Wired up in `c_prot_http_cl::ac_disconnected`
      (the critical case for response parsers), `c_prot_http_sr::ac_disconnected`,
      and `c_websocket::ac_disconnected`.
    - **fix(ghttp_parser)**: on `HPE_PAUSED_UPGRADE`, `ghttp_parser_received()`
      now returns the actual number of bytes llhttp consumed (computed via
      `llhttp_get_error_pos()`) instead of lying that it consumed the whole
      buffer.  This lets the caller re-route any tail bytes that belong to
      the new protocol (e.g. a WebSocket frame piggy-backed on the same TCP
      segment as the upgrade request) to the next handler.
    - **CRITICAL fix(ghttp_parser)**: HTTP/1.1 pipelining was silently broken —
      `on_message_complete()` called `ghttp_parser_reset()`, which in turn called
      `llhttp_init()` from inside the llhttp callback, corrupting the parser's
      internal state machine so every subsequent message in the same buffer was
      swallowed without a log.  Affects every yuno serving or consuming HTTP
      over keep-alive when more than one message is in flight on a single
      connection (c_prot_http_sr, c_prot_http_cl, c_websocket).  Fix: reset the
      per-message app fields inline in `on_message_complete` without touching
      llhttp; leave `ghttp_parser_reset()` for the other (non-callback) call
      sites.  Surfaced by the new test suite `tests/c/c_auth_bff/test8_queue_full`.
    - fix(c_auth_bff): `mt_stop` now tears down the inbound `C_PROT_HTTP_SR + C_TCP`
      bottom chain in addition to the outbound `priv->gobj_http`.  Previously a
      SIGTERM to an auth_bff yuno with active browser connections logged
      "Destroying a RUNNING gobj" during shutdown.
    - fix(c_auth_bff): drop stale Keycloak reply when the browser disconnected
      mid-round-trip.  New `browser_alive` gate in `result_token_response` /
      `result_kc_logout` + new `responses_dropped` stat counter.  Prevents the
      UAF-ish "gobj DESTROYED" path that surfaced as sporadic test failures
      during fast cancel-then-retry login flows.
    - fix(c_auth_bff): outbound Keycloak watchdog.  New per-instance attr
      `kc_timeout_ms` (default 30000, 0 disables) armed via a `C_TIMER0` child
      right after the outbound HTTP client is created and cleared in
      `ac_end_task`.  On fire, responds 504 to the browser and drains the
      task.  Closes the "Keycloak hangs → channel wedged forever" deadlock that
      was otherwise a permanent-until-SIGTERM state under silent-upstream
      conditions.  New `kc_timeouts` stat counter.
    - fix(c_auth_bff): count any 2xx Keycloak reply as `kc_ok`.  Previously only
      status == 200 counted, which made every successful `/logout` (Keycloak
      returns spec-compliant 204 No Content) land in `kc_errors` and poison
      the health-signal ratio.
    - fix(c_auth_bff): `mt_stats` filter now mirrors the default `stats_parser.c`
      two-stage matcher (full name OR underscore-prefix) and is
      case-insensitive, so `gobj_stats(bff, "kc_", ...)` returns the kc_* set
      as expected.  `redact_for_trace()` key matching is also case-insensitive
      so HTTP headers like "Cookie"/"cookie"/"COOKIE" are all masked.
    - feat(c_auth_bff): `pending_queue_size` is now a per-instance attr
      (default 16, clamped to [1, 1024]) instead of a compile-time constant.
      Stored as a `PENDING_AUTH *` ring dynamically allocated in `mt_create`.
    - feat(tests/c/c_auth_bff): new 11-binary test suite.  Self-contained yunos
      that spin up a mock Keycloak (signed HS256 JWTs, scriptable latency /
      return status / override body) alongside the BFF and drive
      `/auth/login`, `/auth/callback`, `/auth/refresh`, `/auth/logout`
      through happy paths, validation errors, Keycloak 401, slow KC, queue
      contention (pipelined POSTs, overflow drop), browser cancel
      mid-round-trip, cancel-then-retry state cleanup, and KC total silence.
      Exposed and regression-gated the watchdog, browser_alive, and
      ghttp_parser fixes above.
    - refactor(gobj): drop TLS knowledge from gobj-c, inject from ytls layer
      via the new `gobj_add_global_variable()` extension point; removes the
      `CONFIG_HAVE_OPENSSL/MBEDTLS` `#if` blocks from `gobj_global_variables()`.

## v7.2.1 -- 07/Apr/2026
    - TLS: change Kconfig from radio (choice) to checkboxes — both OpenSSL and mbedTLS can be
      enabled simultaneously for runtime backend selection per connection
    - TLS: add `__tls_libraries__` global variable (reports all compiled backends)
    - Documentation: add Test Suite page, fix glossary warnings, improve gobj-js and lib-yui READMEs
    - Remove obsolete defconfig and REVIEW.md
    - Fix duplicate measure_times declarations in yev_loop.h

## v7.2.0 -- 04/Apr/2026
    - Fully static glibc binaries (CONFIG_FULLY_STATIC): GCC and Clang, with custom
      static resolver (yuneta_getaddrinfo) and NSS replacements (static_getpwuid, etc.)
    - mbedTLS support as alternative TLS backend (~3x smaller static binaries vs OpenSSL)
    - Fix mbedTLS bad_record_mac: accumulate TLS records before writing
    - Add TRACE_TLS trace level and mbedTLS debug callback for TLS diagnostics
    - JS kernel restructured: gobj-js (7.1.x) and lib-yui (7.1.x) published to npm
    - Replace bootstrap-table+jQuery with Tabulator in gui_treedb
    - Vite 8 build for lib-yui (ES/CJS/UMD/IIFE bundles)
    - MQTT 5.0: will properties, user properties, topic alias, subscription identifiers
    - Fix MQTT QoS 2 infinite loop and flow control (receive-maximum, keepalive)
    - OAuth2 BFF (auth_bff yuno) with PKCE, httpOnly cookies, security hardening
    - TreeDB: compound link improvements, undo/redo history sync, new tr2search/treedb_list utils
    - G6 graph visualization: C_G6_NODES_TREE and C_YUI_JSON_GRAPH GClasses
    - Fix c_watchfs: memory leak, event name mismatch (EV_FS_CHANGED), buffer bugs
    - Fix c_fs: memory leak in destroy_subdir_watch
    - Fix XSS vulnerabilities in gui_treedb webapp
    - Kconfig: add CONFIG_C_PROT_MQTT, organize protocol modules submenu
    - Remove deprecated musl compiler option

## v7.0.1 -- 29/Mar/2026
    - Release 7.0.1
    - JS kernel (yunetas npm package) published as v0.3.0
    - Updated and documented .deb packaging (packages/)

## v7.0.0 -- 28/Sep/2025
    - Publish first 7.0.0 for production

## v7.0.0-b17 -- 26/Sep/2025
    - fix remote console (controlcenter) blocked when paste text

## v7.0.0-b15 -- 22/Sep/2025
    - fix yuneta_agent: wrong assignment of ips to public service

## v7.0.0-b14 -- 11/Sep/2025
    - improve .deb
    - yuno-skeleton to /yuneta/bin and skeletons to /yuneta/bin/skeletons
    - check inherited files only for daemons

## v7.0.0-b12 -- 7/Sep/2025
    - now you can select openresty or nginx in .deb

## v7.0.0-b10 -- 2/Sep/2025
    - jwt in remote connection

## v7.0.0-b9 -- 2/Sep/2025
    - Remote control (controlcenter) ok

## v7.0.0-b8 -- 29/Aug/2025
    - GObj: fix bug with rename events

## v7.0.0-b7 -- 29/Aug/2025
    - Fixed: avoid that yunos (fork child) inherit the socket/file descriptors from agent.
