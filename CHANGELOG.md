# **Changelog**

## Unreleased
    - **fix(C_TCP_S): `channel_filter` attr lets two TLS listeners
      share an IOGATE without colliding**.  The "new method"
      (no `child_tree_filter`) iterates the parent IOGATE's
      C_CHANNEL siblings and writes `ytls`, `use_ssl`, `fd_listen`
      onto each — designed for a single C_TCP_S dispatching one
      pre-allocated channel pool.  When two C_TCP_S siblings used
      the same IOGATE, the second one overwrote the first's per-
      channel state across all channels: every channel ended up
      pointing to the second listener's `ytls` (wrong cert) and
      `fd_listen` (wrong socket).  Symptom in production: a second
      TLS listener (e.g. wattyzer cert on :1602 next to estadodelaire
      cert on :1600 in db_history's `__top_side__`) made connections
      to :1600 fail TLS with the wattyzer cert and connections to
      :1602 hang on handshake.  "1 plain + 1 TLS" never tripped this
      because the plain listener doesn't write `ytls`.

      New `channel_filter` string attr on C_TCP_S takes an `fnmatch`
      glob; the new method only claims channels whose `gobj_name`
      matches.  Empty (default) preserves current behaviour.  Use to
      partition the pool when multiple C_TCP_S share an IOGATE:

          "children": [
            {"name":"secure_top_server",
              "kw":{"url":"wss://0.0.0.0:1600",
                    "channel_filter":"tcps-*",
                    "crypto":{...estadodelaire cert...}}},
            {"name":"secure_top_server_wattyzer",
              "kw":{"url":"wss://0.0.0.0:1602",
                    "channel_filter":"tcps_w-*",
                    "crypto":{...wattyzer cert...}}}
          ]

      …with two `[^^children^^]` ranges, one per channel-name prefix.
      Also adds `channel_filter` to the "No channels found" error log
      so a typo'd filter is diagnosable.

## v7.3.2 -- 09/May/2026
    - **feat(release): publish runtime `.deb` on GitHub Releases +
      one-liner `install.sh`**.  First CI workflow in the repo
      (`.github/workflows/release-deb.yml`) builds the AMD64 `.deb`
      on `release.published` (or `workflow_dispatch` against an
      existing tag) via `packages/AMD64.sh` and uploads it as a
      release asset.  Pairs with a new `install.sh` at the repo
      root: a POSIX one-shot installer that detects host arch
      (`amd64` / `armhf` / `riscv64`), queries the GitHub Releases
      API for the latest (or pinned) tag, downloads the matching
      `yuneta-agent-*-<arch>.deb`, and installs it via
      `dpkg + apt-get -f`:

          curl -fsSL https://raw.githubusercontent.com/artgins/yunetas/main/install.sh | sudo sh

      Pin a version with `sudo sh -s -- 7.3.2`.  ARMhf / ARM32 /
      RISCV64 wait for cross-compile or matching runners.  Past
      releases (7.2.0 .. 7.3.1) have no `.deb` assets — the
      workflow operates forward.

    - **refactor(packages): extract `RELEASE` to a shared
      `packages/RELEASE` file** (reset to `1`).  The four arch
      wrappers had `RELEASE` hardcoded with divergent counters
      (3× "9", 1× "6"); now all four read from one file the same
      way they read `YUNETA_VERSION`.  Yunetas isn't widely
      distributed yet, so the renumbering is harmless.

    - **docs(installation): rewrite as 7-step "guía burros" path**.
      `installation.md` restructured: prerequisites + 7 numbered
      steps from "create the `yuneta` user" through "build and
      test", with verbose detail (apt explanations, miniconda
      bootstrap, full `menuconfig` options) tucked into dropdowns.
      Adds a top-of-page **Quick install** section with the
      `install.sh` one-liner and clarifies that the PyPI `yunetas`
      package (0.x) is the management CLI, **not** the framework
      runtime (7.x).  Step 5 documents the env vars `yunetas-env.sh`
      exports — `YUNETAS_BASE`, `YUNETAS_OUTPUTS`, `YUNETAS_YUNOS`
      — plus the `PATH` prepends and the layout contract
      (`outputs/` and project repos as siblings of the `yunetas`
      repo).  Adds an explicit "re-source per shell" warning, a
      silent footgun in cron / SSH / CI sessions where
      `ybatch` / `ycommand` vanish from `PATH`.

    - **fix(gobj-js): `DTP_STRING` attr coerces null / undefined to
      `""`**.  `json2item` used `JSON.stringify()` as the catch-all
      coercion for non-string values; for `null` that produced the
      literal 4-char string `"null"`, which leaked into IEvent
      payloads.  Specifically: `c_ievent_cli`'s `IDENTITY_CARD`
      sent `"jwt": "null"` to the backend, defeating the
      `empty_string()` check in `c_ievent_srv` that drives the BFF
      httpOnly-cookie auth path; `verify_token` then tried to
      validate the literal `"null"` as a JWT and failed with
      "No OAuth2 Issuer found".  Treat `null` and `undefined` as
      `""` in `DTP_STRING` to bring JS in line with the C runtime
      (where `DTP_STRING` cannot hold a `NULL` pointer).

    - **refactor(C kernel): log hygiene for monitor stats**.  Several
      warning counters in the global-warnings dashboard were noisier
      than they needed to be:
        * `c_auth_bff`: 4xx HTTP responses logged as warning instead
          of info (5xx still error).  Sudden 4xx bursts now show in
          dashboards that filter on warning severity.
        * `c_prot_mqtt`, `c_prot_mqtt2`: malformed CONNECT frames
          (client-side protocol issues) downgraded from error to
          warning; rejection messages tightened so v1 and v2 paths
          bucket into the same precise counter.  Dump the offending
          gbuf when `handle__connect` returns < 0 so the bad CONNECT
          can be inspected in the trace.
        * `c_authz`, `c_ievent_srv`: dropped the duplicate
          "Authentication rejected" warning in `c_ievent_srv` (each
          `result < 0` path in `c_authz::mt_authenticate` already
          logs its own); audited `mt_authenticate` so every
          `result < 0` contributes to the per-msg stats counter;
          fixed a `peername`-empty branch whose `msg` field was
          leaking into the unrelated `dst_service`-not-found stats
          bucket.
        * `ydaemon`: translated the lone Spanish `msg` field
          ("Soy el Matador" → "I am the killer") so monitors and
          search tools group cleanly under the English-only
          convention.

    - **feat(lib-yui): toolbar brand / avatar / dropdown item types
      with per-item `show_on`**.  Three new toolbar item kinds
      validated by `shell_toolbar_helpers.js` and rendered by
      `c_yui_shell.js`: `type:"brand"` (logo image + wordmark, with
      optional action — passive `<div>` if action is omitted),
      `type:"avatar"` (circular initials rendered from a
      host-registered provider via the new
      `yui_shell_set_avatar_provider` /
      `yui_shell_refresh_avatars` helpers), and
      `action.type:"dropdown"` (panel mounted on the popup layer
      with `divider` entries, focus-trap, escape-stack push/pop and
      capture-phase click-outside dismissal — closes on `scroll`
      and `resize` to match native `<select>` UX).  `show_on` now
      applies per item, not just per area.  CSS for all three
      shipped, SHELL.md §3.4 cheatsheet rewritten and §10
      "Implemented" updated.  23 new unit tests for the validators,
      27 chromium e2e specs still pass.

    - **build(linux-ext-libs): nginx / openresty link against system
      libs; ncurses switched to widec for UTF-8**.  The vendored
      OpenSSL / PCRE2 stay only for yuneta's own static binaries
      (`ytls`, `yev_loop`); nginx and openresty now embed
      `libssl` / `libcrypto` / `libpcre` / `libz` from the host,
      same as the distro-packaged nginx — closes a latent
      Makefile-clobbering bug in `re-install-libs.sh`.  Ncurses
      re-enabled `--enable-widec` (v1.11) so `ycli` and `mqtt_tui`
      render UTF-8 emoji / accents instead of `M-x` escape
      sequences; consumers migrated to `<ncursesw/...>` and call
      `setlocale(LC_ALL, "")` before `initscr()`.  Also:
      `MAKEFLAGS=-j$(nproc)` for parallel builds, mbedtls Debug →
      Release, and explicit Release+static+PIC flags across mbedtls
      / jansson / pcre2 / libbacktrace / argp-standalone.

    - **fix(c_auth_bff): shrink `legacy_base` buffer to silence
      `-Wformat-truncation`**.  PATH_MAX-sized `legacy_base` plus
      `/token` or `/logout` suffix into a PATH_MAX destination
      tripped GCC's truncation analysis.  A legacy Keycloak base
      URL is realistically well under 1 KB.

    - **fix(lib-yui): TomSelect re-initialisation guards**.  The
      "Tom Select already initialized on this element" exception
      could be thrown when `build_topic_modal` ran twice — the
      query for `.select2-multiple` was matching inputs in earlier
      modals still attached to the popup-layer.  Scope the query
      to the freshly built `$element`; also add a defensive skip
      when the element already has a `tomselect` instance.

    - **feat(gobj-c, gobj-js): EV_ON_OPEN_ERROR — close before open**.
      When a connection-oriented gobj closes before ever opening (TCP
      connect failed, TLS cert refused, non-101 handshake response,
      handshake timeout, firewall) it now publishes a separate
      `EV_ON_OPEN_ERROR` instead of `EV_ON_CLOSE`, preserving the
      EV_ON_OPEN→EV_ON_CLOSE FSM contract for subscribers that only
      handle close in their connected state.  Declared as a kernel
      event in `g_ev_kernel.{h,c}` and wired in:
        * `kernel/c/root-linux/src/c_ievent_cli.c` (IEvent client)
        * `kernel/c/root-linux/src/c_websocket.c` (low-level WS)
        * `kernel/js/gobj-js/src/c_ievent_cli.js` (browser client)
      Mirrors the browser WebSocket split (.onopen/.onclose/.onerror).
      Flagged with `EVF_NO_WARN_SUBS` so backend FSMs that ignore it
      don't trip the no-subscribers warning; interactive frontends
      opt in.  Retry policy unchanged: the connection-responsible
      gobj keeps reconnecting forever while running — only the parent
      (by stopping the gobj) decides to give up.  Each emission also
      writes a `log_warning` (`MSGSET_CONNECT_DISCONNECT` in C)
      including the remote yuno identity / url / peername — gives
      logcenter and other monitors a precise per-attempt alert that
      a silent retry loop is in progress.

    - **fix(lib-yui): bare-route redirect skips decorative items**.
      `navigate_to()` was using `submenu.items[0].route` as the
      fallback for a level-1 container — undefined when item 0 is a
      `type:"header"` / `type:"divider"`, which caused the bare route
      to fall through to "no target".  Use the first item with a
      `route` instead; `submenu.default` still wins.  SHELL.md §3
      updated.

    - **feat(lib-yui): item tooltips**.  Nav and toolbar items accept a
      `tooltip` field (fallback: `aria_label`); rendered as the HTML
      `title` attribute on the generated `<a>`/`<button>`.

    - **feat(yuno-skeleton): `js_gui` template**.  New skeleton type for
      JS GUI yunos — Vite + lib-yui declarative shell with locales/
      (en+es), public/ web assets, 5 placeholder primary areas, and a
      burger drawer hosting Account + Help.  Registered in
      `__skeletons__.json` (type: Yuno; vars: version, description,
      author, author_email, license_name).

    - **feat(gobj-js, lib-yui): translatable tooltips**.  Nav and toolbar
      items rendered by lib-yui now also emit `data-i18n-title="<key>"`
      next to their `title` attribute, and `refresh_language()` in
      gobj-js gained a second pass that walks `[data-i18n-title]` and
      re-translates the `title`.  Hover tooltips swap language alongside
      the visible labels.

    - **feat(gobj-js, lib-yui): translatable aria-labels**.  Nav and
      toolbar renderers now also emit `data-i18n-aria-label="<key>"`
      next to their `aria-label` attribute (toolbar root, action items,
      brand, avatar, dropdown panel, dropdown rows, and nav items), and
      `refresh_language()` walks `[data-i18n-aria-label]` to rewrite
      `aria-label`.  Screen-reader names now follow the active locale.

    - **fix(lib-yui): toolbar dropdown anchor drift on scroll/resize**.
      The `position:fixed` panel coordinates are frozen at open time
      from `getBoundingClientRect()`; any layout shift previously left
      the panel detached from its trigger.  Match native `<select>` UX
      and dismiss the dropdown on scroll (capture, passive — catches
      every ancestor scroller) and on window resize.

    - **refactor(gui_treedb): apply locale convention**.  Trimmed
      en.js/es.js to the 19 keys actually called from src/ (auth_bff
      protocol IDs + the half-dozen `t(...)` calls in
      `c_yuneta_gui.js`); deleted ~140 aspirational entries that had
      no caller.  Renamed `remote-service` → `remote service`,
      `connection-backend-refused` → `connection to backend refused`
      (rule: spaces, not kebab); fixed top-level `nombre:` → `name:`
      to match the rest of the codebase.  Added
      `keySeparator: false` + `nsSeparator: false` in
      `setup_locale()` so a future dotted key (e.g. device-namespace)
      doesn't fall silently to nested-lookup.  Same
      `scripts/validate-locales.mjs` + `prebuild` wiring as
      wattyzer.  Auth_bff snake_case codes kept as-is (wire
      contract, see `c_auth_bff.c`).

    - **feat(yuno-skeleton): locale convention + validator**.  The
      `js_gui` template now ships `scripts/validate-locales.mjs`
      (asserts every i18n key is ASCII + lower-case + present in every
      locale) wired as `npm run validate-locales` and `prebuild`.
      en.js/es.js header banners spell out the convention so new
      yunos inherit it from day one.

