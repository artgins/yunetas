(yunetas-cli)=
# The `yunetas` CLI

`yunetas` is the **management and build front-end** for the whole project. One
command drives the SDK build, the external-project workflow, and node
deployment — what used to be a handful of ad-hoc scripts and manual `cmake` /
`make` invocations. As the project has grown it has become the single entry
point an operator or developer reaches for first.

It is a Python tool, distributed on PyPI and installed with `pipx`:

```bash
pipx install yunetas            # first install
pipx install --force yunetas    # upgrade to the latest release
yunetas version                 # print the installed CLI version
```

> ℹ️ The PyPI package `yunetas` is the **management/build CLI**, *not* the C
> framework runtime. The runtime is the `.deb`, the `.rpm`, or a source build. The
> CLI drives that build and talks to the local agent. The two version
> independently. Source for the CLI lives in
> [`utils/python/tui_yunetas`](https://github.com/artgins/yunetas/tree/7.27.7/utils/python/tui_yunetas)
> (a git submodule). For development you can install it editable instead of
> from PyPI.

Most commands assume the build environment is sourced first — see
[Installation](installation.md) for the one-time setup (`menuconfig`,
`yunetas-env.sh`, building the external libraries):

```bash
source yunetas-env.sh
```

## Command map

| Area | Commands |
|------|----------|
| Build | `init`, `build`, `clean`, `test` |
| Projects | `register-project`, `unregister-project`, `list-projects` |
| Deploy targets | `register-node`, `unregister-node`, `list-nodes` |
| Deploy | `sync`, `sync-binaries`, `sync-configs`, `upgrade-yunos`, `set-start-priorities` |
| This node | `restart-agents` |
| Misc | `version` |

`yunetas --help` documents every command and option in full (since CLI 0.18.0).
bare `yunetas` prints just the list, and `yunetas <command> --help` the detail
of one.

## Build

```bash
yunetas init      # create build dirs; read compiler/build-type from .config
yunetas build     # make install: SDK + every registered project, in dep order
yunetas test      # build the SDK and the root build/, then ctest
yunetas clean     # make clean in the build dirs
```

`init` regenerates `yuneta_version.h` / `yuneta_config.h` and runs `cmake` in
each module's `build/`. `build` runs `make install` in dependency order.

`build` and `test` run `make -j<N>`, where `N` is the number of cores that the
process can use. `test` runs `ctest -j<N>` too, and writes the log of each run
to `build/<date>.j<N>.txt`. Compare the times of two runs only when they have
the same `N`:

```bash
yunetas build -j 4            # make -j4
yunetas test -j 4             # make -j4, then ctest -j4
yunetas test --serial         # ctest runs one test after the other
yunetas test --clean          # make clean in the root build/ first
```

On an SDK older than 7.26.3, `test` runs ctest serially, because the suite of
that SDK does not pass in parallel.

**Re-run `init`:**

- the first time after cloning, and
- any time `.config` changes (compiler, build type, enabled modules).

By default these act on the **SDK plus every registered project**. Narrow the
scope with positional names or `--sdk-only`:

```bash
yunetas build myproject       # only that registered project (SDK skipped)
yunetas build --sdk-only      # only the SDK
```

## External projects

You can register projects, for example an app repo, so that the SDK build is immediately
followed by theirs. The registry is machine-local user state
(`~/.yuneta/projects.json`), kept outside the source tree.

```bash
yunetas register-project /path/to/project   # must contain yunos/CMakeLists.txt
yunetas list-projects
yunetas unregister-project <name>
```

Once registered, `init` / `build` / `clean` process the SDK first, then each
project. Pass names to restrict to specific projects, or `--sdk-only` to skip
them.

## Deploy

Deploy is **two steps: push the artifacts, then promote them.** The deploy
subcommands run the agent tools that ship inside the CLI package
(`yunetas.agent_tools`). They talk to the local agent over `ycommand`, or to a
[registered node](#cli-deploy-targets) with `--node`.

Each deploy command exits with `1` when a push failed, or when a restart that
the command did failed. A script can use that exit code.

> 📘 **New to deploying?** Read
> [Deploying yunos step by step](deploying-yunos.md) first — a scenario-driven
> guide (hot-patch, version bump, config-only, new yuno, rollback) with the
> exact commands for each case. This section is the command reference.

```bash
# 1. Push binaries AND configs together (recommended)
yunetas sync -n               # = sync-binaries + sync-configs, dry-run
yunetas sync                  # for real

# 2. Promote the freshly pushed releases to primary and restart
yunetas upgrade-yunos -n      # preview the agent commands
yunetas upgrade-yunos         # snapshot -> find-new-yunos -> deactivate-snap
```

Push both artifact kinds with **`sync`**, not one at a time: a binary bump must
never ship without its matching config bump. A new fail-closed runtime (TLS
verify-by-default) against a stale no-CA config is exactly what breaks OIDC
login. `sync` couples the steps so neither is forgotten, and aborts before the
configs pass if the binaries push fails (no half-deploy). Shared arguments
(`-n` dry-run, `-a` all, OAuth2 options…) are forwarded to both underlying
tools. For a tool-specific flag, use the individual command:

```bash
yunetas sync-binaries -n      # outputs/yunos vs the local agent
yunetas sync-binaries -n --role gate_x       # only that role (repeatable)
yunetas sync-configs -n       # each project's yunos/batches/<host>/, auto-matched to local realms
yunetas sync-configs -n --host my.host.com   # or target one batches dir explicitly
```

Give `--role` when you changed one yuno: without it, `sync-binaries` also
proposes every other binary that differs by a few bytes from the node's,
because your SDK is newer than the node's (since CLI 0.23.0). A role you
name that has no local build is an error (exit 2), not "Everything is up to
date" (since CLI 0.23.3).

`sync-configs` (and the configs pass of `sync`) walks the registered projects.
Without `--host` it queries the local agent (`*list-realms`) and syncs every
`batches/<host>/` whose name is a realm_id the agent manages — a node running
several realms deploys all the relevant ones in one pass (a batches dir is
named after its realm_id, the deploy FQDN). If the agent cannot be reached it
uses a single hostname match instead.

`upgrade-yunos` takes an optional rollback snapshot (idempotent by name,
default `pre-upgrade-<YYYYMMDD>`, `--no-snap` to skip. If a snap is already
active it reuses that one instead of shooting another), lists the new yuno rows
`find-new-yunos` creates and asks for confirmation (`--yes` to skip),
registers them (`find-new-yunos create=1`), then runs `deactivate-snap` — which
triggers the agent's `restart_nodes()` (SIGKILL + treedb reload), promoting the
newest release of every yuno.

### Resuming a half-applied upgrade

The deploy is **idempotent**: re-running `sync` then `upgrade-yunos` after a run
that pushed the artifacts but never promoted them, for example when `deactivate-snap` was
not reached) finishes the job instead of failing. When the binaries, configs and
yuno rows from the prior run are already on the agent, its create commands answer
`... already exists` — and that is treated as a benign already-present state, not
an error (since CLI 0.11.1):

- `sync-binaries` / `sync-configs` report such a row as `ALREADY PRESENT`
  (yellow) and count it as ok, not a red `FAILED`.
- `upgrade-yunos` does **not** abort when `find-new-yunos create=1` answers
  "already exists" — it falls through to `deactivate-snap`, the step that
  promotes the new releases. (Before 0.11.1 it aborted *before* the
  restart, and the operator had to run `deactivate-snap` by hand.)
- Since SDK 7.25.5 the agent itself tells these rows apart: `find-new-yunos`
  lists a registered-but-not-promoted row as
  `already registered, pending promotion (deactivate-snap): create-yuno id=...`,
  and `find-new-yunos create=1` skips it instead of answering "already
  exists". The preview is still not empty, so `upgrade-yunos` still goes on
  to `deactivate-snap`.
- Since CLI 0.19.4 `upgrade-yunos` lists those rows apart from the new ones,
  prints `N created, M already registered` after `create=1`, and skips
  `create=1` when every row is already registered (0.19.3 counted every row
  of the preview as created). For example, `1 created, 1 already registered.`
  Get it with `pipx upgrade yunetas`.

A genuine (non-idempotent) error still fails closed, and the agent's comments are
printed so a mixed result stays visible. So the safe recovery from any
interrupted upgrade is to run `yunetas sync && yunetas upgrade-yunos` again.

For a same-version **hot-patch** (no `APP_VERSION` bump) you can skip
`upgrade-yunos`: `sync-binaries` stops each rebuilt yuno, overwrites its
binary and starts it again (`--no-restart` to skip that).

A config is applied in one of two ways:

- **Same version, new content** (`UPDATE`): the yuno reads the config when it
  starts. `sync-configs -r` restarts the yunos that use it.
- **New version** (`BUMP`): a yuno runs the config version of its own row, and
  only `find-new-yunos create=1` + `deactivate-snap` change that row. A restart
  does not apply a `BUMP`, so `-r` does not restart these yunos. Run
  `yunetas upgrade-yunos` (since CLI 0.22.0; before, `-r` restarted the yuno
  on the old version).

### Start priorities

`set-start-priorities` sets the `start_priority` of each yuno (its launch tier,
`0..9`) from its role. The agent starts the yunos in ascending order and stops
them in descending order. Run it once per node: a version bump keeps the tier
of each yuno.

```bash
yunetas set-start-priorities -n                         # show the plan, write nothing
yunetas set-start-priorities -a                         # apply without asking
yunetas set-start-priorities --rule 'scheduler_wz=8'    # add a rule for an app role
yunetas set-start-priorities -n --node prod             # a registered node
```

The deploy tools are documented under [Tools](tools.md):
[`sync_binaries.py`](tools/sync_binaries.md),
[`sync_configs.py`](tools/sync_configs.md),
[`set_start_priorities.py`](tools/set_start_priorities.md).

(cli-restart-agents)=
## The agents of this node

A build from source installs `yuneta_agent` and `yuneta_agent22` straight into
`/yuneta/agent`, while the old processes go on running the old binary.
`yunetas build` then ends with a warning, and `restart-agents` restarts them
in the safe order (since CLI 0.23.0):

```bash
yunetas restart-agents -n     # show the state of both agents and the plan
yunetas restart-agents        # ask, then restart
```

```text
  yuneta_agent     main   STALE: a restart gives 7.27.5  in its systemd unit
  yuneta_agent22   spare  STALE: a restart gives 7.27.5  in its systemd unit
```

1. It keeps the binary that runs as `/yuneta/agent/<agent>.bak-running-<date>`.
   After the restart, that binary exists nowhere else.
2. It restarts the main agent, and checks that it runs the binary on disk and
   answers a command.
3. Only then does it restart the spare. The spare is the way into a node whose
   main agent does not come up, so a failure leaves it alone.

When the spare does not run, the command refuses to restart the main agent
and exits 1 (since CLI 0.23.3): if the main agent then did not come up, the
node would have no agent at all. Start the spare first, or say that you
accept the risk:

```bash
yunetas restart-agents --without-spare   # restart the main agent alone
```

An agent restarted outside its unit does not inherit the operator's
credentials (`YUNETA_OAUTH_PASSW`, `YUNETA_OAUTH_CLIENT_SECRET`,
`YUNETA_OAUTH_JWT`): they would stay in the agent and in every yuno it
launches.

An agent that runs in its systemd unit is restarted with `sudo -n systemctl
restart <unit>`. An agent that runs outside its unit (as on a development
machine) is restarted with `--stop` and its own command line again: a
`systemctl restart` there starts a second agent, which exits "already
running" and leaves the old one. Run the command as the user that owns the
agents.

The restart is never automatic after a build. The release suite builds on a
node in a worktree of its own, and an automatic restart would load that build
into the node's agents.

(cli-deploy-targets)=
## Deploy targets

A **node** is a registered deploy target, so a remote push is `--node <name>`
instead of a url plus four OAuth2 flags:

```bash
yunetas register-node prod --url wss://myhost:1993 \
    --issuer https://auth.example.com/realms/r --client-id myhost --user-id me
yunetas register-node prod --ssh yuneta@myhost   # or: tunnel to its local agent
yunetas list-nodes
yunetas sync --node prod
```

The registry (`~/.yuneta/nodes.json`, mode 600) stores **where** a node is and
**which identity** you present — never a credential. Pass the secret at call
time via `$YUNETA_OAUTH_PASSW`, `$YUNETA_OAUTH_CLIENT_SECRET` or
`$YUNETA_OAUTH_JWT`:

```bash
export YUNETA_OAUTH_PASSW="$(secret-tool lookup service yunetacontrol user me@example.com)"
yunetas sync -n --node prod
yunetas upgrade-yunos --node prod
```

The CLI never puts the password or the client secret on a command line, where
every local user can read them with `ps` (since CLI 0.22.0). The deploy tools
read them from the environment and log in once. The `ycommand` calls of the
CLI itself, for example in `upgrade-yunos`, also log in once and send only the
token.

A credential that a yuno needs, such as the SMTP password of an emailsender,
goes in its config file like any other value, and `sync-configs` pushes the file
as it is. (CLI 0.16.0-0.19.x kept such values out of the file, as
`"__SECRET__"` placeholders filled from `~/.yuneta/secrets/<node>/`; 0.20.0
removed that: a push from anything but `sync-configs` registered the
placeholder itself.)

```json
{
    "__version__": "3",
    "__description__": "v3: the SMTP password is back in the file",
    "global": {
        "Emailsender.url": "smtps://smtp.example.com:465",
        "Emailsender.username": "no-reply@example.com",
        "Emailsender.password": "the-password",
        "Emailsender.from": "no-reply@example.com"
    }
}
```

## Misc

```bash
yunetas version               # installed CLI version
```

## See also

- [Installation](installation.md) — prerequisites and the one-time build setup.
- [Activate the environment](#activate-environment) — `yunetas-env.sh`, in every shell.
- [Tools](tools.md) — the agent tools that the deploy commands run.
- [`ycommand`](utilities/ycommand.md) — the control-plane client used to talk
  to a running yuno.
