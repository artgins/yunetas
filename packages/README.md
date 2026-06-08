# packages

OS packaging for the Yuneta Agent. Both packagers stage the **same**
`/yuneta` runtime payload — agent binaries, CLI tools, web server, init
scripts, kernel tuning, TLS certificate management, and a dedicated `yuneta`
system user — and differ only in the packaging format and the distro-specific
install glue.

| Format | Directory            | Distros                          | Service / sudo glue            |
|--------|----------------------|----------------------------------|--------------------------------|
| `.deb` | [`deb/`](deb/README.md) | Debian / Ubuntu               | `update-rc.d`, `sudo` group    |
| `.rpm` | [`rpm/`](rpm/README.md) | RHEL / Rocky / Alma / Fedora  | `chkconfig`, `wheel` group     |

```
packages/
├── README.md       <- this index
├── templates/      <- shared agent config samples (yuneta_agent*.json.sample)
├── deb/            <- Debian packaging (AMD64/ARM32/ARMhf/RISCV64 wrappers)
└── rpm/            <- RPM packaging (x86_64/aarch64/riscv64 wrappers)
```

`templates/` is shared by both packagers (referenced by absolute path from
`$YUNETAS_BASE/packages/templates/`), so it stays at this level rather than
inside `deb/` or `rpm/`.

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
package release number from `../../RELEASE`. See [`deb/README.md`](deb/README.md)
and [`rpm/README.md`](rpm/README.md) for the per-format details (install
actions, file layout, packaged CLI tools, system configuration).
