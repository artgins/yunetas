(cert_management)=
# **TLS certificate management**

End-to-end guide to keeping TLS certificates fresh on a Yuneta host
**without dropping live connections** and without relying on a single
point of failure.

## The problem

A Yuneta agent can hold thousands of persistent TLS connections (MQTT
brokers, intake gates, control-plane channels). Whenever the server
certificate is renewed — typically by [Let's Encrypt](https://letsencrypt.org/) — we want:

1. **Zero dropped connections** — existing TLS sessions keep working.
2. **New connections use the new cert** — from the moment the swap
   happens.
3. **No reliance on a single hook** — if the certbot deploy hook fails
   silently, the system must still pick up the new certs.
4. **Proactive alerting** — if a cert is close to expiry and nobody
   has renewed it yet, we want logs that say so, not a handshake
   failure days later.

## The three-layer defense in depth

```{figure} ../_static/cert_defense.svg
:alt: Layer 1 deploy hook (immediate) and Layer 2 agent auto-sync (≤15 min) both trigger reload-certs → ytls_reload_certificates(); Layer 3 expiry monitor independently logs warning/critical. Each layer covers the one above failing.
:width: 100%

Two layers drive the reload; a third just makes a missed renewal loud. No
single layer is required for the others.
```

| Layer | Trigger | Action | Latency |
|---|---|---|---|
| 1. Deploy hook (fast path) | certbot success | Hook copies certs, invokes [`reload-certs`](#reload-certs-yuno) on every yuno | Immediate |
| 2. Agent auto-sync (self-healing) | Timer inside `c_agent` (default 15 min) | Detects cert file changes under `/yuneta/store/certs/` and broadcasts `reload-certs` | ≤15 min |
| 3. Expiry monitor (alerting) | Timer inside `c_yuno` (default 1 h) | Reads `not_after` of every TLS listener. Logs warning / critical | Independent of renewal path |

No single layer is required for the others. Layer 2 covers layer 1
failing. Layer 3 covers **both** failing.

## Layer 1 — deploy hook (fast path)

The `.deb` package ([`packages/deb/make-yuneta-agent-deb.sh`](https://github.com/artgins/yunetas/blob/7.32.5/packages/deb/make-yuneta-agent-deb.sh)) installs a
certbot `deploy-hook` at
`/etc/letsencrypt/renewal-hooks/deploy/reload-certs`:

1. Run `/yuneta/store/certs/copy-certs.sh --wait` as root. The package ships this
   script (`packages/templates/copy-certs.sh`). Root only reads the
   certificates in `/etc/letsencrypt/live/<name>/` (normally
   `root:root 0700`). The user `yuneta` (`setpriv --reset-env`, in a
   session of its own) compares each one with its copy in
   `/yuneta/store/certs/`, and writes only a file that changed. The script
   does these checks:
    - It takes the names from `/yuneta/store/certs/certs.list`, or else
      from the directories of `/etc/letsencrypt/live/`. The blanks around
      a line are trimmed, and a line can hold several names. A name has
      only `[A-Za-z0-9._-]`, does not start with a dot, and has no `..`.
      The script skips any other name with a line, and copies the others.
    - Each line that the script says goes to its output and to the
      journal with the tag `copy-certs`. From the hook, its output goes to
      `/var/log/yuneta/copy-certs.log`. From the agent's sync, its output
      goes nowhere (the agent runs with its output on `/dev/null`), so read
      the journal:
      ```bash
      journalctl -t copy-certs --since today
      ```
      If `logger` fails (for example, the node has no journal), the script
      appends the line, with its time, to `/var/log/yuneta/copy-certs.log`.
      It does not do this when its output already goes to that file (the
      hook), so a line is never there twice.
    - If `yuneta` cannot read `certs.list`, the script copies nothing and
      exits with 1. The hook then logs `copy-certs.sh FAILED`, and the
      agent's sync says `copy-script-failed`.
    - Each `.pem` must resolve to a file under `/etc/letsencrypt/`.
      Otherwise the script skips that certificate.
    - It refuses a `/yuneta/store/certs/` or `private/` that is a
      symbolic link (exit 1, nothing copied), and it skips a certificate
      whose destination file is a symbolic link.
    - It writes the changed files of a certificate to temporary files in
      their directories. Only when all of them are written does it move
      them into place, the key first and the `.crt` last. Before each move
      it keeps the old file as a hard link, and when a later move fails it
      puts the moved files back. If it cannot link the old file, it keeps
      a copy (`cp -p`) and says so. This occurs with
      `fs.protected_hardlinks=1` and a file that root left there: `yuneta`
      can link only its own files. If it can neither link nor read the old
      file, the copy of that certificate fails. Give the file to `yuneta`
      (`chown yuneta:yuneta <file>`). So a failure leaves the old set whole,
      never an old `.crt` with a new key, and the `.crt` (which the agent's
      sync watches) changes last. If the old files cannot be put back
      either, the script says that the set can be mixed, and it keeps
      the old file as `<file>.old.XXXXXX`. No key is
      written anywhere else. An unchanged file is not written, so it keeps
      its mtime. The agent's sync (layer 2) reloads the yunos only when the
      size or the mtime of a file changes.
    - It says `Copied <name>` and runs `/yuneta/bin/check-certs-validity.sh`
      (as `yuneta`) only when something changed.
    - It runs one at a time: the hook and the agent's sync take the lock
      `/run/yuneta-copy-certs/lock` (`flock`). Root makes its directory
      with mode `0700`, so no other user can hold the lock. The agent
      waits for its sync to end, so the run of the sync never waits for
      the lock. If another run holds the lock, the run of the sync copies
      nothing, says `Another copy-certs.sh holds ...` and exits with 75.
      The agent then says `busy`, not a failure, and its next sync does
      the copy. At the third busy sync since a copy last ran past the
      lock, the agent logs one WARNING. A copy that fails does not end
      that count. When a copy exits with 0 again, the agent logs one INFO.
      The hook runs the script with `--wait`: it waits up to 120 seconds
      for the other run, and then it copies. After 120 seconds it fails
      with exit 1. To run it by hand:
      ```bash
      sudo /yuneta/store/certs/copy-certs.sh --wait
      ```
    - On a node with no certificate, it says `No certificates to copy` to
      the journal once, not at each tick of the sync. A mark
      `/yuneta/store/certs/.no-certificates` remembers that it said it, and
      goes when a certificate comes.
2. Reload the web server (`nginx -s reload` or `openresty -s reload`), as
   root.
3. Broadcast the yuno-level `reload-certs` command to every running
   yuno, as `yuneta`:
   ```bash
   setsid -w setpriv --reuid=yuneta --regid=yuneta --init-groups --reset-env -- \
       /yuneta/bin/ycommand -c 'command-yuno command=reload-certs service=__yuno__'
   ```
   If `ycommand` fails, the hook starts `/yuneta/bin/restart-yuneta` as
   `yuneta` in a transient unit of its own. certbot runs the hook in a unit
   that stops when certbot ends, and a job left in the background there
   dies with it. The unit is `Type=exec`, so `systemd-run` returns when the
   script started (not when it ended), and it fails when the script cannot
   start. The hook then logs `restart-yuneta did NOT start`:
   ```bash
   systemd-run -p Type=exec --unit=yuneta-restart-<UTC time> --uid=yuneta --gid=yuneta \
       /yuneta/bin/restart-yuneta
   journalctl -u 'yuneta-restart-*'     # what it did
   ```
   On Red Hat with SELinux, PID 1 can start only a file labelled `bin_t`,
   so the `.rpm` labels `/yuneta/bin/restart-yuneta` `bin_t`. To check it:
   `ls -Z /yuneta/bin/restart-yuneta`. A node without systemd starts it in
   the background. On a node without the units of the agents,
   `restart-yuneta` starts the agent with `--start`. Inside a unit it
   refuses to do that, because such an agent dies with the unit.
4. Touch `/var/lib/yuneta/last-deploy-hook-run` with a Unix timestamp
   so the monitor in layer 3 can detect a hook that never runs.

Each step runs independently (`set +e`) so one broken yuno cannot
cascade-abort the hook. All output goes to `/var/log/yuneta/deploy-hook.log`.

(reload-certs-yuno)=
### The `reload-certs` command chain

`command-yuno command=reload-certs service=__yuno__` is routed to
`c_yuno`'s `reload-certs` handler, which walks the gobj tree and
invokes each [`C_TCP_S`](#gclass-c-tcp-s) / [`C_UDP_S`](#gclass-c-udp-s) listener's own `reload-certs`
command. Each listener:

1. Reads its current `crypto` attribute (cert paths).
2. Calls [`ytls_reload_certificates()`](../api/ytls/ytls.md#ytls_reload_certificates)
   on its `ytls` handle.
3. The ytls layer builds a fresh `SSL_CTX` (or mbed-TLS state),
   validates it, and atomically swaps it in. Live sessions keep the
   old context alive via refcount. New sessions use the fresh one.

## Layer 2 — agent auto-sync (self-healing)

The yuno_agent (`c_agent`) runs a second [`C_TIMER`](#gclass-c-timer) child dedicated to
cert sync. Every `cert_sync_interval_sec` seconds (default **900**, min
30):

1. Invoke `cert_sync_copy_cmd` (default
   `/usr/bin/sudo -n /yuneta/store/certs/copy-certs.sh`). The `yuneta` user
   has `NOPASSWD:ALL` in `/etc/sudoers.d/90-yuneta` so the script can
   read `/etc/letsencrypt/live/`.
2. Snapshot `size+mtime` of every `*.crt` under `cert_sync_store_dir`
   before and after the copy.
3. If any file changed, invoke [`cmd_command_yuno`](https://github.com/artgins/yunetas/blob/7.32.5/yunos/c/yuno_agent/src/c_agent.c#L7456) internally to
   broadcast `reload-certs` to every running yuno — plus `gobj_command(gobj_yuno(), "reload-certs", ...)`
   on the agent itself.
4. Update the read-only stats attrs:
   `cert_sync_last_check`, `cert_sync_last_action`,
   `cert_sync_last_result`, `cert_sync_failures`. `cert_sync_last_result`
   is `reloaded`, `no-change`, `busy` (another run of `copy-certs.sh`
   holds its lock, see layer 1) or `copy-script-failed`.
   `cert-sync-status` also gives `busy_ticks`, the syncs in a row that
   found the lock taken.

### Agent commands

```bash
# Force a sync immediately, useful after a manual cert install.
ycommand -c 'cert-sync-now'

# View the current cert-sync state (including when the deploy hook
# last ran, if ever).
ycommand -c 'cert-sync-status'
```

`cert-sync-status` reads `/var/lib/yuneta/last-deploy-hook-run` so a
stale or missing timestamp is a direct hint that the deploy hook is
not firing.

### Configurable attrs

| Attr | Default | Set by | Purpose |
|---|---|---|---|
| `cert_sync_enabled` | `1` | config, or `write-attr` (persistent, at once) | Master on/off for the timer |
| `cert_sync_interval_sec` | `900` | config, or `write-attr` (persistent, at once) | Seconds between checks (floor 30) |
| `cert_sync_store_dir` | `/yuneta/store/certs` | config only | Watched directory |
| `cert_sync_copy_cmd` | `sudo -n /yuneta/store/certs/copy-certs.sh` | config only | Privileged copy command, run with `sh -c`, with no signal blocked |

A `write-attr` of `cert_sync_enabled` or `cert_sync_interval_sec` takes
effect at once: `0` stops the timer, and `1` or a new interval arms it
again. Up to 7.31.29 it took effect only after a restart of the agent:

```bash
ycommand -S __yuno__ -c 'write-attr gobj=agent attribute=cert_sync_enabled value=0'
```

The command and its directory are set in the agent's configuration only, never
at run time. The exit code 75 of the command is reserved: the agent reads
it as `busy`, not as a failure. Up to 7.25.21, `write-attr` was able to set the command the agent runs
with `system()` (as root, through `sudo`), and persist it. A value persisted
then is no longer read. Set it in `/yuneta/agent/yuneta_agent.json`:

```json
{
    "global": {
        "agent.cert_sync_copy_cmd": "/usr/bin/sudo -n /yuneta/store/certs/copy-certs.sh",
        "agent.cert_sync_store_dir": "/yuneta/store/certs"
    }
}
```

## Layer 3 — expiry monitor (alerting)

The yuno itself (`c_yuno`) piggy-backs on its existing
`ac_timeout_periodic` action. Every `timeout_cert_check` seconds
(default **3600**, 0 disables):

1. Walk every descendant gobj of gclass `C_TCP_S` / `C_UDP_S` with
   `use_ssl=TRUE`.
2. Invoke each listener's `view-cert` command, which returns
   [`ytls_get_cert_info()`](../api/ytls/ytls.md#ytls_get_cert_info)
   augmented with `days_remaining`.
3. For each listener:
   - `days_remaining <= cert_critical_days` → [`gobj_log_critical()`](#gobj_log_critical).
   - `days_remaining <= cert_warn_days`     → [`gobj_log_warning()`](#gobj_log_warning).

The monitor only alerts — it never triggers a reload. The auto-sync
layer owns that responsibility, cleanly separating "something needs
action" (alert) from "here is the action" (reload).

### Configurable attrs (persistent on the yuno)

| Attr | Default | Purpose |
|---|---|---|
| `timeout_cert_check` | `3600` | Seconds between expiry scans (0 disables) |
| `cert_warn_days` | `7` | Warning threshold |
| `cert_critical_days` | `2` | Critical threshold |

### Yuno commands

```bash
# Walk all TLS listeners and report {subject, not_after, days_remaining, ...}
# plus an aggregate min_days_remaining.
ycommand -c 'command-yuno command=cert-expiry-status service=__yuno__'

# Trigger reload-certs on every TLS listener of this yuno (same
# handler the deploy hook uses).
ycommand -c 'command-yuno command=reload-certs service=__yuno__'
```

### Per-listener commands

Each `C_TCP_S` / `C_UDP_S` gobj also exposes `reload-certs` and
`view-cert` directly, useful for targeting a single listener:

```bash
ycommand -c 'command-yuno command=reload-certs gobj=my_tcp_server'
ycommand -c 'command-yuno command=view-cert    gobj=my_tcp_server'
```

## File layout and permissions

| Path | Owner | Mode | Written by |
|---|---|---|---|
| `/etc/letsencrypt/live/<domain>/` | `root:root` | `0700` | certbot |
| `/yuneta/store/certs/<domain>.crt` | `yuneta:yuneta` | `0644` | `copy-certs.sh` |
| `/yuneta/store/certs/<domain>.chain` | `yuneta:yuneta` | `0644` | `copy-certs.sh` |
| `/yuneta/store/certs/private/<domain>.key` | `yuneta:yuneta` | `0600` | `copy-certs.sh` |
| `/yuneta/store/certs/private/` | `yuneta:yuneta` | `0700` | postinst, or `copy-certs.sh` (as `yuneta`) |
| `/var/lib/yuneta/last-deploy-hook-run` | `root:root` | `0644` | deploy hook |
| `/var/log/yuneta/deploy-hook.log` | `root:root` | `0644` | deploy hook |

The yuno never reads `/etc/letsencrypt/`. The privileged bridge is the
`copy-certs.sh` script, run as root by the deploy hook, and with `sudo` by
the agent timer (`yuneta` has `NOPASSWD:ALL`). The script is in
`/yuneta`, which `yuneta` owns. So `yuneta` can change a script that root
runs, and removing `sudo` from `yuneta` does not make it a privilege
boundary. [Files outside `/yuneta`](#files-outside-yuneta-root-run) lists
these files.

To copy only some certificates, list their names in `certs.list`:

```text
# /yuneta/store/certs/certs.list
example.com
www.example.com
```

## How live sessions survive the swap

```{figure} ../_static/cert_hotswap.svg
:alt: The ytls handle is repointed from old_ctx to new_ctx and frees its own reference to old_ctx; live SSL sessions still hold their own refs, so old_ctx survives until the last in-flight session closes, while new sessions attach to new_ctx.
:width: 100%

The ytls handle drops its ref on `old_ctx`, but each live session keeps its
own — so `old_ctx` outlives the swap until the last handshake on it closes.
```

This is the core correctness property — documented here because it is
the easiest thing to break when touching the reload path.

**OpenSSL backend.** `SSL_new(ctx)` increments the `SSL_CTX` refcount
automatically. `SSL_free(ssl)` decrements it. When
[`ytls_reload_certificates()`](../api/ytls/ytls.md#ytls_reload_certificates)
runs:

1. Build `new_ctx` with fresh cert files.
2. Swap `ytls->ctx = new_ctx`.
3. `SSL_CTX_free(old_ctx)` — drops the ytls handle's ref.

Live `SSL *` objects retain their own ref on `old_ctx`, so the old
context lives until the last in-flight session closes. A new call to
[`ytls_new_secure_filter()`](../api/ytls/ytls.md#ytls_new_secure_filter)
uses `new_ctx`.

**mbed-TLS backend.** The ytls layer maintains an explicit
`mbedtls_state_t` bundle (`mbedtls_ssl_config` + `mbedtls_x509_crt` +
`mbedtls_pk_context`) with a refcount. Each `hsskt` takes a ref on
creation and releases it on [`ytls_free_secure_filter()`](#ytls_free_secure_filter). The swap drops
the ytls handle's ref. Same end result: live sessions stay valid.

## Troubleshooting

**Hook seems to run but certs never update.** `ycommand -c 'cert-sync-status'`
returns `deploy_hook_last_run: 0` or a stale timestamp. Check
`/var/log/yuneta/deploy-hook.log` — each step logs its own success /
failure line. The agent auto-sync will still pick up the new certs
within `cert_sync_interval_sec`, so this is an alert condition rather
than a service outage.

**Reload returns `-1` and nothing changes.** The new cert material
failed validation — wrong key, unparseable PEM, missing file. Check
the yuno log for the `SSL_CTX_use_certificate_file() FAILED`,
`SSL_CTX_check_private_key() FAILED` or `mbedtls_x509_crt_parse_file() FAILED`
entries. The **previous** context is kept intact, so traffic is
unaffected — you can fix the source files and re-run
`ycommand -c 'cert-sync-now'`.

**`days_remaining: -5` on a listener.** The cert expired 5 days ago.
Either the renewal path is completely broken or the listener is
reading a file that certbot does not manage. Compare
`gobj_read_attr(listener, "crypto")` against the actual renewal
source.

## Tests

Five tests ship in-tree to protect the feature:

- [`tests/c/ytls/test_cert_reload.c`](https://github.com/artgins/yunetas/blob/7.32.5/tests/c/ytls/test_cert_reload.c) — swap A→B + rollback on invalid cert.
- [`tests/c/ytls/test_cert_info.c`](https://github.com/artgins/yunetas/blob/7.32.5/tests/c/ytls/test_cert_info.c) — cert introspection edge cases
  (short / long validity, self-signed invariant, serial shape,
  client-side NULL, already-expired cert).
- [`tests/c/ytls/test_cert_reload_mem.c`](https://github.com/artgins/yunetas/blob/7.32.5/tests/c/ytls/test_cert_reload_mem.c) — 1000 reloads with no live
  session, asserts `get_cur_system_memory() == 0`.
- [`tests/c/yev_loop/yev_events_tls/test_yevent_reload_live.c`](https://github.com/artgins/yunetas/blob/7.32.5/tests/c/yev_loop/yev_events_tls/test_yevent_reload_live.c) — one
  reload while a TCP session is live. The session keeps working.
- [`tests/c/yev_loop/yev_events_tls/test_yevent_reload_stress.c`](https://github.com/artgins/yunetas/blob/7.32.5/tests/c/yev_loop/yev_events_tls/test_yevent_reload_stress.c) — 50
  reloads with a live session, one echo message per iteration.

Run the full suite under valgrind for an exhaustive leak check:

```bash
valgrind --leak-check=full \
    outputs/tests/c/ytls/test_cert_reload_mem
```
