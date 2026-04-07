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
sudo apt install ./dist/yuneta-agent-7.2.1-9-amd64.deb
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
├── AMD64.sh                    <- architecture wrapper (x86_64)
├── ARM32.sh                    <- architecture wrapper (ARMv7)
├── ARMhf.sh                    <- architecture wrapper (ARMv6/v7 hard-float)
├── RISCV64.sh                  <- architecture wrapper (RISC-V 64-bit)
├── make-yuneta-agent-deb.sh    <- main packaging script (~1360 lines)
├── README.md                   <- this file
├── authorized_keys/            <- (optional) SSH keys to bundle
│   └── authorized_keys
└── webserver/                  <- (optional) web server selection
    └── webserver               <- contents: "nginx" or "openresty"
```

**Generated at build time:**

```
packages/
├── build/deb/<arch>/<package>/  <- staging tree
└── dist/<package>.deb           <- final .deb output
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

## What the .deb Installation Does

When you run `sudo apt install ./yuneta-agent-*.deb`, the following happens step by step:

### 1. File Extraction

The package manager extracts the full `/yuneta/` tree, configuration files under `/etc/`, and the `/var/crash/` directory onto the filesystem. No existing configuration files are overwritten (they are marked as `conffiles`).

### 2. Post-Installation Script (`postinst`)

The `postinst` script runs automatically after file extraction and performs these actions **in order**:

#### 2.1. User and Group Setup
- **Creates the `yuneta` login user** (if it doesn't exist) with:
  - Home directory: `/home/yuneta`
  - Shell: `/bin/bash`
  - UID >= 1000 (non-system user, regular login)
  - Disabled password (SSH key or sudo access)
- **Adds `yuneta` to system groups**: `adm`, `sudo`, `tty`, `dialout`, `cdrom`, `audio`, `video`, `plugdev`, `netdev`, `render`, `input`, `gpio`, `i2c`, `spi`, `uucp`, `wireshark`, `bluetooth`, `scanner`, `lp`, `lpadmin`, `sambashare`, `docker`, `libvirt`, `kvm`, `lxd`
  - Missing groups are silently skipped (override with `YUNETA_CREATE_MISSING_GROUPS=1`)
  - Override group list with `YUNETA_GROUPS` env var

#### 2.2. Agent Configuration
- Creates `/yuneta/agent/yuneta_agent.json` from the bundled `.sample` file (only if the file doesn't already exist)
- Creates `/yuneta/agent/yuneta_agent22.json` from the bundled `.sample` file (only if the file doesn't already exist)
- Existing configuration files are **never** overwritten on upgrade

#### 2.3. Locale Configuration
- Enables `en_US.UTF-8` and `es_ES.UTF-8` in `/etc/locale.gen`
- Runs `locale-gen` and sets `LANG=en_US.UTF-8`

#### 2.4. Ownership and Permissions
- Sets `/yuneta/` tree ownership recursively to `yuneta:yuneta`
- Sets `/yuneta/store/certs/private/` to mode `0700` (only owner can access private keys)
- Sets `/var/crash/` to mode `0775` with group `yuneta` (for core dumps)

#### 2.5. Kernel Parameter Tuning
- Applies `/etc/sysctl.d/99-yuneta-core.conf` via `sysctl --system`

#### 2.6. SSH Keys (Optional)
- If `/etc/yuneta/authorized_keys` exists (bundled at build time), installs it to `/home/yuneta/.ssh/authorized_keys` with proper permissions (directory `0700`, file `0600`)

#### 2.7. Syslog Setup
- Enables and starts `rsyslog` to ensure `/var/log/syslog` is available

#### 2.8. SysV Service Installation
- Installs `/etc/init.d/yuneta_agent` (if not present)
- Creates runlevel symlinks via `update-rc.d yuneta_agent defaults`
- **Starts the service** on `configure`/`reconfigure` actions

#### 2.9. PAM Limits
- Adds `session required pam_limits.so` to `/etc/pam.d/common-session` and `/etc/pam.d/common-session-noninteractive` (if not already present)

#### 2.10. Reboot
- **Interactive install**: prompts the user to reboot (default: yes)
- **Non-interactive install** (`DEBIAN_FRONTEND=noninteractive`): reboots automatically
- Creates `/run/reboot-required` flag

### 3. Package Removal

- **`prerm`**: Stops the `yuneta_agent` service gracefully before removing files
- **`postrm remove`**: Removes SysV runlevel symlinks (keeps conffiles)
- **`postrm purge`**: Also deletes `/etc/init.d/yuneta_agent`

### 4. Configuration Files Preserved on Upgrade

These files are marked as `conffiles` and will not be overwritten on upgrade:
- `/etc/profile.d/yuneta.sh`
- `/etc/sudoers.d/90-yuneta`
- `/etc/init.d/yuneta_agent`
- `/etc/letsencrypt/renewal-hooks/deploy/reload-certs`
- `/etc/yuneta/authorized_keys` (if bundled)
- `/etc/yuneta/webserver` (if bundled)

## Installed Filesystem Layout

The `.deb` installs the following tree:

```
/yuneta/
├── bin/                                 <- CLI tools (21 utilities)
│   ├── ncurses/                         <- ncurses libraries
│   ├── nginx/                           <- bundled nginx
│   ├── openresty/                       <- bundled openresty
│   └── skeletons/                       <- project/gclass skeleton templates
├── agent/                               <- agent binaries + configs
│   ├── yuneta_agent                     <- main agent binary
│   ├── yuneta_agent22                   <- agent variant (port 22)
│   ├── yuneta_agent44                   <- agent variant (port 44)
│   ├── yuneta_agent.json                <- agent config (created by postinst)
│   ├── yuneta_agent22.json              <- agent22 config (created by postinst)
│   └── service/                         <- service management helpers
│       ├── install-yuneta-service.sh
│       ├── remove-yuneta-service.sh
│       ├── restart-yuneta
│       ├── install-certbot-snap.sh
│       ├── install-yuneta-dev-deps.sh
│       ├── check-certs-validity.sh
│       ├── copy-certs.sh
│       └── colas2.sh
├── gui/                                 <- web UI
├── realms/                              <- configuration realms
├── repos/                               <- repositories
├── store/
│   ├── certs/                           <- SSL certificates
│   │   └── private/                     <- private keys (mode 0700)
│   └── queues/gate_msgs2/              <- message queue persistence
├── share/                               <- shared resources
└── development/                         <- build outputs for development
    ├── outputs/                          <- compiled libraries/headers/bins
    ├── outputs_ext/                      <- external dependencies
    ├── tools/cmake/                      <- CMake toolchain files
    └── .config                           <- build configuration

/etc/
├── profile.d/yuneta.sh                  <- PATH, ulimits, shell aliases
├── sudoers.d/90-yuneta                  <- NOPASSWD sudo for yuneta user
├── init.d/yuneta_agent                  <- SysV init script
├── sysctl.d/99-yuneta-core.conf         <- kernel tuning
├── security/limits.d/99-yuneta-core.conf <- resource limits
├── letsencrypt/renewal-hooks/deploy/
│   └── reload-certs                     <- certbot deploy hook
└── yuneta/
    ├── authorized_keys                  <- (optional) SSH keys for yuneta user
    └── webserver                        <- web server selection (nginx/openresty)

/var/crash/                              <- core dumps (group yuneta, mode 0775)
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

| Script | Location | Purpose |
|--------|----------|---------|
| `install-yuneta-service.sh` | `/yuneta/agent/service/` | Enable and start the SysV init service |
| `remove-yuneta-service.sh` | `/yuneta/agent/service/` | Stop and remove the SysV runlevel symlinks |
| `restart-yuneta` | `/yuneta/bin/` | Restart the Yuneta stack (prefers `service`, falls back to `yshutdown` + direct start) |
| `install-certbot-snap.sh` | `/yuneta/bin/` | Install certbot via snap; symlink to `/usr/bin/certbot` |
| `install-yuneta-dev-deps.sh` | `/yuneta/bin/` | Install full build toolchain (gcc, cmake, libs, pipx packages) |
| `check-certs-validity.sh` | `/yuneta/bin/` | Scan `*.crt` files and show expiry status |
| `copy-certs.sh` | `/yuneta/store/certs/` | Copy Let's Encrypt certificates from `/etc/letsencrypt/live/` to `/yuneta/store/certs/` |
| `colas2.sh` | `/yuneta/bin/` | Scan and list two-level message queue directories |

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
- Sets `ulimit -c unlimited` and `ulimit -n unlimited`
- Defines aliases: `y` (cd to yunetas), `salidas` / `outputs` (cd to outputs), `logs` (cd to logs)

### Sudoers (`/etc/sudoers.d/90-yuneta`)

```
yuneta ALL=(ALL) NOPASSWD:ALL
```

Grants the `yuneta` user passwordless sudo for all commands.

### SysV Init Script (`/etc/init.d/yuneta_agent`)

- Starts `yuneta_agent` and `yuneta_agent22` on boot (runlevels 2 3 4 5)
- Controls the selected web server (nginx or openresty, per `/etc/yuneta/webserver`)
- Raises resource limits (`ulimit`) before launching agents
- Supports standard `start`, `stop`, `restart`, `force-reload`, `status` operations
- Runs agents as user `yuneta` via `su -s /bin/sh`

## Package Metadata

### Dependencies

| Type | Packages |
|------|----------|
| **Depends** | `adduser`, `lsb-base`, `rsync`, `locales`, `rsyslog`, `init-system-helpers` |
| **Recommends** | `curl`, `vim`, `sudo`, `tree`, `pipx`, `fail2ban`, `net-tools`, `locate` |
| **Suggests** | Full development toolchain (`git`, `cmake`, `gcc`, `clang`, `python3-dev`, crypto/compression libs, etc.) |

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

To set up certbot after installing the .deb:

```bash
sudo /yuneta/bin/install-certbot-snap.sh
```

## Post-Install Checklist

After installing the `.deb`, the `postinst` script reminds you to run:

1. `sudo /yuneta/bin/install-yuneta-dev-deps.sh` - Install build toolchain (only needed for development)
2. `sudo /yuneta/bin/install-certbot-snap.sh` - Install certbot for ACME TLS certificates

## Versioning

The package version is read from `../YUNETA_VERSION` (e.g., `YUNETA_VERSION=7.2.1`). The release number is set in each architecture wrapper script (e.g., `RELEASE="9"`). The final package name follows the pattern:

```
yuneta-agent-<version>-<release>-<arch>.deb
```

Example: `yuneta-agent-7.2.1-9-amd64.deb`
