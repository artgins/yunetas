(util-ycommand)=
# `ycommand`

`ycommand` is the **primary control-plane tool** of Yuneta. It connects to a
running yuno — over the agent's local socket or a remote WebSocket — and sends
it commands and stats requests, printing the response. Almost every operational
task in this documentation (deploying binaries, inspecting state, toggling
traces, managing realms) is driven through `ycommand`.

It is one of a small family of control-plane clients:

- **`ycommand`** — send commands / read responses (this page).
- [`ybatch`](ybatch.md) — run a file of commands unattended.
- [`ystats`](ystats.md) — subscribe to live statistics.
- [`ycli`](ycli.md) — the full-screen ncurses sibling for interactive browsing.

## Usage

```bash
ycommand -c '<command>'                     # single command on the default yuno
ycommand <command> <args...>                # same as -c, positional form
ycommand --url ws://host:port -c 'help'     # remote yuno
ycommand -i                                 # interactive shell (raw TTY)
cat script.ycmd | ycommand -u ws://...      # long-lived stdin-pipe session
```

The **stdin-pipe** mode kicks in automatically when neither `-c` nor `-i` is
passed and stdin is not a TTY. Lines are read event-driven (one `dup`'d
io_uring read per line), so the session stays alive between commands — a shell
coproc, a `{ echo cmd1; sleep 5; echo cmd2; }` block, or any programmatic driver
can stream commands into the same authenticated session and pay the OAuth2
round-trip only once. EOF on stdin triggers an orderly shutdown.

Run `ycommand --help` for the full flag list.

## Choosing a mode

| Mode | When to use | Trade-off |
|------|-------------|-----------|
| `ycommand -c '<cmd>'` | A single command inside a shell script or one-liner | Pays connect + auth on every invocation |
| `ycommand -i` | Interactive exploration: prompt, TAB, history, `Ctrl+R`, did-you-mean | Requires a TTY; not scriptable |
| `... \| ycommand` (stdin) | A known sequence of N commands without needing to read a response first | No feedback loop — every line is sent regardless of prior output; connect + auth paid once |

(ycommand-auth)=
## Authentication (OAuth2 / OIDC)

The default `ws://127.0.0.1:1991` is the agent's plain local control port and
needs no credentials. The remote / TLS path (typically `wss://host:1993`) is
gated by an OAuth2 JWT, supplied one of two ways:

- **Pre-obtained token** — pass it directly with `--jwt`.
- **Fetch at connect** — pass `--user_id` + `--user_passw` together with an IdP
  (`--issuer` or `--token-endpoint`) and `--client-id`; `ycommand` obtains the
  JWT itself before opening the WebSocket.

When an IdP and a `--user_id` are present, `ycommand` runs a one-shot
token-fetch task at connect time: it resolves the token endpoint (via OIDC
discovery from `--issuer`, or taken verbatim from `--token-endpoint`), exchanges
`user_id` / `user_passw` / `client_id` for a JWT (resource-owner password
grant), then opens the WebSocket carrying that JWT in the handshake. In
stdin-pipe mode this round-trip happens **once** and the resulting session
serves every piped command.

| Flag | Short | Purpose |
|------|-------|---------|
| `--issuer=<url>` | `-I` | OIDC issuer URL; triggers discovery of the token / end-session endpoints |
| `--token-endpoint=<url>` | `-T` | Explicit token endpoint; skips discovery (set with `--end-session-endpoint`) |
| `--end-session-endpoint=<url>` | `-E` | Explicit OIDC end_session endpoint; skips discovery (set with `--token-endpoint`) |
| `--client-id=<id>` | `-Z` | OAuth2 `client_id` (Keycloak / Auth0 / Azure AD / …) |
| `--user_id=<user>` | `-x` | Username for the token grant |
| `--user_passw=<passw>` | `-X` | Password for the token grant |
| `--jwt=<token>` | `-j` | Use a previously obtained JWT; skips the token-fetch task |

```bash
# Fetch a JWT via discovery, then connect to a remote agent
ycommand --url wss://host:1993 \
         --issuer https://auth.example.com/realms/yuneta/ \
         --client-id yuneta \
         --user_id alice --user_passw '••••••' \
         -c 'list-yunos'

# Reuse a JWT you already hold (no token-fetch round-trip)
ycommand --url wss://host:1993 --jwt "$JWT" -c 'stats'
```

The same flags work for [`ybatch`](ybatch.md) and [`ystats`](ystats.md).

## Command-line syntax

