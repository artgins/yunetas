# **Changelog**

## 7.5.12
    - **fix(packages): default agent `node_owner` to `"none"` — no controlcenter
      on a fresh node.** The bundled `yuneta_agent.json.sample` /
      `yuneta_agent22.json.sample` shipped `"node_owner": "owner"`, a placeholder.
      The agent starts the controlcenter client whenever `node_owner != "none"`
      (`c_agent.c` `mt_start`), so a freshly installed standalone node kept
      dialing `tcps://<arch>.owner.yunetacontrol.com:1994` and logging
      `getaddrinfo() FAILED` every ~17 s. The default is now `"none"`, the
      design's built-in off-switch, so a fresh box is quiet. Operators **with** a
      controlcenter still opt in with `YUNETA_OWNER=mycompany` at install time
      (the postinst sed now swaps `"none"` → their owner). Note: blanking the
      config to `{}` does **not** silence it — the framework default for
      `node_owner` is `""`, which is also `!= "none"`; `"none"` must be explicit.
      Affects fresh installs only (the `.json` files are conffiles, never
      overwritten on upgrade).

## 7.5.11
    - **security(ext-libs): bump vendored OpenSSL 3.6.2 → 3.6.3.** Security patch
      release (`configure-libs.sh` v1.16, `TAG_OPENSSL=openssl-3.6.3`). Fixes one
      **High** CVE — CVE-2026-45447, heap use-after-free in `PKCS7_verify()` —
      plus a batch of CMS / QUIC / ASN.1 / AES CVEs (CVE-2026-34180..34183,
      35188, 42764..42770, 45445/45446, 7383, 9076). No API change (3.6 series).
      OpenSSL is linked **statically into every yuno**, so every yuno must be
      rebuilt + relinked to pick it up; the release CI builds ext-libs fresh, so
      the published `.deb`/`.rpm` get it automatically. Stayed on 3.6 (not 4.0),
      same LTS rationale as before.
    - **feat(install): no prompt — `install.sh` runs straight through.** The
      installer no longer asks `Install the developer toolchain? [Y/n]` mid-run;
      it installs everything in one pass without stops. Use `--runtime-only` to
      skip the toolchain on a pure deployment box. (Served from `main`, so it
      ships on push.)

## 7.5.10
    - **fix(deb): drop obsolete `libpcre3-dev` from the dev-deps helper.** PCRE1
      (`libpcre3-dev`) was removed from current Ubuntu (26.04) — it is "referred
      to but has no installation candidate", so the helper printed
      `[!] Failed: libpcre3-dev`. Yuneta does not need it (it builds its own
      static PCRE2, and the bundled nginx is static), so it is removed;
      `libpcre2-dev` stays. The `apt-cache show` guard didn't catch it (the
      transitional record still resolves), so the helper now just installs and
      reports the real failures.
    - **fix(deb): honest dev-deps end summary.** The Debian helper ended with a
      vague `Dev environment setup attempt complete` and only printed failures
      inline; it now collects them and reports `all N packages installed` or the
      exact list that did NOT install, matching the `.rpm` helper.

## 7.5.9
    - **fix(deb): never auto-reboot in `postinst`.** The Debian `postinst` forced
      a reboot at the end of install (auto-yes when non-interactive), which under
      `curl | sh` rebooted the box mid-flight — killing the SSH session before
      `install.sh` could install the developer toolchain. The kernel tuning is
      already applied live (`sysctl --system`), so a reboot is not required: it
      now only leaves the `reboot-required` hint and recommends a reboot, never
      forcing one. Matches the `.rpm` `%post` policy.
    - **feat(install): `install.sh` installs certbot on both distros.** After the
      package, the installer now runs the bundled certbot helper (snap on Debian,
      EPEL `dnf` on RHEL) so TLS for the bundled web server is set up in the same
      run, regardless of `--runtime-only` (it is a runtime/ops tool).

## 7.5.8
    - **fix(rpm): dev-deps helper used a dnf5-only flag that RHEL 9 rejects.**
      The 7.5.7 helper ran `dnf install --skip-unavailable`, but
      `--skip-unavailable` only exists in dnf5 (Fedora); RHEL 9 / Rocky 9 ship
      dnf4, which errors `unrecognized arguments: --skip-unavailable` and aborts
      the whole transaction — so the toolchain (git, clang, gcc, wget, …)
      installed nothing again. Now uses `--setopt=strict=0`, the dnf4-native way
      to skip unavailable packages (and valid on dnf5 too).
    - **fix(rpm): create the `nogroup` group so the bundled nginx starts.** nginx
      falls back to its compiled-default group `nogroup`, which exists on Debian
      but not on RHEL; without it nginx aborted at startup
      (`getgrnam("nogroup") failed`). `%post` now creates it (RHEL-only, before
      the service starts).
    - **fix(rpm): honest web-server start in the init script.** `start_web()` ran
      `nginx || true; log_end_msg 0`, printing `OK` even when nginx failed to
      start. It now captures the real exit code and reports it, like the agent
      start. (Matches the no-silent-failure rule.)
    - **feat(install): `install.sh` is now a single cross-distro installer that
      sets up everything in one run.** It detects the distro (`apt` vs `dnf`),
      and on RHEL/Rocky/Alma enables **EPEL + CRB** first; pulls the matching
      package (`.deb` / `.rpm`) from the latest Release and installs it; then
      installs the **full developer toolchain** (git, mercurial, clang, gcc,
      cmake, ninja, wget, pipx, …) by delegating to the bundled, resilient
      `/yuneta/bin/install-yuneta-dev-deps.sh` — so a fresh box is build-ready
      from one command, with no second script to remember. Asks first when a
      terminal is attached (reads `/dev/tty`, so it works under `curl | sh`);
      installs by default when non-interactive. `--runtime-only` skips the
      toolchain for pure deployment boxes. Served from `main`, so it reaches
      users on push (it installs the latest published Release packages). Was
      Debian-only and runtime-only before.

## 7.5.7
    - **fix(rpm): dev-deps helper no longer installs nothing when one package is
      unavailable.** `install-yuneta-dev-deps.sh` ran `dnf -y install "${PKGS[@]}"`
      as a single atomic transaction, so one unfindable package (typically a
      CRB-only `-devel`/`-static` when CRB was never enabled) aborted the WHOLE
      set — leaving `clang`, `wget`, `gcc`, `cmake` … none installed — and the
      `|| echo continuing` + final `[✓] complete` hid it. It now uses
      `dnf --skip-unavailable` (installs every available package, skips only the
      missing) and reports per-package with `rpm -q` which ones did NOT install,
      pointing at EPEL/CRB when a `-devel`/`-static` is absent, instead of a
      green lie. Matches the `.deb` helper's per-package resilience. (`wget` and
      `clang` are dev-deps installed by this helper, not by the base `.rpm`.)

## 7.5.6
    - **fix(rpm): honest agent start in `%post`; don't source the `set -u`-unsafe
      RHEL init functions.** Two RHEL-only packaging bugs in the 7.5.5 `.rpm`
      (the `.deb` was never affected). (1) The generated `/etc/init.d/yuneta_agent`
      runs under `set -u`; on RHEL `/lib/lsb/init-functions` is absent so it fell
      to sourcing `/etc/init.d/functions`, which references unset vars
      (`SYSTEMCTL_SKIP_REDIRECT`…) and aborted the whole init script with
      "unbound variable" **before the agent was ever launched** — the binary was
      fine, the service "failed". It now defines the only two functions it uses
      (`log_daemon_msg`/`log_end_msg`) itself and only sources Debian's
      `set -u`-clean LSB file when present. (2) `%post` started the agent with
      `service start || true`, hiding a failed start behind RPM's always-"Complete"
      transaction. It now re-reads the effective `kernel.io_uring_disabled` after
      `sysctl --system`, only starts when io_uring is usable, captures the real
      result, and prints a loud "AGENT IS NOT RUNNING" warning with diagnosis
      hints (`systemctl status` / `journalctl` / `getenforce` for SELinux) instead
      of a green install over a dead agent.

## 7.5.5
    - **refactor(packaging): split Debian packaging into `packages/deb/`.**
      With the new `packages/rpm/`, the Debian scripts moved from the root of
      `packages/` into a sibling `packages/deb/` (`AMD64`/`ARM32`/`ARMhf`/
      `RISCV64` wrappers + `make-yuneta-agent-deb.sh` + its `README.md`), so the
      two packagers are now symmetric: `packages/deb/` and `packages/rpm/`. The
      shared agent config samples stay at `packages/templates/` (referenced by
      both via an absolute `$YUNETAS_BASE/packages/templates/` path);
      `packages/README.md` is now a short index. The deb arch wrappers read
      `../../YUNETA_VERSION` + `../../RELEASE` (one level deeper); the release CI
      and the `.gitignore` `authorized_keys`/`webserver` rules follow the move.
      No change to the produced `.deb` or its contents.
    - **feat(build): RHEL/Rocky/Alma build support (was Debian-only).**
      Yuneta now builds and runs on the RHEL family; verified end-to-end on
      Rocky Linux 9.7 (full static build + 110/110 ctest). New
      `install-dependencies.sh` auto-detects the distro from `/etc/os-release`
      and installs the right packages with `apt` (Debian) or `dnf` (RHEL,
      enabling EPEL + CRB). RHEL-specific build fixes that ride along:
      `configure-libs.sh` v1.15 forces `-DCMAKE_INSTALL_LIBDIR=lib` on the
      CMake libs (mbedtls/pcre2/jansson/argp), which on RHEL default to
      `lib64` and so were missed by the kernel's `outputs_ext/lib` link path
      (no-op on Debian); `set_compiler.sh` gained a `dnf reinstall` branch;
      and the postgres module includes `<libpq-fe.h>` (not
      `<postgresql/libpq-fe.h>`) with the dir resolved via `pg_config` in
      CMake, since the libpq header sits in `/usr/include` on RHEL vs
      `/usr/include/postgresql` on Debian. RHEL also needs `glibc-static`/
      `libstdc++-static`/`libxcrypt-static` (CRB) for the default static link.
    - **feat(runtime): document the io_uring requirement on RHEL.** Yuneta's
      `yev_loop` is io_uring-based, and RHEL 9 / Rocky 9 / Alma 9 ship
      `kernel.io_uring_disabled=2` (fully disabled), so every yuno aborts at
      startup until it is re-enabled (`kernel.io_uring_disabled=0`). Called
      out in `installation.md`; `install-dependencies.sh` warns when it
      detects the disabled state. No code change — a deployment prerequisite.
    - **feat(packaging): RPM packaging for the Yuneta Agent (`packages/rpm/`).**
      Counterpart of the Debian `packages/`: stages the same `/yuneta` payload
      and builds an `.rpm` with `rpmbuild` (`make-yuneta-agent-rpm.sh` +
      `x86_64`/`aarch64`/`riscv64` wrappers). RHEL-specific envelope: `.spec`
      instead of `control`, `%post`/`%preun`/`%postun` instead of
      maintainer scripts, `useradd`/`wheel`/`chkconfig`/langpacks/EPEL-certbot,
      and the shipped `kernel.io_uring_disabled=0`. Built + inspected on Rocky
      9.7 (`rpm -qlp`/`--scripts`/`rpmlint`); not installed.
    - **ci(release): `release-deb.yml` -> `release-packages.yml`, now also
      publishes the x86_64 `.rpm`.** The release job builds the `.rpm` next to
      the AMD64 `.deb` and uploads both as release assets. It runs on the same
      Ubuntu runner: the default build is fully static, so the binaries also
      run on RHEL/Rocky — no EL9 container needed.
    - **fix(tests): make `ytls/test_cert_info` portable to OpenSSL >= 3.5.**
      The expired-cert helper used `openssl x509 -req -days -1`, which
      OpenSSL >= 3.5 rejects ("end date before start date"). It now falls back
      to `-not_before/-not_after` with fixed past dates (OpenSSL >= 3.2) when
      the negative-span form fails, keeping older OpenSSL (e.g. Debian 12's
      3.0.x) working. Not RHEL-specific — any host with a modern OpenSSL.
    - **fix(emu_device): don't truncate the final frame on standalone exit.**
      `finish_replay()` called `exit(0)` right after queueing the last frames,
      before io_uring completed the write — the final frame could be lost. In
      standalone CLI mode it now waits for the C_TCP `EV_TX_READY` drain signal
      (tx queue empty + in-flight write completed) and exits from `ac_tx_ready`;
      empty replays still exit immediately. Verified end-to-end: a 3-frame replay
      delivered all bytes including the last frame, clean exit. Agent-managed
      mode is unchanged (it never exited).
    - **chore(auth_bff): remove the deprecated `idp_url` + `realm` pair.** The
      legacy Keycloak path-scheme fallback (build
      `<idp_url>/realms/<realm>/protocol/openid-connect/{token,logout}`) was
      `SDF_DEPRECATED` since the 2026-04-30 OIDC migration and a release has
      shipped with the warning. Both `SDATA` attrs and the resolution branch in
      `c_auth_bff` `mt_create` are gone; configure `issuer` (discovery) or the
      explicit `token_endpoint` + `end_session_endpoint` instead. No yunetas or
      private deployment still set the legacy pair (all on `issuer`). Docs
      updated (`YUNO_AUTH.md` §2.5, `guide_oauth2_pkce_bff.md`). The now-subjectless
      `tests/c/c_auth_bff/test17_legacy_idp_url` is removed; the suite is 18/18.
    - **docs(auth): ROPC in `c_task_authenticate` deferred by design.** The CLI
      grant stays `grant_type=password` (works on Keycloak, the only deployed
      IdP). Documented the constraint and the real migration path (device-flow
      for interactive + client-credentials for headless CI, not loopback PKCE —
      the six CLI callers have no browser) in the `c_task_authenticate.c` header,
      `YUNO_AUTH.md` §3.4, and `TODO.md`. No behavior change.

