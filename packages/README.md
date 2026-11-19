# packages

This directory holds the OS packaging for the Yuneta Agent. It makes a `.deb`
and an `.rpm` of the same `/yuneta` runtime payload.

## Quick Start

```bash
# 1. Build yunetas first (see top-level CLAUDE.md)
source yunetas-env.sh
yunetas init && yunetas build

# 2a. Debian / Ubuntu
cd packages/deb && ./AMD64.sh          # -> dist/yuneta-agent-<ver>-<rel>-amd64.deb

# 2b. RHEL / Rocky / Alma
cd packages/rpm && ./x86_64.sh         # -> dist/yuneta-agent-<ver>-<rel>.x86_64.rpm
```

Each wrapper reads the framework version from `../../YUNETA_VERSION` and the
package release number from `../../RELEASE`. For the details of each format
(install actions, file layout, packaged CLI tools, system configuration), see
[`deb/README.md`](deb/README.md) and [`rpm/README.md`](rpm/README.md).

## The two packagers

Both packagers stage the same `/yuneta` runtime payload. The payload has the
agent binaries, CLI tools, web server, init scripts, kernel tuning, TLS
certificate management, and a dedicated `yuneta` system user. The packagers
differ only in the packaging format and the install glue of each distro.

| Format | Directory            | Distros                          | Service / sudo glue            |
|--------|----------------------|----------------------------------|--------------------------------|
| `.deb` | [`deb/`](deb/README.md) | Debian / Ubuntu               | `update-rc.d`, `sudo` group    |
| `.rpm` | [`rpm/`](rpm/README.md) | RHEL / Rocky / Alma / Fedora  | `chkconfig`, `wheel` group     |

```
packages/
├── README.md       <- this index
├── templates/      <- shared files: agent config samples, agent units, yuneta-node-uuid, yuneta-agent-cert
├── install-yuneta-node-uuid.sh <- the node uuid by hand, on a node without the package
├── install-yuneta-agent-cert.sh <- the agent's own certificate by hand, on a node without the package
├── tests/          <- tests of the shared scripts (run them from anywhere)
├── deb/            <- Debian packaging (AMD64/ARM32/ARMhf/RISCV64 wrappers)
└── rpm/            <- RPM packaging (x86_64/aarch64/riscv64 wrappers)
```

Both packagers share `templates/`. They reference it by absolute path from
`$YUNETAS_BASE/packages/templates/`. For this reason, `templates/` stays at
this level and not inside `deb/` or `rpm/`.

## The node uuid

`templates/yuneta-node-uuid` writes the node uuid from the hardware (see its
header). `tests/test-yuneta-node-uuid.sh` tests it against a fake `/sys`.

`install-yuneta-node-uuid.sh` installs it by hand on a node that builds from
source and does not take the package. The script is idempotent (a second run
changes nothing). To make sure that a node is correct, run it again.

## The certificate of the agent

`templates/yuneta-agent-cert` makes the own pair of the agent
(`/yuneta/agent/certs/yuneta_agent.{crt,key}`) on the node. It makes a pair in
these cases:

- There is no pair.
- The pair is the pair published with Yuneta up to 7.30.5.
- The script made the pair for another node uuid, for example on a cloned
  disk. `yuneta_agent.uuid` holds the uuid that each pair was made for. The
  script removes the key of the other node. It does not keep it.

For the details, see the header of the script. The units of both agents run
it, after `yuneta-node-uuid`.

`install-yuneta-agent-cert.sh` installs it by hand on a node without the
package, after `install-yuneta-node-uuid.sh` ran. The packages carry no agent
pair. They carry only `certs/controlcenter.crt`.

`tests/test-yuneta-agent-cert.sh` tests the script in a private directory: a
cloned disk against a fake `/sys`, with `yuneta-node-uuid`. The test also
makes sure of these points:

- `yunos/c/yuno_agent/service/` (`yuneta_agent`, `install-yuneta-service.sh`,
  `remove-yuneta-service.sh`) holds the files of the `.deb`. A node that
  builds from source installs that copy.
- The helper, the postinsts, the `%pre` of the `.rpm`, and the SysV script
  find an agent outside its unit by its `argv[0]`. They find it when its
  executable is replaced, renamed, behind a symlinked `/yuneta`, or
  unreadable. They leave the SysV script alone. For this point, the test runs
  the `install-yuneta-service.sh` of both packagers in a fake root, with stubs
  for `systemctl`, `pgrep` and `su`, and a fake `/proc`. The cases are systemd
  with and without the units, a unit that does not restart, and an old
  `/etc/init.d/yuneta_agent`.
- The build of the agent gives its warning about the installed scripts
  (`cmake -P`).
