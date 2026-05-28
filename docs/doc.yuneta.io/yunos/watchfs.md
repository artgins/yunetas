(yuno-watchfs)=
# `watchfs`

Directory watcher. It watches a path with the `C_FS` gclass and runs a shell
command whenever a matching file changes. Handy for auto-rebuild / auto-reload
loops during development.

## Architecture

```
C_WATCHFS
    C_FS      <- filesystem watch (emits EV_FS_CHANGED / EV_FS_RENAMED)
    C_TIMER
```

`C_FS` watches `path` (optionally the whole subtree) and emits `EV_FS_CHANGED`
and `EV_FS_RENAMED`. `C_WATCHFS` filters the event's filename against the
`patterns` (a list of POSIX regular expressions, compiled with `regcomp`) and,
on a match, runs `command`.

## Configuration

| Attribute | Default | Purpose |
|-----------|---------|---------|
| `path` | — | Directory to watch |
| `recursive` | `false` | Watch the whole directory tree |
| `patterns` | — | List of POSIX regex; only matching filenames trigger `command` |
| `command` | — | Shell command to run on a matching event |
| `use_parameter` | `false` | Append the changed filename as an argument to `command` |
| `ignore_changed_event` | `false` | Ignore `EV_FS_CHANGED` |
| `ignore_renamed_event` | `false` | Ignore `EV_FS_RENAMED` |
| `info` | `false` | Log discovered subdirectories |

## Commands

| Command | Description |
|---------|-------------|
| `help` | Command help |

## Debugging

| GClass | Level | Shows |
|--------|-------|-------|
| `C_WATCHFS` | `trace_user` | Watch/match/exec activity |

Enable with
`ycommand command-yuno id=<id> service=__yuno__ command=set-gclass-trace gclass=C_WATCHFS set=1 level=trace_user`.