## v7.5.4 -- 08/Jun/2026
    - **feat(mqtt/security): subscribe-side ACL enforcement.** Completes the
      publish/subscribe ACL started in 7.5.3. The per-topic SUBACK reason is
      built in the broker's `ac_mqtt_subscribe`, so the check lives there
      (alongside the existing `deny_subscribes` gate), calling
      `mqtt_acl_check(…, "read")` per requested filter — a denied filter is not
      added and gets a `MQTT_RC_NOT_AUTHORIZED` (v5) / `0x80` (v3.x) SUBACK
      reason, logged. Unchanged when `enable_acl` is off or a group has no
      `subscribe_acl` patterns.
    - **fix(security/authz): per-command authz gate redesign.** A local agent
      pilot showed the 7.5.3 gate (`enable_command_authz`) was undeployable — it
      denied a yuno's own internal startup commands (e.g. `open-treedb`) and the
      yuno exited. Two bugs fixed in gobj-c: (A) a specific-authz lookup on a
      concrete gobj now falls back to the **global** authz table
      (`authzs_list`), so `__execute_command__` resolves on any gobj (was
      "authz not found" → deny-all, root included); (B) the gate fires **only
      for external commands** (those whose kw carries the authenticated
      `__username__` injected by `c_ievent_srv`), so internal `gobj_command()`
      calls are never gated (`command_parser`). Re-piloted: the agent now boots
      clean with the gate on and `ycommand` (root) works. `enable_command_authz`
      remains **default-off**.
    - **feat(security/authz): seed root role model** in the C_AUTHZ yunos that
      lacked one (`controlcenter`, `mqtt_broker`, `emailsender`) via
      `Authz.initial_load` (role `root` + user `yuneta`, mirroring the agent),
      a prerequisite for enabling the command-authz gate there.

## v7.5.3 -- 08/Jun/2026
    - **feat(security): per-command authorization re-armed (gated opt-in).** The
      `SDF_AUTHZ_X` check at the command-dispatch boundary in `command_parser.c`
      (commented out for years) now runs again, but only when the yuno sets the
      new `enable_command_authz` attr (`c_yuno`, `SDF_RD`, default `"0"`), so the
      default posture is unchanged and non-breaking. Self-issued commands
      (`src == gobj`) bypass the check; a denial returns `-403` and is logged
      (`MSGSET_AUTH`). Turning the gate on requires a running `C_AUTHZ` role
      model (the global checker is fail-closed). Test:
      `tests/c/command_authz/`.
    - **feat(mqtt): publish-side ACL (model A, group-based, default off).** New
      `enable_acl` attr on `C_MQTT_BROKER` plus `publish_acl`/`subscribe_acl`
      array columns on the `client_groups` topic (schema_version 25→26,
      topic_version 3→4; additive). `C_PROT_MQTT2` queries the broker over a
      direct `EV_MQTT_ACL_CHECK` event on PUBLISH; allow when ACL off or no
      patterns authored, deny unknown clients, deny logged. Subscribe-side
      wiring is staged (schema + helper ready) but not yet enforced. Test:
      `tests/c/c_mqtt/acl`. Also fixed a latent fkey bug: the helper now passes
      `{fkey_only_id:1}` so `client_groups` resolves as plain ids (otherwise the
      ACL silently allowed all).
    - **fix(emu_device): frame emission path.** `window`/`interval` were coerced
      to 0 (CLI values passed as `json_string` into `DTP_INTEGER` attrs;
      `cmd_write_*` used `kw_get_str`+`atoi`), so the replay sent nothing. Now
      `json_integer(atoi(...))` on the CLI side and `kw_get_int(KW_WILD_NUMBER)`
      in the commands; also freed the replay resources on `mt_play` error paths
      and log a skipped record with no `frame64`.
    - **refactor(emu_device): moved from `yunos/c/` to `utils/c/`.** It is a
      standalone CLI utility yuno (a device-gate emulator run by hand for
      testing), not a deployable service — now installed to `/yuneta/bin` like
      the other `utils/c` tools.
    - **docs:** `YUNO_AUTH.md` rewritten to describe the gated authz (was
      documented as "commented out", including the auth-flow diagram);
      `mqtt_broker.md` gains an Authorization (ACL) section.

## v7.5.2 -- 06/Jun/2026
    - **security: hardening batch (memory-safety + injection across the stack).**
      - **gobj-c:** NULL-guard in `gbuffer_deserialize`; bounded recursion in
        `kw_find_path` and the kwid comparators (hostile-JSON stack exhaustion).
      - **root-linux:** NUL-terminate accumulated URL / header field / header
        value in `ghttp_parser` (over-read fed to `json_string`); guard
        `/auth/logout` against an uninitialized refresh-token read.
      - **ytls:** re-entrant use-after-free in `encrypt_data` WANT path + a
        double `gbuffer_get` (stream corruption).
      - **yev_loop:** bound DNS response parsing + unpredictable transaction id;
        defer event free until in-flight io_uring CQEs drain (UAF).
      - **timeranger2:** validate on-disk md2 `__offset__`/`__size__` before
        read AND before the `delete_instance` payload wipe (cross-record
        overwrite); guard `read_md` return; bound the inotify parse loop.
      - **libjwt:** backport GHSA-q843-6q5f-w55g algorithm-confusion JWT forgery
        fix + the full cfd8902 hardening; add an in-tree regression test
        (`tests/c/libjwt/`, RSA/EC/EdDSA/`alg:none`).
      - **modbus:** reject MBAP length < 3 (heap overflow). **dba_postgres:**
        escape SQL identifiers/literals in insert + create-table (SQLi).
      - **emailsender:** reject CR/LF in header/envelope fields (SMTP/MIME
        injection). **mqtt:** reject property-length underflow in C_PROT_MQTT2.

## v7.5.1 -- 03/Jun/2026
    - **fix(treedb): multi-version parent reverse-hook hygiene.** Two
      in-memory hook quirks around versioned (pkey2) parents are fixed at the
      treedb layer:
        - **Unlink targeted only the primary parent version.** A child's fkey
          ref carries just `parent_topic^parent_id^hook` (no version), so
          `treedb_clean_node` unlinked from the PRIMARY instance, leaving a
          stale entry on the non-primary version the child was actually hooked
          on. It now locates the parent-version instance that really holds the
          child (`find_parent_version_holding_child` over the pkey2 index) and
          unlinks that one; the primary-instance behaviour is the fallback for
          hook+fkey combos the read-only probe can't match.
        - **Duplicate hook entries.** Repeated create/link of the same child id
          left it more than once in the parent hook (`yunos:["5000","5000"]`),
          inflating "Using in N". `_link_nodes` and the loader
          `link_child_to_parent` now dedup by child id before appending
          (idempotent link), and the child-side fkey array is deduped too.
      A skipped duplicate is **warned** (not silently swallowed): a re-link, or
      a duplicate fkey self-healed on load, is surfaced via `gobj_log_warning`.
      Closes the TODO follow-up; both quirks also self-heal on reload.

## v7.5.0 -- 03/Jun/2026
    - **fix(agent): version-aware, stale-safe `delete-config`/`delete-binary`
      usage guard.** The "Using in N yunos" guard read the raw `yunos` hook
      count, which is config-id-level (shared across versions) and can carry
      stale/duplicate refs — so an UNUSED config/binary version could not be
      pruned while another version was in use, and lingering refs blocked
      deletes. New `count_yunos_using()` validates every hooked yuno id via
      `gobj_get_node` (a deleted yuno → NULL → skipped) and, when a version is
      given, counts only yunos pinned to THAT version (config↔`name_version`,
      binary↔`role_version`). So an unused/superseded version prunes cleanly,
      only the in-use version blocks, and `force=1` overrides. Combined with the
      durable per-instance delete below, `delete-config version=`/`delete-binary
      version=` now remove a single version durably. (Underlying treedb
      multi-version reverse-hook hygiene remains a minor follow-up — see TODO.md.)
    - **feat(treedb): durable per-instance (pkey2) node delete.**
      `treedb_delete_instance` now tombstones EVERY md2 row belonging to a
      `(key, pkey2_value)` via `tranger2_delete_instance()` (enumerated with a
      transient disk list) and drops the in-memory pkey2 slot — previously it
      only dropped the in-memory slot, so the instance resurrected on the next
      reopen (a treedb instance spans several rows: create + each link/update
      re-appends one with the same id/pkey2; tombstoning just the latest let an
      earlier row reload). The primary index is untouched (callers route only
      non-primary instances; the loader re-elects the highest surviving rowid).
      Whole-key delete (`treedb_delete_node`/`tranger2_delete_key`) is unchanged.
      Exposed through the agent: `delete-yuno`/`delete-config`/`delete-binary`
      now accept the pkey2 (`yuno_release`/`version`) to prune a single
      non-primary release/version, listing via `gobj_list_instances` and
      reading the running-guard from the primary (instance records carry a
      stale `yuno_running`). New regression test covers a multi-row instance +
      close/reopen. (Remaining follow-up: stale reverse-hooks on linked parents
      — see TODO.md.)
    - **fix(agent): `find-new-yunos` inherits node placement across a version
      bump.** A version-bump deploy (`install-binary` + `find-new-yunos
      create=1` + `deactivate-snap`) created the fresh `yunos` row at schema
      defaults, dropping the operator-set `start_priority`/`sched_priority`/
      `cpu_core` and forcing a re-run of `tools/agent/set_start_priorities.py`.
      `cmd_find_new_yunos` now copies those three fields from the prior primary
      row into the emitted `create-yuno` command (and `pm_create_yuno` accepts
      them), so launch tiers and CPU placement survive the bump. Same-version
      REBUILD hot-patches were already unaffected (they keep the existing row);
      genuinely-new yunos still get defaults plus the `util`-tag seed.

