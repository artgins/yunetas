(yuno-watchfs)=
# `watchfs`

Directory watcher. It watches a path with the [`C_FS`](#gclass-c-fs) gclass and runs a shell
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
| `patterns` | — | List of POSIX regex. Only matching filenames trigger `command` |
| `command` | — | Shell command to run on a matching event |
| `use_parameter` | `false` | Append the path of the changed file, quoted, as an argument to `command` |
| `ignore_changed_event` | `false` | Ignore `EV_FS_CHANGED` |
| `ignore_renamed_event` | `false` | Ignore `EV_FS_RENAMED` |
| `info` | `false` | Log discovered subdirectories |

The command runs with `/bin/sh -c`, through `run_process2()`: no signal of
the yuno blocked, no fd of the yuno inherited. With `use_parameter` the
command and the quoted path of the file must fit in 16 KB (the path in
`PATH_MAX`): one that does not fit is not run, and an ERROR says so
(*"Command with the file too long: not run"*). Up to 7.29.3 it was cut at
4 KB with nothing said, and the cut command ran. For example, with:

```json
{
    "path": "/yuneta/development/docs",
    "patterns": ".*\\.md",
    "command": "make html",
    "use_parameter": true
}
```

a change of a `.md` file runs `make html '<path>/<file>'`. The changes of a
burst run the command once, 200-500 ms after the last one, with the last
file that matched. Up to 7.29.3 the command was given no file: it got
`'/'`.

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
