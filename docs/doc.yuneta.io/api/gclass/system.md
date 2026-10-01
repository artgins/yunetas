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
| **States** | `ST_IDLE` |
| **Output events** | `EV_FS_CHANGED`, `EV_FS_RENAMED`, both with `{"path", "filename"}` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `path` | `string` | Directory to watch. |
| `recursive` | `bool` | `1`: watch the whole tree under `path`, the subdirectories created later included. `0` (default): only the entries of `path` itself. |
| `info` | `bool` | Log the watched directory on startup. |
| `subscriber` | `pointer` | Who gets its events. Default: its parent (the CHILD subscription model). |
| `size_dl_watch` | `int` (stats) | `1` while the path is watched, `0` if not. One watcher, whether recursive or not (the name is older than that). |

A watch that cannot be armed fails the start of `C_FS` (logged), and a watch
whose read fails later (`FS_WATCHER_GONE_TYPE`) is logged and gone:
`size_dl_watch` reads `0`, and nothing more is published.

### What it publishes

One event per change the watcher reports, `path` being the directory and
`filename` the entry that changed:

| Change (fs_watcher type) | Event |
|---|---|
| directory created / deleted (`FS_SUBDIR_CREATED_TYPE`, `FS_SUBDIR_DELETED_TYPE`) | `EV_FS_CHANGED` |
| file created / deleted / modified (`FS_FILE_CREATED_TYPE`, `FS_FILE_DELETED_TYPE`, `FS_FILE_MODIFIED_TYPE`) | `EV_FS_CHANGED` |
| rename (`FS_FILE_RENAME_TYPE`, not reported by the watcher today) | `EV_FS_RENAMED` |
| events lost (`FS_OVERFLOW_TYPE`) | `EV_FS_CHANGED` once, for the watched root (`filename` empty); the rescan pass that follows publishes nothing |

A host creates it as its child and declares both events in its FSM: a
`C_FS` subscribes its parent (or the gobj in `subscriber`) by itself, the
CHILD subscription model:

```C
priv->gobj_fs = gobj_create("", C_FS, json_pack("{s:s, s:b}",
    "path", "/yuneta/development/docs",
    "recursive", 1
), gobj);
gobj_start(priv->gobj_fs);
```

Up to 7.25.20 it subscribed nobody, and every host subscribed by hand
(`gobj_subscribe_event(priv->gobj_fs, NULL, 0, gobj)`). A host that still
does subscribes twice, and the second subscription replaces the first with
a warning (*"subscription(s) REPEATED, will be deleted and override"*): drop
the call.

Up to 7.25.20 the types were read as bits: a deleted directory published
nothing (and leaked the kw built for it), and the other changes were
published only because the value of "file modified" (5) shared bits with
theirs.

`recursive` is one watcher, whichever its value: with `1` it recurses by
itself (`FS_FLAG_RECURSIVE_PATHS`). Up to 7.25.20 a recursive `C_FS` also
added a recursive watcher for each subdirectory it found at start, each with
its own inotify fd, so a change N levels down was published N+1 times (a file
created in `a/b/` three times); and every watcher recursed, so `recursive: 0`
reported the subdirectories too. With `"recursive", 0`:

```C
priv->gobj_fs = gobj_create("", C_FS, json_pack("{s:s, s:b}",
    "path", "/yuneta/development/docs",
    "recursive", 0      // docs/x.md is published, docs/api/y.md is not
), gobj);
```

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
| **Input events** | `EV_SEND_MESSAGE` (a gbuffer to send: to its address, or, with none, to the known peer its label names -- see *Answer a peer*), and from its `C_UDP_S`: `EV_RX_DATA`, `EV_TX_READY`, `EV_STOPPED`; `EV_TIMEOUT_PERIODIC` from its timer, `EV_TIMEOUT` from its restart timer |
| **Output events** | `EV_ON_OPEN` (a new channel), `EV_ON_MESSAGE` (a whole frame, its gbuffer in the kw, without the NUL; the gbuffer carries its peer as label and address), `EV_ON_CLOSE` (a channel with no datagram for `seconds_inactivity`) |

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

### Answer a peer

The gbuffer of an `EV_ON_MESSAGE` carries its peer twice: as its LABEL
(`gbuffer_getlabel()`, `"ip:port"`, the name of its channel) and as its
ADDRESS (`gbuffer_getaddr()`). `EV_SEND_MESSAGE` takes either:

- a gbuffer with an address (`gbuffer_setaddr()`) goes to that address,
  whatever its label;
- a gbuffer with NO address goes to the known peer its label names -- a
  channel that is open, whose address is taken;
- with neither, it is refused: an ERROR, *"EV_SEND_MESSAGE without a peer: no
  address, and its label names no known peer; dropped"*, and the event answers
  -1.

