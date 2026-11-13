(util-ycli)=
# `ycli`

`ycli` is the **full-screen ncurses sibling of [`ycommand`](ycommand.md)**: the
same control plane (connect to a yuno, send commands, read responses), but in a
windowed terminal UI built for live debugging and browsing rather than
scripting. Use `ycli` when you want to keep several views open and watch a
system interactively. Use [`ycommand`](#util-ycommand) when you want a single command or a
scriptable pipe.

(The source directory was `yunos/c/yuno_cli` before 7.4.x. The binary has always
been `ycli`.)

## Usage

```bash
ycli [--url ws://host:port]
```

Navigate with the arrow keys. Press `?` inside the UI for help. The connection
and authentication options match [`ycommand`](ycommand.md) (default
`ws://127.0.0.1:1991`. OAuth2 / OIDC flags for the remote / TLS path — see
[Authentication](#ycommand-auth)).

## What it adds over `ycommand`

- **Multi-session / multi-window** — open several connections and views at one time.
  the active window decides where a command is sent.
- **Shared line editor** — `ycli` and `ycommand` both use the `C_EDITLINE`
  gclass, so line editing, history, `Ctrl+R` / `Ctrl+S` incremental search
  (`ESC` / `Ctrl+G` to cancel) and TAB completion behave identically in both.
- **Popup completion** — the multi-candidate TAB list is drawn as an ncurses
  popup above the editline: `Tab` / `Up` / `Down` navigate, `Enter` commits the
  candidate to the line without submitting, `ESC` / `Ctrl+G` cancel.
- **Per-connection command cache** — the remote command set is warmed on connect
  via `list-gobj-commands`, so completions follow whichever window has focus.
- **Local vs remote routing** — commands prefixed with `!` run locally (in
  `ycli` itself). Everything else goes to the focused remote yuno.
- **Safe local config filenames (since 7.6.0)** — like `ycommand`, a config
  answer (`view-config` / `read-json` / `read-file` / `edit-config`) saved under
  `~/.yuneta/configs/` has its peer-supplied record name sanitized to a single
  inert basename (`[A-Za-z0-9._-]`, no leading dot, separators folded) before it
  reaches the editor command — closing an RCE via `/bin/sh -c`.
- **Only the answers it asked for** — as in `ycommand`: the agent answers its
  own requests (`read-file`, `read-json`, `read-binary-file`, `running-keys`,
  `running-bin`, `edit-config`, `view-config`, `edit-yuno-config`,
  `view-yuno-config`) with an event of its own (`EV_READ_FILE`,
  `EV_EDIT_CONFIG`, ...), and `ycli` writes it under `~/.yuneta/configs/`,
  opens the editor on it, or leaves a `create-config` on the line. It takes
  one only while a request of those is pending ON THAT CONNECTION, one answer
  per request; a line counts as a request when the request is its COMMAND:
  its first word, or, of a `command-agent`, the first word of its `cmd2agent`,
  so `command-agent cmd2agent='read-file filename=/etc/hosts'` through a
  control center counts too, and `help cmd=read-file` does not (up to 7.30.26
  a name anywhere in the line counted: its answer never spent the request, and
  the next answer nobody asked was taken). Any other is ignored with a
  WARNING in the log:

  ```text
  "msg": "answer of a request this ycli did not make, ignored", "event": "EV_READ_BINARY_FILE", "connection": "C_IEVENT_CLI^ws://127.0.0.1:1991"
  ```

  A request refused at once (an `EV_MT_COMMAND_ANSWER` with an error, such as
  *"Command sent to 0 nodes"*) leaves nothing pending, and the end of a
  connection clears what was pending on it. Up to 7.30.26 `ycli` took any
  that came from its peers.
- **`read-binary-file` writes data** — the file is written `0600`, not
  executable (`0700` up to 7.30.26).
  A link left in `~/.yuneta/configs/` under the file's name is not followed:
  the write fails, with an ERROR *"Cannot write the binary file"* naming the
  path (up to 7.30.26 it wrote through the link, and a file it could not open
  was taken for written, with nothing said):

  ```bash
  ln -s ~/.bashrc ~/.yuneta/configs/yuneta_agent
  # then, typed in ycli: read-binary-file filename=/yuneta/bin/yuneta_agent
  # -> ERROR "Cannot write the binary file", ~/.bashrc untouched
  ```

  A `content64` that is not base64, or not a string, writes nothing, and is a
  WARNING (*"base64 of a peer not decodable, refused"*, *"content64 of a
  binary file that is not a string, not written"*); the status line says
  *"Binary file NOT saved"*. Up to 7.30.26 the first was an ERROR with a
  stack, the second said nothing, and both said *"Binary file save in"*.
- **Text and json answers are written the same way** — `read-file` (text)
  and `read-json`, `view-config`, `edit-config`, `view-yuno-config`,
  `edit-yuno-config` (json) are written
  `0600`, never through a link, and a failure is an ERROR *"Cannot write the
  file"* naming the path, answered *"File NOT saved in '<path>'"*. A failed
  write does NOT open the editor: up to 7.30.26 the file followed a link,
  took its mode from the umask, and a failed write opened the editor on what
  the file held before (`edit-config` then offered it as a `create-config`).
  A text that is not a string is a WARNING, and nothing is written:

  ```bash
  ln -s ~/notes.json ~/.yuneta/configs/my_yuno.json
  # then, typed in ycli: edit-config id=my_yuno
  # -> ERROR "Cannot write the file", ~/notes.json untouched, no editor
  ```

## Key bindings

In addition to the [shared editline shortcuts](#ycommand-shortcuts):

| Shortcut | Action |
|----------|--------|
| `Tab` / `Up` / `Down` | Navigate the completion popup. `Enter` commits the candidate |
| `ESC` / `Ctrl+G` | Cancel the popup or incremental search |
| `Ctrl+K` | Delete from cursor to end of line (readline) |
| `Ctrl+U` / `Ctrl+Y` | Clear the whole line |
| `Ctrl+L` | Clear the display pane |

## Window gclasses

`ycli` is itself a yuno. Its TUI is composed from a small set of window
gclasses driven by `C_CLI`:

| GClass | Purpose |
|--------|---------|
| `C_CLI` | Main CLI controller |
| `C_EDITLINE` | Line editor (from the console module) |
| `C_WN_STDSCR` | Root ncurses screen |
| `C_WN_LAYOUT` | Window layout manager |
| `C_WN_BOX` | Box container |
| `C_WN_LIST` | Scrollable list |
| `C_WN_STATIC` | Static text display |
| `C_WN_STSLINE` | Status line |
| `C_WN_TOOLBAR` | Toolbar |

## See also

- [`ycommand`](ycommand.md) — the line-oriented control plane (single command,
  interactive shell, or stdin pipe).
- Repository README (for code navigators):
  [`utils/c/ycli/README.md`](https://github.com/artgins/yunetas/blob/7.31.0/utils/c/ycli/README.md).
