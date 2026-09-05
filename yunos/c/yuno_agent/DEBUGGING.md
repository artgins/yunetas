# Debugging a yuno

This document covers what you do when a yuno does not behave correctly. It
explains how to enable the traces that show you what happens, where the output
goes, and how to follow one message through several yunos. It also explains the
part that the centralized log aggregator (`logcenter`) plays.

This document is the companion to [`YUNO_LIFECYCLE.md`](YUNO_LIFECYCLE.md).
That one covers how the agent manages yunos. This one covers how to look
*inside* them.

---

## 1. Mental model

Three observation layers are independent. A confusion between them is the
first source of frustration:

| Layer                  | Question it answers                        | How you turn it on        |
|------------------------|--------------------------------------------|---------------------------|
| **Log (severity)**     | "Did something bad happen?"                | Always on. Filter by severity in the log file. |
| **Trace (categories)** | "What was the system *doing* a moment ago?"    | `set-global-trace` / `set-gclass-trace` / `set-gobj-trace` — off by default. |
| **Audit**              | "What commands did operators run on this yuno?" | Always written when `use_audit_command_file=true`. |

You configure four destinations per yuno, with `daemon_log_handlers` in the
yuno config JSON:

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

One log line can go to all four destinations at the same time. No destination
is "the" log. They are different sinks.

---

## 2. Severity levels (`gobj_log_*`)

These are the calls every gclass uses to record events. They are **not**
traces — they fire regardless of trace settings. The six public ones are
defined in [`kernel/c/gobj-c/src/glogger.c`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/gobj-c/src/glogger.c):

