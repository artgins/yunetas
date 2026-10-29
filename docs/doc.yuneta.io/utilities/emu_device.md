(util-emu_device)=
# `emu_device`

Device-gate emulator — replays recorded device frames to an ingest endpoint at
a controlled rate, so device-facing GClasses and ingest pipelines can be tested
without real hardware.

A standalone utility yuno (built under `utils/c/emu_device`, installed to
`/yuneta/bin`). Run it from the CLI, not deployed as a managed service.

## How it works

```
C_EMU_DEVICE
    __output_side__ : C_IOGATE > C_CHANNEL > C_PROT_RAW > C_TCP   (connects to `url`)
    C_TIMER
```

- On **play**, it starts a TimeRanger2 reader and loads every matching record
  from the `path` / `database` / `topic` topic (filtered by `from_t`/`to_t`,
  `from_rowid`/`to_rowid`, the flag masks and `key`/`notkey`. All keys by
  default) into memory.
- When the output side **connects** (`url`, a raw-TCP client), it first sends
  the `leading` frame (if set), then emits **`window` frames every `interval`
  milliseconds**. Each frame is the base64 `frame64` field of a record,
  decoded to bytes and sent as a raw message.
- When the loaded frames are exhausted it stops the periodic emission (and, run
  standalone from the CLI, exits).

```{note}
The replay loads all matching records into memory up front (`tranger2_open_list`),
so point it at a bounded range when replaying a large topic.
```

## Configuration

| Attribute | Default | Purpose |
|-----------|---------|---------|
| `url` | — | `__output_side__` target (device gate to feed) |
| `window` | `1` | Frames to send per interval |
| `interval` | `1000` | Interval between bursts (ms) |
| `leading` | — | Base64 frame sent once on connect |
| `path` / `database` / `topic` | — | TimeRanger2 replay source (records with a `frame64` field) |
| `from_t` / `to_t` / `from_rowid` / `to_rowid` | — | Time / rowid range of the replay. A time is read as [`tranger2_time_of_text()`](<#tranger2_time_of_text>) reads a time bound, and passed to the tranger as text, which takes a date to the unit of the topic's axis: `"from_t": "2026-10-01T00:00:00+0000"`, `"from_t": "-10"` (the last 10 units of the topic's `t` axis of each key: 10 seconds, or 10 milliseconds on an `sf_t_ms` topic). A number of another shape (`"007"`, `"1,5"`) or a text that is no date stops the replay with the ERROR *"time bound refused"* (`why`, `bound`, `value`). Up to 7.29.3 `"007"` was 7, `"1,5"` or `"garbage"` selected nothing in silence, and a date stayed in seconds on a topic in milliseconds |
| `key` / `notkey` | — | Key filters (default: all keys) |
| `user_flag_mask_set` / `user_flag_mask_notset` | — | User-flag filters |
| `system_flag_mask_set` / `system_flag_mask_notset` | — | System-flag filters |
| `timeout` | `2000` | Timer tick (ms) |

## Commands

| Command | Parameters | Description |
|---------|------------|-------------|
| `read-parameters` | — | Window, interval, frames sent / loaded |
| `write-window` | `window` | Set frames-per-interval |
| `write-interval` | `interval` | Set interval (ms) |
| `help` | — | Command help |

## Debugging

| GClass | Level | Shows |
|--------|-------|-------|
| `C_EMU_DEVICE` | `info` | Connect / send / finish activity |