## v7.3.1 -- 30/Apr/2026
    - **breaking(auth): standard OIDC migration of `c_auth_bff` and
      `c_task_authenticate`**.  Both gclasses now resolve IdP endpoints
      in the same priority order:

        1. Explicit `token_endpoint` + `end_session_endpoint` attrs
           (full URLs, skips discovery — one fewer round-trip).
        2. `issuer` attr — task chain prepends a GET of
           `<issuer>/.well-known/openid-configuration` and caches the
           resolved endpoints in priv before the auth flow runs.
        3. Refuse to start.

      Any conformant OIDC IdP works (Keycloak, Auth0, Cognito, Azure AD,
      Authentik, ...).  Hardcoded Keycloak path scheme removed.

      - **`c_task_authenticate` and its 6 callers** (`c_cli`, `c_mqtt_tui`,
        `c_ycommand`, `c_ystats`, `c_ytests`, `c_ybatch`) had their
        legacy `auth_url`+`auth_system` attrs **removed outright** and
        the `azp` attr **renamed to `client_id`** to match the form
        parameter actually sent on `/token` and `/logout`.
      - **CLI flag set** in `ycommand` / `ystats` / `ytests` / `ybatch` /
        `mqtt_tui` is now `-I/--issuer`, `-T/--token-endpoint`,
        `-E/--end-session-endpoint`, `-Z/--client-id`.  Old `-K/--auth_system`,
        `-k/--auth_url` and `-Z/--azp` (renamed) are gone.
      - **`c_auth_bff` keeps `idp_url`+`realm`** as a deprecated path
        (warning fired at `mt_create`); removal scheduled once one
        release has shipped with the warning in place.  See
        [`TODO.md`](TODO.md) for the remaining smoke tests against
        non-Keycloak IdPs and the open ROPC-vs-PKCE question.

    - **feat(gobj, gobj-js): `SDF_DEPRECATED` attribute flag**.  New
      sdata flag (`0x00000100`) to mark a gclass attribute as deprecated.
      Both the C runtime and the JS runtime emit a warning when a
      deprecated attribute is set during gobj creation, naming the
      gclass and the attr.  First adopter: `c_authz::authz_yuno_role`
      (use `authz_service` instead).

    - **test(c_task_authenticate)**: new self-contained suite under
      `tests/c/c_task_authenticate/` (`test1_discovery`,
      `test2_explicit_endpoints`, `test4_discovery_failure`).  Mock IdP
      gclass with `override_*_body` knobs for failure injection;
      shared `test_main.c` boilerplate; the driver subscribes to
      `EV_ON_TOKEN`, asserts the result code, and dies.

    - **test(c_auth_bff)**: new `test17_legacy_idp_url` covers the
      `idp_url`+`realm` deprecation path that tests 1–16 missed.
      Captures the deprecation warning at `LOG_OPT_UP_WARNING` and
      drives the full login flow against the same mock-Keycloak.

    - **feat(lib-yui): declarative app shell `C_YUI_SHELL` + `C_YUI_NAV`**.
      A JSON-driven replacement for `C_YUI_MAIN` + `C_YUI_ROUTING`, shipped
      alongside the legacy stack (no migration planned — see
      [`SHELL.md` §10](kernel/js/lib-yui/SHELL.md)).  New GUIs can adopt
      the new shell; existing GUIs keep using the old one unchanged.
      - **Layered grid**: 6 z-stacked layers (`base`, `overlay`, `popup`,
        `modal`, `notification`, `loading`) and 7 zones (`top`, `top-sub`,
        `left`, `center`, `right`, `bottom-sub`, `bottom`) inside `base`,
        all driven by a single declarative JSON config.
      - **Six menu layouts**: `vertical`, `icon-bar`, `tabs`, `drawer`,
        `submenu`, `accordion`.  Same menu may render differently per
        zone via `render[zone]`.  Auto-expand of the active branch on
        accordion when the route changes.
      - **`show_on` parser**: zone visibility per Bulma breakpoint with
        the operators `>=`, `<=`, `<`, `>`, enumeration and `|`.  Pure
        module (`shell_show_on.js`), 13 `node --test` unit tests.
      - **Three lifecycle modes per item** (`eager` / `keep_alive` /
        `lazy_destroy`) decide when the routed view is created and
        destroyed.
      - **Single router**: `C_YUI_NAV` publishes `EV_NAV_CLICKED`; the
        shell publishes `EV_ROUTE_REQUESTED` (intent, audit witness)
        and `EV_ROUTE_CHANGED` (fact).  Hash-based 2-level routing,
        no dependency on `C_YUI_ROUTING`.
      - **Drawer overlay** on the `overlay` layer with focus-trap
        (Tab/Shift+Tab cycling, focus restoration on close), backdrop
        click closes via `EV_DRAWER_CLOSE_REQUESTED` (canonical close
        path with focus-trap release + escape-stack pop).
      - **Escape priority chain**: `priv.escape_stack` is a LIFO of
        `{layer, handler}`; the global `keydown` listener calls only
        the top entry.  Modal-over-drawer closes the modal first.
        Public API `yui_shell_push_escape` / `yui_shell_pop_escape`
        for app-level overlays.
      - **Modal / notification API** on top of the shell layers
        (`yui_shell_show_info` / `show_warning` / `show_error` /
        `show_modal` for non-blocking; `yui_shell_confirm_ok` /
        `confirm_yesno` / `confirm_yesnocancel` for blocking dialogs
        that resolve a Promise).  Each modal/dialog auto-pushes onto
        the Escape stack and installs a focus-trap.  Bulma `.modal-card`
        / `.notification` markup verbatim.  Generic focus-trap moved
        to `shell_focus_trap.js` with 10 unit tests.
      - **Canonical i18n via `data-i18n` + `refresh_language`**: every
        translatable text node carries `data-i18n="<canonical key>"`;
        apps switch language by calling
        `refresh_language(shell.$container, t)` from `@yuneta/gobj-js`,
        the same flow `c_yui_main.js` uses in `change_language()`.
        Modals/dialogs accept `opts.t` so they render in the active
        language at open time AND retranslate live afterwards.
      - **Generalised secondary-nav loop**: `instantiate_menus()` walks
        every menu mounted via a `"menu.<id>"` host whose items declare
        a `submenu` (not just `menu.primary`).  Synthesised menu_id is
        `secondary.<owning_menu_id>.<item.id>`, scoped so two
        primary-style menus can share item ids without colliding.
      - **`gcflag_no_check_output_events`** on the shell so the toolbar
        can publish arbitrary user-defined events
        (`action.type:"event"`) without each app having to extend the
        shell's `event_types` table.
      - **Hard contracts**: every view gclass MUST expose `$container`
        in `mt_create`; every navigation through an empty/unknown route
        logs `log_error` and surfaces a placeholder banner; every
        try/catch logs via `log_warning` (no silent swallow).
      - **`validate_config()`**: system-boundary guard run at the top
        of `mt_start`.  Rejects malformed configs with a visible
        "invalid config" banner instead of producing a half-built
        shell.  Checks: object/array shapes, zone-id membership in the
        7 valid zones, `host` syntax (`toolbar` | `menu.<id>` |
        `stage.<id>`), stage zones declared in `shell.zones`, and
        cross-menu route-target uniqueness (warn when two menus claim
        the same target).
      - **Playwright e2e harness**: 22 spec files × 3 browsers
        (chromium + firefox + webkit) = 69 tests covering boot /
        navigation / drawer / modals / multimenu / validator /
        lifecycle / breakpoint / live-i18n.  CI workflow
        `.github/workflows/lib-yui.yml` runs unit + e2e on PRs and
        pushes touching `kernel/js/lib-yui/**` or
        `kernel/js/gobj-js/**`.  `kernel/js/lib-yui/install-e2e-deps.sh`
        helper installs the apt packages WebKit links against
        (`libgstreamer-plugins-bad1.0-0`, `libavif16`).
      - **Test-app**: standalone harness in `kernel/js/lib-yui/test-app/`
        with three presets (`default`, `?preset=accordion`,
        `?preset=multimenu`) plus a deliberately-broken `?preset=invalid`
        used by the validator regression test.  `C_TEST_LANG`
        controller demonstrates the canonical pattern for reacting to
        custom toolbar events (language toggle, hello toast, ask
        dialog).
      - **Docs**: [`SHELL.md`](kernel/js/lib-yui/SHELL.md) (design,
        configuration JSON, GClasses + events, modal/notification API,
        Escape chain, internationalisation),
        [`TODO.md`](kernel/js/lib-yui/TODO.md) (status of every task on
        the new shell), updated `lib-yui/README.md` with the
        "Which app shell to use?" decision tree.
      - **CLAUDE.md**: new "GClass section layout" addendum (JS skeleton
        banners + canonical CHILD/SERVICE subscription model + Always
        braces rule + EVF_NO_WARN_SUBS) so future agents stay on the
        rails the user established for this work.

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