## v7.4.8 -- 03/Jun/2026
    - **feat(agent): per-yuno `start_priority` launch tiers.** The agent's
      `yunos` topic gains `start_priority` (band 0..9, default 5). `run-yuno`
      launches ascending (utilities first), `kill-yuno`/`pause-yuno` descending
      (utilities last, so logcenter captures everyone's shutdown), stable within
      a tier. The node-wide relaunch (`run_enabled_yunos`, used by
      `restart_nodes`/`deactivate-snap` and at startup) honours the same order;
      the force-SIGKILL pass stays unordered (no graceful drain to sequence).
      `create-yuno` seeds `start_priority=1` for `util`-tagged yunos (the set
      `run_util_yunos` already starts first) — no app role names in the agent.
      Schema `topic_version` 19→20 + `schema_version` 22→23; the bump only
      refreshes the col schema files, record data is untouched (no store wipe).
      Assign app tiers per node with `tools/agent/set_start_priorities.py`.
    - **feat(agent): node CPU placement (`sched_priority`, `cpu_core`) from the
      agent treedb.** Both are new `yunos` columns the agent injects into the
      launched yuno's config as its `sched_priority`/`cpu_core` attrs, so OS
      scheduling/affinity is a node-local decision instead of being baked into
      the config that travels across nodes. Defaults only: the user config file
      is merged after the agent's and still wins. `cpu_core=0` (default) = no
      boost, unchanged behaviour.
    - **refactor(c_yuno): scheduling attr `priority` renamed to `sched_priority`.**
      The `sched_setscheduler` attr (default 20, applied only when `cpu_core>0`)
      collided with the per-service start order (0..9) and the agent's
      `start_priority`; renamed so the name states what it does. `SDF_PERSIST`
      fallback is a no-op in practice (consulted only at `cpu_core>0`, which no
      shipped yuno sets); no migration shim. The per-service `priority` is
      unchanged. See `TODO.md`.
    - **refactor(agent): `set-ordered-kill` renamed to `set-graceful-kill`.** The
      command never ordered anything — it only sets `signal2kill=SIGQUIT` (the
      yuno catches it and shuts itself down cleanly). Renamed to the honest axis
      (graceful SIGQUIT vs quick SIGKILL, `set-quick-kill`), which also frees
      "ordered" for the real `start_priority` ordering above. No alias kept.
    - **feat(tools): `set_start_priorities.py` — assign `start_priority` by role.**
      One-shot operator tool mapping each managed yuno's role to a launch tier
      (defaults: utilities=1, `gate_*`=4, `db_*`=7; unmatched left as-is) and
      writing the differences via `update-node` (record base64'd into
      `content64`; the inline `record={...}` form is not coerced by the CLI).
      `--rule PATTERN=PRIO` adds/overrides (matched before the built-ins),
      `--dry-run`/`--all`/`--show-all`. Same OAuth2-once + `-j` plumbing as the
      other agent scripts, so it can drive a remote wss:// agent.
    - **feat(tools): `sync_binaries.py`/`sync_configs.py` order restarts by
      `start_priority`.** Both bounced yunos alphabetically; now they read the
      per-yuno `start_priority` the agent exposes via `*list-yunos` and restart
      ascending, so infrastructure comes back before its dependents. Both
      degrade to the previous order when the agent has no `start_priority` yet.
      The quit/decline message also reads `Cancelled - no changes made.` instead
      of `Aborted.` (which looked like a crash).
    - **feat(c_yuno): `print-role` command — runtime equivalent of `--print-role`.**
      Every yuno (the agent included) now answers a `print-role` command that
      returns its basic identity: `role`, `name`, `alias`, **`version`** (the
      yuno's own APP_VERSION) and **`yuneta_version`** (the framework version),
      plus description/tags/required_services/public_services/service_descriptor.
      Until now that info was only printable offline via the binary's
      `--print-role` flag; there was no way to read a *running* yuno's version.
      Lives in C_YUNO's command table, so it is inherited by all yunos. Address
      the yuno gobj with the `-S __yuno__` flag: `ycommand -S __yuno__ -c
      'print-role'` (inline `service=...` is a command parameter, not routing).
    - **feat(tools): `sync_binaries.py` automates the same-version REBUILD
      hot-patch.** A `REBUILD` (`update-binary`) overwrites the slot the running
      yuno executes from, so it failed with `text-file-busy` and the script only
      printed a "kill-yuno first" reminder. Now, once both confirmation gates are
      cleared, it runs the documented per-role cycle itself, scoped by
      `yuno_role` (never node-wide): `kill-yuno` (only if running; orderly
      SIGQUIT, so the gbmem audit runs) → poll `*list-yunos` until the process
      exits → `update-binary` → `run-yuno play=0` (if it was running) →
      `play-yuno` (if it was playing). Prior run/play state is read from
      `*list-yunos` and restored per role, and a role with several instances
      across realms is handled in one shot. New `--no-restart` flag keeps the old
      print-only behaviour. The version-bump path (`find-new-yunos` +
      `deactivate-snap`, a node-wide bounce) stays a reminder.
    - **feat(tools): `sync_configs.py` gains an opt-in `--restart`.** Installing
      a config does NOT need a kill — unlike `update-binary` (which hits
      `text-file-busy` while the yuno runs), a config push always succeeds on a
      running yuno; it just does not take effect until that yuno next (re)starts.
      So by default the script still only pushes and prints the affected yuno ids
      (from the agent record's `yunos` field) as a `kill-yuno` + `run-yuno`
      reminder — restarting is a separate, optional step. Pass `--restart` to
      also bounce the using yunos right away, scoped by yuno `id` (never
      node-wide): `kill-yuno` (only if running; orderly SIGQUIT) → poll
      `*list-yunos` until it exits → `run-yuno play=0` → `play-yuno` (if it was
      playing), preserving prior run/play state. A stopped yuno is left stopped;
      NEW configs (no agent record) print a reminder.

## v7.4.7 -- 02/Jun/2026
    - **feat(c_authz): `create-user` password is now optional.** KC/IdP-
      authenticated users have no local password (`credentials` null) — auth is
      by JWT. The command no longer rejects an empty password; it only hashes
      credentials when one is given, otherwise creates the user password-less,
      the same way `register-idp-user` and the `initial_load` users do.
    - **fix(c_authz): stop resetting a user's "Created Time" on update.** In
      `ac_create_user` the `new_user` flag was inverted (`user?TRUE:FALSE` is
      TRUE when the node already exists), so updating an existing user wrote
      `time=now` into the record, overwriting its creation timestamp. New users
      were unaffected (treedb auto-stamps the `time`-flagged column on create).
      Corrected to `user?FALSE:TRUE`.
    - **fix(yuno_agent): silence spurious "Event NOT DEFINED in state" on every
      login.** C_AGENT subscribes to all of its `authz` service's output events
      but had no FSM entry for `EV_AUTHZ_USER_LOGIN`/`LOGOUT`/`NEW` (consumed by
      controlcenter from its own local authz), so each login/logout logged an
      error. Added accept-and-ignore handlers.
    - **fix(ycommand): accept `EV_ON_OPEN_ERROR` in `ST_DISCONNECTED`.** A failed
      connect / identity-card NAK publishes `EV_ON_OPEN_ERROR` after the close;
      the FSM lacked it and logged "Event NOT DEFINED in state". Now handled like
      `EV_ON_ID_NAK` (`ac_on_close`).
    - **feat(tools): sync_binaries.py / sync_configs.py — OAuth2 passthrough for
      remote agents.** Both scripts now log in ONCE (Keycloak password grant via
      stdlib, or a `--jwt` passed verbatim) and thread the token through `-j` to
      every `ycommand` call, so they can drive a remote `wss://` agent without
      SSH. New flags: `-I/--issuer` (OIDC discovery), `-T/--token-endpoint`,
      `-Z/--client-id`, `--client-secret`, `-x/--user-id`, `-X/--user-passw`,
      `-j/--jwt`. The no-arg local path is unchanged (no auth). `$$()` already
      resolves client-side, so the LOCAL build is what gets uploaded.
    - **fix(emailsender): handle `EV_ON_OPEN` in `ST_WAIT_RESPONSE`
      (reconnect-on-demand).** When the SMTP link had idle-closed, the head
      message is dispatched anyway and `c_smtp_session` reconnects to deliver
      it; on reaching `ST_IDLE` it publishes `EV_ON_OPEN` *before* beginning the
      stashed message — but `c_emailsender` is already in `ST_WAIT_RESPONSE` (it
      changes state before `EV_SEND_MESSAGE`), so every reconnect-to-deliver
      cycle logged a spurious *"Event NOT DEFINED in state"* even though the
      mail was delivered. `ST_WAIT_RESPONSE` now accepts `EV_ON_OPEN` via
      `ac_on_open_waiting`, which only marks the link ready and does NOT
      re-dequeue (a message is already in flight). Also corrected the misleading
      `ac_disconnected` comment in `c_smtp_session`.
    - **feat(c_authz): IdP (Keycloak) user provisioning.** New commands
      `register-idp-user` (create the user in Keycloak via the admin REST API +
      the local `treedb_authzs` user with the chosen role, then email a
      set-password invite), plus `set-kc-config` / `view-kc-config` to configure
      the admin connection. The connection params are neutral persistent attrs
      (`kc_*`, `SDF_PERSIST`) set at runtime — no endpoints/secrets in code or
      committed config; the secret is masked by `view-kc-config`. Lives in
      C_AUTHZ so every auth-enabled yuno inherits it. The outbound work is an
      async multi-job `C_TASK` over a lazily-created `C_PROT_HTTP_CL`.
    - **feat(c_prot_http_cl): accept any JSON value as the request body.**
      `data` is now read with `kw_get_dict_value` instead of `kw_get_dict`, so a
      JSON array body (e.g. Keycloak `execute-actions-email`) is sent verbatim
      via `json_dumps`; the x-www-form-urlencoded path is unchanged (it only
      iterates objects).
    - **fix(packages): cert-sync no longer reloads TLS on every tick.**
      `copy-certs.sh` re-copied the certs each run (mtime bump → spurious
      `reload-certs` broadcast every 15 min): GNU `install -C` never skips a
      symlink source (letsencrypt `live/*.pem`) and re-copies on root/yuneta
      owner mismatch. Now resolves the symlink with `readlink -f` and sets the
      owner via `install -o/-g`, dropping the trailing `chown`.

## v7.4.6 -- 01/Jun/2026
    - **feat(tools): `tools/agent/sync_binaries.py` — reconcile built yunos
      against the agent and push updates.** Drives from the agent's installed
      binaries (`ycommand -c '*list-binaries'`), looks each one up in
      `outputs/yunos` (`--print-role`), and classifies it
      BUMP/DOWNGRADE/REBUILD/UP-TO-DATE/NO-BUILD. After confirmation it runs
      `install-binary` / `update-binary id=<role> content64=$$(<role>)` for the
      chosen roles; it does not automate the node-wide lifecycle steps
      (`kill-yuno`, `find-new-yunos` + `deactivate-snap`) but prints them as
      reminders. Lives under `tools/` (shipped in the install `.deb`, usable on
      a bare node), and is documented at doc.yuneta.io under the new **Tools**
      section.

    - **feat(tools): `tools/agent/sync_configs.py` — reconcile a directory's
      configs against the agent and push updates.** Config-side sibling of
      `sync_binaries.py`. Because configs are not centralized like binaries
      (they live under each yuno's `batches/<host>/`), it drives from the
      current directory: each `*.json` config's id is its filename (minus
      `.json`) and its version is the `__version__` field inside the file
      (`_*.json` batch helpers and files without `__version__` are skipped). It
      looks each up via `ycommand -c '*list-configs'` and classifies it
      NEW/BUMP/UPDATE/UP-TO-DATE/DOWNGRADE/agent-only. After confirmation it runs
      `create-config` / `update-config id='<id>' content64=$$(<path>)`; a
      DOWNGRADE (local older than the agent) is reported but never pushed. It
      prints the affected yuno ids as a `kill-yuno` + `run-yuno` reminder rather
      than automating the restart. Documented at doc.yuneta.io under **Tools**.

    - **feat(yuno_agent): `install-config` alias for `create-config`.** Added by
      analogy with `install-binary`, so config installs read symmetrically with
      binary installs (`c_agent.c`). Corrected the config-command docs that this
      exposed as stale: `YUNO_LIFECYCLE.md` claimed there was no `install-config`
      and that `update-config` "creates and updates" (it only overwrites an
      existing `(id, version)`); the onboarding recipes in `YUNO_LIFECYCLE.md` /
      `SCAFFOLDING.md` / `YUNO_AUTH.md` used `update-config … version=<v>
      zcontent=$$()` — the real form is `create-config … content64=$$()` (version
      read from the file's `__version__`).

    - **chore(packages): yuno binaries + `tools/agent` on PATH per layout.** In
      `make-yuneta-agent-deb.sh` the hardcoded `outputs/yunos` PATH entries moved
      into the profile snippet's layout-detection branch (full source tree vs
      deployed `.deb` node), and `tools/agent` was added there too, so
      `sync_binaries.py` / `sync_configs.py` are runnable by name on a node.

    - **chore(tools): retire `tools/docs-migration/`.** The myst migration is
      done and the Quarto pilot was abandoned, so the two one-off helpers
      (`myst_to_quarto.py`, `strip_toctrees.py`) were removed.
      `verify_api_coverage.py` is a repo-dev verifier (not a node tool), so it
      moved to `scripts/`; its five stale "extra" reports were resolved by
      correct header→landing mapping (no docs removed). `scripts/` is repo-only;
      `tools/` ships in the `.deb`.

## v7.4.5 -- 30/May/2026
    - **fix(yuno_agent): `delete-yuno`'s snap-tag guard read the wrong metadata
      key — it was dead.** `cmd_delete_yuno` read `__md_treedb__`__tag__`, but
      the metadata key is `tag` (set in `tr_treedb.c`; the kernel guard
      `treedb_delete_node` reads `__md_treedb__`tag`). So the agent-level guard
      always saw 0 and never fired — only the kernel `treedb_delete_node`
      backstopped the actual delete (with a cryptic message), and the bogus
      `KW_REQUIRED` on a missing key risked log noise. Fixed to read
      `__md_treedb__`tag` with flag 0 (default 0 = untagged), matching the
      kernel guard and the `delete-binary` guard, and clarified the message to
      *"tagged by snap N (rollback)"*. Found while adding the `delete-binary`
      guard. Verified the key is populated: `snap-content name=<tag>
      topic_name=yunos` lists the tagged yuno records.

    - **fix(yuno_agent): `delete-binary` refuses to purge a binary a snap
      references (clear reason).** A snap pins the binaries it captured:
      `shoot-snap` stamps its id on each topic's current-primary record
      (md2 `user_flag`, surfaced as `__md_treedb__.tag`), `binaries` included,
      and `activate-snap` rolls back to exactly those records — so the binary
      file must survive or `run-yuno` fails with *"primary binary not found"*.
      The kernel `treedb_delete_node` already refuses a tagged node unless
      `force`, and `cmd_delete_binary` breaks before the `rmrdir` when the node
      delete fails — so the file was never actually lost. But unlike the
      sibling `delete-yuno`, `delete-binary` gave no reason (just a cryptic
      kernel log + a generic failure). Added the explicit agent-level guard
      (mirroring `delete-yuno`, reading `__md_treedb__.tag`): a snap-tagged
      binary is refused with *"referenced by snap N (rollback)"* and `force=1`
      overrides — which breaks that snap's rollback, as documented. Verified
      the mechanism live: `snap-content name=<tag> topic_name=binaries` lists
      the exact binary records the snap pinned.

    - **fix(yuno_agent): `list-binaries` shows the binary in use, not every
      instance.** `cmd_list_binaries` had been switched (df0e50e70) from
      `gobj_list_nodes` to `gobj_list_instances`, which made it return one row
      per `(role, version)` — identical to `list-binaries-instances`, and after
      a same-version `update-binary` (an append) even two rows for the same
      `(role, version)`. The reason given at the time ("the new instance is
      invisible in the primary index until `deactivate-snap`") was the pkey2
      staleness bug since fixed in `dbf532ec9`. Reverted `list-binaries` to
      `gobj_list_nodes("binaries", …)`: ONE node per role — the primary, i.e.
      the binary actually in use. `list-binaries-instances` keeps the full
      `(role, version)` enumeration. Verified live: `list-binaries` returns 15
      rows (one per role, the in-use version) while `list-binaries-instances`
      returns 30 (every record). The primary correctly tracks the in-use
      version — an `update-binary` updates it in place; an `install-binary` of
      a new version only changes it once `deactivate-snap` promotes+reloads it
      (correct: the new binary is not in use until then).

    - **feat(ycommand): `history` / `!history` work non-interactively, and
      fix the local-command hang.** Two problems with the history command
      outside `-i`: (1) the line editor (`C_EDITLINE`, `priv->gobj_editline`)
      is only created in interactive mode, and both `list_history()` (the bare
      `history` intercept) and `cmd_local_history()` (the `!history`
      local-table entry) read only that live editor — so `ycommand history` /
      `ycommand -c history` printed nothing even though the history is
      persisted to `~/.yuneta/history2.txt`. Both now fall back to that file
      when there is no live editor. (2) A trailing **local** command in
      non-interactive mode hung: the shutdown timeout is scheduled from
      `ac_command_answer`, which only fires for **remote** commands (a local
      `history` produces no `EV_MT_COMMAND_ANSWER`), so the queue drained and
      ycommand waited forever for an answer that never came. Added
      `schedule_exit_if_done()` at the tail of `run_next_pending()` — when the
      queue is empty, the session is non-interactive, no async command is in
      flight, and we are not in long-lived stdin-pipe mode, it schedules the
      same shutdown timeout. Interactive sessions and pipe mode (which waits
      for EOF) are unaffected.

    - **feat(snap-content): friendlier snap inspection.** The agent's
      `snap-content` (served by `C_NODE` in `c_node.c`) required the numeric
      `snap_id` AND an exact `topic_name`, so you could not ask "where does
      this snap point?" without already knowing the topic names. Two additive,
      backward-compatible changes: (1) the snap is now selectable by
      `snap_id`, `id` (alias), or `name` (resolved against `__snaps__`); the
      legacy `snap_id=` keeps working. (2) `topic_name` is now optional — when
      omitted, the command returns the **overview** of every topic the snap
      tags and how many records each (a cheap count-only walk via a new
      `snap_count_cb`, not a full load), e.g. `snap-content name=pre-744` →
      `realms:3, yunos:16, binaries:15, configurations:16, public_services:2`.
      Pass `topic_name=<topic>` to drill into one topic's foto as before. The
      `id`/`name` params were added to the param schema in both `c_node.c` and
      the agent's `c_agent.c`.

## v7.4.4 -- 30/May/2026
    - **fix(c_websocket): stop synthesizing `EV_ON_OPEN_ERROR` at the
      transport layer.** `EV_ON_OPEN_ERROR` is a high-level event owned by
      the session layer (`c_ievent_cli`, which emits it with the remote-yuno
      identity). The commit that introduced it ("EV_ON_OPEN_ERROR — close
      before open") also added an emission in `c_websocket` `ac_disconnected`
      for the "transport closed before the WS upgrade completed" case. That
      emission is mislayered and has no consumer: no FSM declares
      `EV_ON_OPEN_ERROR` as an input action, so `c_websocket` publishing it to
      its parent (`C_CHANNEL`, sitting in `ST_CLOSED` because it never opened)
      was rejected by `gobj_send_event` with "Event NOT DEFINED in state". On
      slower nodes the `run-yuno` reconnect window widens and the race fired
      once per affected yuno (1:1 with the close-before-upgrade warning).
      `ac_disconnected` now publishes only `EV_ON_CLOSE` when a real session
      existed, otherwise returns silently (pre-918be48b9 behavior), and
      `EV_ON_OPEN_ERROR` was dropped from `c_websocket` `event_types`. The
      high-level emission in `c_ievent_cli` is unchanged.

    - **fix(c_websocket): raise default `timeout_handshake` 5s → 30s.**
      During a mass yuno launch (`kill-yuno` + `run-yuno`) every yuno's
      `agent_client` (`C_IEVENT_CLI` → … → `C_WEBSOCKET`) reconnects to the
      single-threaded agent at once; the agent's event loop is stalled doing
      launch work (loading binaries, fork/exec, treedb) and could not complete
      each WS upgrade handshake within the old 5s window for the yunos at the
      back of the queue → "Timeout waiting websocket handshake" in synchronized
      bursts (one per launched yuno). The timeout firing was counterproductive:
      it `ws_close` + `EV_DROP`s and reconnects after
      `timeout_between_connections`, adding load to the very herd that caused
      it. The new 30s default sits comfortably above the observed agent
      loop-stall during mass launch; the attr is per-instance configurable
      (`SDF_PERSIST`) so a public-facing WS server that wants faster dead-peer
      detection can still tighten its own. The remaining root-cause work
      (jitter on `timeout_between_connections` in `c_tcp` to break the
      synchronized reconnect herd) is not addressed here.

    - **feat(yuno_agent): single command response for `run-yuno`, plus a
      `play` knob.** Scripts driving the agent need exactly ONE answer per
      command to stay in sync; `kill-yuno`/`pause-yuno`/`play-yuno` already
      do, but `run-yuno` emitted ~2N answers over N yunos. Two independent
      causes were fixed: (1) `cmd_run_yuno` created one `C_COUNTER` with
      `max_count=1` INSIDE the per-yuno loop (one answer each); it now
      aggregates the `EV_ON_OPEN` filters into a single counter with
      `max_count=total` AFTER the loop, mirroring kill/pause/play — one
      `"N yunos found to run"` answer. (2) The implicit auto-play: on connect
      `ac_on_open` reconciles `must_play` by calling `play-yuno`, one async
      answer per yuno. A new `run-yuno play=0` parameter (default `1`,
      backward-compatible) launches the process(es) WITHOUT auto-play, so a
      script does `run-yuno play=0` (1 answer) then `play-yuno` (1 answer,
      aggregated over already-running yunos). The suppression is per-launch and
      kept **in-memory in the agent**: `run-yuno play=0` records each
      `launch_id` in `priv->no_play_launches`, and `ac_on_open` consumes it by
      matching the connecting yuno's `identity_card`launch_id`, deleting the
      marker on first connect. It is NOT a treedb column and does NOT mutate the
      persistent `must_play`; a watcher crash relaunch reuses the same
      `launch_id` but the marker is already gone, so autonomous `must_play`
      recovery is untouched.

    - **fix(tr_treedb): refresh the pkey2 secondary index on a runtime
      `treedb_save_node()`.** The secondary `pkey2` index kept objects
      SEPARATE from the primary `id` index, populated only while loading
      from disk (`load_pkey2_callback` gated on `sf_loading_from_disk`).
      At runtime `treedb_update_node()` mutated the primary node in place
      and `treedb_save_node()` only appended a tranger row — neither
      touched the secondary index, so `treedb_get_instance()` /
      `treedb_list_instances()` returned the OLD content after an update.
      Surfaced as the agent's `list-binaries` showing the previous binary
      right after a successful `update-binary` (the agent returns the
      in-memory pkey2 index; the new record was already on disk). Now
      `treedb_save_node()` re-points every pkey2 slot of the node at the
      node object itself. No-op for topics without pkey2s. New regression
      test `tests/c/tr_treedb_update_instance` (create → reload from disk →
      update → assert via get_instance/list_instances); it fails against
      the pre-fix code.

    - **fix(emailsender): correctness + resilience of the SMTP send path.**
      (1) Duplicate `MAIL FROM` → "503 MAIL already given": on AUTH-OK the
      session published EV_ON_OPEN (whose subscriber already begins the
      queued send, being idle) and then began it again; snapshot the
      pending flag before publishing. (2) Permanent 5xx rejections now
      dead-letter immediately (reply code forwarded via EV_ON_CLOSE /
      EV_ON_MESSAGE; `code>=500` = permanent, 4xx/timeout/drop = transient
      retry). (3) Binary (non-UTF-8) bodies persisted base64 under
      `body_base64` instead of being silently dropped by `json_stringn`.
      (4) Reconnection is owned by `c_smtp_session`, not the sender: after a
      `timeout_inactivity` idle-close, `c_emailsender` just dispatches the head
      message (it no longer carries any reconnect timer/backoff), and
      `c_smtp_session` — which must redo the handshake — reconnects its bottom
      `C_TCP` on `EV_SEND_MESSAGE` in `ST_DISCONNECTED` and sends on reaching
      `ST_IDLE`. A handshake failure (AUTH 535, EHLO/banner 5xx) is transient
      for the in-flight message (the server never saw it); only a 5xx in the
      message's own MAIL/RCPT/DATA transaction dead-letters it. (5) Fixed a
      shutdown SIGSEGV at its root: `tira_dela_cola()` now returns early when
      the yuno is not playing (`gobj_pause()` clears the playing flag before
      `mt_pause()`/`close_queues()`), so a deferred EV_ON_CLOSE delivered
      during shutdown no longer touches the closed queue — no defensive NULL
      check needed.

    - **fix(c_tcp): retry with backoff after a failed reconnect in the
      inactivity model.** `set_disconnected()` always cleared the timer in the
      `timeout_inactivity` model — correct for a deliberate idle-close, but it
      also stalled a failed on-demand reconnect (no retry, and no
      EV_DISCONNECTED for a never-connected socket). Now only an idle-close
      (`ac_timeout_inactivity` sets `idle_closed`) skips the retry; a connect
      failure / dropped link schedules the `timeout_between_connections`
      backoff and retries via `EV_TIMEOUT -> ac_connect`, like the classic
      model. This is the layer that owns reconnection/backoff (the emailsender
      rework relies on it).

    - **fix(c_tcp): keep the pending tx queue across a FAILED reconnect in the
      inactivity model.** `set_disconnected()` flushed `dl_tx` on every
      disconnect, so bytes queued while disconnected (to be sent on the
      on-demand reconnect) were lost if the connect failed before succeeding —
      the message vanished silently and was never delivered. Now the queue is
      kept when the connection was NEVER established (`inform_disconnection`
      still FALSE) in the `timeout_inactivity` model on a running gobj, and
      `start_pending_writes()` flushes it once a retry connects. An established
      connection still flushes (its byte stream is broken); `mt_stop()` still
      flushes unconditionally (no leak on stop). New regression test
      `tests/c/c_tcp_inactivity` test4 (queue while the server is down → fail
      retries → server up → echo confirms delivery); it fails against the
      pre-fix code (no echo, FIFO timeout).

    - **refactor(c_tcp): `timeout_inactivity` / `timeout_between_connections`
      / `rx_buffer_size` are deployment config (`SDF_RD`), not runtime
      knobs** — dropped `SDF_WR` (and the misleading `SDF_PERSIST` on the
      two timeouts); widened `priv->timeout_inactivity` to `json_int_t`.

    - **fix(yev_loop): retry the static resolver's UDP `recv()` on `EINTR`,
      and log `gai_strerror(ret)` not `strerror(errno)`.** A signal
      interrupting the blocking DNS `recv()` made `yuneta_getaddrinfo()`
      fail spuriously (the logged "Interrupted system call" was a stale
      residual errno; getaddrinfo-family return an `EAI_*` code). Both
      `getaddrinfo() FAILED` sites now log the real `gai_*` cause.

    - **fix(ytls): send SNI in OpenSSL client handshakes.** The
      OpenSSL backend never set the TLS `server_name` extension —
      the code was a `// TODO SSL_set_tlsext_host_name` stub — so
      client ClientHellos went out without SNI. Virtual-hosted TLS
      endpoints behind a CDN/WAF (e.g. an Imperva Incapsula front
      end) reject SNI-less handshakes with HTTP 403. The mbedTLS
      backend already set SNI and `c_tcp` already supplies
      `ssl_server_name`; only the OpenSSL path dropped it. Now
      stores `ssl_server_name` in `init()` and calls
      `SSL_set_tlsext_host_name()` per-connection in
      `new_secure_filter()` for client sockets. Server side is
      unaffected (no servername callback registered, so incoming
      SNI is ignored). Verified end-to-end: 403 → 200 against an
      Imperva-fronted HTTPS API.

    - **fix(timeranger2): fire key-delete callbacks for `rt_by_disk`
      followers.** `fire_key_deleted_locally()` skipped every entry
      with an `fs_event_client` (i.e. every `rt_by_disk` follower),
      assuming the `FS_SUBDIR_DELETED` inotify branch fired their
      `key_deleted_callback`. But that branch's only firing
      mechanism *was* `fire_key_deleted_locally()`, which skipped
      them — so a `rt_by_disk` follower's key-delete callback never
      fired on a `tranger2_delete_key()`. The follower's in-memory
      state only reconciled on restart (LOADING reload from
      `keys/`); live deletes were silently dropped for every
      fs-watcher follower framework-wide. Split the fan-out by
      transport via a new `fs_followers` flag: the master in-process
      path fires only non-watcher subscribers (rt_mem
      lists/iterators); the `FS_SUBDIR_DELETED` inotify branch fires
      only the `rt_disk` followers (the inotify event IS their
      signal). Each subscriber now fires exactly once; also removes
      a latent same-process double-fire of non-watcher subscribers.
      Verified: a master `tranger2_delete_key` now drops the key
      from a separate-process follower's in-memory cache live, no
      restart.

    - **fix(yuno_agent): promote highest `yuno_release` to primary on
      `restart_nodes`.** The treedb primary for a yuno-id is the
      highest-ROWID record, not the highest `yuno_release`:
      lifecycle writes (kill/run/snap) append records for whatever
      release is *active*, so after `install-binary` +
      `find-new-yunos` an older release could stay primary and
      `deactivate-snap` relaunched it instead of the new version
      (the long-standing "force volatil" TODO; a `shoot-snap`
      between `find-new-yunos` and `deactivate-snap` reliably
      triggered it). New `promote_highest_release_yunos()` runs in
      `restart_nodes()` BEFORE the treedb reload: for each id whose
      highest non-disabled `yuno_release` is newer than the current
      primary, it re-appends that release so it becomes the highest
      rowid; the reload then makes it primary and
      `run_enabled_yunos()` launches it. An append does NOT move the
      in-memory primary index — only the reload rebuilds it — so the
      promote must precede the `gobj_stop/start`. Version order via
      the existing `get_n_v()`. (`volatil` itself was already
      honored — `mt_update_node` routes volatil updates to
      `set_volatil_values`, in-memory only — so the culprit was the
      non-volatil lifecycle/snap writes, not the run-update.)
      Verified: a multi-yuno realm upgraded to a new release with a
      single `deactivate-snap`.

    - **fix(yev_loop): retry transient ENOMEM in
      `io_uring_queue_init_params`**. A synchronised restart of
      many yunos (e.g. an agent `deactivate-snap` on a node with
      10+ yunos) used to drop 1-3 SIGABRT cores per yuno in
      `/var/crash`, even though every yuno eventually came up
      after the `ydaemon` watcher relaunched it. Root cause was
      `yev_loop_create` aborting via `LOG_OPT_ABORT` on a
      transient `-ENOMEM` from io_uring init: rings consume
      pinned kernel memory (RLIMIT_MEMLOCK / vm.max_user_locks)
      and a simultaneous restart of N yunos saturates that
      budget for a few ms while the previous rings' pages are
      released. Forensic evidence: 12 cores at 13:23 today with
      identical bt bottoming at `yev_loop.c:184`, all `err=-12`
      with `entries=32768`. Fix wraps the init call in a
      5-iteration exponential-backoff retry (100/200/400/800/1600
      ms ≈ 3 s) for `ENOMEM`/`EAGAIN` only; non-transient errors
      (EINVAL, ENOSYS, EPERM…) fall through to the original
      abort path unchanged. Each retry logs a warning so an
      operator can see the pressure event without it being
      silent. Local stress test (3 consecutive `deactivate-snap`
      cycles = 48 yuno restarts) generated 0 cores.

    - **fix(ycommand): keep stdin-pipe queue draining when a
      command returns -1**. The long-lived stdin-pipe mode added
      in 7.4.3 inherited the ybatch convention from `-c` / `-i` /
      file-fed batches: a `-1` result with no leading `-` on the
      command drops the rest of the queue. That convention is
      hostile to stdin-pipe deploys — the operator has already
      piped every line in, and one common non-fatal `-1` (e.g.
      `install-binary` returning "Binary already exists" for a
      slot that's already filled) silently swallows the rest.
      Surfaced on the 7.4.3 wattyzer deploy: binaries got
      registered, then `find-new-yunos` + `deactivate-snap`
      vanished, leaving yunos on the old release until the
      trailing commands were re-run by hand. Fix: in
      `stdin_pipe_mode` every command behaves as `ignore-fail`
      (no queue clear on error). Explicit `-` prefix path stays.
      Repro matrix (4 install-binary in pipe, slot pre-filled):
      before fix 3/4 responses, after fix 4/4. The earlier
      "WS frame interleaving" hypothesis logged in TODO.md was
      ruled out (kept as a post-mortem trail).

    - **fix(emailsender): retry queued emails instead of
      dead-lettering on the first failure, and persist the body**.
      Any send failure (SMTP server down, wrong URL, rejected
      AUTH, or simply the SMTP child still connecting when the
      dequeue timer fired) used to move the email straight to the
      `emails_failed` dead-letter queue and unload it — `max_retries`
      was declared but never used and nothing drains the failed
      queue, so one transient hiccup shelved the message forever.
      Now the message stays at the head of `emails_queue` and is
      only dispatched while the SMTP session is connected and
      authenticated (a momentary outage just waits and retries on
      reconnect); transient failures are retried up to `max_retries`
      total attempts before being dead-lettered. The body is now
      persisted as a string in the queue — it was carried as a
      transient gbuffer pointer that the dequeued kw's auto-decref
      freed after the first attempt, so retries (and any yuno
      restart) lost the body. Also split RCPT recipients on `;` as
      well as `,` (an Outlook-style list or a stray trailing `;`,
      e.g. logcenter's summary `to`, was rejected by the server as
      `501 Invalid TO`). Deployed and validated live on
      `emailsender^artgins`.

    - **feat(emu_device): implement the frame-emission path on
      timeranger2**. The device-gate emulator was a scaffold: its
      replay was written against the removed timeranger v1 API and
      its `__output_side__` had no TCP connex (the v6 Connex/Tcp0
      globals were dead). Ported to v7 — the output side is built
      in code (`C_IOGATE > C_CHANNEL > C_PROT_RAW > C_TCP` to `url`,
      like `sgateway`); `mt_play` loads matching `frame64` records
      via `tranger2_open_list`, and on connect it sends the
      `leading` frame then `window` frames every `interval` ms.
      Compile-verified only; end-to-end runtime validation (needs a
      `frame64` topic + a TCP sink) is tracked in `TODO.md`.

## v7.4.3 -- 27/May/2026
    - **feat(emailsender)!: drop libcurl, native SMTP over ytls**.
      `emailsender` was the only yuno that linked libcurl, which
      dragged OpenSSL/libssh2/c-ares/libidn2/libpsl/libnghttp2/3/
      zlib/brotli into its runtime graph. The dev-host glibc kept
      bumping while production stayed on older versions (e.g. 2.36
      on `app.wattyzer.com`), so emailsender was the one yuno where
      every upgrade needed a build environment matched to the
      target — and it was deliberately skipped from the 7.4.1
      deploy bundle for that reason. Three new building blocks
      land in this release: (1) `C_SMTP_SESSION` (yunos/c/
      emailsender/src/c_smtp_session.{c,h}) — a CHILD-pattern
      protocol gclass that owns a `C_TCP` bottom and walks the
      RFC 5321 submission FSM (banner → EHLO → AUTH PLAIN →
      MAIL FROM → RCPT TO → DATA → \r\n.\r\n → QUIT) with
      multi-recipient support, RFC 5321 §4.5.2 dot-stuffing and
      a best-effort QUIT on `mt_stop`. Uses `istream_read_until_
      delimiter("\r\n", 2, EV_RX_LINE)` so raw bytes from C_TCP
      (`EV_RX_DATA`) and parsed lines (`EV_RX_LINE`) flow through
      distinct actions and never feed back into the istream.
      (2) `mime_encoder.{c,h}` — pure helpers, no gclass; builds a
      complete RFC 5322 message with optional single attachment
      (`multipart/mixed`) or inline-image attachment with
      Content-ID (`multipart/related`), base64-wrapped at 76
      chars per RFC 2045 §6.8, and RFC 2047 base64 encoded-words
      for non-ASCII Subject / From display-name. (3) Cutover in
      `c_emailsender.c`: `priv->curl` → `priv->smtp` as a
      pure_child created with `{url, username, password,
      helo_name=gethostname()}`; the synchronous `gobj_send_event
      (priv->curl, EV_CURL_COMMAND, …)` + immediate
      `process_curl_response` flow becomes async — `ac_smtp_
      command` MIME-encodes, sends `EV_SEND_MESSAGE` to the smtp
      child and stays in `ST_WAIT_RESPONSE`; the response arrives
      later via the new `ac_on_message` handler. Kernel-side
      precursor: `_yev_protocol_fill_hints()` in `kernel/c/
      yev_loop/src/yev_loop.c` learns the `smtps` schema (port
      465, marked `secure=TRUE`) alongside the existing
      `mqtts`/`wss` — strictly additive. Together this removes
      `libcurl4-openssl-dev` from `docs/doc.yuneta.io/
      installation.md`, drops `find_package(CURL REQUIRED)` and
      `${CURL_LIBRARIES}` from the yuno's CMakeLists, deletes
      `c_curl.{c,h}` outright, and brings the emailsender binary
      down to `ldd` reporting only `libgcc_s` + `libc` (vs the
      previous ~12 shared libs). `emailsender^artgins` is already
      pointed at `smtps://ssl0.ovh.net:465` in all three realms +
      the staging batch, so the cutover is a no-config-change
      deploy. **Breaking change for callers building EV_SEND_EMAIL
      kw manually**: the libcurl-era attrs `strict_tls` and
      `auto_inline_images` are ignored (TLS is now decided by the
      URL schema; auto-inline-image HTML rewriting was a libcurl
      `curl_mime_*` feature not reimplemented). The `cmd_send_
      email` command schema is unchanged; existing callers (the
      whole estadodelaire batch + every realm config) keep
      working without edits.

    - **feat(ycommand): long-lived stdin-pipe session keeps OAuth2
      auth open across many commands**. Until now `ycommand` had
      three input shapes: `-c CMD` (one command, exit), `-i`
      (interactive editline over a raw TTY), and an undocumented
      synchronous pipe path inside `ac_on_open` that did
      `while(fgets(line, stdin))` — fine for pre-buffered batches
      but it blocked the yev_loop between lines, so any
      programmatic driver that wanted to send a command, read its
      response, then send another would hang. `-i` was also
      unusable for non-TTY drivers (e.g. claudia-console running
      `ycommand` via Bash) because `tty_keyboard_init`
      unconditionally calls `enableRawMode`, which fails with
      "NOT a TTY" on a piped fd. Net effect: every remote command
      paid the full OAuth2 ROPC round-trip (~200-400 ms against
      Keycloak) even when the caller had ten queued up. New
      behavior, no new flag: when stdin is not a TTY and neither
      `-c` nor `-i` was supplied, ycommand sets up an io_uring read
      event on `dup(STDIN_FILENO)` (yev_loop refuses fd<=0, so we
      dup) and drives lines through the existing
      `split_commands_into_queue` + `run_next_pending` machinery.
      The process stays alive between commands, EOF triggers an
      orderly shutdown via the existing `set_timeout(timer,
      wait*1000)` → `ac_timeout` → `exit()` path, and a
      `priv->cmd_in_flight` gate serialises async dispatches so a
      stdin line arriving mid-flight enqueues instead of racing.
      Auth happens once; the rest of the session is free. Tested
      against local `ws://127.0.0.1:1991` (4-line batches and
      delayed sequences with 3 s gaps between lines) and against
      `wss://app.wattyzer.com:1993` + OAuth2 (three commands with
      2 s gaps, single ROPC). Backwards-compatible: the pre-existing
      `-c` and `-i` paths are untouched, the pipe-mode trigger is a
      strict superset of the previous synchronous fgets behaviour,
      and `echo cmd | ycommand` keeps producing the same output as
      before (just via an event-driven reader). `c_ycommand.c`
      grew +220/-25; no changes elsewhere.

    - **docs(philosophy): add "The Typed-Graph Model" chapter**.
      New page under `philosophy/` slotted between
      [Design Principles](doc.yuneta.io/philosophy/design_principles.md)
      and [Domain Model](doc.yuneta.io/philosophy/domain_model.md),
      articulating the conceptual claim the framework rests on: data
      and behavior are two views of the same typed graph
      (`topic`↔`gclass`, `node`↔`gobj`, `hook`/`fkey`↔
      subscription/`bottom_gobj`, with `sdata_desc_t` describing
      schemas on both planes). Sections cover: the unit is the
      typed binding, not just the node; the two-plane primitive
      table; what kinds of organisation the model can express
      (hierarchies, matrix, workflows, communication topologies,
      versioned-over-time); what does not fit cleanly (schemaless
      iteration, OLAP, eventually-consistent distributed state,
      truly opaque payloads); the implicit axiom; the payoffs at
      scale; and the empirical justification from 15 + years of
      v2/v6 production. Cross-links added in `philosophy/`
      neighbours and reverse-links from `yunos/c/yuno_agent/`'s
      `ENTRY_POINT.md` (See also), `GOBJ.md` (Conceptual frame
      callout: behavior plane) and `YUNO_TREEDB.md` (Conceptual
      frame callout: information plane) so a reader landing in the
      technical chapters can step up one level on demand. Build is
      warning-free; the new chapter appears in the TOC under
      Philosophy.

    - **fix(install-binary): surface the real cause in error
      response**. `cmd_install_binary` built its failure comment as
      `json_sprintf("Cannot create binary: %s",
      gobj_log_last_message())`, which produced *"Cannot create
      binary: "* (empty cause + trailing space) whenever the
      underlying `treedb_create_node` returned NULL because the
      `(id, pkey2=version)` combination already existed — that path
      logs *"Node already exists"* via `gobj_log_warning`, and
      `gobj_log_warning` does not populate `last_message` (only
      `LOG_ERR` and above do). After the per-command reset in
      `command_parser` (7.4.1, `b1abd7f69`), the buffer is `""` by
      then. Two-layer fix, same shape as the snap commands in 7.4.1:
      `treedb_create_node` now calls `gobj_log_set_last_message()`
      alongside the warning so the cause (*"Node already exists in
      '<topic>': id='<id>'"*) reaches every caller that pipes
      `gobj_log_last_message()` into the response (≈13 callers in
      `c_node.c` benefit alongside `cmd_install_binary`);
      `cmd_install_binary` reads `last_msg` once and falls back to
      `"(see log)"` if it's empty, so the response is always
      informative regardless of whether layer-1 was reached. Drive-by:
      removed the stale `// TODO check tranger2_write_user_flag`
      marker above `treedb_shoot_snap` — the function was completed
      in 7.4.0/7.4.1 (`4c89e4b2c` + `46f8f0434`) and the audit
      confirmed no remaining wiring gap.

## v7.4.1 -- 27/May/2026
    - **fix(command_parser): stop misleading stale strerror in
      command responses**. Many `cmd_*` in `c_node.c` build their
      failure comment as `json_string(gobj_log_last_message())`,
      but `gobj_log_set_last_message()` is only called by
      `gobj_log_*()` with priority `<= LOG_ERR`. If the failure
      path logs at `LOG_INFO` (or doesn't log at all),
      `last_message` keeps whatever it had from the previous
      `LOG_ERR` — frequently `strerror(errno)` of an earlier TCP
      disconnect ("Connection reset by peer"). The response was
      being delivered correctly with that strerror as the comment;
      ycommand rendered it verbatim and it looked indistinguishable
      from a real network error, sending operators down a wild
      diagnostic chase. Two-part fix at the kernel command
      boundary in `kernel/c/gobj-c/src/command_parser.c`: (1)
      `command_parser()` resets `last_message` to `""` at entry,
      so every command dispatch starts with a clean slate;
      (2) `build_command_response()` substitutes `"(see log)"`
      when the response is a failure (`result != 0`) and the
      comment is an empty string, so callers that use the bare
      `json_string(gobj_log_last_message())` idiom produce a
      useful placeholder instead of `ERROR -1: `. Success
      responses keep their empty comment (cmd_topics etc. return
      data without a comment by design). Documented the new
      semantics in
      `docs/doc.yuneta.io/api/logging/log.md`
      (`gobj_log_last_message` + `gobj_log_set_last_message`).
    - **fix(agent): `list-binaries` enumerates every
      `(role, version)` instance**. `cmd_list_binaries` called
      `gobj_list_nodes("binaries", ...)`, which only returns the
      in-memory primary per id (role). After an `install-binary`
      that added a second version under the same role, the new
      instance was invisible until a `deactivate-snap` rebuilt
      the primary index — and even then only the most-recent
      version survived. The doc claimed *"returns all rows"* but
      the call had never matched that promise. Switched to
      `gobj_list_instances("binaries", "", ...)`: the topic has
      `pkey2=version`, so the instances iterator returns one row
      per `(role, version)` and multi-version installs are
      visible from the moment `install-binary` appends the
      record. Validated with two coexisting `emailsender`
      versions (7.4.1 + 7.4.2): `list-binaries` now returns four
      rows instead of three. `list-binaries-instances` stays as
      the explicit "instances" alias. `YUNO_LIFECYCLE.md` table
      updated to match.
    - **fix(snap): implement `snap-content` + recover error
      responses to ycommand**. (a) `cmd_snap_content` in
      `c_node.c` was a literal `"TODO"` stub. Now walks the
      requested topic with `tranger2_open_list` filtered by
      `user_flag=snap_id` and a `load_record_callback` that
      drains matching records into a `json_array` carried via
      the rt's `extra` (merged into the rt object by
      `json_object_update_missing_new`, so the callback reads
      `list->snap_data`, not `list->extra->snap_data`).
      Validates `topic_name` and `snap_id` (1..65534, matching
      the `uint16_t` md2 `user_flag` range), returns the schema
      + the array and a `(count)` comment. (b) Error responses
      from `shoot-snap` / `activate-snap` / `deactivate-snap`
      were arriving at ycommand as `"Connection reset by peer"`
      instead of the real cause. Root cause:
      `json_string(gobj_log_last_message())` produced an empty
      JSON string whenever the kernel function logged with
      `gobj_log_info` (which doesn't populate `last_message`),
      and the buffer still held the strerror of a prior
      disconnect. Two-layer fix: `treedb_shoot_snap` /
      `treedb_activate_snap` explicitly call
      `gobj_log_set_last_message()` in their "already exists"
      and "not found" paths; `cmd_shoot_snap` /
      `cmd_activate_snap` / `cmd_deactivate_snap` build the
      error comment with
      `json_sprintf("Cannot ... '%s': %s", name, empty_string(last)?"(see log)":last)`
      so the comment is never empty even if some future caller
      forgets the layer-1 update. Note: there are ~13 other
      `cmd_*` in `c_node.c` with the same
      `json_string(gobj_log_last_message())` pattern — same trap
      — covered by the systemic `command_parser` reset documented
      in the entry above.
    - **fix(snap): preserve previous snap's tag + harden
      `run_yuno` launcher**. (a) `treedb_shoot_snap` stamped
      `user_flag` IN PLACE on every primary record. A second
      `shoot-snap` over a record that hadn't changed since the
      first snap therefore overwrote the older snap's tag,
      making that record unreachable from
      `activate-snap(older)`. Fix: if the primary record already
      carries a tag from a different snap, append a CLONE via
      `tranger2_append_record(user_flag=new)` so the older
      record keeps its tag. Untagged primaries (and same-snap
      re-stamps) still take the in-place path — no extra
      storage. The in-memory `__md_treedb__` is intentionally
      left untouched on the clone branch so subsequent
      `treedb_save_node()` appends keep using the original base
      `rowid`. Validated end-to-end: shoot A → 13 records carry
      `uflag=1`; shoot B (no intermediate change) → still 13
      records carry `uflag=1` + 13 clones carry `uflag=2`;
      `activate-snap A` and `activate-snap B` both restore all
      yunos. (b) `build_yuno_running_script` in `c_agent.c` took
      an uninitialised `char bfbinary[]` from the caller's stack
      and could early-return `0` (silently) on a missing realm
      or binary. All three callers ignored the return value,
      then ran or serialised whatever garbage was on the stack —
      hence the corrupted `/yuneta/bin/<role>^<name>.sh`
      launchers (~21-27 bytes of stack noise) that
      `activate-snap` produced when the binary record didn't come
      back. Fix: zero-init `bfbinary` at function entry, log the
      two early-return sites (no silent errors), and have every
      caller check the return value and respond with an error
      instead of piping uninitialised memory into
      `run_process2()` or a JSON reply. Treedb `treedb_shoot_snap`
      doc page updated to describe the new clone-vs-stamp
      behaviour.
    - **fix(timeranger2): silence two `-W` warnings without
      losing errors**. (a) `treedb_shoot_snap` in
      `kernel/c/timeranger2/src/tr_treedb.c` now returns the
      accumulated `ret` so `tranger2_write_user_flag` failures
      across topics surface to callers instead of being silently
      dropped. (b) `mirror_key_delete_to_disks` in
      `timeranger2.c` uses `build_path()` to assemble the
      `disks/<rt_id>/<key>` path; `build_path` already syslogs
      `LOG_CRIT` on overflow, so no silent skip on truncation
      (closes a `-Wformat-truncation` warning without
      introducing a silent early-out).
    - **docs(api/treedb): document the real snap semantics**.
      The `treedb_shoot_snap` / `treedb_activate_snap` API pages
      described the surface only — name, parameters, return — and
      missed the parts that determine whether snaps actually work
      for the caller: `treedb_shoot_snap` tags the live `.md2`
      record's `user_flag` in place via
      `tranger2_write_user_flag` (so rowid order is preserved
      and re-shoots overwrite prior tags on the same record);
      `treedb_activate_snap("__clear__")` is the deactivate
      path; the active/inactive toggle only flips a flag, and
      the new primary visibility materialises on the **next**
      `treedb_open_db()` — not on the call itself. Updated
      `docs/doc.yuneta.io/api/timeranger2/treedb.md` §§
      `treedb_shoot_snap` + `treedb_activate_snap` with the
      reload semantics + the in-place tag mechanic + the
      16-bit snap-id ceiling. Companion to the `treedb_shoot_snap`
      completion shipped in the same release.
    - **feat(tr_treedb): complete `treedb_shoot_snap` so
      `activate-snap <name>` rolls back primaries correctly**.
      The TODO at the heart of `treedb_shoot_snap` was a dead
      branch: it walked the primary index of every topic but
      the actual `tranger2_write_user_flag` call was commented
      out, so snaps only ever created an entry in `__snaps__`
      and never tagged any record. The agent's `activate-snap
      <name>` path queried `snap_tag` correctly on reload (see
      `treedb_open_db` line 1299 — the user_flag filter is
      enforced), but with no record carrying the tag the load
      returned an empty primary index, and the rollback silently
      did nothing — the test suite never caught it because there
      was none. Wired the tag write in-place via
      `tranger2_write_user_flag(tranger, topic_name, key, t,
      i_rowid, user_flag)` so the existing record gets stamped
      without inflating the `.md2` (using `treedb_save_node`
      would create a new instance at the highest rowid and
      then steal "latest" after `deactivate-snap`, masking the
      newer records the user actually wants live). Also tightened
      the snap-id range check from 32-bit (`0xFFFFFFFF`) to 16-bit
      (`0xFFFF`) since `user_flag` is `uint16_t` end-to-end. New
      regression: `tests/c/tr_treedb_snap` walks 9 phases
      modelling the agent's upgrade lifecycle (seed v1 → add v2
      → shoot snap_v1 → deactivate → reload picks v2 → add v3 →
      deactivate → reload picks v3 → shoot snap_v3 → activate
      snap_v1 → reload rolls back to v1 → activate snap_v3 →
      reload to v3 → deactivate → stays at v3) on two topics
      (`binaries` + `yunos`) keyed exactly like the agent's
      `binaries` / `configurations` / `yunos`. `tr_treedb` and
      `tr_treedb_delete_instance` rerun green against the patched
      library — no regressions.
    - **docs(yuno_agent): document the version-bump upgrade flow**.
      `kill-yuno` + `run-yuno` does not pick up a new release —
      `cmd_run_yuno` walks the `yunos` topic primary index, which
      keeps pointing at the older `pkey2` (`yuno_release`) after
      `find-new-yunos create=1` appends the new row. The fix is
      always `install-binary` → `find-new-yunos create=1` →
      `deactivate-snap`. `deactivate-snap` with no args (and no
      active snap) is the only supported way to trigger
      `restart_nodes()` (`c_agent.c:8816`), which SIGKILLs every
      running yuno, `gobj_stop/start`s the treedb resource so the
      primary index is rebuilt from disk with the newest `pkey2`
      first, then runs every must-play yuno. Equivalent to
      `yshutdown` + `restart-yuneta` at the agent-process level,
      but without restarting the daemon. Added as `YUNO_LIFECYCLE.md`
      §6.5 + §6.6 (rollback via `shoot-snap` / `activate-snap`) and
      refreshed `CLAUDE.md` §3 with the same-version vs version-bump
      split. Verified live with `gate_pvpc 1.3.1.0 → 1.3.1.1`
      on the local agent.

## v7.4.0 -- 26/May/2026
    - **chore(lib-yui)!: declarative shell stack removed —
      `@yuneta/lib-yui` jumps to 8.0.0**. The new declarative
      shell (`C_YUI_SHELL`, `C_YUI_NAV`, `C_YUI_PAGER`,
      `C_YUI_WIZARD`, `shell_modals` and every `shell_*_helpers`
      module + the full Playwright e2e suite and the `test-app/`
      vite project) was already being maintained from the
      wattyzer-vendored copy at `wattyzer/gui/src/lib-yui`. The
      kernel copy was just dead weight in the bundle consumed
      by estadodelaire (legacy apps that only use
      `C_YUI_MAIN` + `WINDOW` + `TABS` + routing) and the two
      copies had drifted enough to make every fold-back a
      conflict. Verified by grep that neither legacy consumer
      imports any shell symbol before deleting. `dist/lib-yui.es.js`
      is now 3.4 MB / gzip 706 KB (~25% lighter). Migration: if
      you need the declarative shell, the canonical copy is
      `wattyzer/gui/src/lib-yui/` (private repo) as of
      2026-05-15. The yuno-skeleton `js_gui` scaffold that
      referenced `register_c_yui_shell` was dropped here too;
      the replacement scaffold lives in
      `wattyzer/templates/js_gui/`.
    - **chore(ext-libs): three security bumps (v1.12 → v1.13 →
      v1.14)**. v1.12: nginx 1.28.3 → 1.30.1 (CVE-2026-42945),
      openresty → 1.29.2.4, openssl 3.6.1 → 3.6.2. v1.13: nginx
      → 1.30.2 (CVE-2026-9256, buffer overflow in
      `ngx_http_rewrite_module`). v1.14: openresty → 1.29.2.5
      (backports the CVE-2026-9256 patch into the
      openresty-bundled nginx + a `proxy_protocol v2`
      over-read fix). All three are pin-only — nginx and
      openresty are separate dynamically-linked binaries (see
      `configure-libs.sh` v1.10), so no yuneta consumer /
      header / CMake change rides along. OpenSSL deliberately
      held on the 3.6 LTS series; the 4.0 jump (non-LTS,
      EOL 2027-05, drops engines / legacy init) is tracked
      separately.
    - **refactor(tranger2): rename `tranger2_delete_record` →
      `tranger2_delete_key`**. Locks the vocabulary
      timeranger2 was using loosely: *record* = a primary key
      (whole `keys/<key>/` directory, deleted via
      `tranger2_delete_key`); *instance* = one row of that
      key's `.md2` index, addressed by
      `(key, __t__, rowid)`. The legacy name is kept as a
      source-level alias
      (`#define tranger2_delete_record tranger2_delete_key`),
      so external callers keep compiling unchanged. In-tree
      caller (`treedb_delete_node`) updated; README, the
      timeranger2 API page (with the old MyST anchor preserved
      so external links to `(tranger2_delete_record)=` keep
      resolving) and the appendix index follow the rename. A
      subsequent commit dropped "soft" from the delete-instance
      vocabulary: granularity, not reversibility — both
      deletes are irrecoverable.
    - **feat(tranger2): `tranger2_delete_instance()` — per-row
      tombstone**. Mutates one row of the `.md2` index in place via
      `sf_deleted_instance = 0x0400` (reinstated in `system_flag2_t`
      on the inherited side of the mask, so `rt_by_disk` followers
      see the same tombstone as the master). Optional `zero_payload`
      overwrites the matching `__size__` bytes at `__offset__` in the
      data `.json` for sensitive-data wipes. Three read sites honour
      the bit and skip dead rows: `tranger2_open_iterator` history
      loop, `tranger2_iterator_get_page`, and
      `publish_new_rt_disk_records`. Treedb is downstream and
      inherits the skip with no `tr_treedb` change. Master-only.
      Second delete of the same row is a silent no-op. `rowid`s do
      NOT renumber; `iterator_size` / `total_rows` keep counting
      slots, not live rows. `tranger2_read_record_content` and
      `tranger2_read_user_flag` still serve dead rows when the caller
      addresses them directly (audit / wipe-verification tooling).
      Coverage: `tests/c/timeranger2/test_delete_instance.c` (5
      sub-cases).
    - **feat(tranger2): `tranger2_delete_key()` propagates to
      subscribers**. Pre-2026-05-26 the function `rmrdir`'d
      `keys/<key>/` and cleared the in-memory rollup cache, but
      never notified subscribers — `rt_mem` listeners kept stale
      references and `rt_by_disk` followers in other processes kept
      their cached view alive. Now: (1) `topic/disks/<rt_id>/<key>/`
      subdirectories are removed BEFORE the live `keys/<key>/`, so
      followers catch the deletion on the standard inotify channel
      (`FS_SUBDIR_DELETED_TYPE`, the v7 TODO branch that had been
      logging "NOT processed" since inception is now wired); (2)
      in-process subscribers receive a registered
      `tranger2_key_deleted_callback_t` via the new
      `tranger2_set_rt_key_deleted_callback()` setter. Additive
      typedef and setter — no breaking signature change to
      `open_rt_mem` / `open_rt_disk` / `open_iterator`. Coverage:
      `tests/c/timeranger2/test_delete_key_propagation.c` (5
      sub-cases). Wattyzer's "tombstone-then-delete" workaround in
      `db_history_wz` becomes redundant after one production cycle
      of coexistence — cleanup planned in the wattyzer repo, not
      here.
    - **fix(tr_treedb): repair `treedb_delete_instance`
      (pkey2-index cleanup only)**. The function was a dead
      copy-paste of `treedb_delete_node` with the real work
      fenced behind `if(0) { ... tranger2_delete_instance(...) }`
      and an `else` returning -1 with *"Cannot delete node"*. The
      dead branch was also wrong — it would have wiped the
      underlying `.md2` row while the primary index and the
      other `pkey2_*` indexes still referenced it (state
      corruption). The only in-tree caller
      (`c_node.c::ac_delete_node`) was getting -1 on every call
      without acting on the return, so the breakage was silent.
      Rewritten to do what the function name promises: drop the
      in-memory entry for THIS pkey2 via `delete_secondary_node`,
      fire `EV_TREEDB_NODE_DELETED`, preserve the JSON_INCREF /
      DECREF pattern. The whole-node wipe stays the job of
      `treedb_delete_node` → `tranger2_delete_key`. Contract
      spelled out in the `.c` and `.h` docstrings.
    - **fix(ytls/openssl): ship the full certificate chain**.
      `build_ssl_ctx()` was loading the server certificate via
      `SSL_CTX_use_certificate_file()`, which only parses the first
      cert of a PEM bundle.  With a Let's Encrypt fullchain.pem on
      disk, that meant the listener served only the leaf — browsers
      hid the issue via AIA-fetch / cached intermediates, but
      strict-TLS clients (e.g. Node's native `fetch`, used by the
      Playwright QA driver against the public URL) failed chain
      verification.  Switched to
      `SSL_CTX_use_certificate_chain_file()` (chain-aware, PEM-only
      — no `SSL_FILETYPE_PEM` arg).  The mbedTLS backend
      (`mbedtls_x509_crt_parse_file`) was always chain-aware so it
      was not affected.  Every yuno that exposes a TLS server with
      the OpenSSL backend needs a relink + redeploy to pick up the
      fix; for binaries shared by several live yunos (e.g.
      `auth_bff` 1802+1804) the atomic `mv old old.bak; cp new old`
      pattern avoids the `ETXTBSY` that breaks `update-binary`.
    - **chore(ytls, c_authz): drop OpenSSL legacy init/cleanup
      calls** (OpenSSL 4.0 prep). Removed four
      deprecated-since-1.1.0 calls that were no-ops on the 3.x
      series and disappear in 4.0: `SSL_library_init()` +
      `OpenSSL_add_all_algorithms()` (with the redundant
      `__initialized__` guard) in `ytls/openssl.c` init,
      `EVP_cleanup()` in cleanup, and
      `OpenSSL_add_all_digests()` (with its
      `CONFIG_HAVE_OPENSSL` wrapper) in `c_authz.c`. OpenSSL
      ≥ 1.1.0 auto-initialises on first use and cleans up via
      `atexit`. The `OPENSSL_API_COMPAT 30100` define already
      gates the rest of the 1.1.x compat surface; the yuneta
      source is now 4.0-clean (the jump itself stays deferred).
    - **fix(c_prot_tcp4h, c_prot_mqtt): guard state reset
      against in-publish disconnect cascade**. Under io_uring,
      publishing `EV_ON_MESSAGE` is synchronous and can trigger
      a full disconnect cascade upstream (authz NAK in
      `C_IEVENT_CLI` → `EV_DROP` → `C_TCP` ac_drop →
      `try_to_stop_yevents` → `set_disconnected` publishes
      `EV_DISCONNECTED` → the protocol gclass moves to
      `ST_DISCONNECTED`). The caller of `frame_completed`
      then unconditionally reset the FSM back to
      `ST_WAIT_FRAME_HEADER` / `ST_CONNECTED`, leaving the
      protocol "connected" without an underlying TCP. Symptom:
      *"Event NOT DEFINED in state: EV_CONNECTED in
      C_PROT_TCP4H@ST_WAIT_FRAME_HEADER"* alternating with
      broken-pipe / local-dropping cycles. Guard
      (`state != ST_DISCONNECTED`) added — same form already
      present in `c_websocket.c` and `c_prot_mqtt2.c`. The
      twin guard initially added to `c_prot_modbus_m` was
      reverted in a follow-up: the modbus-master flow does
      not expose the same cascade (see `GOBJ.md §8.13`).
      Verified live on `app.wattyzer.com` across the
      controlcenter dial-out loop.
    - **fix(gobj): `gobj_read_attrs` honours `mt_reading` via a new
      `item2json` helper**.  The bulk reader (behind `view-attrs`,
      introspection and `db_save_persistent_attrs`) was the only
      attribute path bypassing `mt_reading`, so `SDF_RSTATS` counters
      kept in `priv->X` read as zero through `view-attrs` even though
      `stats-yuno` (typed readers) saw the live value.  The helper
      dispatches by `DTP_*` and falls back to the stored value when
      `mt_reading` is absent or returns `!v.found`.  `gobj_read_attr`
      (single, borrowed-ref) is intentionally left alone: its
      "Return is NOT yours!" contract is incompatible with allocating
      a fresh `json_t` from a typed override.
    - **fix(gobj-js): mirror C — `gobj_read_attrs` honours
      `mt_reading`**.  Same shape as the C kernel patch, simplified
      (dynamic types, no `DTP_*` switch): `undefined` falls back to
      the stored value.  Defensive — no JS gclass implements
      `mt_reading` today, so this only closes the symmetry.
    - **fix(c_tcp): set `v.found = 1` for `cur_tx_queue` in
      `mt_reading`**.  Pre-existing one-liner; the branch updated
      `v.v.i` but forgot the discriminant, so the value was always
      shadowed by the stored zero.  Surfaces now that
      `gobj_read_attrs` consults `mt_reading`.
    - **fix(lib-yui): normalize `navigator.language` before
      `Intl.DateTimeFormat`**. Playwright Firefox without locale
      config (and some embedded webviews) report
      `navigator.language` as the literal string `"undefined"`;
      passing that to `Intl.DateTimeFormat` throws `RangeError`
      and breaks SPA bootstrap. Guard with a string +
      `"undefined"` check, fall back to `undefined` so Intl
      resolves to the system locale. Fold-back of wattyzer
      `1bf08aa`.
    - **feat(yuno_agent): `stats-yuno` defaults `service` to the
      matched `yuno_role`**. `cmd_stats_yuno` previously passed
      an empty `service` when the operator didn't spell it out,
      so the remote fell back to `priv->gobj_service` (the top
      `C_YUNO` instance) and returned only the few attrs declared
      on `c_yuno` — typically all zero, missing the real
      `SDF_RSTATS` counters that live on the citizen service
      (e.g. `C_AUTOMATIONS_WZ`'s `alarms_seen` /
      `tracks_seen` / `fires_seen` / `runs_done`). Convention is
      `gobj_create_default_service(yuno_role, GCLASS, ...)` so
      service name == yuno role; the operator now gets the real
      counters with the natural `stats-yuno yuno_role=X`
      invocation. Pass `service=__yuno__` to explicitly query the
      top `C_YUNO` attrs (previous default).
    - **fix(tr_treedb): include the required-field name in error
      logs**. The three "Field required" `gobj_log_error` sites in
      `check_desc_field` / `normalize_node_field_value` /
      `convert_node2tranger` logged the same opaque message; now
      the field name is interpolated into `msg` so the offending
      column is visible at a glance without expanding the
      structured payload.
    - **chore(yuno_agent): increase agent log file size**. Bumps
      the agent's own log rotation threshold in `yuno_agent` and
      `yuno_agent22` (two-line `main.c` change). Avoids
      tighter-than-needed rollovers under the trace volume the
      onboarding doc work surfaced.
    - **note**: validated by relinking every yuno (15 binaries) and
      running the full `ctest` suite (93/93 passed, 436 s).  A
      project-wide `yunetas build` after a kernel-side change does
      pick up the relink correctly — the `rm <yuno_bin> &&
      make install` workaround is only needed when rebuilding a
      single yuno's `build/` directory in isolation.

## v7.3.4 -- 16/May/2026
    - **chore(release): corrective republish of `@yuneta/lib-yui`**.
      `@yuneta/lib-yui@7.3.3` was published to npm from a branch that
      had the 7.3.3 release prep (gobj-js dependency pin, CHANGELOG,
      version) but **not** the G6 v5 canvas panning fix (PR #115):
      the published 7.3.3 tarball is missing
      `src/g6_drag_canvas_touch.js` and the `autoResize:false` /
      `ensure_drag_canvas_patch` changes, so installing lib-yui from
      npm still had the broken touch/desktop panning. npm versions
      are immutable, so 7.3.4 republishes `@yuneta/lib-yui` from
      `main` with the complete set (#115 + #116). No source changes
      versus what `main` already contained at 7.3.3 — this is a
      packaging correction only.
    - **chore(release): `@yuneta/gobj-js` 7.3.3 → 7.3.4 (lockstep)**.
      `@yuneta/gobj-js@7.3.3` on npm was already correct (it carries
      the `createElement2` nullish-`data-i18n` guard). It is bumped to
      7.3.4 with no functional change purely to keep the two JS
      packages in lockstep and avoid version-skew confusion; the
      `@yuneta/lib-yui` peer range moves to `^7.3.4`.
    - **note**: deprecate the bad artifact —
      `npm deprecate "@yuneta/lib-yui@7.3.3" "incomplete: missing the
      G6 panning fix; use >=7.3.4"`.

## v7.3.3 -- 16/May/2026
    - **fix(lib-yui): G6 v5 canvas panning (touch broken, desktop
      desynced)** (PR #115).  G6 v5.1.0 `drag-canvas` derived the pan
      delta from `event.movement`, which `@antv/g` fills from native
      `PointerEvent.movementX/Y`: left at 0 for touch pointers on most
      mobile browsers (canvas barely panned) and skewed by OS pointer
      acceleration / `devicePixelRatio` on desktop (graph lagged the
      cursor).  New `g6_drag_canvas_touch.js` subclasses `DragCanvas`,
      reuses its clamp/cursor logic, and pans by the `event.viewport`
      delta — reliable on mouse and touch, correct for any canvas
      scale/zoom.  Registered once over the built-in `'drag-canvas'`
      id so every graph (gobj-tree, json-graph, treedb editor) is
      fixed without per-consumer changes.  `c_yui_gobj_tree_js.js`
      and `c_yui_json_graph.js` also stop fighting G6 `autoResize`
      (window-only in v5) and size the canvas to its content box via
      a self-contained `ResizeObserver`.

    - **fix(gobj-js): `createElement2` no longer poisons `data-i18n`
      with `undefined`**.  A nullish `i18n` attribute (e.g. a field
      whose `header` is undefined) rendered the literal
      `data-i18n="undefined"` and suppressed translation, leaving
      form labels blank.  The attribute is now skipped when the value
      is `null`/`undefined`; an explicit empty string is still
      honoured.

    - **fix(lib-yui): pin `@yuneta/gobj-js` dependency**.  The
      peer dependency was `"*"`, and a stale lockfile had frozen it
      to the ancient `@yuneta/gobj-js@0.3.0` from npm — so lib-yui
      (and downstream apps) silently built against 0.3.0 and updates
      had no effect.  Peer range is now `^7.3.3` and a
      `file:../gobj-js` devDependency makes local builds use the
      in-tree source.  Downstream apps (wattyzer, estadodelaire)
      must likewise repin and reinstall so they stop
      resolving 0.3.0.

    - **feat(lib-yui): shell-mountable developer panel**.
      `build_dev_panel` plus a new `C_YUI_GOBJ_TREE_JS` hierarchical
      gobj-tree viewer; the dev panel is now a real window box (was a
      floating transparent overlay), with silent optional
      `__yui_main__` lookup and theme-aware styling.

    - **feat(lib-yui): TreeDB graph node redesign**.  Unified node
      design system: doc-style HTML node cards (theme-aware), soft
      topic palette, rectangular leaves, size tiers, ports back to
      the topic colour, no circles, click-detail popover (no hover
      tooltips), redesigned edges/ports, dark-theme contrast fix.

    - **feat(lib-yui): graph toolbar / context-menu**.  All
      toolbar/context-menu icons unified on one FA7 sprite;
      theme-aware G6 context menu (readable in dark); G6 popup
      transitions disabled (immediate, no glide); "reset zoom" home
      icon restored; bolder/longer "create node" plus.

    - **feat(lib-yui): shell / nav**.  View-owned dynamic 3rd-level
      runtime subroute; unknown route falls back to the default
      route; secondary-nav zone collapses from config; single-row
      toolbar on touch; `toolbar type:"connection"` + `context_action`
      + modal `on_close`; TreeDB topics persist the selected topic
      across reloads with self-contained tab navigation; TreeDB
      table row-count footer and email/tel/url subtypes; edit/delete
      modal mount fallback.

    - **fix(lib-yui): self-containment / responsiveness**.
      `C_G6_NODES_TREE` self-contained `ResizeObserver` and toolbar
      reconfig guarded until the graph is rendered;
      `C_YUI_TREEDB_GRAPH` emits `EV_OPERATION_MODE_CHANGED`; g6
      views detect theme from `<html data-theme>`; `C_YUI_UPLOT`
      responsive width.

    - **feat(treedb): system schema v5 → v6**.  Restore the
      `cols.topics` fkey, refresh `cols` topic, show all system
      topics; keep the original ArtGins start year as a range
      (2024-2026).

    - **feat(ycommand): `--editor`/`-e` and stdout dump**.  Dump a
      file to stdout when stdout is not a TTY (was: always vim);
      `ac_read_file` signals exit on success, not just on error;
      `pty_sync_spawn` drains the master pty after child exit
      (was: truncated `cat`).

    - **feat(c_mqiogate): `broadcast` method** to fan out events to
      every child (tidy `lastdigits` to mirror it).

    - **chore(packages / ci)**.  Ship sanitized agent JSON templates
      from the repo (drop the `/yuneta/agent/` dependency); move
      `RELEASE` to the repo root; `release-deb` generates a default
      `.config` via `alldefconfig` and drops the pgdg apt source.

    - **chore(ext-libs): TODO bump nginx 1.28.3 → 1.30.1**
      (CVE-2026-42945) for the next ext-libs refresh.

    - **misc(C)**: remove `sf_deleted_record` flag; fix stale
      `md2_record_t` "Size: 96 bytes" comment; log the key with bad
      metadata; revert the `C_TCP_S channel_filter` two-TLS-listener
      change.

    - **feat(tr2list): `--dry-run` and `--follow` modes**.
      `--dry-run` / `-n` prints the resolved search parameters and
      the `match_cond` JSON (times already resolved by `approxidate`),
      plus a human-readable rendering of any `from-t`/`to-t`/`from-tm`/
      `to-tm` set — respects `--print-local-time` and flags millisecond
      input.  `--follow` / `-F` opens an `rt_disk` list and runs
      `yev_loop_run` until SIGINT (tail-f style; single topic, so it
      errors out when combined with `--recursive`).  `--help` now
      documents the full `approxidate` grammar accepted by TIME
      options (units, specials, absolute forms) via argp's `\v`
      separator.
    - **feat(treedb_list): `--dry-run` and `--follow` modes**.
      `--dry-run` / `-n` runs `resolve_treedb_path` and prints the
      deduced path / database / topic alongside the filter and
      options JSON (also flags resolution failure and falls back to
      the raw user input).  `--follow` / `-F` keeps listening for
      node CREATED / UPDATED / DELETED / LINKED / UNLINKED events
      after the initial listing — uses the existing rt_disk path
      that treedb already opens internally when `master=false`, plus
      a `treedb_set_callback` that honours `--topic` and `--ids`.
      Errors out on `--follow --recursive` and on `--follow` with
      `--print-tranger` / `--print-treedb`.
    - **fix(helpers/approxidate): accept short unit suffixes**.
      `1s`, `1m`, `1h`, `1d`, `1w`, `1M`, `1y` (plus `1sec`, `1mi`,
      `1min`, `1mo`, `1hr`, `1wk`, `1yr`) now resolve to the
      expected relative duration instead of silently falling through
      to the numeric date parser as day-of-month / year.  Lowercase
      `m` keeps minute, uppercase `M` is month (case-sensitive to
      disambiguate, mirroring `sleep` / `find -mmin` conventions).
      `mon` is intentionally left as Monday so weekday parsing
      keeps its existing behaviour.  Benefits every yuneta tool
      that consumes `approxidate` (tr2list, treedb_list, ybatch,
      tr2search, tr2keys, tr2migrate, ...).
    - **refactor(tr2list): simpler `--dry-run` time block**.
      Each time line now ends with `(<show_date_relative>)` —
      `2 hours ago`, `3 days ago`, etc. — instead of echoing the
      raw input and warning about parser footguns.  The warning
      block in `--help` is dropped and replaced by a `short`
      group listing the new 1-3 char unit forms accepted by
      `approxidate`.

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