| Function                | Priority      | At                    |
|-------------------------|---------------|-----------------------|
| [`gobj_log_alert`](#gobj_log_alert)        | `LOG_ALERT`   | [glogger.c:499](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/gobj-c/src/glogger.c#L499)         |
| [`gobj_log_critical`](#gobj_log_critical)     | `LOG_CRIT`    | [glogger.c:514](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/gobj-c/src/glogger.c#L514)         |
| [`gobj_log_error`](#gobj_log_error)        | `LOG_ERR`     | [glogger.c:529](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/gobj-c/src/glogger.c#L529)         |
| [`gobj_log_warning`](#gobj_log_warning)      | `LOG_WARNING` | [glogger.c:544](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/gobj-c/src/glogger.c#L544)         |
| [`gobj_log_info`](#gobj_log_info)         | `LOG_INFO`    | [glogger.c:559](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/gobj-c/src/glogger.c#L559)         |
| [`gobj_log_debug`](#gobj_log_debug)        | `LOG_DEBUG`   | [glogger.c:574](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/gobj-c/src/glogger.c#L574)         |

glogger.c declares two more channels that are not syslog channels:

- `LOG_AUDIT` (8) — the framework writes these lines **without the standard
  header**. They record the command audit. A tool that filters on the standard
  timestamp format skips them, so read the audit file directly.
- `LOG_MONITOR` (9) — the monitoring tools use this channel.

**Per-yuno HARD RULE** (see [`CLAUDE.md`](../../../CLAUDE.md)): every
error-return path calls `gobj_log_error` or carries an
`// Error already logged` comment. If you cannot find the error in the log,
that yuno has a bug. The log did not lose it.

---

## 3. Trace categories

A trace is the running commentary that the framework can emit. It is off by
default. It is **noisy**, so enable it only when you need it, and disable it
when you finish.

### 3.1 Global trace levels

Defined in `s_global_trace_level[16]` at [`kernel/c/gobj-c/src/gobj.c`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/gobj-c/src/gobj.c):

| Bit | Name              | Emits when                                                          |
|-----|-------------------|---------------------------------------------------------------------|
| 0   | `machine`         | Every FSM event dispatch + every state change. The big one. See §6. |
| 1   | `create_delete`   | gobj created / destroyed                                            |
| 2   | `create_delete2`  | Same as above, plus the kw payload                                  |
| 3   | `subscriptions`   | `gobj_subscribe_event` / `gobj_unsubscribe_event`                   |
| 4   | `start_stop`      | `gobj_start` / `gobj_stop`                                          |
| 5   | `ev_kw`           | Dump the `kw` JSON payload on every event dispatch (huge volume)    |
| 6   | `authzs`          | Authorization checks                                                |
| 7   | `states`          | State changes (subset of `machine`)                                 |
| 8   | `gbuffers`        | gbuffer alloc / free / realloc                                      |
| 9   | `timer`           | One-shot timer fires                                                |
| 10  | `fs`              | Filesystem ops — including timeranger2 appends                      |
| 11  | `liburing`        | io_uring submit / complete                                          |
| 12  | `timer_periodic`  | Periodic timer fires (separate from `timer` to avoid spam)          |
| 13  | `liburing_timer`  | io_uring-backed timers                                              |
| 14  | `commands`        | `gobj_command` invocations                                          |

These are global bits. When you enable one, it affects **every** gobj in the
yuno.

### 3.2 Per-gclass trace levels

Each gclass declares its own up-to-16 levels in `s_user_trace_level[16]`.
Example: [`c_tcp_s.c`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/root-linux/src/c_tcp_s.c)

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

- [`C_TCP`](#gclass-c-tcp), [`C_TCP_S`](#gclass-c-tcp-s): `traffic`, `connect`, `tls`, `listen`, `accepted`, `not-accepted`
- [`C_PROT_HTTP_SR`](#gclass-c-prot-http-sr), [`C_PROT_HTTP_CL`](#gclass-c-prot-http-cl): `traffic`
- [`C_IEVENT_SRV`](#gclass-c-ievent-srv), [`C_IEVENT_CLI`](#gclass-c-ievent-cli): `ievents`, `ievents2` (the second dumps full kw)
- [`C_WEBSOCKET`](#gclass-c-websocket): gclass-specific `debug` for HTTP-upgrade handshakes
- [`C_TIMER`](#gclass-c-timer), [`C_TIMER0`](#gclass-c-timer0): their own `tick` / `periodic`

To see what a gclass offers, run `get-gclass-trace gclass=<X>` (see §4).

### 3.3 Per-gobj trace levels

These levels are the same as the per-gclass levels, but they are scoped to one
gobj instance. They are useful when you have ten TCP connections and you want
the trace of one connection. API:
[`gobj_set_gobj_trace()`](#gobj_set_gobj_trace) at [`kernel/c/gobj-c/src/gobj.c:11256`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/gobj-c/src/gobj.c#L11256).

### 3.4 The `no_trace` parallel system

For every "set trace" command there is a "set no-trace" counterpart. The
framework **subtracts** the no-trace mask from the effective trace mask. So
you can enable a noisy level globally, then silence it on specific gclasses or
gobjs.
Functions: [`gobj_set_global_no_trace()`](#gobj_set_global_no_trace) at [gobj.c:11711](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/gobj-c/src/gobj.c#L11711),
[`gobj_set_gclass_no_trace()`](#gobj_set_gclass_no_trace) at [gobj.c:11617](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/gobj-c/src/gobj.c#L11617), [`gobj_set_gobj_no_trace()`](#gobj_set_gobj_no_trace) at [gobj.c:11746](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/gobj-c/src/gobj.c#L11746).

### 3.5 Deep trace mode

[`gobj_set_deep_tracing(level)`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/gobj-c/src/gobj.c#L11338)
enables all traces, and the masks do not apply. There is **no
[`ycommand`](#util-ycommand) for it**. It is available only in the C API, and
the framework uses it internally for emergency dumps. Do not use it unless you
can accept the volume.

---

## 4. Turning traces on and off

All commands go to the yuno itself, addressed to its `__yuno__` service.
Handlers in [`kernel/c/root-linux/src/c_yuno.c`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/root-linux/src/c_yuno.c):

```bash
# discover what a gclass offers
ycommand -c 'command-yuno id=<yuno> service=__yuno__ command=get-gclass-trace gclass=C_TCP_S'

# enable / disable
ycommand -c 'command-yuno id=<yuno> service=__yuno__ command=set-global-trace level=machine set=1'
ycommand -c 'command-yuno id=<yuno> service=__yuno__ command=set-gclass-trace gclass=C_TCP_S level=traffic set=1'
ycommand -c 'command-yuno id=<yuno> service=__yuno__ command=set-gobj-trace gobj=<short_name> level=machine set=1'

# silence (no_trace) — per gclass or per gobj
ycommand -c 'command-yuno id=<yuno> service=__yuno__ command=set-gclass-no-trace gclass=C_TIMER level=periodic set=1'
ycommand -c 'command-yuno id=<yuno> service=__yuno__ command=set-gobj-no-trace gobj=<short_name> level=machine set=1'

# inspect current state
ycommand -c 'command-yuno id=<yuno> service=__yuno__ command=get-global-trace'
ycommand -c 'command-yuno id=<yuno> service=__yuno__ command=get-gclass-trace gclass=C_TCP_S'
ycommand -c 'command-yuno id=<yuno> service=__yuno__ command=get-gobj-trace gobj=<short_name>'
```

The short form in CLAUDE.md,
`ycommand -c 'command-yuno id=<id> service=__yuno__ command=…'`, is exactly
this. The shorter form `ycommand -c 'set-global-trace …'` sends `command-yuno`
to the yuno that is registered as the default yuno.

> **There is no `set-global-no-trace` command, by design.** From the control
> plane the no-trace switch is exposed per gclass (`set-gclass-no-trace`) and
> per gobj (`set-gobj-no-trace`) only — that is where silencing one noisy
> level while a broad trace is on makes sense. To silence a global level, do
> not mask it: clear it with `set-global-trace … set=0`.
>
> The C API [`gobj_set_global_no_trace()`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/gobj-c/src/gobj.h#L2130) is a different thing and is
> used everywhere: each yuno's `main.c` calls it once at start up, almost
> always as `gobj_set_global_no_trace("timer_periodic", TRUE)`. That is why a
> global `machine` trace does not drown in timer ticks. It is a compile-time
> decision of the yuno, it is never persisted, and no command changes it.

(persistence-of-traces)=
### Persistence

**Every trace set from the control plane is persisted. None of them are
live-only.** `set-global-trace`, `set-gclass-trace` and `set-gobj-trace` all
end in `gobj_save_persistent_attrs()` on the yuno's `trace_levels` attribute,
and their `no-trace` counterparts on `no_trace_levels`:

| Command                 | Saver                                        | Key in the attr    |
|-------------------------|----------------------------------------------|--------------------|
| `set-global-trace`      | [`save_global_trace`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/root-linux/src/c_yuno.c#L2164)  | `__global_trace__` |
| `set-gclass-trace`      | [`save_user_trace`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/root-linux/src/c_yuno.c#L2247)    | the gclass name    |
| `set-gobj-trace`        | [`save_user_trace`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/root-linux/src/c_yuno.c#L2247)    | the gobj name      |
| `set-gclass-no-trace`   | [`save_user_no_trace`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/root-linux/src/c_yuno.c#L2300) | the gclass name    |
| `set-gobj-no-trace`     | [`save_user_no_trace`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/root-linux/src/c_yuno.c#L2300) | the gobj name      |

They are re-applied on the next start: `set_user_gclass_traces()` and
`set_user_gclass_no_traces()` run from `mt_create`, `set_user_gobj_traces()`
right after the children are built.

CAUTION: a forgotten `set-gclass-trace gclass=C_TCP_S level=traffic set=1`
survives a restart exactly like a global one, and it fills your disk. It gives
no message first. Always pair the enable and the disable in the same session.

> An entry whose key names a gclass that no longer exists is skipped **without
> a log** at start up ([`c_yuno.c:4992`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/root-linux/src/c_yuno.c#L4992), a deliberate exception to the
> no-silent-errors rule: a mistyped gclass name is the common case). So a trace
> that does not turn on after a restart usually means a typo in the persisted
> attr. Read it with `list-persistent-attrs`, and clear it with
> `remove-persistent-attrs`.

### 4.1 When the yuno never reaches the agent (`--global-trace`)

Every command above travels over the yuno's control channel to the agent. So
none of them work for the failure that most needs a trace: a yuno that dies,
hangs or fails **before** that channel is ready. `ycommand` cannot reach it,
and `list-yunos` reports `running=false` even while the process is alive.

Since 7.8.2 the levels can be armed on the command line instead:

```bash
# one level, or several — repeatable and comma-separated
auth_bff --config-file='[...]' --global-trace=machine
auth_bff --config-file='[...]' --global-trace=machine,create_delete,start_stop

# what levels exist
auth_bff --global-trace=list
```

The framework applies them after it registers every gclass, and before the
first service starts, so they cover start up itself. An unknown level stops the
yuno with a message that points at `list`. The yuno does not ignore it.

To reproduce a yuno that the agent launches, take its command line from
`running-bin id=<id>` or `running-keys id=<id>`. You can also use the script
that the agent writes at
`/yuneta/realms/<realm>/<yuno>/bin/<role>^<id>.sh`. Then append the flag.

Two older methods, and their limits:

- **`kill -10 <pid>`** (SIGUSR1) cycles the global mask
  `0` → `0x00FF0000` → `0x0FFF0000` → `0xFFFF0000` → `0`. It is useful on a
  process that already runs. It does nothing for a problem during start up.
- **`--verbose-log=N` is not a trace switch.** It only overrides the *stdout*
  log handler's field bitmask (which fields each line prints). `--verbose-log=3`
  prints *fewer* fields than the config default of 255, which is why it reads as
  "it does nothing". Use `--global-trace`.

### 4.2 Two warnings that arrive without being asked for

Some failures below the framework cannot wait for a trace, so they report
themselves:

- **`getaddrinfo() BLOCKED the event loop`** (`gobj_log_warning`, msgset `OS`,
  from `yev_loop.c`) — name resolution is synchronous and runs inside the loop.
  A slow resolver therefore stops **every** gobj in the process, not only the
  socket that you open. The framework emits the warning with the host and the
  elapsed `msec` when the time is more than 1 s. If you see it, the yuno is not
  slow. It is stopped.
- **`YUNETAS static_resolv: …`** in **syslog** (`journalctl`) — the
  `CONFIG_FULLY_STATIC` resolver writes it. That resolver is below the gobj
  log and cannot reach it. Three conditions emit the message: a `nameserver`
  in `/etc/resolv.conf` that does not answer (rate limited to one message per
  nameserver per 5 min), a resolution of more than 1 s, or a failed
  allocation.

A dead first `nameserver` costs ~6 s (A + AAAA timeouts) on **every** lookup,
so a yuno that opens many channels can spend minutes in start up. The resolver
caches answers since 7.8.2, which limits the cost to the first lookup. But the
correction belongs in the node's `/etc/resolv.conf`.

---

## 5. Reading the logs

### 5.1 File paths

Per-yuno log file, built by [`yuneta_log_file()`](#yuneta_log_file) at
[`kernel/c/root-linux/src/yunetas_environment.c`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/root-linux/src/yunetas_environment.c):

```
/yuneta/logs/<yuno_role_plus_name>/<filename_mask>
```

The mask is the value that you set in
`daemon_log_handlers.<handler>.filename_mask` (see §5.4). By convention it is
`<role>-W.log`, where a rotation counter replaces the `W`.

Active log discovery:

```bash
ls -lt /yuneta/logs/<yuno>/
tail -f /yuneta/logs/<yuno>/<latest>.log | grep -a "keyword"
```

### 5.2 Log line format

Every log record is a **JSON object** built in [`glogger.c`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/gobj-c/src/glogger.c). Fields
added automatically by [`discover()`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/gobj-c/src/glogger.c#L1231) at [glogger.c:1231](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/gobj-c/src/glogger.c#L1231):

| Field             | Source                                                   |
|-------------------|----------------------------------------------------------|
| `timestamp`       | `current_timestamp()`                                    |
| `priority`        | `LOG_ERR` / `LOG_WARNING` / …                            |
| `node_uuid`       | host node identity                                       |
| [`process`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/root-linux/src/entry_point.c#L123)         | yuno binary name                                         |
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
[`entry_point.c`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/root-linux/src/entry_point.c)). The library renames the old files, and the active filename never moves.
There is no rotation by time. There is no cron. The rotation happens on the
next write that crosses the threshold.

### 5.4 Where to configure handlers

In the yuno's config JSON, under `environment.daemon_log_handlers` (or
`console_log_handlers` in non-daemon mode), parsed at
[`kernel/c/root-linux/src/entry_point.c`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/root-linux/src/entry_point.c):

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

`handler_options` is a bitmask of `LOG_HND_OPT_*` (glogger.h) that selects
which severities the handler accepts. `255` accepts all of them. If you clear
bits, the handler drops DEBUG, INFO, AUDIT and the other severities.

To add or remove handlers at run time, use the `add-log-handler` and
`del-log-handler` commands of [`c_yuno.c`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/root-linux/src/c_yuno.c).

---

## 6. The FSM trace (`machine`)

This is the most useful trace for the behavior of a gobj. It is defined in
[`glogger.c`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/gobj-c/src/glogger.c) (`trace_machine`). Called from the event dispatcher in
[`gobj.c`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/gobj-c/src/gobj.c#L7507):

- Before dispatch: a `🔜` line per event entry.
  - "Event NOT DEFINED" error: a `📛` line. This is the canonical failure in
  which the parent FSM does not declare the event of the child (see CLAUDE.md
  "CHILD vs SERVICE" section).
- After dispatch: a `🔄` line per executed event.
- State change: a `🔀🔀` line.

Two output formats, switched by the integer variable `trace_machine_format`.

**Format 1 — one line per transition. THE DEFAULT**, on a node and in the
browser alike (`gobj.c`: `trace_machine_format = 1;  // 0 legacy, 1 simpler`;
gobj-js followed at 7.13.7):

```
🔜 EV_RX_DATA !!c_tcp :open
🔄 EV_RX_DATA !!c_tcp :open from !!service_main
🔝🔝 EV_ON_MESSAGE c_prot_tcp4h^output-0 :wait_payload
🔝🔄 EV_ON_MESSAGE (EV_ON_MESSAGE) c_channel^output-0
```

Format 1 writes **no return line and no state line**: the transition line
already carries the state it ran in.

**Format 0 — legacy, three lines for one transition:**

```
🔜 mach(!!c_tcp), st: :open, ev: EV_RX_DATA, from(!!service_main)
🔄 mach(!!c_tcp), st: :open, ev: EV_RX_DATA, from(c_tcp_s^server)
🔀🔀 mach(!!c_tcp), new st(:closed), prev st(:open)
<- mach(!!c_tcp), st: :closed, ev: EV_RX_DATA, ret: 0
```

Every line of either format is indented by its nesting depth, two spaces per
level, so an event sent from inside another one's action sits under it. That
indentation is the only thing that says a transition happened **during**
another — keep it when you render the line anywhere else.

`!!` before a name means that the gobj is **not running** at that moment. Two
of them in a row are usually the bug.

### Scoping the machine trace

The machine trace of a whole yuno gives too much output on anything larger
than a toy test. You can make it narrow in two ways:

```bash
# only one gclass
ycommand -c 'command-yuno id=<yuno> service=__yuno__ command=set-gclass-trace gclass=C_TCP_S level=machine set=1'

# only one instance
ycommand -c 'command-yuno id=<yuno> service=__yuno__ command=set-gobj-trace gobj=<short_name> level=machine set=1'
```

Both are persisted and are re-applied on the next start, like every other
trace set from the control plane (see [Persistence](#persistence-of-traces)).
Disable them in the same session.

### The same trace in the browser

This chapter is about a yuno on a node. But a browser SPA runs the **same
kernel**, ported to JavaScript. Since `@yuneta/gobj-js` **7.9.5** the JS
runtime has this level model, with the same names and the same bits. A habit
that you learn here therefore transfers, and you can read two traces side by
side.

```javascript
gobj_set_global_trace("machine", true);          // the big one, same as above
gobj_set_gclass_trace("C_MY_VIEW", "machine", true);
gobj_set_gobj_no_trace(noisy_src, "machine", true);   // veto, by the SOURCE

set_log_callback((level, msg) => { ... });       // the trace arrives as `debug`
```

There is no `ycommand` on that side. The switch is the call above, and the
output goes to the browser console. It goes to any other destination that
`set_log_callback()` selects, and that is how the dev panel of gobj-ui shows
the machine inside the app.
[`doc.yuneta.io/navigation`](https://doc.yuneta.io/navigation) runs three demos
with the panel connected to that callback. Read them if you want to see the
lines before you write your own code.

---

## 7. Following a message end-to-end

Canonical request flow on a typical Yuneta service:

```
   external client
         │
         ▼
  ┌─────────────┐   gclass trace 'traffic'
  │   C_TCP_S   │   gobj_trace_dump_gbuf(gobj, gbuf, …)
  └──────┬──────┘
         │
         ▼
  ┌─────────────────┐   gclass trace 'traffic'
  │ C_PROT_HTTP_SR  │
  └────────┬────────┘
           │
           ▼
  ┌─────────────────┐   gclass trace 'ievents' / 'ievents2'
  │   C_IEVENT_SRV  │   trace_inter_event2(gobj, prefix, event, kw)
  └────────┬────────┘
           │
           ▼
  ┌─────────────────┐   global trace 'machine' lights up the FSM dispatch
  │  service gclass │   gclass-specific traces fire its custom emit points
  └────────┬────────┘
           │
           ▼
  ┌─────────────────┐   global trace 'fs'
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
  [`kernel/c/root-linux/src/msg_ievent.h`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/root-linux/src/msg_ievent.h).
- Pushed on outgoing request, popped + reversed on incoming response, at
  [`kernel/c/root-linux/src/msg_ievent.c`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/root-linux/src/msg_ievent.c).

To grep the same transaction across multiple yunos' logs:

```bash
grep -a 'ievent_gate_stack' /yuneta/logs/*/*.log | grep '<the user or src_yuno you care about>'
```

The framework propagates **no automatic UUID** for calls that are not ievents.
A direct C function call has nothing to grep. The correlation is available only
when the message crosses an ievent boundary.

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

A SPA built on the JS gobj framework can connect to a yuno. Then it can
display **live** what crosses the websocket: the same lines that you see in
the local log file, plus the bodies of the ievent messages.

### Wire-up

- C side: nothing special — inter-events flow as usual through `C_IEVENT_SRV`
  → `C_WEBSOCKET`.
- JS side: `C_IEVENT_CLI` ([`kernel/js/gobj-js/src/c_ievent_cli.js`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/js/gobj-js/src/c_ievent_cli.js)) parses
  incoming inter-events in [`ac_on_message()`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/js/gobj-js/src/c_ievent_cli.js#L1181) and, if
  configured, invokes `trace_ievent_callback(prefix, iev_msg, direction, size)`.
- The SPA installs that callback by writing the attribute:
  `gobj_write_attr(gobj_yuno(), "trace_ievent_callback", info_traffic)`
  ([`kernel/js/gobj-ui/src/yui_dev.js`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/js/gobj-ui/src/yui_dev.js)).
- The `info_traffic()` function ([yui_dev.js:29](https://github.com/artgins/yunetas/blob/7.18.1/kernel/js/gobj-ui/src/yui_dev.js#L29)) appends the message into the
  DOM container `#developer-traffic-logger`, which lives inside either:
  - the **legacy** `C_YUI_WINDOW` modal, or
  - the **modern** `build_dev_panel()` modal ([yui_dev.js:452](https://github.com/artgins/yunetas/blob/7.18.1/kernel/js/gobj-ui/src/yui_dev.js#L452)).

Both still ship. An app selects one of them from the version of its shell.

### Rendering

- Header bar: blue, with direction icon (white = sent, yellow = received,
  red = error), byte size, and `HH:MM:SS.SSSS` timestamp.
- Body: read-only vanilla-jsoneditor (`pinned ^0.23.0`, see project memory
  note about not bumping it).
- Footer: ON/OFF traffic counter.
- Auto-scroll to bottom on each message.

### Filtering on the SPA side

**The UI has no filter.** The viewer shows everything that flows through the
websocket. The only two controls are on and off: the `trace_inter_event`
boolean and the `trace_ievent_callback` itself. To filter, change what the
yuno emits with the `ycommand` controls from §4.

### Teardown order — the recursion gotcha

When the websocket closes, `ac_on_close` ([`c_ievent_cli.js:897`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/js/gobj-js/src/c_ievent_cli.js#L897)) fires
`EV_ON_CLOSE`. `set_remote_log_functions` redirects the JS `log_error` and
`log_warning` calls to the DOM callback. If it is still installed, the callback
captures the warning that the teardown path emits. The callback changes the
DOM, the change can fire more events, and those events log again. The result is
an infinite recursion.

The correction at [`c_ievent_cli.js`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/js/gobj-js/src/c_ievent_cli.js) is to call
`set_remote_log_functions(null)` **before** anything publishes `EV_ON_CLOSE`.
That call clears the hooks and resets them to the console (see
[`helpers.js`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/js/gobj-js/src/helpers.js)). The memory note "Remote-log unwire order" records the
incident.

---

## 9. The logcenter yuno

`yunos/c/logcenter/` collects the logs that every yuno on the host, or on the
LAN, ships over UDP. It is **not enabled by default**. A yuno ships to UDP
only if its config lists a `udp` handler.

### How it listens

- UDP server (`c_gss_udp_s`) on `udp://127.0.0.1:1992` by default
  ([`c_logcenter.c`](https://github.com/artgins/yunetas/blob/7.18.1/yunos/c/logcenter/src/c_logcenter.c)).
- Wire format: `<priority-digit><8hex-seq><json-payload><8hex-crc>`,
  fragmented per `udp_frame_size` (default 1500, [`log_udp_handler.c`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/gobj-c/src/log_udp_handler.c)).

### What it does on receipt

In [`c_logcenter.c`](https://github.com/artgins/yunetas/blob/7.18.1/yunos/c/logcenter/src/c_logcenter.c):

- [`ac_on_message()`](https://github.com/artgins/yunetas/blob/7.18.1/yunos/c/logcenter/src/c_logcenter.c#L1324) parses each packet.
- Writes the JSON record to its own rotatory file `W.log` via
  [`write2logs()`](https://github.com/artgins/yunetas/blob/7.18.1/yunos/c/logcenter/src/c_logcenter.c#L848) / [`_log_bf()`](#_log_bf).
  Default size cap **600 MB** (`max_rotatoryfile_size`, in megabytes).
- Updates in-memory counters per severity / `msgset` / `msg`
  ([`do_log_stats()`](https://github.com/artgins/yunetas/blob/7.18.1/yunos/c/logcenter/src/c_logcenter.c#L858), [c_logcenter.c:858](https://github.com/artgins/yunetas/blob/7.18.1/yunos/c/logcenter/src/c_logcenter.c#L858)).

### What it exposes

Commands ([`c_logcenter.c`](https://github.com/artgins/yunetas/blob/7.18.1/yunos/c/logcenter/src/c_logcenter.c)):

| Command           | Effect                                                                  |
|-------------------|-------------------------------------------------------------------------|
| `display-summary` | Print the in-memory counters: alerts, criticals, errors, warnings.      |
| `send-summary`    | Email the same summary (used as a daily/weekly batch).                  |
| `search`          | Search the stored log file for matching lines.                          |
| `tail`            | Last N lines of the centralized log.                                    |
| `reset-counters`  | Zero the in-memory counters.                                            |

Use it like any other yuno. Target it by `yuno_role=logcenter`, because the
numeric `id` of the yuno changes with the realm. `command-yuno` implies the
default service:

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

Three more commands are useful ([`c_logcenter.c`](https://github.com/artgins/yunetas/blob/7.18.1/yunos/c/logcenter/src/c_logcenter.c)):
`send-summary`, `enable-send-summary` and `disable-send-summary` control the
email rollup. `restart-yuneta-on-queue-alarm` is the auto-recovery hook for a
UDP queue that floods.

### Per-yuno vs centralized — when to use each

- **Per-yuno tail** when you know which yuno is misbehaving and want raw
  control over `grep`. `/yuneta/logs/<yuno>/<file>.log` is full fidelity.
- **logcenter** when you need correlation across multiple yunos, or when
  a yuno crashes too fast to read its own file, or for the rollup
  counters / email summaries.

Both can run at the same time. The file handler writes locally, and the UDP
handler ships to logcenter in parallel. They are not exclusive.

---

## 10. Sharp edges

### 10.1 Traces accumulate — disable them when you finish

A forgotten `set-global-trace level=machine set=1` survives a restart
(see §4 *Persistence*). The logs then grow without limit. Always pair the
enable and the disable in the same operational session. Before you leave, make
sure that the state is correct with `get-global-trace`.

### 10.2 Persistence asymmetry

| Scope         | Persists across restart?       |
|---------------|--------------------------------|
| `global`      | **Yes** (via `trace_levels` attr) |
| `gclass`      | No                             |
| `gobj`        | No                             |
| `no_trace`    | No (all flavours)              |

If you persisted a global level by mistake, clear it explicitly with
`set-global-trace level=<name> set=0`. To delete the file does not help,
because the value is in the treedb config of the yuno.

### 10.3 `ievent_gate_stack` is only on inter-event hops

A direct C function call between gobjs in the same yuno does **not** carry
the stack, because there is no metadata to attach. The correlation exists only
across yuno boundaries. Plan your traces for this limit.

### 10.4 `LOG_AUDIT` lines have no standard header

[`glogger.c`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/gobj-c/src/glogger.c) writes the audit lines raw. A line filter that expects
the timestamp prefix misses them. When you look for operator actions, read the
audit file directly.

### 10.5 UDP can drop

UDP logs are not reliable. Under a burst, for example a machine trace that is
fully on, the kernel buffer can overflow, and logcenter loses lines. It gives
no message. The local file handler drops nothing, so trust the local file when
you are not sure.

### 10.6 `ev_kw` is enormous

`set-global-trace level=ev_kw set=1` writes the full `kw` JSON payload of every
event to the log. It is useful on a single-shot test. It is ruinous on a busy
service. If you need it narrowly, combine it with a `machine` trace that is
scoped to one gclass.

### 10.7 SPA dev-panel teardown order

`set_remote_log_functions(null)` MUST come before
[`do_disconnect`](https://github.com/artgins/yunetas/blob/7.18.1/modules/c/mqtt/src/c_prot_mqtt.c#L1580) / `destroy_shell`. See §8 and memory
`feedback_remote_log_unwire_order`.

### 10.8 Deep tracing has no `ycommand` switch

[`gobj_set_deep_tracing()`](#gobj_set_deep_tracing) is available only in C (gobj.c). If a yuno generates
traces that you cannot configure, look for a `gobj_set_deep_tracing` call that
someone left in its `mt_create`.

---

## 11. Operational recipes

### 11.1 Watch what a yuno does

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

Enable logcenter. Add this to the config JSON of every yuno:

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

See §7. The set of commands is `set-gclass-trace ... traffic`,
`set-global-trace ... machine`, `set-global-trace ... fs` and
`set-gclass-trace C_IEVENT_SRV ... ievents2`. Do not forget to disable
everything afterwards.

### 11.5 Capture an FSM bug in one gobj only

```bash
ycommand -c "command-yuno id=$YUNO service=__yuno__ command=set-gobj-trace gobj=<short_name> level=machine set=1"
ycommand -c "command-yuno id=$YUNO service=__yuno__ command=set-gobj-trace gobj=<short_name> level=ev_kw   set=1"
tail -F /yuneta/logs/$YUNO/*.log | grep -a '<short_name>'
# … done …
ycommand -c "command-yuno id=$YUNO service=__yuno__ command=set-gobj-trace gobj=<short_name> level=machine set=0"
ycommand -c "command-yuno id=$YUNO service=__yuno__ command=set-gobj-trace gobj=<short_name> level=ev_kw   set=0"
```

The disable step is not optional. These traces are **persisted**: a restart
does not clear them, it re-applies them. A forgotten `ev_kw` on a busy gobj
then writes for as long as the yuno lives.

### 11.6 Spot the canonical "Event NOT DEFINED in state" error

That single string is the most common FSM failure. The parent FSM did not
declare an event that a child publishes (see CLAUDE.md "CHILD vs SERVICE").
The framework logs it at `LOG_ERR`, and the trace settings do not change that,
so:

```bash
grep -a '"msg":"Event NOT DEFINED in state"' /yuneta/logs/*/*.log
```

This command works on any host, and it needs no trace.

---

## 12. Code pointers

| What                                          | Where                                                                  |
|-----------------------------------------------|------------------------------------------------------------------------|
| Severity log API                              | [`kernel/c/gobj-c/src/glogger.c`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/gobj-c/src/glogger.c)                                |
| `LOG_AUDIT` / `LOG_MONITOR`                   | [`kernel/c/gobj-c/src/glogger.c`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/gobj-c/src/glogger.c)                                  |
| Trace emit API (`gobj_trace_msg/json/dump`)   | [`kernel/c/gobj-c/src/glogger.c:778`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/gobj-c/src/glogger.c#L778)                          |
| Global trace level table                      | [`kernel/c/gobj-c/src/gobj.c`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/gobj-c/src/gobj.c)                                   |
| Per-gclass trace declaration (example)        | [`kernel/c/root-linux/src/c_tcp_s.c`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/root-linux/src/c_tcp_s.c)                             |
| Trace mask lookup                             | [`kernel/c/gobj-c/src/gobj.c`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/gobj-c/src/gobj.c) ([`gobj_trace_level`](#gobj_trace_level))          |
| Per-gobj trace API                            | [`kernel/c/gobj-c/src/gobj.c`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/gobj-c/src/gobj.c) (`gobj_set_gobj_trace`)             |
| `no_trace` API                                | [`kernel/c/gobj-c/src/gobj.c`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/gobj-c/src/gobj.c)                       |
| Deep trace                                    | [`kernel/c/gobj-c/src/gobj.c`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/gobj-c/src/gobj.c)                               |
| `trace_machine` print                         | [`kernel/c/gobj-c/src/glogger.c:1161`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/gobj-c/src/glogger.c#L1161)                                   |
| FSM dispatch trace sites                      | [`kernel/c/gobj-c/src/gobj.c`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/gobj-c/src/gobj.c)                           |
| Trace persistence (`trace_levels` attr)       | [`kernel/c/root-linux/src/c_yuno.c`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/root-linux/src/c_yuno.c)                           |
| Trace commands exposed by every yuno          | [`kernel/c/root-linux/src/c_yuno.c`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/root-linux/src/c_yuno.c)                             |
| `daemon_log_handlers` parser                  | [`kernel/c/root-linux/src/entry_point.c`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/root-linux/src/entry_point.c)                        |
| Log file path builder                         | [`kernel/c/root-linux/src/yunetas_environment.c`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/root-linux/src/yunetas_environment.c)                    |
| Log line `discover()` (metadata fields)       | [`kernel/c/gobj-c/src/glogger.c:1234`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/gobj-c/src/glogger.c#L1234)                              |
| UDP wire format                               | [`kernel/c/gobj-c/src/log_udp_handler.c`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/gobj-c/src/log_udp_handler.c)                        |
| `ievent_gate_stack` constant                  | [`kernel/c/root-linux/src/msg_ievent.h`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/root-linux/src/msg_ievent.h)                                  |
| `ievent_gate_stack` push/pop                  | [`kernel/c/root-linux/src/msg_ievent.c`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/c/root-linux/src/msg_ievent.c)                             |
| logcenter listener                            | [`yunos/c/logcenter/src/c_logcenter.c`](https://github.com/artgins/yunetas/blob/7.18.1/yunos/c/logcenter/src/c_logcenter.c)                     |
| logcenter commands                            | [`yunos/c/logcenter/src/c_logcenter.c`](https://github.com/artgins/yunetas/blob/7.18.1/yunos/c/logcenter/src/c_logcenter.c)                           |
| SPA dev-panel renderer                        | [`kernel/js/gobj-ui/src/yui_dev.js`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/js/gobj-ui/src/yui_dev.js)                         |
| SPA inter-event callback hook                 | [`kernel/js/gobj-js/src/c_ievent_cli.js`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/js/gobj-js/src/c_ievent_cli.js)                     |
| SPA teardown order                            | [`kernel/js/gobj-js/src/c_ievent_cli.js`](https://github.com/artgins/yunetas/blob/7.18.1/kernel/js/gobj-js/src/c_ievent_cli.js)                       |
