# **Changelog**

## v7.3.0 -- 18/Apr/2026
    - **feat(ytls, c_yuno, c_agent): TLS certificate hot-reload with
      three-layer defence-in-depth**. Lets a Yuneta host keep thousands of
      persistent TLS connections alive across a Let's Encrypt renewal,
      with no deploy-hook single point of failure.
      - **ytls**: new [`ytls_reload_certificates()`](docs/doc.yuneta.io/api/ytls/ytls.md)
        that rebuilds the backend context (OpenSSL `SSL_CTX` or mbed-TLS
        `mbedtls_state_t` bundle), validates it, and atomically swaps it
        in. Live sessions hold their own refcount on the previous context,
        so already-established connections keep working until they close.
        Invalid material rolls back cleanly — traffic is never interrupted
        by a bad reload. `ytls_get_cert_info()` returns
        `{subject, issuer, not_before, not_after, serial, days_remaining}`
        for the live context, not just the file on disk.
      - **c_agent**: new cert auto-sync timer (attr
        `cert_sync_interval_sec`, default 900 s) that re-reads
        `/yuneta/store/certs/` via `sudo -n copy-certs.sh`; when any
        `size+mtime` changes, broadcasts `reload-certs` to every running
        yuno. Exposes `cert-sync-now` / `cert-sync-status` commands and
        self-heals if the certbot deploy hook fails silently.
      - **c_yuno**: periodic expiry monitor (attr `timeout_cert_check`,
        default 3600 s) that walks every `C_TCP_S` / `C_UDP_S` listener
        and logs `gobj_log_warning()` at `cert_warn_days` (default 7) and
        `gobj_log_critical()` at `cert_critical_days` (default 2).
        Alert-only — the sync layer owns the reload responsibility.
      - **c_tcp_s / c_udp_s**: per-listener `reload-certs` and `view-cert`
        commands, routable via `ycommand -c 'command-yuno command=reload-certs
        service=__yuno__'` or `gobj=<name>` for a single listener.
      - **packages**: `/etc/letsencrypt/renewal-hooks/deploy/reload-certs`
        hook copies certs, reloads the web server and broadcasts the
        yuno-level reload. Each step runs with `set +e`; output is logged
        to `/var/log/yuneta/deploy-hook.log` and the hook writes its last
        run timestamp to `/var/lib/yuneta/last-deploy-hook-run` so
        `cert-sync-status` can spot a hook that never runs.
      - **tests**: `tests/c/ytls/test_cert_reload`,
        `test_cert_info`, `test_cert_reload_mem` (1000 reloads, zero leak)
        and `tests/c/yev_loop/yev_events_tls/test_yevent_reload_live`,
        `test_yevent_reload_stress` (50 reloads with a live session).
      - **docs**: new guide [`guide/guide_cert_management.md`](docs/doc.yuneta.io/guide/guide_cert_management.md)
        covers the end-to-end story, layered design and file / permission
        layout; `guide/guide_ytls.md` gains a hot-reload section.

    - **feat(gobj): `gobj_set_manual_start()` + `gobj_flag_manual_start`**.
      A gobj can now opt out of the automatic `start-tree` walk so its
      parent keeps ownership of lifecycle but decides *when* to bring it
      up. Used in `c_auth_bff` to keep `gobj_idprovider` dormant until the
      BFF has validated its configuration.

    - **feat(ycommand)**: major interactive / scripting overhaul.
      - TAB completion of command names, parameter names and boolean values,
        from a remote `list-gobj-commands` cache fetched at connect time
        (routed through `service=__yuno__`) and from a local command table
        for `!cmd` built-ins.
      - Inline parameter hints in gray (`<name=type>` required,
        `[name=type]` optional, already-typed params dropped).
      - Connect-time informative prompt (`<role>^<name>> `) and schema-driven
        table rendering in both interactive and non-interactive modes (use
        the `*cmd` prefix to force raw-JSON form).
      - `Ctrl+R` / `Ctrl+S` incremental history search, `Ctrl+L` clear screen,
        bash-style `!!` / `!N` history expansion, erasedups history.
      - c_cli-style local commands via the `!` prefix: `!help` (alias `!h` /
        `!?`), `!history`, `!clear-history`, `!exit` / `!quit`,
        `!source <file>` (alias `!.`). Full keybinding + syntax reference
        available as `!help` and in `utils/c/ycommand/README.md`.
      - Command chaining with `cmd1 ; cmd2 ; cmd3` (quote/brace-aware split),
        `-cmd` ignore-fail (ybatch convention), stdin piping
        (`cat batch.ycmd | ycommand -u ws://...`). A single shared
        command queue drains one command at a time, waiting for the previous
        response before sending the next.
      - `did-you-mean` suggestions on `command not available` errors,
        Levenshtein-matched against the cache.
      - Positional command form (`ycommand kill-yuno id=foo`, equivalent to
        `-c`). The `-c` flag still wins when both are present.
    - **feat(c_editline)**: new public helpers shared by every editline
      client — `editline_set_completion_callback` /
      `editline_set_hints_callback` / `editline_add_completion` /
      `editline_history_count` / `editline_history_get`. New events
      `EV_EDITLINE_REVERSE_SEARCH` / `EV_EDITLINE_FORWARD_SEARCH` for
      incremental history search; candidate list + description is rendered
      on TAB when multiple options exist.
    - **fix(c_editline)**: after the user selects a TAB candidate, the
      keystroke that committed the selection (Enter, Backspace, printable)
      is now re-dispatched so the action takes effect in the same press
      instead of requiring a second press.
    - **fix(ycommand)**: `on_read_cb` no longer drops trailing bytes of a
      batched read that matched a keytable entry, so rapid TAB+value typing
      no longer needs a second press.
    - **feat(ycli)**: TAB completion brought in line with ycommand, adapted
      to the multi-window ncurses UI.
      - `!cmd<TAB>` completes local `c_cli` commands; `cmd<TAB>` (no `!`)
        completes remote commands of the yuno attached to the focused
        display window. Cache is per-connection, fetched silently on
        `EV_ON_OPEN` via `list-gobj-commands` and dropped on
        `EV_ON_CLOSE`.
      - Multi-candidate list is rendered in a temporary ncurses popup
        above the editline (no more blocking `read(STDIN_FILENO)` inside
        the yev_loop callback); cycling is driven through the normal FSM
        (TAB / Up / Down navigate, Enter commits to the edit line only,
        Esc / Ctrl+G / Backspace cancel, printable keys commit + insert).
      - Scrollable popup with a status row (`N/M  ↑ K above  ↓ L below`)
        rendered in dim attributes so A_REVERSE on the selected row can
        never bleed into it.
      - Inline hints (`<req=type>` / `[opt=type]`) in gray (A_BOLD on
        COLOR_BLACK = bright-black / gray in most terminals).
    - **feat(c_editline)**: new `EV_EDITLINE_CANCEL` event for escape-style
      cancellation of reverse-i-search and TAB-popup sub-modes; `refreshSearchLine`
      now draws through ncurses (`wmove/waddnstr/wrefresh`) on `use_ncurses`
      clients instead of bypassing the pane via `printf`.
    - **feat(ycli / ycommand)**: `Ctrl+K` switched to readline semantics —
      delete from cursor to end of line (`EV_EDITLINE_DEL_EOL`).
      `Ctrl+U` / `Ctrl+Y` remain "delete whole line"; `Ctrl+L` is the
      clear-screen shortcut (previously shared with `Ctrl+K`).
    - **docs**: added `utils/c/ycommand/README.md`, `TODO.md` and updated
      `docs/doc.yuneta.io/{utilities,yunos,modules}.md` to cover the new
      features.

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
    - **refactor(c_auth_bff): IdP-agnostic naming, single-job task, queue +
      routing hardening**. The BFF used to be visibly wired to Keycloak
      (`kc_*` attrs, stats, logs). Code, attrs and stats now use the
      generic `idp_*` prefix; any OIDC provider fits. The outbound IdP
      gobj chain is now named `<bff-name>-idp` for trace clarity.
      - **Pending queue** migrated from a fixed-size `PENDING_AUTH *` ring
        to a `dl_list`, drained one job at a time. Configurable per
        instance via `pending_queue_size` (default 16, clamped to
        `[1, 1024]`). Overflow bumps `q_full_drops` and the browser sees
        a mapped `error_code`; peak depth is exposed as `q_max_seen`.
      - **Flush-on-disconnect**: when a browser closes mid-round-trip the
        BFF flushes its pending queue for that channel; late IdP replies
        for disconnected clients are dropped (`responses_dropped` counter)
        instead of being forwarded. Each task also carries a per-browser
        generation so a cross-user token leak cannot occur.
      - **Single-job task, teardown-safe close**: the C_TASK instance
        holds a single job at a time; `mt_stop` drains the inbound
        `C_PROT_HTTP_SR + C_TCP` chain and the outbound `gobj_http` so a
        SIGTERM with live browser connections no longer logs
        "Destroying a RUNNING gobj".
      - **Outbound watchdog**: per-instance attr `idp_timeout_ms`
        (default 30000, 0 disables) armed via a `C_TIMER0` child right
        after the outbound HTTP client is created and cleared in
        `ac_end_task`. On fire, responds 504 to the browser and drains
        the task; closes the "IdP silence → channel wedged forever"
        deadlock. New `idp_timeouts` stat counter.
      - **IdP health signal fix**: count any 2xx IdP reply as `idp_ok`;
        previously only 200 counted, so every successful `/logout`
        (Keycloak returns spec-compliant 204 No Content) poisoned the
        ratio as an `idp_error`.
      - **Logout routing fix**: route the logout reply to the bottom
        browser channel, not to the dangling `_browser_src` from an
        earlier round-trip.
      - **`mt_stats` filter** mirrors the default `stats_parser.c`
        two-stage matcher (full name OR underscore-prefix) and is
        case-insensitive, so `gobj_stats(bff, "idp_", ...)` returns the
        idp_* set as expected. `redact_for_trace()` key matching is also
        case-insensitive so HTTP headers like "Cookie"/"cookie"/"COOKIE"
        are all masked.
      - **Stats moved to PRIVATE_DATA + `mt_stats`** for zero hot-path
        cost; the gclass now also exposes a stats/queue-state command
        through the normal command interface.
      - **Stable `error_code`** in every BFF response (snake_case, e.g.
        `invalid_refresh_token`, `idp_unreachable`, `queue_full`) — the
        GUI uses this as its i18n translation key. Action-aware error
        mapping wired through `gui_treedb`.
      - **Log hygiene**: 4xx IdP replies are logged as `INFO`, not
        `ERROR` (a wrong password is not a server error), with
        `MSGSET_PROTOCOL`. New `messages` / `traffic` trace levels; 👤
        BFF log prefix and ⏩/⏪ direction arrows across BFF traces.
      - **Own orchestrator GClass** at the top of the `auth_bff` yuno
        (replaces the citizen-yuno shortcut) and `gobj_idprovider` is
        tagged `gobj_flag_manual_start` so it stays dormant until the
        BFF validates its configuration.
      - `gobj_http` single-instance invariant is now asserted in debug
        builds to catch re-entrancy regressions.

    - **perf(auth_bff)**: new `perf_auth_bff` ping-pong-style live
      throughput benchmark (`performance/c/perf_auth_bff/`). Default
      10 s run, ~180 000 ops on the reference box; registered as ctest.

    - **test(c_auth_bff)**: 16-binary suite self-contained under
      `tests/c/c_auth_bff/` with a scriptable mock Keycloak
      (`c_mock_keycloak`): signed HS256 JWTs, configurable latency /
      status / body override. Covers login, callback, refresh, logout,
      validation errors, IdP 401, slow IdP, queue pipelining + overflow,
      browser cancel mid-round-trip, cancel-then-retry, cross-user stale
      replies, expired refresh, 405 / missing body / unknown endpoint.
      Gates the watchdog, `browser_alive`, flush-on-disconnect and
      ghttp_parser fixes.

    - **test(c_llhttp_parser)**: sanity suite for the vendored llhttp
      library and the `ghttp_parser` wrapper (`tests/c/c_llhttp_parser/`).

    - **stress(auth_bff)**: new concurrent stress runner
      (`stress/c/auth_bff/`) that exercises the pending queue, the
      watchdog and the flush-on-disconnect path.

    - **fix(c_prot_http_sr)**: omit response body on 1xx / 204 / 304
      replies (RFC 7230). The parser path was emitting a body for these
      status codes, confusing downstream clients and tripping some
      proxies.

    - **fix(c_task)**: `volatil` gobjs now self-destroy at end-of-work —
      making the long-standing `// auto-destroy` comment actually true.
      The outbound HTTP client used by the BFF is created `volatil` so
      teardown is explicit and framework-free (PR #95). Also silences
      the `-Wcomment` warning in the auto-destroy comment and dedups
      `TRACE_MESSAGES` / `TRACE_MESSAGES2` output.

    - **fix(lib-yui)**: restore `publi_page` iframe rendering for
      logged-out users — a regression in the login split hid the public
      landing page behind the auth screen.

    - **fix(ytls/openssl)**: guard `flush_clear_data` against a
      re-entrant `sskt` free under specific TLS teardown paths.

    - **build(libjwt)**: yuno skeleton `CMakeLists.txt` templates now
      link `${JWT_LIBS}` out of the box (PR #92).

    - **refactor(gobj)**: drop TLS knowledge from `gobj-c`, inject it
      from the ytls layer via a new `gobj_add_global_variable()`
      extension point. Removes the `CONFIG_HAVE_OPENSSL/MBEDTLS` `#if`
      blocks from `gobj_global_variables()` and keeps the core
      backend-agnostic — `root-linux`'s `yunetas_register_c_core()`
      publishes `__tls_library__` and `__tls_libraries__` at startup.

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
