# System GClasses

Core runtime, file-system watcher, pseudo-terminal, OTA updates, and
miscellaneous system services.

**Source:** `kernel/c/root-linux/src/c_yuno.c`, `c_fs.c`, `c_pty.c`,
`c_ota.c`, `c_gss_udp_s.c`

---

(gclass-c-yuno)=
## C_YUNO

Main yuno GClass — the root grandmother of every yuno application.
Manages services, configuration, logging, tracing, IP filtering, and
system-wide commands.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_IDLE` |

### Commands (selection)

| Command | Description |
|---------|-------------|
| `view-gclass-register` | List all registered GClasses. |
| `view-services` / `view-top-services` | List services. |
| `view-gobj` / `view-gobj-tree` | Inspect the gobj tree. |
| `enable-gobj` / `disable-gobj` | Enable or disable a gobj. |
| `write-attr` / `view-attrs` / `attrs-schema` | Read/write gobj attributes. |
| `set-global-trace` / `set-gclass-trace` / `set-gobj-trace` | Configure trace levels. |
| `reset-all-traces` / `set-deep-trace` | Trace management. |
| `info-global-trace` | Show trace configuration. |
| `view-config` | Show yuno configuration. |
| `info-mem` | Memory usage info. |
| `info-cpus` / `info-ifs` / `info-os` | System information. |
| `list-allowed-ips` / `add-allowed-ip` | IP access control. |
| `truncate-log-file` | Truncate the log file. |
| `view-log-counters` | Show log event counters. |
| `add-log-handler` / `del-log-handler` / `list-log-handlers` | Log handler management. |

See also the [Yuno API](../runtime/yuno.md) for C helper functions.

---

(gclass-c-fs)=
## C_FS

File-system watcher — monitors directory changes using the
[fs_watcher](../timeranger2/fs_watcher.md) engine.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_IDLE` |
| **Output events** | `EV_ON_MESSAGE` (directory change notifications) |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `path` | `string` | Directory to watch. |
| `recursive` | `bool` | Watch subdirectories recursively. |
| `info` | `bool` | Report found subdirectories on startup. |

---

(gclass-c-pty)=
## C_PTY

Pseudo-terminal — spawns a shell or process in a PTY and provides
bidirectional I/O.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_DISCONNECTED`, `ST_CONNECTED` |
| **Input events** | `EV_TX_DATA`, `EV_DROP` |
| **Output events** | `EV_RX_DATA`, `EV_CONNECTED`, `EV_DISCONNECTED` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `process` | `string` | Command to execute (default: user's shell). |
| `rows` | `integer` | Terminal rows. |
| `cols` | `integer` | Terminal columns. |
| `cwd` | `string` | Working directory. |
| `max_tx_queue` | `integer` | Maximum transmit queue size. |

---

(gclass-c-ota)=
## C_OTA

Over-the-air update manager — downloads and applies firmware updates.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_IDLE`, `ST_WAIT_RESPONSE` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `url_ota` | `string` | OTA server URL. |
| `cert_pem` | `string` | TLS certificate (PEM). |
| `ota_state` | `string` | Current OTA state (read-only). |
| `force` | `bool` | Force update even if role differs. |
| `timeout_validate` | `integer` | Validation timeout in seconds. |

### Commands

| Command | Description |
|---------|-------------|
| `download-firmware` | Start firmware download. |

---

(gclass-c-gss-udp-s)=
## C_GSS_UDP_S

Gossamer UDP server — manages multiple virtual channels over a single
UDP socket.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_IDLE` |
| **Input events** | `EV_RX_DATA`, `EV_TX_DATA`, `EV_TIMEOUT`, `EV_STOPPED` |
| **Output events** | `EV_ON_MESSAGE`, `EV_ON_OPEN`, `EV_ON_CLOSE` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `url` | `string` | UDP listening URL. |
| `timeout_base` | `integer` | Base timeout in seconds. |
| `seconds_inactivity` | `integer` | Channel inactivity timeout. |
| `disable_end_of_frame` | `bool` | Disable end-of-frame detection. |
