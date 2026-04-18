# ycommand

Control-plane CLI for Yuneta services. Sends commands and stats requests to a
running yuno over its local socket or a remote WebSocket URL. Supports
single-command mode, an interactive shell and batch scripting.

## Usage

```bash
ycommand -c '<command>'                     # single command on the default yuno
ycommand <command> <args...>                # same as -c, positional form
ycommand --url ws://host:port -c 'help'     # remote yuno
ycommand -i                                 # interactive shell
cat script.ycmd | ycommand -u ws://...      # batch from stdin
```

Authentication options: `--jwt`, `--user_id`/`--user_passw`, or a config file.
Run `ycommand --help` for all flags.

## Command line syntax

| Form                          | Meaning                                                         |
| ----------------------------- | --------------------------------------------------------------- |
| `cmd arg=val ...`             | Send to the yuno's default service                              |
| `cmd ... service=__yuno__`    | Send to the yuno itself                                         |
| `cmd ... service=<name>`      | Send to a named service inside the yuno                         |
| `*cmd ...`                    | Force response display in form (raw JSON) mode                  |
| `!cmd`                        | Run a **local** ycommand command (see `!help`)                  |
| `!!`                          | Re-run the last command (bash-style history expansion)          |
| `!N`                          | Re-run history entry N (1-based, matches `!history` output)     |
| `cmd1 ; cmd2 ; ...`           | Chain; each waits for the previous response                     |
| `-cmd`                        | Ignore errors for this command (`ybatch` convention)            |
| `#` at start of line          | Comment (ignored by `!source` / stdin piping)                   |

The `;` split is quote/brace aware: `;` inside `"..."`, `'...'` or `{...}` is
treated as literal, so JSON-valued parameters like `kw={"a":1;b:2}` pass
through untouched.

## Interactive shortcuts

| Shortcut                    | Action                                                      |
| --------------------------- | ----------------------------------------------------------- |
| `TAB`                       | Complete command or parameter; list candidates if ambiguous |
| `TAB` after `param=`        | Complete values for boolean params (`true` / `false`)       |
| `Ctrl+R` / `Ctrl+S`         | Reverse / forward incremental history search                |
| `Up` / `Down`               | Previous / next history entry                               |
| `Ctrl+A` / `Ctrl+E`         | Start / end of line                                         |
| `Ctrl+B` / `Ctrl+F`         | Move left / right one character                             |
| `Ctrl+D` / `Backspace`      | Delete char under / before cursor                           |
| `Ctrl+K`                    | Delete from cursor to end of line (readline)                |
| `Ctrl+U` / `Ctrl+Y`         | Delete whole line                                           |
| `Ctrl+W`                    | Delete previous word                                        |
| `Ctrl+T`                    | Swap character with previous                                |
| `Ctrl+L`                    | Clear screen                                                |

While in `Ctrl+R`/`Ctrl+S` search mode, the prompt changes to
`(reverse-i-search)'pat': <match>`; `Enter` accepts and submits, an arrow key
accepts and keeps editing, `ESC` or `Ctrl+G` cancel.

## Local commands (invoked with `!`)

Run `!help` (alias `!h` or `!?`) inside an interactive session for the full
list. Short aliases follow the `c_cli` convention:

| Command           | Aliases | Description                                  |
| ----------------- | ------- | -------------------------------------------- |
| `!help`           | `!h`, `!?` | List all shortcuts and local commands     |
| `!history`        | `!hi`   | Show the command history with 1-based indices (`!N`) |
| `!clear-history`  | `!clh`  | Erase history                                 |
| `!exit` / `!quit` | `!x`, `!q` | Leave ycommand                             |
| `!source <file>`  | `!. <file>` | Read commands from `<file>` and splice them at the head of the queue |

## Batch / scripting

ycommand can run a list of commands sequentially, each waiting for the
previous response — similar to `ybatch` but with the line-oriented ycommand
syntax instead of JSON.

- `cmd1 ; cmd2 ; cmd3` on a single line.
- `!source script.ycmd` inside an interactive session.
- `cat script.ycmd | ycommand -u ws://...` from a shell pipe.
- Any command may be prefixed with `-` to keep the batch going on error
  (`ybatch` convention). Without the `-`, the first error drops the queue.

Script file format: one command per line, `#` comments and blank lines
ignored, each line may itself contain `;`-chained commands.

## Other features

- **did-you-mean**: on a `command not available` error, ycommand compares the
  typed command name against the in-memory cache (fetched via
  `list-gobj-commands` at connect) and suggests the closest match when it is
  within an edit-distance threshold.

- **Informative prompt**: after connecting, the prompt reflects the remote
  yuno (`yuneta_agent^<name>> `).

- **Inline parameter hints**: after typing `<cmd> `, the remaining parameters
  appear in gray to the right of the cursor with required params as
  `<name=type>` and optional as `[name=type]`. Already-typed params drop from
  the hint.

- **Output rendering**: schema-bearing responses render as aligned tables
  with bold-cyan headers in both interactive and non-interactive modes. Use
  the `*cmd` prefix to force raw-JSON (form) mode instead.

- **History dedup**: consecutive and prior occurrences of the same command
  are removed from history (bash `HISTCONTROL=erasedups` style).

See [TODO.md](TODO.md) for planned features.
