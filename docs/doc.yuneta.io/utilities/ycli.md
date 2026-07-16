(util-ycli)=
# `ycli`

`ycli` is the **full-screen ncurses sibling of [`ycommand`](ycommand.md)**: the
same control plane (connect to a yuno, send commands, read responses), but in a
windowed terminal UI built for live debugging and browsing rather than
scripting. Use `ycli` when you want to keep several views open and watch a
system interactively; use [`ycommand`](#util-ycommand) when you want a single command or a
scriptable pipe.

(The source directory was `yunos/c/yuno_cli` before 7.4.x; the binary has always
been `ycli`.)

## Usage

```bash
ycli [--url ws://host:port]
```

Navigate with the arrow keys; press `?` inside the UI for help. The connection
and authentication options match [`ycommand`](ycommand.md) (default
`ws://127.0.0.1:1991`; OAuth2 / OIDC flags for the remote / TLS path — see
[Authentication](#ycommand-auth)).

## What it adds over `ycommand`

- **Multi-session / multi-window** — open several connections and views at once;
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
  `ycli` itself); everything else goes to the focused remote yuno.
- **Safe local config filenames (since 7.6.0)** — like `ycommand`, a config
  answer (`view-config` / `read-json` / `read-file` / `edit-config`) saved under
  `~/.yuneta/configs/` has its peer-supplied record name sanitized to a single
  inert basename (`[A-Za-z0-9._-]`, no leading dot, separators folded) before it
  reaches the editor command — closing an RCE via `/bin/sh -c`.

## Key bindings

In addition to the [shared editline shortcuts](#ycommand-shortcuts):

| Shortcut | Action |
|----------|--------|
| `Tab` / `Up` / `Down` | Navigate the completion popup; `Enter` commits the candidate |
| `ESC` / `Ctrl+G` | Cancel the popup or incremental search |
| `Ctrl+K` | Delete from cursor to end of line (readline) |
| `Ctrl+U` / `Ctrl+Y` | Clear the whole line |
| `Ctrl+L` | Clear the display pane |

## Window gclasses

`ycli` is itself a yuno; its TUI is composed from a small set of window
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
  [`utils/c/ycli/README.md`](https://github.com/artgins/yunetas/blob/7.8.0/utils/c/ycli/README.md).
