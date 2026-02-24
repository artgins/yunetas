# packages

Debian packaging for the Yuneta Agent. Builds `.deb` packages that install the complete Yuneta runtime — agent binaries, CLI tools, web server, init scripts, kernel tuning, TLS certificate management, and a dedicated `yuneta` system user — under the `/yuneta` hierarchy.

## Quick Start

```bash
# 1. Build yunetas first (see top-level CLAUDE.md)
source yunetas-env.sh
yunetas init && yunetas build

# 2. Run from this directory
cd packages/

# 3. Build for your architecture
./AMD64.sh          # x86_64
./ARM32.sh          # ARMv7
./ARMhf.sh          # ARMv6/v7 hard-float
./RISCV64.sh        # RISC-V 64-bit

# 4. Install the resulting .deb
sudo apt install ./dist/yuneta-agent-7.0.0-9-amd64.deb
```

## Supported Architectures

| Script | Architecture | Debian arch | Release |
|--------|-------------|-------------|---------|
| `AMD64.sh` | x86_64 | `amd64` | 9 |
| `ARM32.sh` | ARMv7 | `arm32` | 9 |
| `ARMhf.sh` | ARMv6/v7 hard-float | `armhf` | 9 |
| `RISCV64.sh` | RISC-V 64-bit | `riscv64` | 6 |

Each wrapper script reads the version from `../YUNETA_VERSION`, then calls `make-yuneta-agent-deb.sh` with the appropriate parameters.

## File Layout

```
packages/
├── AMD64.sh                    ← architecture wrapper (x86_64)
├── ARM32.sh                    ← architecture wrapper (ARMv7)
├── ARMhf.sh                    ← architecture wrapper (ARMv6/v7 hard-float)
├── RISCV64.sh                  ← architecture wrapper (RISC-V 64-bit)
├── make-yuneta-agent-deb.sh    ← main packaging script (~1360 lines)
├── README.md                   ← this file
├── authorized_keys/            ← (optional) SSH keys to bundle
│   └── authorized_keys
└── webserver/                  ← (optional) web server selection
    └── webserver               ← contents: "nginx" or "openresty"
```

**Generated at build time:**

```
packages/
├── build/deb/<arch>/<package>/  ← staging tree
└── dist/<package>.deb           ← final .deb output
```

## How It Works

### Build Pipeline

1. Architecture wrapper (`AMD64.sh`, etc.) parses `YUNETA_VERSION` and calls `make-yuneta-agent-deb.sh`
2. Main script resolves `YUNETAS_BASE` (from env var or `/yuneta/development/yunetas`)
3. Creates a fresh staging tree under `./build/deb/<arch>/`
4. Copies pre-built binaries from `/yuneta/bin/` and `/yuneta/agent/`
5. Bundles compiled outputs (`outputs/`, `outputs_ext/`) for development use
6. Generates all configuration files, init scripts, and helper scripts inline
7. Creates Debian control files (`control`, `conffiles`, `postinst`, `prerm`, `postrm`)
8. Normalizes permissions (0755 dirs, 0644 files, 0755 executables)
9. Builds `.deb` via `dpkg-deb --build` and outputs to `./dist/`

### Prerequisites

- All yunetas binaries must be pre-built (`yunetas build`)
- `YUNETAS_BASE` env var must be set (or `/yuneta/development/yunetas` must exist)
- Wrapper scripts **must be executed from this directory** (`cd packages/`)

## Installed Filesystem Layout

The `.deb` installs the following tree:

```
/yuneta/
├── bin/                                 ← CLI tools (21 utilities)
│   ├── ncurses/                         ← ncurses libraries
│   ├── nginx/                           ← bundled nginx
│   ├── openresty/                       ← bundled openresty
│   └── skeletons/                       ← project/gclass skeleton templates
├── agent/                               ← agent binaries + configs
│   ├── yuneta_agent                     ← main agent binary
│   ├── yuneta_agent22                   ← agent variant (port 22)
│   ├── yuneta_agent44                   ← agent variant (port 44)
│   ├── yuneta_agent.json                ← agent config (created by postinst)
│   ├── yuneta_agent22.json              ← agent22 config (created by postinst)
│   └── service/                         ← service management helpers
│       ├── install-yuneta-service.sh
│       ├── remove-yuneta-service.sh
│       ├── restart-yuneta
│       ├── install-certbot-snap.sh
│       ├── install-yuneta-dev-deps.sh
│       ├── check-certs-validity.sh
│       ├── copy-certs.sh
│       └── colas2.sh
├── gui/                                 ← web UI
├── realms/                              ← configuration realms
├── repos/                               ← repositories
├── store/
│   ├── certs/                           ← SSL certificates
│   │   └── private/                     ← private keys (mode 0700)
│   └── queues/gate_msgs2/              ← message queue persistence
├── share/                               ← shared resources
└── development/                         ← build outputs for development
    ├── outputs/                          ← compiled libraries/headers/bins
    ├── outputs_ext/                      ← external dependencies
    ├── tools/cmake/                      ← CMake toolchain files
    └── .config                           ← build configuration

/etc/
├── profile.d/yuneta.sh                  ← PATH, ulimits, shell aliases
├── sudoers.d/90-yuneta                  ← NOPASSWD sudo for yuneta user
├── init.d/yuneta_agent                  ← SysV init script
├── sysctl.d/99-yuneta-core.conf         ← kernel tuning
├── security/limits.d/99-yuneta-core.conf ← resource limits
├── letsencrypt/renewal-hooks/deploy/
│   └── reload-certs                     ← certbot deploy hook
└── yuneta/
    ├── authorized_keys                  ← (optional) SSH keys for yuneta user
    └── webserver                        ← web server selection (nginx/openresty)

/var/crash/                              ← core dumps (group yuneta, mode 0775)
```

## Bundled CLI Tools

| Tool | Purpose |
|------|---------|
| `ycommand` | Send commands to running yunos |
| `ystats` | Query yuno statistics |
| `ylist` | List running yunos |
| `yshutdown` | Graceful shutdown |
| `ytests` | Run test suites |
| `ybatch` | Batch command execution |
| `ycli` | Interactive CLI |
| `keycloak_pkey_to_jwks` | Convert Keycloak public keys to JWKS |
| `list_queue_msgs2` | Inspect timeranger2 message queues |
| `tr2keys` | List timeranger2 topic keys |
| `tr2list` | List timeranger2 records |
| `tr2migrate` | Migrate timeranger2 data |
| `watchfs` | File system watcher tool |
| `fs_watcher` | inotify-based directory watcher |
| `inotify` | Raw inotify monitor |
| `yclone-gclass` | Clone a GClass from template |
| `yclone-project` | Clone a project from template |
| `ymake-skeleton` | Generate project/gclass skeleton |
| `yuno-skeleton` | Generate yuno skeleton |
| `yscapec` | Escape C strings |
| `ytestconfig` | Test JSON configuration |

## Agent Variants

| Binary | Description |
|--------|-------------|
| `yuneta_agent` | Main agent (default port) |
| `yuneta_agent22` | Agent listening on port 22 |
| `yuneta_agent44` | Agent listening on port 44 |

Each agent has a corresponding `.json` configuration file generated on first install by `postinst`.

## Service Management Helper Scripts

| Script | Purpose |
|--------|---------|
| `install-yuneta-service.sh` | Enable and start the SysV init service |
| `remove-yuneta-service.sh` | Stop and remove the SysV runlevel symlinks |
| `restart-yuneta` | Restart the Yuneta stack (prefers `service`, falls back to `yshutdown` + direct start) |
| `install-certbot-snap.sh` | Install certbot via snap; symlink to `/usr/bin/certbot` |
| `install-yuneta-dev-deps.sh` | Install full build toolchain (gcc, cmake, libs, pipx packages) |
| `check-certs-validity.sh` | Scan `*.crt` files and show expiry status |
| `copy-certs.sh` | Copy Let's Encrypt certificates from `/etc/letsencrypt/live/` to `/yuneta/store/certs/` |
| `colas2.sh` | Scan and list two-level message queue directories |

## System Configuration

### Kernel Tuning (`/etc/sysctl.d/99-yuneta-core.conf`)

