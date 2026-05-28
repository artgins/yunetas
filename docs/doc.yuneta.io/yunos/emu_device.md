(yuno-emu_device)=
# `emu_device`

Device-gate emulator — intended to feed an ingest endpoint with device frames
at a controlled rate, so device-facing GClasses and ingest pipelines can be
tested without real hardware.

```{warning}
The frame-emission path (and the timeranger2 replay source) is **commented out
in the current source** (`c_emu_device.c`). The yuno builds and accepts its
configuration/commands, but it does **not** currently transmit frames — treat
it as a scaffold / work-in-progress, not a working generator.
```

## Intended model

```
C_EMU_DEVICE
    __output_side__ : C_IOGATE > C_CHANNEL > C_PROT_RAW > C_TCP   (connects to `url`)
    C_TIMER
```

On each `interval` tick the emulator is meant to push `window` frames out
through `__output_side__` (a raw-TCP client connecting to `url`, the device
gate / ingest endpoint). Frames would be sourced from a TimeRanger2 topic
selected by the `path` / `database` / `topic` attributes and filtered by time,
rowid, key and user/system flags.

## Configuration

| Attribute | Default | Purpose |
|-----------|---------|---------|
| `url` | — | `__output_side__` target (device gate to feed) |
| `window` | `1` | Frames to send per interval |
| `interval` | `1000` | Interval between bursts (ms) |
| `leading` | — | Leading data prepended to frames |
| `path` / `database` / `topic` | — | TimeRanger2 replay source |
| `from_t` / `to_t` / `from_rowid` / `to_rowid` | — | Time / rowid range of the replay |
| `key` / `notkey` | — | Key filters |
| `user_flag_mask_set` / `user_flag_mask_notset` | — | User-flag filters |
| `system_flag_mask_set` / `system_flag_mask_notset` | — | System-flag filters |
| `use_very_first` | — | Start from the very first record |
| `timeout` | `2000` | Periodic tick (ms) |

## Commands

| Command | Parameters | Description |
|---------|------------|-------------|
| `read-parameters` | — | View current parameters |
| `write-window` | `window` | Set frames-per-interval |
| `write-interval` | `interval` | Set interval (ms) |
| `help` | — | Command help |

## Debugging

| GClass | Level | Shows |
|--------|-------|-------|
| `C_EMU_DEVICE` | `info` | User-level information |
