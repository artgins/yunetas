# Debugging a yuno

This document covers what you actually do when something inside a running yuno
is misbehaving: how to turn the right knobs to see what's happening, where the
output lands, how to follow a single message end-to-end through several yunos,
and how the centralised log aggregator (`logcenter`) fits in.

Companion to [`YUNO_LIFECYCLE.md`](YUNO_LIFECYCLE.md). That one covers how the agent
manages yunos; this one covers how to look *inside* them.

---

## 1. Mental model

Three observation layers are independent. Confusing them is the first source
of frustration:

| Layer                  | Question it answers                        | How you turn it on        |
|------------------------|--------------------------------------------|---------------------------|
| **Log (severity)**     | "Did something bad happen?"                | Always on. Filter by severity in the log file. |
| **Trace (categories)** | "What was the system *doing* just now?"    | `set-global-trace` / `set-gclass-trace` / `set-gobj-trace` — off by default. |
| **Audit**              | "What commands did operators run on this yuno?" | Always written when `use_audit_command_file=true`. |

Two destinations, configurable per yuno via `daemon_log_handlers` in the yuno
config JSON:

```
                        ┌──────────────────────────┐
       severity logs    │     file handler         │  → /yuneta/logs/<yuno>/<mask>.log
       + traces  ──────►│     (rotatory, ~8 MB)    │
                        └──────────────────────────┘
                        ┌──────────────────────────┐
                        │     udp handler          │  → udp://host:port
                        │     (default :1992)      │  → typically the logcenter yuno
                        └──────────────────────────┘
                        ┌──────────────────────────┐
                        │     stdout (console mode)│  → terminal when not daemonised
                        └──────────────────────────┘
                        ┌──────────────────────────┐
                        │     remote_log over      │
                        │     ievent / websocket   │  → SPA "dev panel" (live viewer)
                        └──────────────────────────┘
```

Same log line can fan out to all four destinations at once. None of them is
"the" log — they are just different sinks.

---

## 2. Severity levels (`gobj_log_*`)

These are the calls every gclass uses to record events. They are **not**
traces — they fire regardless of trace settings. The six public ones are
defined in `kernel/c/gobj-c/src/glogger.c`:

