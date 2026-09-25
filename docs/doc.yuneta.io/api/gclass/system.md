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
| `info-inotify` | inotify limits (`/proc/sys/fs/inotify/*`) plus this yuno's own usage (instances + watches). |
| `list-allowed-ips` / `add-allowed-ip` | IP access control. |
| `truncate-log-file` | Truncate the log file. |
| `view-log-counters` | Show log event counters. |
| `add-log-handler` / `del-log-handler` / `list-log-handlers` | Log handler management. |
| `shutdown` | Stop this yuno, orderly. |

:::{note}
`shutdown` answers first and dies after: the response travels over the very
event loop the shutdown stops, so the handler only arms a timer and the
periodic action does the dying (up to one `timeout_periodic`, 1 s by default).

It exits with code **0**, so the ydaemon watcher does **not** relaunch the
yuno — asking for a shutdown and getting a restart would be a surprise. To
start it again, use the agent (`run-yuno`).

Like every `SDF_AUTHZ_X` command, its per-command authz check only runs where
`enable_command_authz` is on, and that is **off by default** (see
[`YUNO_AUTH.md`](../../../../yunos/c/yuno_agent/YUNO_AUTH.md) §4.5). That is
not a new exposure — whoever can send commands to a yuno can already
`disable-gobj` its whole tree — but do not read the flag as a lock that is
already closed.

This is not a replacement for [`kill-yuno`](../../deploying-yunos.md), which
is the deploy path: `kill-yuno` is asked of the **agent**, which knows the
yuno and deregisters it. `shutdown` is asked of the **yuno itself**, which is
what you have when the yuno answers and the agent does not, or when the yuno
runs under no agent at all.
:::

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

`process` is remote-settable (the agent's authz-gated `open-console` command),
so since 7.6.0 it is resolved against a **fixed trusted-dir allowlist**, never
the inherited `$PATH`: an absolute path is accepted only if executable, a bare
name is resolved against `/bin`, `/usr/bin`, `/sbin`, `/usr/sbin`,
`/usr/local/bin` (in order), a relative-with-slash name is rejected, and
anything else fails closed (no exec). The spawn uses `execv`, not `execvp`, so a
planted `$PATH` entry cannot hijack a bare process name.

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
UDP socket. It is what logcenter listens with: a
[`C_UDP_S`](#gclass-c-udp-s) child, one CHANNEL per peer, and the frames of
each channel joined until a NUL (the log handler of a yuno sends a log line
longer than one datagram in pieces, the NUL after the last one).

| Property | Value |
|----------|-------|
| **States** | `ST_IDLE` |
| **Input events** | `EV_SEND_MESSAGE` (a gbuffer to send, to the peer set with `gbuffer_setaddr()`), and from its `C_UDP_S`: `EV_RX_DATA`, `EV_TX_READY`, `EV_STOPPED`; `EV_TIMEOUT_PERIODIC` from its timer |
| **Output events** | `EV_ON_OPEN` (a new channel), `EV_ON_MESSAGE` (a whole frame, its gbuffer in the kw, without the NUL), `EV_ON_CLOSE` (a channel with no datagram for `seconds_inactivity`) |

A channel is keyed by the PEER of the datagram, the label `C_UDP_S` writes in
the gbuffer (`"ip:port"`). So the pieces of the frames of two peers never mix,
however they interleave:

```text
peer A "a1"   peer B "b1"   peer A "a2\0"   peer B "b2\0"
-> EV_ON_OPEN, EV_ON_OPEN, EV_ON_MESSAGE "a1a2", EV_ON_MESSAGE "b1b2"
```

Up to 7.25.4 `C_UDP_S` wrote that label only when tracing, every peer was the
channel `""`, and the same datagrams came out as `"a1b1a2"` and `"b2"` -- on
logcenter, corrupt log records whenever two yunos sent long lines at the same
time. `tests/c/c_udp_s_rx`.

```C
json_t *kw_gss = json_pack("{s:s, s:I}",
    "url", "udp://127.0.0.1:1992",
    "seconds_inactivity", (json_int_t)300
);
hgobj gss = gobj_create("logs", C_GSS_UDP_S, kw_gss, gobj);    // gobj hears EV_ON_*
```

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `url` | `string` | UDP listening URL. |
| `timeout_base` | `integer` | Period of the inactivity check, in milliseconds (default `5000`). |
| `seconds_inactivity` | `integer` | Seconds without a datagram before a channel is closed (default `300`). |
| `disable_end_of_frame` | `bool` | Publish every datagram as it comes (`EV_ON_MESSAGE` with the `EV_RX_DATA` kw), without joining until a NUL. |
| `max_channels` | `integer` | Peers held at once (default `1024`, `0` no limit). A datagram of a new peer beyond it is dropped. |
| `max_frame_size` | `integer` | Bytes of one frame (default `1048576`). A frame with no NUL within it is delivered cut at this size. |
| `max_pending_bytes` | `integer` | Bytes of all the unfinished frames together (default `8388608`, `0` no limit). Beyond it the datagram is dropped, with the unfinished frame of its peer. |

### What a peer can hold

A frame costs this yuno memory before it is a frame, and the peer decides how
much: every source port of a host is a peer, and each keeps its unfinished
frame for `seconds_inactivity`. So three caps bound it:

| Cap | When it is reached | Logged |
|-----|--------------------|--------|
| `max_channels` | the datagrams of a NEW peer are dropped; the peers held go on | once, *"Too many peers, datagrams of new peers dropped"*, and again only after a peer has gone |
| `max_frame_size` | what came is delivered cut, and a new frame begins | *"Frame without end within max_frame_size, delivered cut"*, at most once per 10 s, with the count |
| `max_pending_bytes` | the datagram is dropped, with the unfinished frame of its peer | *"Too many bytes in unfinished frames, ..."*, at most once per 10 s, with the count |

The buffer of a frame starts at 4 KB and doubles as bytes come, up to
`max_frame_size`. In 7.25.5, before these caps, it was 1 MB from the first
byte of each peer, and a sender of one-byte datagrams from many source ports
took logcenter to its memory ceiling (up to 7.25.4 every peer shared one
channel, and the corrupt joins above were the price). `tests/c/c_udp_s_rx`,
case 4.

The defaults fit logcenter, which creates its `C_GSS_UDP_S` with only the
`url`: one peer per yuno of the node. A host that expects more peers, or
longer frames, passes the caps when it creates it:

```C
json_t *kw_gss = json_pack("{s:s, s:i, s:i}",
    "url", "udp://127.0.0.1:1992",
    "max_channels", 4096,
    "max_pending_bytes", 32*1024*1024
);
hgobj gss = gobj_create("logs", C_GSS_UDP_S, kw_gss, gobj);
```