| Form | Meaning |
|------|---------|
| `cmd arg=val ...` | Send to the yuno's default service |
| `cmd ... service=__yuno__` | Send to the yuno itself |
| `cmd ... service=<name>` | Send to a named service inside the yuno |
| `*cmd ...` | Force response display in form (raw JSON) mode |
| `!cmd` | Run a **local** `ycommand` command (see `!help`) |
| `!!` | Re-run the last command (bash-style history expansion) |
| `!N` | Re-run history entry N (1-based, matches `!history` output) |
| `cmd1 ; cmd2 ; ...` | Chain; each waits for the previous response |
| `-cmd` | Ignore errors for this command (the [`ybatch`](#util-ybatch) convention) |
| `#` at start of line | Comment (ignored by `!source` / stdin piping) |

The `;` split is quote/brace aware: a `;` inside `"..."`, `'...'` or `{...}` is
treated as literal, so JSON-valued parameters like `kw={"a":1;b:2}` pass through
untouched.

(ycommand-shortcuts)=
## Interactive shortcuts

Available in `-i` mode (and shared with [`ycli`](ycli.md) through the common
`C_EDITLINE` line editor).

| Shortcut | Action |
|----------|--------|
| `TAB` | Complete command or parameter; list candidates if ambiguous |
| `TAB` after `param=` | Complete values for boolean params (`true` / `false`) |
| `Ctrl+R` / `Ctrl+S` | Reverse / forward incremental history search |
| `Up` / `Down` | Previous / next history entry |
| `Ctrl+A` / `Ctrl+E` | Start / end of line |
| `Ctrl+B` / `Ctrl+F` | Move left / right one character |
| `Ctrl+D` / `Backspace` | Delete char under / before cursor |
| `Ctrl+K` | Delete from cursor to end of line (readline) |
| `Ctrl+U` / `Ctrl+Y` | Delete whole line |
| `Ctrl+W` | Delete previous word |
| `Ctrl+T` | Swap character with previous |
| `Ctrl+L` | Clear screen |

While in `Ctrl+R` / `Ctrl+S` search mode the prompt changes to
`(reverse-i-search)'pat': <match>`; `Enter` accepts and submits, an arrow key
accepts and keeps editing, `ESC` or `Ctrl+G` cancel.

## Local commands (invoked with `!`)

Run `!help` (alias `!h` or `!?`) inside an interactive session for the full
list. Short aliases follow the `c_cli` convention:

| Command | Aliases | Description |
|---------|---------|-------------|
| `!help` | `!h`, `!?` | List all shortcuts and local commands |
| `!history` | `!hi` | Show the command history with 1-based indices (`!N`) |
| `!clear-history` | `!clh` | Erase history |
| `!exit` / `!quit` | `!x`, `!q` | Leave `ycommand` |
| `!source <file>` | `!. <file>` | Read commands from `<file>` and splice them at the head of the queue |

## Batch / scripting

`ycommand` can run a list of commands sequentially, each waiting for the
previous response — similar to [`ybatch`](ybatch.md) but with the line-oriented
`ycommand` syntax instead of JSON:

- `cmd1 ; cmd2 ; cmd3` on a single line.
- `!source script.ycmd` inside an interactive session.
- `cat script.ycmd | ycommand -u ws://...` from a shell pipe.
- Any command may be prefixed with `-` to keep the batch going on error. Without
  the `-`, the first error drops the queue.

Script file format: one command per line, `#` comments and blank lines ignored,
each line may itself contain `;`-chained commands.

## Other features

- **did-you-mean** — on a `command not available` error, `ycommand` compares the
  typed command name against the in-memory cache (fetched via
  `list-gobj-commands` at connect) and suggests the closest match when it is
  within an edit-distance threshold.
- **Informative prompt** — after connecting, the prompt reflects the remote yuno
  (`yuneta_agent^<name>> `).
- **Inline parameter hints** — after typing `<cmd> `, the remaining parameters
  appear in gray to the right of the cursor, required as `<name=type>` and
  optional as `[name=type]`. Already-typed params drop from the hint.
- **Output rendering** — schema-bearing responses render as aligned tables with
  bold-cyan headers in both interactive and non-interactive modes. Use the
  `*cmd` prefix to force raw-JSON (form) mode instead.
- **History dedup** — consecutive and prior occurrences of the same command are
  removed from history (bash `HISTCONTROL=erasedups` style).
- **Safe local config filenames (since 7.6.0)** — when a `view-config` /
  `read-json` / `read-file` / `edit-config` answer is saved under
  `~/.yuneta/configs/`, the peer-supplied record name/id is sanitized first:
  characters outside `[A-Za-z0-9._-]` fold to `_`, a leading dot is forbidden,
  and path separators collapse to a single inert basename. This closes an RCE
  where a hostile name flowed into the `"<editor> <path>"` string handed to
  `/bin/sh -c`. (Same fix in `ycli`.)

## See also

- [`ybatch`](ybatch.md), [`ystats`](ystats.md), [`ycli`](ycli.md) — the rest of
  the control-plane family.
- Repository README (for code navigators):
  [`utils/c/ycommand/README.md`](https://github.com/artgins/yunetas/blob/7.7.2/utils/c/ycommand/README.md).