| Parameter | Value | Purpose |
|-----------|-------|---------|
| `net.core.somaxconn` | 65535 | TCP listen backlog |
| `kernel.core_pattern` | `/var/crash/core.%e` | Core dump location |
| `fs.file-max` | 4000000 | System-wide file descriptor limit |
| `fs.nr_open` | 4000000 | Per-process file descriptor limit |

### Resource Limits (`/etc/security/limits.d/99-yuneta-core.conf`)

| User | Type | Resource | Value |
|------|------|----------|-------|
| `yuneta` | soft/hard | core | unlimited |
| `yuneta` | soft/hard | nofile | unlimited |

### Shell Environment (`/etc/profile.d/yuneta.sh`)

- Adds `/yuneta/bin` and `/yuneta/agent` to `PATH`
- Sets `ulimit -c unlimited` and `ulimit -n 100000`
- Defines aliases: `y` (cd /yuneta), `salidas` / `outputs` (cd to outputs), `logs` (cd /var/log)

### SysV Init Script (`/etc/init.d/yuneta_agent`)

- Starts `yuneta_agent` and `yuneta_agent22` on boot
- Controls the selected web server (nginx or openresty, per `/etc/yuneta/webserver`)
- Supports standard `start`, `stop`, `restart`, `status` operations
- Raises resource limits (`ulimit`) before launching agents

## Package Metadata

### Dependencies

| Type | Packages |
|------|----------|
| **Depends** | `adduser`, `lsb-base`, `rsync`, `locales`, `rsyslog`, `init-system-helpers` |
| **Recommends** | `curl`, `vim`, `sudo`, `tree`, `pipx`, `fail2ban`, `net-tools`, `locate` |
| **Suggests** | Full development toolchain (`git`, `cmake`, `gcc`, `clang`, `python3-dev`, crypto/compression libs, etc.) |

### Post-Install Actions (`postinst`)

1. Creates login user `yuneta` (UID >= 1000, bash shell, home `/home/yuneta`)
2. Adds to groups: `adm`, `sudo`, `tty`, `dialout`, `docker`, `libvirt`, `kvm`, `gpio`, `i2c`, `spi`, and more (overridable via `$YUNETA_GROUPS`)
3. Generates default agent JSON configs if missing
4. Installs SSH authorized_keys from `/etc/yuneta/authorized_keys` if present
5. Enables locales (`en_US.UTF-8`, `es_ES.UTF-8`)
6. Enables rsyslog for `/var/log/syslog`
7. Sets ownership: `/yuneta` tree to `yuneta:yuneta`
8. Sets permissions: `/yuneta/store/certs/private` to `0700`
9. Adds `pam_limits.so` to PAM session config
10. Enables and starts the SysV service
11. Prompts for reboot (auto-reboots in non-interactive mode)

### Pre-Remove Actions (`prerm`)

- Stops the service gracefully before upgrade or removal

### Post-Remove Actions (`postrm`)

- On remove: removes SysV runlevel symlinks (keeps conffiles)
- On purge: also removes `/etc/init.d/yuneta_agent`

## Optional Build-Time Configuration

### SSH Keys

Place SSH public keys in `authorized_keys/authorized_keys` before building. They will be installed to the `yuneta` user's `~/.ssh/authorized_keys` during package installation.

```bash
mkdir -p authorized_keys
cp ~/.ssh/id_rsa.pub authorized_keys/authorized_keys
```

### Web Server Selection

Create a `webserver/webserver` file to choose between nginx and openresty (default: nginx):

```bash
mkdir -p webserver
echo "openresty" > webserver/webserver
```

### Let's Encrypt Integration

The package installs a certbot deploy hook at `/etc/letsencrypt/renewal-hooks/deploy/reload-certs` that:

1. Copies renewed certificates from `/etc/letsencrypt/live/` to `/yuneta/store/certs/`
2. Reloads the selected web server
3. Restarts the Yuneta stack in the background

Certificates are auto-discovered unless `/yuneta/store/certs/certs.list` exists, in which case only listed certificates are updated.

## Versioning

The package version is read from `../YUNETA_VERSION` (e.g., `YUNETA_VERSION=7.0.0`). The release number is set in each architecture wrapper script (e.g., `RELEASE="9"`). The final package name follows the pattern:

```
yuneta-agent-<version>-<release>-<arch>.deb
```

Example: `yuneta-agent-7.0.0-9-amd64.deb`
