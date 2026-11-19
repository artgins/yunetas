# ycommand

The control-plane CLI for Yuneta services. It sends commands and stats requests
to a running yuno over its local socket or a remote WebSocket URL. It has three
modes: a single command, an interactive shell, and a long-lived stdin-pipe session.

## Usage

```bash
ycommand -c '<command>'                     # single command on the default yuno
ycommand <command> <args...>                # same as -c, positional form
ycommand --url ws://host:port -c 'help'     # remote yuno
ycommand -i                                 # interactive shell (raw TTY)
cat script.ycmd | ycommand -u ws://...      # long-lived stdin-pipe session
```

If you give neither `-c` nor `-i`, and stdin is not a TTY, ycommand starts the
stdin-pipe mode. It reads the lines through events (one io_uring read on a
`dup` of stdin for each line). Thus the session stays open between commands.
A shell coproc, a `{ echo cmd1; sleep 5; echo cmd2; }` block, or any program
can send commands into the same authenticated session. The OAuth2 round-trip
occurs only one time. EOF on stdin starts an orderly shutdown.

For authentication, see [Authentication (OAuth2 / OIDC)](#authentication-oauth2--oidc)
below. For the full list of flags, run `ycommand --help`.

## Choosing a mode

| Mode                        | When to use                                                                 | Trade-off                                                       |
| --------------------------- | --------------------------------------------------------------------------- | --------------------------------------------------------------- |
| `ycommand -c '<cmd>'`       | A single command in a shell script or a one-line command                    | Pays connect + auth on every invocation                         |
| `ycommand -i`               | Interactive exploration: prompt, TAB, history, `Ctrl+R`, did-you-mean       | Requires a TTY. You cannot use it in a script                   |
| `... \| ycommand` (stdin)   | A known sequence of N commands, with no need to read a response first       | No feedback loop: ycommand sends every line, whatever the prior output. Connect + auth paid once |

## Authentication (OAuth2 / OIDC)

The default `ws://127.0.0.1:1991` is the plain local control port of the agent.
It needs no credentials. The remote / TLS path (usually `wss://host:1993`)
requires an OAuth2 JWT. You can give the JWT in one of two ways:

- A token that you already have: give it with `--jwt`.
- A token fetched at connect: give `--user_id` + `--user_passw` with an IdP
  (`--issuer` or `--token-endpoint`) and `--client-id`. ycommand gets the JWT
  itself before it opens the WebSocket.

If you give an IdP and a `--user_id`, ycommand runs a token-fetch task one time
at connect:

1. It finds the token endpoint, through OIDC discovery from `--issuer`, or
   as given in `--token-endpoint`.
2. It exchanges `user_id` / `user_passw` / `client_id` for a JWT
   (resource-owner password grant).
3. It opens the WebSocket with that JWT in the handshake.

In stdin-pipe mode this round-trip occurs one time, and the session serves
every piped command.

| Flag                            | Short | Purpose                                                                          |
| ------------------------------- | ----- | -------------------------------------------------------------------------------- |
| `--issuer=<url>`                | `-I`  | OIDC issuer URL. Starts the discovery of the token / end-session endpoints       |
| `--token-endpoint=<url>`        | `-T`  | Explicit token endpoint. Skips discovery (set with `--end-session-endpoint`)     |
| `--end-session-endpoint=<url>`  | `-E`  | Explicit OIDC end_session endpoint. Skips discovery (set with `--token-endpoint`) |
| `--client-id=<id>`              | `-Z`  | OAuth2 `client_id` (Keycloak / Auth0 / Azure AD / …)                             |
| `--user_id=<user>`              | `-x`  | Username for the token grant                                                     |
| `--user_passw=<passw>`          | `-X`  | Password for the token grant                                                     |
| `--jwt=<token>`                 | `-j`  | Use a JWT that you already have. Skips the token-fetch task                      |
| `--ssl-use-system-ca[=0\|1]`    |       | Validate the server cert against the OS CA store. Default: on. To disable, give `=0` |
| `--ssl-trusted-certificate=<f>` |       | PEM file/dir of trusted CAs for a private-CA endpoint                            |
| `--ssl-allow-insecure-client`   |       | Connect WITHOUT validation of the server cert (MITM risk, off by default)        |

The `--ssl-*` flags apply to both TLS legs of a remote session: the OIDC
`task-authenticate` to the issuer, and the `wss://` connection to the agent.
ycommand ignores them for plain `ws://`. Validation is on by default, so a
public-CA endpoint (for example Let's Encrypt) works with no flag. For a
private CA, use `--ssl-trusted-certificate`. To skip the validation, use
`--ssl-allow-insecure-client`.

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

## Command line syntax

| Form                          | Meaning                                                         |
| ----------------------------- | --------------------------------------------------------------- |
| `cmd arg=val ...`             | Send to the default service of the yuno                         |
| `cmd ... service=__yuno__`    | Send to the yuno itself                                         |
| `cmd ... service=<name>`      | Send to a named service inside the yuno                         |
| `*cmd ...`                    | Force response display in form (raw JSON) mode                  |
| `!cmd`                        | Run a local ycommand command (see `!help`)                      |
| `!!`                          | Re-run the last command (bash-style history expansion)          |
| `!N`                          | Re-run history entry N (1-based, matches `!history` output)     |
| `cmd1 ; cmd2 ; ...`           | Chain. Each command waits for the previous response             |
| `-cmd`                        | Ignore errors for this command (`ybatch` convention)            |
| `#` at start of line          | Comment (ignored by `!source` / stdin piping)                   |

The `;` split knows quotes and braces. A `;` inside `"..."`, `'...'` or `{...}`
is literal text. Thus JSON-valued parameters like `kw={"a":1;b:2}` pass
through with no change.

## Interactive shortcuts

| Shortcut                    | Action                                                      |
| --------------------------- | ----------------------------------------------------------- |
| `TAB`                       | Complete command or parameter. If ambiguous, list candidates |
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

In the `Ctrl+R`/`Ctrl+S` search mode, the prompt changes to
`(reverse-i-search)'pat': <match>`. `Enter` accepts and submits. An arrow key
accepts, and you continue to edit. `ESC` or `Ctrl+G` cancels.

## Local commands (invoked with `!`)

For the full list, run `!help` (alias `!h` or `!?`) in an interactive session.
The short aliases follow the `c_cli` convention:

| Command           | Aliases | Description                                  |
| ----------------- | ------- | -------------------------------------------- |
| `!help`           | `!h`, `!?` | List all shortcuts and local commands     |
| `!history`        | `!hi`   | Show the command history with 1-based indices (`!N`) |
| `!clear-history`  | `!clh`  | Erase history                                 |
| `!exit` / `!quit` | `!x`, `!q` | Leave ycommand                             |
| `!source <file>`  | `!. <file>` | Read commands from `<file>` and splice them at the head of the queue |

`history` (and `!history`) also work outside interactive mode. Outside `-i`,
there is no live line editor, so they print the persisted history file
(`~/.yuneta/history2.txt`). `ycommand history` / `ycommand -c history` show what
past interactive sessions ran, then exit.

## Batch / scripting

ycommand can run a list of commands in sequence. Each command waits for the
previous response. This is like `ybatch`, but with the line-oriented ycommand
syntax, not JSON.

- `cmd1 ; cmd2 ; cmd3` on a single line.
- `!source script.ycmd` inside an interactive session.
- `cat script.ycmd | ycommand -u ws://...` from a shell pipe.
- To continue the batch after an error of a command, put `-` before the
  command (`ybatch` convention). Without the `-`, the first error drops the queue.

The format of a script file: one command on each line. ycommand ignores `#`
comments and blank lines. A line can contain `;`-chained commands.

## Other features

- did-you-mean: on a `command not available` error, ycommand compares the
  typed command name with the in-memory cache (fetched with
  `list-gobj-commands` at connect). If the closest match is within an
  edit-distance threshold, ycommand suggests it.

- Informative prompt: after the connection, the prompt shows the remote
  yuno (`yuneta_agent^<name>> `).

- Inline parameter hints: after you type `<cmd> `, the remaining parameters
  show in gray to the right of the cursor. A required parameter shows as
  `<name=type>`, and an optional one as `[name=type]`. The parameters that you
  typed drop from the hint.

- Output rendering: a response with a schema shows as an aligned table with
  bold-cyan headers, in interactive and non-interactive modes. To force the
  raw-JSON (form) mode, use the `*cmd` prefix.

- History dedup: ycommand removes the consecutive and prior occurrences of
  the same command from the history (bash `HISTCONTROL=erasedups` style).

For the planned features, see [TODO.md](TODO.md).
