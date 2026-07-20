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
> framework runtime. The runtime is the `.deb`/`.rpm` (or a source build); the
> CLI drives that build and talks to the local agent. The two version
> independently. Source for the CLI lives in
> [`utils/python/tui_yunetas`](https://github.com/artgins/yunetas/tree/7.8.6/utils/python/tui_yunetas)
> (a git submodule); for development it can be installed editable instead of
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
| Deploy | `sync`, `sync-binaries`, `sync-configs`, `upgrade-yunos` |
| Secrets | `list-secrets` |
| Misc | `venv`, `version` |

`yunetas --help` documents every command and option in full (since CLI 0.18.0);
bare `yunetas` prints just the list, and `yunetas <command> --help` the detail
of one.

## Build

```bash
yunetas init      # create build dirs; read compiler/build-type from .config
yunetas build     # make install: SDK + every registered project, in dep order
yunetas test      # ctest
yunetas clean     # wipe the build dirs
```

`init` regenerates `yuneta_version.h` / `yuneta_config.h` and runs `cmake` in
each module's `build/`; `build` runs `make install` in dependency order.

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

Projects (e.g. an app repo) can be registered so the SDK build is immediately
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
subcommands wrap the agent helpers under
[`tools/agent/`](tools.md) and talk to the local agent over `ycommand`.

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
tools; for a tool-specific flag use the individual command:

```bash
yunetas sync-binaries -n      # outputs/yunos vs the local agent
yunetas sync-configs -n       # each project's yunos/batches/<host>/, auto-matched to local realms
yunetas sync-configs -n --host my.host.com   # or target one batches dir explicitly
```

`sync-configs` (and the configs pass of `sync`) walks the registered projects.
Without `--host` it queries the local agent (`*list-realms`) and syncs every
`batches/<host>/` whose name is a realm_id the agent manages — a node running
several realms deploys all the relevant ones in one pass (a batches dir is
named after its realm_id, the deploy FQDN). If the agent can't be reached it
falls back to a single hostname match.

`upgrade-yunos` takes an optional rollback snapshot (idempotent by name,
default `pre-upgrade-<YYYYMMDD>`, `--no-snap` to skip; if a snap is already
active it reuses that one instead of shooting another), lists the new yuno rows
`find-new-yunos` would create and asks for confirmation (`--yes` to skip),
registers them (`find-new-yunos create=1`), then runs `deactivate-snap` — which
triggers the agent's `restart_nodes()` (SIGKILL + treedb reload), promoting the
newest release of every yuno.

### Resuming a half-applied upgrade

The deploy is **idempotent**: re-running `sync` then `upgrade-yunos` after a run
that pushed the artifacts but never promoted them (e.g. `deactivate-snap` was
not reached) finishes the job instead of failing. When the binaries, configs and
yuno rows from the prior run are already on the agent, its create commands answer
`... already exists` — and that is treated as a benign already-present state, not
an error (since CLI 0.11.1):

- `sync-binaries` / `sync-configs` report such a row as `ALREADY PRESENT`
  (yellow) and count it as ok, not a red `FAILED`.
- `upgrade-yunos` does **not** abort when `find-new-yunos create=1` comes back
  "already exists" — it falls through to `deactivate-snap`, the step that
  actually promotes the new releases. (Before 0.11.1 it aborted *before* the
  restart, and the operator had to run `deactivate-snap` by hand.)

A genuine (non-idempotent) error still fails closed, and the agent's comments are
printed so a mixed result stays visible. So the safe recovery from any
interrupted upgrade is simply to re-run `yunetas sync && yunetas upgrade-yunos`.

For a same-version **hot-patch** (no `APP_VERSION` bump) you can skip
`upgrade-yunos`: `sync`, then bounce the affected yunos with
[`ycommand`](utilities/ycommand.md) (`kill-yuno` + `run-yuno` / `play-yuno`).

The wrapped scripts are documented under [Tools](tools.md):
[`sync_binaries.py`](tools/sync_binaries.md),
[`sync_configs.py`](tools/sync_configs.md).

## Deploy targets and secrets

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
`$YUNETA_OAUTH_JWT`.

A config that needs a credential declares it in the committed file as
`"__SECRET__"`; the value lives on the deploy machine in
`~/.yuneta/secrets/<node>/<config-id>.json` and is merged in just before the
push. A missing **or empty** value refuses the push rather than shipping a
blank password. Rotating a credential means bumping `__version__` in the
committed config, which is what makes the rotation visible in git while the
value never touches it.

```bash
yunetas list-secrets          # which configs have an overlay, and which fields
```

There is deliberately no `set-secret`: an argument would land in your shell
history and in the process table. Write the overlay with an editor, mode 600.

## Misc

```bash
yunetas venv create <name>    # manage a Python virtualenv
yunetas venv delete <name>
yunetas version               # installed CLI version
```

## See also

- [Installation](installation.md) — prerequisites and the one-time build setup.
- [Activating](activating.md) — initialise, build, run the agent, first yuno.
- [Tools](tools.md) — the `tools/agent/` scripts the deploy commands wrap.
- [`ycommand`](utilities/ycommand.md) — the control-plane client used to talk
  to a running yuno.
