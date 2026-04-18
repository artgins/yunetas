(cert_management)=
# **TLS certificate management**

End-to-end guide to keeping TLS certificates fresh on a Yuneta host
**without dropping live connections** and without relying on a single
point of failure.

## The problem

A Yuneta agent can hold thousands of persistent TLS connections (MQTT
brokers, intake gates, control-plane channels). Whenever the server
certificate is renewed — typically by Let's Encrypt — we want:

1. **Zero dropped connections** — existing TLS sessions keep working.
2. **New connections use the new cert** — from the moment the swap
   happens.
3. **No reliance on a single hook** — if the certbot deploy hook fails
   silently, the system must still pick up the new certs.
4. **Proactive alerting** — if a cert is close to expiry and nobody
   has renewed it yet, we want logs that say so, not a handshake
   failure days later.

## The three-layer defense in depth

| Layer | Trigger | Action | Latency |
|---|---|---|---|
| 1. Deploy hook (fast path) | certbot success | Hook copies certs, invokes [`reload-certs`](#reload-certs-yuno) on every yuno | Immediate |
| 2. Agent auto-sync (self-healing) | Timer inside `c_agent` (default 15 min) | Detects cert file changes under `/yuneta/store/certs/` and broadcasts `reload-certs` | ≤15 min |
| 3. Expiry monitor (alerting) | Timer inside `c_yuno` (default 1 h) | Reads `not_after` of every TLS listener; logs warning / critical | Independent of renewal path |

No single layer is required for the others. Layer 2 covers layer 1
failing. Layer 3 covers **both** failing.

## Layer 1 — deploy hook (fast path)

The `.deb` package (`packages/make-yuneta-agent-deb.sh`) installs a
certbot `deploy-hook` at
`/etc/letsencrypt/renewal-hooks/deploy/reload-certs`:

1. Run `/yuneta/store/certs/copy-certs.sh` as root. This reads
   `/etc/letsencrypt/live/*/` (normally `root:root 0700`) and writes
   mirrored copies into `/yuneta/store/certs/` owned by
   `yuneta:yuneta`. The script uses `install -C` so mtime only bumps
   when the content actually changes.
2. Reload the web server (`nginx -s reload` or `openresty -s reload`).
3. Broadcast the yuno-level `reload-certs` command to every running
   yuno:
   ```bash
   sudo -u yuneta -H /yuneta/bin/ycommand \
       -c 'command-yuno command=reload-certs service=__yuno__'
   ```
4. Touch `/var/lib/yuneta/last-deploy-hook-run` with a Unix timestamp
   so the monitor in layer 3 can detect a hook that never runs.

Each step runs independently (`set +e`) so one broken yuno cannot
cascade-abort the hook. All output goes to `/var/log/yuneta/deploy-hook.log`.

(reload-certs-yuno)=
### The `reload-certs` command chain

`command-yuno command=reload-certs service=__yuno__` is routed to
`c_yuno`'s `reload-certs` handler, which walks the gobj tree and
invokes each `C_TCP_S` / `C_UDP_S` listener's own `reload-certs`
command. Each listener:

1. Reads its current `crypto` attribute (cert paths).
2. Calls [`ytls_reload_certificates()`](../api/ytls/ytls.md#ytls_reload_certificates)
   on its `ytls` handle.
3. The ytls layer builds a fresh `SSL_CTX` (or mbed-TLS state),
   validates it, and atomically swaps it in. Live sessions keep the
   old context alive via refcount; new sessions use the fresh one.

## Layer 2 — agent auto-sync (self-healing)

The yuno_agent (`c_agent`) runs a second `C_TIMER` child dedicated to
cert sync. Every `cert_sync_interval_sec` seconds (default **900**, min
30):

1. Invoke `cert_sync_copy_cmd` (default
   `sudo -n /yuneta/store/certs/copy-certs.sh`). The `yuneta` user
   has `NOPASSWD:ALL` in `/etc/sudoers.d/90-yuneta` so the script can
   read `/etc/letsencrypt/live/`.
2. Snapshot `size+mtime` of every `*.crt` under `cert_sync_store_dir`
   before and after the copy.
3. If any file changed, invoke `cmd_command_yuno` internally to
   broadcast `reload-certs` to every running yuno — plus `gobj_command(gobj_yuno(), "reload-certs", ...)`
   on the agent itself.
4. Update the read-only stats attrs:
   `cert_sync_last_check`, `cert_sync_last_action`,
   `cert_sync_last_result`, `cert_sync_failures`.

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

### Configurable attrs (persistent on the agent)

| Attr | Default | Purpose |
|---|---|---|
| `cert_sync_enabled` | `1` | Master on/off for the timer |
| `cert_sync_interval_sec` | `900` | Seconds between checks (floor 30) |
| `cert_sync_store_dir` | `/yuneta/store/certs` | Watched directory |
| `cert_sync_copy_cmd` | `sudo -n /yuneta/store/certs/copy-certs.sh` | Privileged copy command |

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
   - `days_remaining <= cert_critical_days` → `gobj_log_critical()`.
   - `days_remaining <= cert_warn_days`     → `gobj_log_warning()`.

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
| `/yuneta/store/certs/private/` | `yuneta:yuneta` | `0700` | postinst |
| `/var/lib/yuneta/last-deploy-hook-run` | `root:root` | `0644` | deploy hook |
| `/var/log/yuneta/deploy-hook.log` | `root:root` | `0644` | deploy hook |

The yuno never reads `/etc/letsencrypt/`. The privileged bridge is the
`copy-certs.sh` script, invoked via `sudo` by either the deploy hook
(as root) or the agent timer (as `yuneta`, since `yuneta` has
`NOPASSWD:ALL`).

## How live sessions survive the swap

This is the core correctness property — documented here because it is
the easiest thing to break when touching the reload path.

**OpenSSL backend.** `SSL_new(ctx)` increments the `SSL_CTX` refcount
automatically; `SSL_free(ssl)` decrements it. When
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
creation and releases it on `ytls_free_secure_filter()`; the swap drops
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

- `tests/c/ytls/test_cert_reload.c` — swap A→B + rollback on invalid cert.
- `tests/c/ytls/test_cert_info.c` — cert introspection edge cases
  (short / long validity, self-signed invariant, serial shape,
  client-side NULL, already-expired cert).
- `tests/c/ytls/test_cert_reload_mem.c` — 1000 reloads with no live
  session, asserts `get_cur_system_memory() == 0`.
- `tests/c/yev_loop/yev_events_tls/test_yevent_reload_live.c` — one
  reload while a TCP session is live; the session keeps working.
- `tests/c/yev_loop/yev_events_tls/test_yevent_reload_stress.c` — 50
  reloads with a live session, one echo message per iteration.

Run the full suite under valgrind for an exhaustive leak check:

```bash
valgrind --leak-check=full \
    outputs/tests/c/ytls/test_cert_reload_mem
```