| Function                | Priority      | At                    |
|-------------------------|---------------|-----------------------|
| `gobj_log_alert`        | `LOG_ALERT`   | [glogger.c:499](https://github.com/artgins/yunetas/blob/7.5.1/kernel/c/gobj-c/src/glogger.c#L499)         |
| `gobj_log_critical`     | `LOG_CRIT`    | [glogger.c:514](https://github.com/artgins/yunetas/blob/7.5.1/kernel/c/gobj-c/src/glogger.c#L514)         |
| `gobj_log_error`        | `LOG_ERR`     | [glogger.c:529](https://github.com/artgins/yunetas/blob/7.5.1/kernel/c/gobj-c/src/glogger.c#L529)         |
| `gobj_log_warning`      | `LOG_WARNING` | [glogger.c:544](https://github.com/artgins/yunetas/blob/7.5.1/kernel/c/gobj-c/src/glogger.c#L544)         |
| `gobj_log_info`         | `LOG_INFO`    | [glogger.c:559](https://github.com/artgins/yunetas/blob/7.5.1/kernel/c/gobj-c/src/glogger.c#L559)         |
| `gobj_log_debug`        | `LOG_DEBUG`   | [glogger.c:574](https://github.com/artgins/yunetas/blob/7.5.1/kernel/c/gobj-c/src/glogger.c#L574)         |

Plus two parallel non-syslog channels declared in glogger.c:

- `LOG_AUDIT` (8) — written **without the standard header**. Used for command
  audit. Tooling that filters on the standard timestamp format will skip these
  lines — read the audit file directly.
- `LOG_MONITOR` (9) — used by monitoring tools.

**Per-yuno HARD RULE** (see [`CLAUDE.md`](../../../CLAUDE.md)): every
error-return path calls `gobj_log_error` or carries an
`// Error already logged` comment. If you can't find the error in the log,
it's a bug in that yuno, not "the log lost it".

---

## 3. Trace categories

Tracing is the off-by-default running commentary the framework can emit. It
is **noisy** — only turn it on when needed, and turn it off when done.

### 3.1 Global trace levels

Defined in `s_global_trace_level[16]` at `kernel/c/gobj-c/src/gobj.c`:

| Bit | Name              | Emits when                                                          |
|-----|-------------------|---------------------------------------------------------------------|
| 0   | `machine`         | Every FSM event dispatch + every state change. The big one. See §6. |
| 1   | `create_delete`   | gobj created / destroyed                                            |
| 2   | `create_delete2`  | Same as above, plus the kw payload                                  |
| 3   | `subscriptions`   | `gobj_subscribe_event` / `gobj_unsubscribe_event`                   |
| 4   | `start_stop`      | `gobj_start` / `gobj_stop`                                          |
| 5   | `ev_kw`           | Dump the `kw` JSON payload on every event dispatch (huge volume)    |
| 6   | `authzs`          | Authorisation checks                                                |
| 7   | `states`          | State changes (subset of `machine`)                                 |
| 8   | `gbuffers`        | gbuffer alloc / free / realloc                                      |
| 9   | `timer`           | One-shot timer fires                                                |
| 10  | `fs`              | Filesystem ops — including timeranger2 appends                      |
| 11  | `liburing`        | io_uring submit / complete                                          |
| 12  | `timer_periodic`  | Periodic timer fires (separate from `timer` to avoid spam)          |
| 13  | `liburing_timer`  | io_uring-backed timers                                              |
| 14  | `commands`        | `gobj_command` invocations                                          |

These are global bits — when on, **every** gobj in the yuno is affected.

### 3.2 Per-gclass trace levels

Each gclass declares its own up-to-16 levels in `s_user_trace_level[16]`.
Example: `c_tcp_s.c`

```c
enum {
    TRACE_LISTEN        = 0x0001,
    TRACE_NOT_ACCEPTED  = 0x0002,
    TRACE_ACCEPTED      = 0x0004,
    TRACE_TLS           = 0x0008,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
    {"listen",          "Trace listen"},
    {"not-accepted",    "Trace not accepted connections"},
    {"accepted",        "Trace accepted connections"},
    {"tls",             "Trace tls"},
    {0, 0},
};
```

The names are gclass-specific. Common ones across runtime gclasses:

- `C_TCP`, `C_TCP_S`: `traffic`, `connect`, `tls`, `listen`, `accepted`, `not-accepted`
- `C_PROT_HTTP_SR`, `C_PROT_HTTP_CL`: `traffic`
- `C_IEVENT_SRV`, `C_IEVENT_CLI`: `ievents`, `ievents2` (the second dumps full kw)
- `C_WEBSOCKET`: gclass-specific `debug` for HTTP-upgrade handshakes
- `C_TIMER`, `C_TIMER0`: their own `tick` / `periodic`

Discover what a gclass actually offers with `get-gclass-trace gclass=<X>`
(see §4).

### 3.3 Per-gobj trace levels

Same as per-gclass but scoped to a single gobj instance. Useful when you have
ten TCP connections and only want the trace for one. API:
`gobj_set_gobj_trace()` at [`kernel/c/gobj-c/src/gobj.c:11256`](https://github.com/artgins/yunetas/blob/7.5.1/kernel/c/gobj-c/src/gobj.c#L11256).

### 3.4 The `no_trace` parallel system

For every "set trace" command there is a "set no-trace" counterpart. The
no-trace mask is **subtracted** from the effective trace mask, so you can
turn on a noisy level globally and silence it on specific gclasses / gobjs.
Functions: `gobj_set_global_no_trace()` at [gobj.c:11396](https://github.com/artgins/yunetas/blob/7.5.1/kernel/c/gobj-c/src/gobj.c#L11396),
`gobj_set_gclass_no_trace()` at [gobj.c:11617](https://github.com/artgins/yunetas/blob/7.5.1/kernel/c/gobj-c/src/gobj.c#L11617), `gobj_set_gobj_no_trace()` at
gobj.c.

### 3.5 Deep trace mode

`gobj_set_deep_tracing(level)` at gobj.c forces all traces on
regardless of masks. **Not exposed as a `ycommand`** — only via the C API,
mostly used internally for emergency dumps. Avoid unless you are willing to
deal with the volume.

---

## 4. Turning traces on and off

All commands go to the yuno itself, addressed to its `__yuno__` service.
Handlers in `kernel/c/root-linux/src/c_yuno.c`:

```bash
# discover what a gclass offers
ycommand -c 'command-yuno id=<yuno> service=__yuno__ command=get-gclass-trace gclass=C_TCP_S'

# enable / disable
ycommand -c 'command-yuno id=<yuno> service=__yuno__ command=set-global-trace level=machine set=1'
ycommand -c 'command-yuno id=<yuno> service=__yuno__ command=set-gclass-trace gclass=C_TCP_S level=traffic set=1'
ycommand -c 'command-yuno id=<yuno> service=__yuno__ command=set-gobj-trace gobj=<short_name> level=machine set=1'

# silence (no_trace)
ycommand -c 'command-yuno id=<yuno> service=__yuno__ command=set-global-no-trace level=ev_kw set=1'
ycommand -c 'command-yuno id=<yuno> service=__yuno__ command=set-gclass-no-trace gclass=C_TIMER level=periodic set=1'

# inspect current state
ycommand -c 'command-yuno id=<yuno> service=__yuno__ command=get-global-trace'
ycommand -c 'command-yuno id=<yuno> service=__yuno__ command=get-gclass-trace gclass=C_TCP_S'
ycommand -c 'command-yuno id=<yuno> service=__yuno__ command=get-gobj-trace gobj=<short_name>'
```

CLAUDE.md's shorthand `ycommand -c 'command-yuno id=<id> service=__yuno__ command=…'`
is exactly this. The default-yuno shortcut `ycommand -c 'set-global-trace …'`
is in fact `command-yuno` to whatever yuno is registered as default.

### Persistence

- **Global traces** are persisted: `c_yuno.c` (`save_global_trace`)
  writes them to the yuno's `trace_levels` attribute, and they are
  re-applied on the next start.
- **Gclass and gobj traces** are **live-only**. They die with the process.

Implication: a forgotten `set-global-trace level=machine set=1` will survive
a restart and quietly fill your disk. Always pair on/off in the same session.

---

## 5. Reading the logs

### 5.1 File paths

Per-yuno log file, built by `yuneta_log_file()` at
`kernel/c/gobj-c/src/yunetas_environment.c`:

```
/yuneta/logs/<yuno_role_plus_name>/<filename_mask>
```

The actual mask is whatever you set in `daemon_log_handlers.<handler>.filename_mask`
(see §5.4). Conventionally `<role>-W.log` (the `W` is replaced by a rotation
counter).

Active log discovery:

```bash
ls -lt /yuneta/logs/<yuno>/
tail -f /yuneta/logs/<yuno>/<latest>.log | grep -a "keyword"
```

### 5.2 Log line format

Every log record is a **JSON object** built in `glogger.c`. Fields
added automatically by `discover()` at [glogger.c:1234](https://github.com/artgins/yunetas/blob/7.5.1/kernel/c/gobj-c/src/glogger.c#L1234):

| Field             | Source                                                   |
|-------------------|----------------------------------------------------------|
| `timestamp`       | `current_timestamp()`                                    |
| `priority`        | `LOG_ERR` / `LOG_WARNING` / …                            |
| `node_uuid`       | host node identity                                       |
| `process`         | yuno binary name                                         |
| `hostname`        | from `gethostname`                                       |
| `pid`             | process id                                               |
| `gclass`          | the gclass that emitted the line                         |
| `gobj_name`       | the gobj instance name                                   |
| `state`           | current FSM state of that gobj                           |
| `gobj_full_name`  | dotted path (only if `gobj_full_name` trace is on)       |
| `id`              | sequence id                                              |
| `msgset`, `msg`   | the `"msgset","msg"` pair every `gobj_log_*` call passes |
| any `key,value`   | extra fields the caller passed                           |

Searching is JSON-friendly:

```bash
grep -a '"priority":3' /yuneta/logs/<yuno>/<file>.log       # all errors
grep -a '"gclass":"C_TCP_S"' /yuneta/logs/<yuno>/<file>.log  # one gclass
grep -a '"msg":"Event NOT DEFINED in state"' …               # the canonical FSM bug
```

### 5.3 Rotation

The `rotatory` library rotates the file when it crosses a size threshold
(default 8 MB, configurable via `max_megas_rotatoryfile_size`,
`entry_point.c`). Old files are renamed; the active filename never moves.
There is no time-based rotation. There is no cron — rotation happens on the
next write that would cross the threshold.

### 5.4 Where to configure handlers

In the yuno's config JSON, under `environment.daemon_log_handlers` (or
`console_log_handlers` in non-daemon mode), parsed at
`kernel/c/root-linux/src/entry_point.c`:

```json
"environment": {
    "daemon_log_handlers": {
        "to_file": {
            "handler_type": "file",
            "filename_mask": "mqtt_broker-W.log",
            "handler_options": 255
        },
        "to_udp": {
            "handler_type": "udp",
            "url": "udp://127.0.0.1:1992",
            "handler_options": 255
        }
    }
}
```

`handler_options` is a bitmask of `LOG_HND_OPT_*` (glogger.h) selecting
which severities the handler accepts. `255` = all of them; clearing bits drops
DEBUG / INFO / AUDIT / etc.

Add or remove handlers at runtime via `c_yuno.c`
(`add-log-handler` / `del-log-handler` commands).

---

## 6. The FSM trace (`machine`)

Single most useful trace when debugging gobj behaviour. Defined in
`glogger.c` (`trace_machine`). Called from the event dispatcher in
`kernel/c/gobj-c/src/gobj.c`:

- Before dispatch (gobj.c): a `🔜` line per event entry.
- "Event NOT DEFINED" error (gobj.c): a `📛` line — this is the
  canonical "parent FSM doesn't declare the child's event" failure (see
  CLAUDE.md "CHILD vs SERVICE" section).
- After dispatch (gobj.c): a `🔄` line per executed event.
- State change (gobj.c → 7775): a `🔀🔀` line.

Two output formats, switched by the integer variable `trace_machine_format`:

**Format 1 — short, ANSI-coloured:**

```
🔜 EV_RX_DATA !!c_tcp:open
🔄 EV_RX_DATA !!c_tcp :open from !!service_main
🔀🔀 mach(!!c_tcp), new st(:closed), prev st(:open)
```

**Default — verbose:**

```
🔜 mach(!!c_tcp), st: :open, ev: EV_RX_DATA, from(!!service_main)
🔄 mach(!!c_tcp), st: :open, ev: EV_RX_DATA, from(c_tcp_s^server)
🔀🔀 mach(!!c_tcp), new st(:closed), prev st(:open)
```

`!!` before a name means the gobj is **not running** at that moment. Two
of those in a row is usually the bug.

### Scoping the machine trace

The whole-yuno machine trace is overwhelming on anything bigger than a toy
test. Two ways to narrow:

```bash
# only one gclass
ycommand -c 'command-yuno id=<yuno> service=__yuno__ command=set-gclass-trace gclass=C_TCP_S level=machine set=1'

# only one instance
ycommand -c 'command-yuno id=<yuno> service=__yuno__ command=set-gobj-trace gobj=<short_name> level=machine set=1'
```

Both are live-only — they vanish on restart.

---

## 7. Following a message end-to-end

Canonical request flow on a typical Yuneta service:

```
   external client
         │
         ▼
  ┌─────────────┐   gclass trace 'traffic'  (c_tcp_s.c, 566)
  │   C_TCP_S   │   gobj_trace_dump_gbuf(gobj, gbuf, …)
  └──────┬──────┘
         │
         ▼
  ┌─────────────────┐   gclass trace 'traffic'  (c_prot_http_sr.c, 285, 370)
  │ C_PROT_HTTP_SR  │
  └────────┬────────┘
           │
           ▼
  ┌─────────────────┐   gclass trace 'ievents' / 'ievents2'  (c_ievent_srv.c, 483)
  │   C_IEVENT_SRV  │   trace_inter_event2(gobj, prefix, event, kw)
  └────────┬────────┘
           │
           ▼
  ┌─────────────────┐   global trace 'machine' lights up the FSM dispatch
  │  service gclass │   gclass-specific traces fire its custom emit points
  └────────┬────────┘
           │
           ▼
  ┌─────────────────┐   global trace 'fs'  (timeranger2.c, 2361)
  │   timeranger2   │   record append + rowid emitted
  │   (treedb)      │
  └────────┬────────┘
           │
           ▼  outbound publish
  ┌─────────────────┐   gclass trace 'ievents' / 'ievents2'
  │   C_IEVENT_SRV  │
  └────────┬────────┘
           │
           ▼
  ┌─────────────────┐   gclass-specific trace
  │  C_WEBSOCKET    │   gobj_trace_dump frames
  └────────┬────────┘
           │
           ▼
        SPA browser
```

### The correlation id

Inter-event messages between yunos carry a metadata block named `__md_iev__`
inside the `kw`. Inside it is the **`ievent_gate_stack`** — a LIFO of
hops, each entry: `{src_yuno, src_service, dst_yuno, dst_service, user, host, …}`.

- Constant `IEVENT_STACK_ID = "ievent_gate_stack"` at
  `kernel/c/gobj-c/src/msg_ievent.h`.
- Pushed on outgoing request, popped + reversed on incoming response, at
  `kernel/c/gobj-c/src/msg_ievent.c`.

To grep the same transaction across multiple yunos' logs:

```bash
grep -a 'ievent_gate_stack' /yuneta/logs/*/*.log | grep '<the user or src_yuno you care about>'
```

There is **no automatic UUID** propagated for non-ievent calls (a direct C
function call has nothing to grep). The correlation is only available when
the message actually traverses an ievent boundary.

### Practical sequence to follow one HTTP request

```bash
YUNO=my_service_01

# 1. ingress + protocol
ycommand -c "command-yuno id=$YUNO service=__yuno__ command=set-gclass-trace gclass=C_TCP_S        level=traffic set=1"
ycommand -c "command-yuno id=$YUNO service=__yuno__ command=set-gclass-trace gclass=C_PROT_HTTP_SR level=traffic set=1"

# 2. internal FSM
ycommand -c "command-yuno id=$YUNO service=__yuno__ command=set-global-trace level=machine set=1"

# 3. broker/topic write
ycommand -c "command-yuno id=$YUNO service=__yuno__ command=set-global-trace level=fs set=1"

# 4. egress to SPA
ycommand -c "command-yuno id=$YUNO service=__yuno__ command=set-gclass-trace gclass=C_IEVENT_SRV level=ievents  set=1"
ycommand -c "command-yuno id=$YUNO service=__yuno__ command=set-gclass-trace gclass=C_IEVENT_SRV level=ievents2 set=1"

# trigger the request, capture the noise
tail -F /yuneta/logs/$YUNO/*.log > /tmp/$YUNO.trace &
# … reproduce …
kill %1

# disable everything
ycommand -c "command-yuno id=$YUNO service=__yuno__ command=set-gclass-trace gclass=C_TCP_S        level=traffic  set=0"
ycommand -c "command-yuno id=$YUNO service=__yuno__ command=set-gclass-trace gclass=C_PROT_HTTP_SR level=traffic  set=0"
ycommand -c "command-yuno id=$YUNO service=__yuno__ command=set-global-trace level=machine set=0"
ycommand -c "command-yuno id=$YUNO service=__yuno__ command=set-global-trace level=fs      set=0"
ycommand -c "command-yuno id=$YUNO service=__yuno__ command=set-gclass-trace gclass=C_IEVENT_SRV level=ievents  set=0"
ycommand -c "command-yuno id=$YUNO service=__yuno__ command=set-gclass-trace gclass=C_IEVENT_SRV level=ievents2 set=0"
```

---

## 8. The SPA-side "dev panel" viewer

When a SPA built on the JS gobj framework is connected to a yuno, it can
display **live** what crosses the websocket — the same lines you'd see in
the local log file, plus the ievent message bodies.

### Wire-up

- C side: nothing special — inter-events flow as usual through `C_IEVENT_SRV`
  → `C_WEBSOCKET`.
- JS side: `C_IEVENT_CLI` (`kernel/js/gobj-js/src/c_ievent_cli.js`) parses
  incoming inter-events in `ac_on_message()` at [c_ievent_cli.js:1092](https://github.com/artgins/yunetas/blob/7.5.1/kernel/js/gobj-js/src/c_ievent_cli.js#L1092) and, if
  configured, invokes `trace_ievent_callback(prefix, iev_msg, direction, size)`
  at c_ievent_cli.js.
- The SPA installs that callback by writing the attribute:
  `gobj_write_attr(gobj_yuno(), "trace_ievent_callback", info_traffic)`
  (`kernel/js/lib-yui/src/yui_dev.js, 432, 537`).
- The `info_traffic()` function ([yui_dev.js:29](https://github.com/artgins/yunetas/blob/7.5.1/kernel/js/lib-yui/src/yui_dev.js#L29)) appends the message into the
  DOM container `#developer-traffic-logger`, which lives inside either:
  - the **legacy** `C_YUI_WINDOW` modal (yui_dev.js), or
  - the **modern** `build_dev_panel()` modal ([yui_dev.js:452](https://github.com/artgins/yunetas/blob/7.5.1/kernel/js/lib-yui/src/yui_dev.js#L452)).

Both still ship — apps pick one based on the shell version.

### Rendering

- Header bar: blue, with direction icon (white = sent, yellow = received,
  red = error), byte size, and `HH:MM:SS.SSSS` timestamp.
- Body: read-only vanilla-jsoneditor (`pinned ^0.23.0`, see project memory
  note about not bumping it).
- Footer: ON/OFF traffic counter.
- Auto-scroll to bottom on each message (yui_dev.js).

### Filtering on the SPA side

**None in the UI.** The viewer shows whatever flows through the websocket.
The only knobs are on/off — `trace_inter_event` boolean and the
`trace_ievent_callback` itself. To "filter", you change what the yuno emits
using the `ycommand` knobs from §4.

### Teardown order — the recursion gotcha

When the websocket closes, `ac_on_close` ([`c_ievent_cli.js:897`](https://github.com/artgins/yunetas/blob/7.5.1/kernel/js/gobj-js/src/c_ievent_cli.js#L897)) fires
`EV_ON_CLOSE`. If `set_remote_log_functions` is still installed (it
hijacks JS `log_error`/`log_warning` to the DOM callback), the warning
emitted **inside** the teardown path is captured, mutates the DOM, which
can fire more events, which log again — infinite recursion.

The fix at `c_ievent_cli.js` is to call
`set_remote_log_functions(null)` (clears the hooks, resets to console —
see `helpers.js`) **before** anything publishes `EV_ON_CLOSE`. See
memory note "Remote-log unwire order" for the historical incident.

---

## 9. The logcenter yuno

`yunos/c/logcenter/` aggregates UDP-shipped logs from every yuno on the
host (or LAN). It is **not enabled by default** — each yuno only ships to
UDP if its config lists a `udp` handler.

### How it listens

- UDP server (`c_gss_udp_s`) on `udp://127.0.0.1:1992` by default
  (`c_logcenter.c, 196-199`).
- Wire format: `<priority-digit><8hex-seq><json-payload><8hex-crc>`,
  fragmented per `udp_frame_size` (default 1500, `log_udp_handler.c`).

### What it does on receipt

`c_logcenter.c`:

- `ac_on_message()` at c_logcenter.c parses each packet
  (c_logcenter.c).
- Writes the JSON record to its own rotatory file `W.log` via
  `write2logs()` / `_log_bf()` (c_logcenter.c, 1290). Default size
  cap 8 MB (`max_rotatoryfile_size`, c_logcenter.c).
- Updates in-memory counters per severity / `msgset` / `msg`
  (`do_log_stats()`, [c_logcenter.c:858](https://github.com/artgins/yunetas/blob/7.5.1/yunos/c/logcenter/src/c_logcenter.c#L858)).

### What it exposes

Commands (`c_logcenter.c`):

| Command           | Effect                                                                  |
|-------------------|-------------------------------------------------------------------------|
| `display-summary` | Print the in-memory counters: alerts, criticals, errors, warnings.      |
| `send-summary`    | Email the same summary (used as a daily/weekly batch).                  |
| `search`          | Search the stored log file for matching lines.                          |
| `tail`            | Last N lines of the centralised log.                                    |
| `reset-counters`  | Zero the in-memory counters.                                            |

Use it like any other yuno (target it by `yuno_role=logcenter` so you
don't depend on the yuno's numeric `id`, which varies per realm; the
default service is implied for `command-yuno`):

```bash
# rollup counters (Alert/Critical/Error/Warning/Info + Connect/Disconnect breakdown)
ycommand -c 'command-yuno yuno_role=logcenter command=display-summary'

# last N log lines (default ~100; can pass lines=N)
ycommand -c 'command-yuno yuno_role=logcenter command=tail lines=200'

# substring search (parameter is text=, not match=); maxcount caps the hits
ycommand -c 'command-yuno yuno_role=logcenter command=search text="EV_ON_CLOSE" maxcount=20'

# wipe the rollup counters — useful before reproducing an issue so the
# next display-summary only shows the new run
ycommand -c 'command-yuno yuno_role=logcenter command=reset-counters'
```

Other commands worth knowing about (`c_logcenter.c`):
`send-summary` / `enable-send-summary` / `disable-send-summary` (email
rollup), `restart-yuneta-on-queue-alarm` (auto-recovery hook when the
UDP queue floods).

### Per-yuno vs centralised — when to use each

- **Per-yuno tail** when you know which yuno is misbehaving and want raw
  control over `grep`. `/yuneta/logs/<yuno>/<file>.log` is full fidelity.
- **logcenter** when you need correlation across multiple yunos, or when
  a yuno crashes too fast to read its own file, or for the rollup
  counters / email summaries.

Both can run at the same time — the file handler writes locally and the
UDP handler ships to logcenter in parallel. They are not exclusive.

---

## 10. Sharp edges

### 10.1 Traces stack up; disable when done

A forgotten `set-global-trace level=machine set=1` survives a restart
(see §4 *Persistence*). Logs balloon. Always pair on/off in the same
operational session, and double-check with `get-global-trace` before you
leave.

### 10.2 Persistence asymmetry

| Scope         | Persists across restart?       |
|---------------|--------------------------------|
| `global`      | **Yes** (via `trace_levels` attr) |
| `gclass`      | No                             |
| `gobj`        | No                             |
| `no_trace`    | No (all flavours)              |

If you persisted a global level by mistake, clear it explicitly:
`set-global-trace level=<name> set=0`. Removing the file does not help —
the value lives in the yuno's treedb config.

### 10.3 `ievent_gate_stack` is only on inter-event hops

A direct C function call between gobjs in the same yuno does **not** carry
the stack — there's no metadata to attach. The correlation only exists
across yuno boundaries. Plan your traces accordingly.

### 10.4 `LOG_AUDIT` lines have no standard header

`glogger.c` — audit lines are written raw. Any line-filter that expects
the timestamp prefix will miss them. Read the audit file directly when
chasing operator actions.

### 10.5 UDP can drop

UDP logs are not reliable. Under burst (e.g. machine trace fully on) the
kernel buffer can overflow and logcenter loses lines silently. The local
file handler does not drop — when in doubt, trust the local file.

### 10.6 `ev_kw` is enormous

`set-global-trace level=ev_kw set=1` dumps every event's full `kw` JSON
payload to the log. Useful on a single-shot test; ruinous on a busy
service. Combine with gclass-scoped `machine` trace if you need it
narrowly.

### 10.7 SPA dev-panel teardown order

`set_remote_log_functions(null)` MUST come before
`do_disconnect` / `destroy_shell`. See §8 and memory
`feedback_remote_log_unwire_order`.

### 10.8 Deep tracing has no `ycommand` switch

`gobj_set_deep_tracing()` is C-only (gobj.c). If you find a yuno
generating apparently-unconfigurable traces, look for a `gobj_set_deep_tracing`
call in its `mt_create` that someone left in.

---

## 11. Operational recipes

### 11.1 Just watch what a yuno is doing

```bash
YUNO=<id>
ycommand -c "command-yuno id=$YUNO service=__yuno__ command=set-global-trace level=machine set=1"
tail -F /yuneta/logs/$YUNO/*.log | grep -a '"msg":'
# reproduce
ycommand -c "command-yuno id=$YUNO service=__yuno__ command=set-global-trace level=machine set=0"
```

### 11.2 Watch traffic on a TCP/HTTP service

```bash
ycommand -c "command-yuno id=$YUNO service=__yuno__ command=set-gclass-trace gclass=C_TCP_S        level=traffic set=1"
ycommand -c "command-yuno id=$YUNO service=__yuno__ command=set-gclass-trace gclass=C_PROT_HTTP_SR level=traffic set=1"
tail -F /yuneta/logs/$YUNO/*.log
# … done …
ycommand -c "command-yuno id=$YUNO service=__yuno__ command=set-gclass-trace gclass=C_TCP_S        level=traffic set=0"
ycommand -c "command-yuno id=$YUNO service=__yuno__ command=set-gclass-trace gclass=C_PROT_HTTP_SR level=traffic set=0"
```

### 11.3 Watch all logs from this host in one place

Enable logcenter. In every yuno's config JSON add:

```json
"daemon_log_handlers": {
    "to_udp": { "handler_type": "udp", "url": "udp://127.0.0.1:1992", "handler_options": 255 }
}
```

Then:

```bash
ycommand -c 'command-yuno yuno_role=logcenter command=tail lines=500'
ycommand -c 'command-yuno yuno_role=logcenter command=search text="<keyword>" maxcount=50'
ycommand -c 'command-yuno yuno_role=logcenter command=display-summary'
ycommand -c 'command-yuno yuno_role=logcenter command=reset-counters'   # wipe the rollup
```

### 11.4 Follow one request end-to-end

See §7 — the block of `set-gclass-trace ... traffic` + `set-global-trace
... machine` + `set-global-trace ... fs` + `set-gclass-trace C_IEVENT_SRV
... ievents2` is the kit. Don't forget to disable everything afterwards.

### 11.5 Capture an FSM bug in one gobj only

```bash
ycommand -c "command-yuno id=$YUNO service=__yuno__ command=set-gobj-trace gobj=<short_name> level=machine set=1"
ycommand -c "command-yuno id=$YUNO service=__yuno__ command=set-gobj-trace gobj=<short_name> level=ev_kw   set=1"
tail -F /yuneta/logs/$YUNO/*.log | grep -a '<short_name>'
# … done …
ycommand -c "command-yuno id=$YUNO service=__yuno__ command=set-gobj-trace gobj=<short_name> level=machine set=0"
ycommand -c "command-yuno id=$YUNO service=__yuno__ command=set-gobj-trace gobj=<short_name> level=ev_kw   set=0"
```

These are live-only — even if you forget the off-step, a yuno restart
clears them.

### 11.6 Spot the canonical "Event NOT DEFINED in state" error

That single string at gobj.c is the most common FSM failure (parent
FSM didn't declare a child's published event — see CLAUDE.md "CHILD vs
SERVICE"). It's logged at `LOG_ERR` regardless of trace settings, so:

```bash
grep -a '"msg":"Event NOT DEFINED in state"' /yuneta/logs/*/*.log
```

works on any host without enabling anything.

---

## 12. Code pointers

| What                                          | Where                                                                  |
|-----------------------------------------------|------------------------------------------------------------------------|
| Severity log API                              | `kernel/c/gobj-c/src/glogger.c`                                |
| `LOG_AUDIT` / `LOG_MONITOR`                   | `kernel/c/gobj-c/src/glogger.c`                                  |
| Trace emit API (`gobj_trace_msg/json/dump`)   | `[kernel/c/gobj-c/src/glogger.c:778](https://github.com/artgins/yunetas/blob/7.5.1/kernel/c/gobj-c/src/glogger.c#L778), 827, 858`                          |
| Global trace level table                      | `kernel/c/gobj-c/src/gobj.c`                                   |
| Per-gclass trace declaration (example)        | `kernel/c/root-linux/src/c_tcp_s.c`                             |
| Trace mask lookup                             | `kernel/c/gobj-c/src/gobj.c` (`gobj_trace_level`)          |
| Per-gobj trace API                            | `kernel/c/gobj-c/src/gobj.c` (`gobj_set_gobj_trace`)             |
| `no_trace` API                                | `kernel/c/gobj-c/src/gobj.c, 11617, 11746`                       |
| Deep trace                                    | `kernel/c/gobj-c/src/gobj.c`                               |
| `trace_machine` print                         | [`kernel/c/gobj-c/src/glogger.c:1161`](https://github.com/artgins/yunetas/blob/7.5.1/kernel/c/gobj-c/src/glogger.c#L1161)                                   |
| FSM dispatch trace sites                      | `kernel/c/gobj-c/src/gobj.c, 7775`                           |
| Trace persistence (`trace_levels` attr)       | `kernel/c/root-linux/src/c_yuno.c`                           |
| Trace commands exposed by every yuno          | `kernel/c/root-linux/src/c_yuno.c`                             |
| `daemon_log_handlers` parser                  | `kernel/c/root-linux/src/entry_point.c`                        |
| Log file path builder                         | `kernel/c/gobj-c/src/yunetas_environment.c`                    |
| Log line `discover()` (metadata fields)       | [`kernel/c/gobj-c/src/glogger.c:1234`](https://github.com/artgins/yunetas/blob/7.5.1/kernel/c/gobj-c/src/glogger.c#L1234)                              |
| UDP wire format                               | `kernel/c/gobj-c/src/log_udp_handler.c`                        |
| `ievent_gate_stack` constant                  | `kernel/c/gobj-c/src/msg_ievent.h`                                  |
| `ievent_gate_stack` push/pop                  | `kernel/c/gobj-c/src/msg_ievent.c`                             |
| logcenter listener                            | `yunos/c/logcenter/src/c_logcenter.c, 196-199`                     |
| logcenter commands                            | `yunos/c/logcenter/src/c_logcenter.c`                           |
| SPA dev-panel renderer                        | `kernel/js/lib-yui/src/yui_dev.js, 405-548`                         |
| SPA inter-event callback hook                 | `kernel/js/gobj-js/src/c_ievent_cli.js, 1111`                     |
| SPA teardown order                            | `kernel/js/gobj-js/src/c_ievent_cli.js, 925`                       |