```C
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gbuffer_t *frame = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);

    gbuffer_t *answer = gbuffer_create(64, 64);
    gbuffer_append_string(answer, "ack");
    gbuffer_setlabel(answer, gbuffer_getlabel(frame));  // or gbuffer_setaddr() with its address
    gobj_send_event(gss, EV_SEND_MESSAGE,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)answer),   // the kw owns it
        gobj
    );

    KW_DECREF(kw)
    return 0;
}
```

Up to 7.25.4 a frame carried neither its peer's label nor its address, so no
host could answer the peer of a frame; and `EV_SEND_MESSAGE` looked the
channel up by the label and did not use it: a send by address (the documented
way) logged *"UDP channel NOT FOUND"* for every datagram, and a send by label
had no address: the kernel refused the datagram (`EINVAL`), and `C_UDP_S` took
that for a disconnection and stopped, reading too.
The label contract is the original one (the pre-v7 `C_UDP_S` sent to the
`"ip:port"` of the label); the address is how `C_UDP_S` sends since v7.
`tests/c/c_udp_s_rx`, case 5.

### When its C_UDP_S stops by itself

`C_UDP_S` stops by itself when its read fails or cannot be started again (no
memory for the next read, a socket error): it logs the cause as an ERROR and
publishes `EV_STOPPED` -- while it still RUNS, which is how `C_GSS_UDP_S`
tells it from a stop made from outside. `C_GSS_UDP_S` then:

- says it once: *"UDP server stopped by itself, it is started again after a
  backoff"*, WARNING, with `backoff_ms`;
- starts the `C_UDP_S` again after that backoff, and says *"UDP server
  started again"*, INFO, with `tx_dropped`, the sends refused meanwhile. The
  backoff is `timeout_base` first, and DOUBLES (up to 5 minutes) each time
  the `C_UDP_S` stops again, or cannot start (*"UDP server cannot start
  again, it is tried again after a backoff"*), within a minute of the last
  try; a restart that holds a minute sets it back to `timeout_base`. A
  failure that persists is said a few times an hour, not at every tick. A
  `C_UDP_S` that cannot start at the first start of the `C_GSS_UDP_S` takes
  the same path (*"UDP server cannot start, it is tried again after a
  backoff"*).

A send is refused whenever the `C_UDP_S` is not in `ST_IDLE`: stopped, or
still stopping (`ST_WAIT_STOPPED`, while a send of its own is in flight, the
window before its `EV_STOPPED`). The event answers -1, and ONE warning is
logged per stop, *"EV_SEND_MESSAGE while the UDP server is stopped,
dropped"*, with `udp_state`; the count comes with the restart, or, if the
`C_GSS_UDP_S` is stopped first, with its stop (*"UDP server stops with sends
refused while it was stopped"*, `tx_dropped`).

A `C_UDP_S` stopped from OUTSIDE (a `gobj_stop()` of it alone: it no longer
runs when its `EV_STOPPED` comes) is not started again -- that stop was
somebody's decision. It is said once, INFO, *"UDP server stopped from
outside, it is not started again"*, and the sends are refused as above.

Its own stop (`gobj_stop()` of the `C_GSS_UDP_S`) stops the `C_UDP_S` too, and
that `EV_STOPPED` is its end, not a restart.

A `timeout_base` of 0 or less would arm no timer at all: no peer forgotten
after `seconds_inactivity`, and no base for the backoff. It is refused at the
create with a WARNING (*"timeout_base <= 0 would arm no timer ..."*), and the
default `5000` is used (and written back to the attribute):

```C
json_t *kw_gss = json_pack("{s:s, s:i}",
    "url", "udp://127.0.0.1:1992",
    "timeout_base", 2000        // inactivity check, and first restart delay
);
hgobj gss = gobj_create("logs", C_GSS_UDP_S, kw_gss, gobj);
```

Up to 7.25.20 the `EV_STOPPED` was taken with no action: the service could
not receive again, and every send reached a stopped `C_UDP_S` and logged
*"Event NOT DEFINED in state"*. `tests/c/c_gss_udp_s_self_stop`.

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `url` | `string` | UDP listening URL. |
| `timeout_base` | `integer` | Period of the inactivity check, and first delay of a restart of the `C_UDP_S`, in milliseconds (default `5000`; `<= 0` is refused with a warning and the default used). |
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
`max_frame_size`. A buffer of 1 MB from the first byte of each peer, with no
cap on the peers, would let a sender of one-byte datagrams from many source
ports take logcenter to its memory ceiling. (Up to 7.25.4 every peer shared
one channel, and the corrupt joins above were the price.) `tests/c/c_udp_s_rx`,
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
