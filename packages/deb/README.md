# packages/deb

Debian packaging for the Yuneta Agent. Builds `.deb` packages that install the complete Yuneta runtime — agent binaries, CLI tools, web server, init scripts, kernel tuning, TLS certificate management, and a dedicated `yuneta` system user — under the `/yuneta` hierarchy.

> 📦 **RHEL/Rocky/Alma?** The RPM counterpart lives in [`../rpm/`](../rpm/README.md):
> it stages the same `/yuneta` payload and builds an `.rpm` with `rpmbuild`.
> See [`../rpm/README.md`](../rpm/README.md) for the RHEL-specific differences
> (SysV via `chkconfig`, `wheel` group, langpacks, EPEL certbot, and the
> shipped `kernel.io_uring_disabled=0`).

## Quick Start

```bash
# 1. Build yunetas first (see top-level CLAUDE.md)
source yunetas-env.sh
yunetas init && yunetas build

# 2. Run from this directory
cd packages/deb/

# 3. Build for your architecture
./AMD64.sh          # x86_64
./ARM32.sh          # ARMv7
./ARMhf.sh          # ARMv6/v7 hard-float
./RISCV64.sh        # RISC-V 64-bit

# 4. Install the resulting .deb
sudo apt install ./dist/yuneta-agent-7.3.4-1-amd64.deb
```

## Supported Architectures

| Script | Architecture | Debian arch |
|--------|-------------|-------------|
| `AMD64.sh` | x86_64 | `amd64` |
| `ARM32.sh` | ARMv7 | `arm32` |
| `ARMhf.sh` | ARMv6/v7 hard-float | `armhf` |
| `RISCV64.sh` | RISC-V 64-bit | `riscv64` |

Each wrapper reads the framework version from `../../YUNETA_VERSION` and the package release number from `../../RELEASE`, then calls `make-yuneta-agent-deb.sh` with the appropriate parameters. All four architectures share the same release counter — bump `RELEASE` once when re-packaging the same framework version.

## File Layout

```
packages/deb/
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
packages/deb/
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
- Wrapper scripts **must be executed from this directory** (`cd packages/deb/`)

## It refuses a node that builds from source

`preinst` aborts the install when `/yuneta/development/yunetas` holds the sources
(`kernel/` and `.git`). The package carries artefacts built on another machine
and lays them into the tree `yunetas build` owns (`outputs/`, `outputs_ext/`),
so fresh objects would link against archives from a **different glibc** — and a
static link takes those silently and corrupts the heap at run time.

It is a refusal, not a warning, so it has to be a decision somebody took rather
than something an install did to a build node on its way past. To go ahead:

```bash
sudo YUNETAS_FORCE_OVER_SOURCE=1 apt install ./<package>
# or, to make it stick for this node:
sudo touch /etc/yuneta/allow-package-over-source
```

Then **rebuild everything on that node, external libraries first**
(`kernel/c/linux-ext-libs`: `extrae.sh` + `configure-libs.sh`, then
`yunetas init` and `yunetas clean && yunetas build`). Rebuilding only the SDK
leaves the packaged externals in place — the same mismatch by a shorter road.

## What the .deb Installation Does

When you run `sudo apt install ./yuneta-agent-*.deb`, the following happens step by step:

### 1. File Extraction

The package manager extracts the full `/yuneta/` tree, configuration files under `/etc/`, and the `/var/crash/` directory onto the filesystem. No existing configuration file is overwritten: those under `/etc/` are marked as `conffiles`, and the web server configuration — which lives outside `/etc/`, where `conffiles` cannot reach — is simply **not shipped** (see below).

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
- Sets `/var/crash/` to mode `0775` with group `yuneta` (for core dumps), and
  installs `/usr/lib/tmpfiles.d/yuneta-crash.conf` so systemd re-applies it on
  every boot. `/var/crash` can be co-owned by another package (kdump's
  `kexec-tools` declares it `root:root 0755`), and a one-shot postinst fix
  holds only until that package's next transaction — after which cores stop
  being written with no diagnostic.

- **Disables `apport`** (`enabled=0` in `/etc/default/apport` +
  `systemctl disable --now`). It is Ubuntu's crash-telemetry client: it exists
  to ship deduplicated reports to `errors.ubuntu.com`, and it discards crashes
  of binaries outside its packaging allowlist (`/yuneta` is not on it), so on a
  Yuneta node it only costs us core dumps. Note `apport --stop` leaves
  `core_pattern` as the bare word `core` (dumps land in the crashing process's
  CWD), so the installer re-applies the sysctl **after** stopping it.
- Installs and enables `yuneta-core-pattern.service`, which re-applies
  `kernel.core_pattern` **after** `apport.service` — a backstop in case an
  apport upgrade re-enables itself. On Ubuntu apport takes that
  sysctl over at every boot and discards cores of non-distro binaries, so
  without this the yunos silently stop producing core dumps after the first
  reboot. Verify with `cat /proc/sys/kernel/core_pattern` — a value starting
  with `|` means a crash handler owns it.

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

#### 2.8bis. Web server service

- Installs `/usr/lib/systemd/system/yuneta-webserver.service` and
  `/yuneta/bin/yuneta-webserver`, and enables and starts the unit.
- **The web server is no longer started by the init script.** It used to be:
  `/etc/init.d/yuneta_agent` ran nginx and let it daemonize, so nothing owned
  the process afterwards. A later `start` found nothing to look at and tried
  again, and the second master died with *"Address already in use"* while the
  first one kept serving.
- The unit runs the server with `daemon off`, so `$MAINPID` is the real master
  and stop and reload reach it. `systemctl reload` sends HUP, which reloads the
  configuration. Reopening the log files is USR1 and stays where it was, in the
  `postrotate` of `/etc/logrotate.d/yuneta`.
- On upgrade the scriptlet asks the old, daemonized server to finish before it
  starts the unit, because the unit cannot take `:80` and `:443` while the old
  master holds them. **That is a short interruption of the node's web server,
  at upgrade time only.**

#### 2.9. PAM Limits
- Adds `session required pam_limits.so` to `/etc/pam.d/common-session` and `/etc/pam.d/common-session-noninteractive` (if not already present)

#### 2.10. Reboot
- **The installer never reboots**, in either mode. The kernel tuning is applied
  live with `sysctl --system`, so a reboot is not needed to run. It only writes
  the `/run/reboot-required` flag and recommends one, to verify that the agent
  comes up at boot.
- This paragraph used to say a non-interactive install reboots automatically.
  It has not been true since the auto-reboot was removed, and reading it before
  installing on a client's node is a bad minute to have.

### The node's web server choice

`/etc/yuneta/webserver` holds one word, `nginx` or `openresty`, and it is
**node state, not package content**. Neither package ships it.

The reason is the one nginx.conf taught in 7.9.1: the file the package would
ship carries the BUILD machine's choice, and the build machine has none, so it
ships `nginx`. On 7.11.0 that pushed `nginx` onto two nodes that run openresty,
and both came back up serving from the wrong tree with a default
configuration.

The handling is the same as nginx.conf, and symmetric between the two
flavours:

| Step | What happens |
|---|---|
| `preinst` / `%pre` | Copies the node's value to `/etc/yuneta/webserver.pkgsave` |
| unpack | A file the package no longer provides is removed by the package manager |
| `postinst` / `%posttrans` | Absent → restore from `.pkgsave`; no `.pkgsave` → seed the build default; present → leave it alone |

So a node that already chose keeps its choice, and only a node that never had
one gets a default.

### 3. Package Removal

- **`prerm`**: Stops the `yuneta_agent` service gracefully before removing files, and stops and disables `yuneta-webserver.service`
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

The web server configuration is preserved by a different mechanism, because it
lives under `/yuneta/bin/`, where `conffiles` does not apply (and where the
`.rpm` cannot tag it either: `%files` lists `/yuneta` as a directory, so
rpmbuild rejects a second entry for a file inside it). These are **stripped
from the payload** instead — a file the package does not contain cannot be
replaced:

- `/yuneta/bin/nginx/conf/nginx.conf` and `conf/conf.d/`
- `/yuneta/bin/openresty/nginx/conf/nginx.conf` and `conf/conf.d/`

Not shipping them is only half the job, though: **dpkg deletes files that an
upgrade no longer provides**, so upgrading from a package that still owned
`nginx.conf` (7.9.0 or older) would remove it. Since 7.9.2 `preinst` copies it
to `nginx.conf.pkgsave` before the unpack, and `postinst` restores from that
copy.

So `postinst` picks, in order:

1. `nginx.conf` already there → leave it alone.
2. `nginx.conf.pkgsave` → restore the node's own config (an upgrade that
   dropped it).
3. `nginx.conf.default` → seed a first install.

It also creates `conf.d/` when missing. Until 7.9.1 the package carried the
build machine's copies and overwrote the node's on every upgrade; 7.9.1 fixed
the overwrite but deleted the file on the way, which is what 7.9.2 fixes.

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
│       ├── install-certbot.sh
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
└── development/
    ├── projects/                        <- home for user project repos
    └── yunetas/                         <- sparse SDK (same YUNETAS_BASE path as a source checkout)
        ├── outputs/                     <- compiled libraries/headers/bins
        ├── outputs_ext/                 <- external dependencies
        ├── tools/cmake/                 <- CMake toolchain files
        └── .config                      <- build configuration

/etc/
├── profile.d/yuneta.sh                  <- PATH, ulimits, shell aliases
├── sudoers.d/90-yuneta                  <- NOPASSWD sudo for yuneta user
├── init.d/yuneta_agent                  <- SysV init script
├── sysctl.d/99-yuneta-core.conf         <- kernel tuning
├── security/limits.d/99-yuneta-core.conf <- resource limits
├── logrotate.d/yuneta                   <- rotation of the web server logs
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
| `install-certbot.sh` | `/yuneta/bin/` | Install certbot via snap; symlink to `/usr/bin/certbot` |
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
| `yuneta` | soft/hard | memlock | unlimited |

`memlock` is not optional. io_uring rings are pinned memory charged against
`RLIMIT_MEMLOCK`, and the budget is **per user**, shared by every yuno running
as `yuneta`. A yuno with `io_uring_entries=32768` pins ~3.1 MB (SQEs
`32768*64` + CQEs `65536*16` + SQ array), so the usual 8 MB default admits only
two of them: from the third yuno on, `yev_loop_create()` fails with `ENOMEM`,
the yuno aborts at startup and the ydaemon watcher relaunches it indefinitely.
The symptom is a node with gigabytes of free RAM whose yunos die of "out of
memory" — it is a limit, never a shortage.

### Log Rotation (`/etc/logrotate.d/yuneta`)

nginx has no rotation of its own. It only knows how to reopen its files when it
gets `USR1`, so without this drop-in `access.log` and `error.log` grow for the
life of the node. When it was added, no log had ever been rotated on any node;
the busiest one was writing 3.5 MB a day.

| Path | Period | Keeps |
|------|--------|-------|
| `/yuneta/bin/nginx/logs/*.log` | daily | 30, compressed |
| `/yuneta/bin/openresty/nginx/logs/*.log` | daily | 30, compressed |
| `/var/log/yuneta/*.log` | monthly | 12, compressed |

Both web server trees are listed. A node runs one or the other — the choice is
in `/etc/yuneta/webserver` — and `missingok` covers the tree that is absent.
The `postrotate` reads each `nginx.pid` and sends `USR1` only to a master that
is alive, so a stale pid file cannot make it signal an unrelated process.

The **yunos do not rotate here**. Each one writes numbered files under
`/yuneta/realms/<realm>/<yuno>/logs/` and rotates them itself.

The package `Depends` on `logrotate` (`Requires` on RPM) and the drop-in is a
conffile, so a node that edits it keeps its version across upgrades.

The owner of the log can change on the first rotation, and that is correct.
`create` gives the new file the owner of the old one, and then nginx reopens it
and hands it to the `user` of its own config so the workers can write.
Measured on the openresty nodes: logrotate leaves `root:root`, and after the
`USR1` the file is `yuneta:root`.

### Intrusion Banning (`/etc/fail2ban/`)

Two files: the filter `filter.d/yuneta-nginx-probe.conf` and the jails
`jail.d/yuneta-nginx.conf`. `fail2ban` is a `Recommends` (`Requires` on RPM
would be too strong for a package that also runs on nodes with no web server).

The filter does not ban on 404s. Search engines collect 404s honestly all day,
and a rate rule would ban Googlebot before it banned anybody worth banning. It
bans on **what was asked for**: any `.php` path (no node runs PHP), the
dot-directories that hold source control or credentials (`.env`, `.git`,
`.aws`, `.ssh`, `.svn`, `.hg`), and the WordPress surface `/wp-*`. Measured
against 15 days of one node's `access.log`: 52 383 matching lines from 972
addresses, and not one of them was a real crawler or another node of the fleet.

**The response status is not looked at, and that took a fresh node to learn.**
The first version matched only 404, 403 and 444, reasoning that a path some app
really serves would stop matching once it answered 200. That holds for a static
site and collapses on a SPA: with `try_files $uri $uri/ /index.html` every
unknown path answers **200** with `index.html`, so `/wp-login.php` came back 200
and the filter never fired — both yunovatios consoles ran it blind. On a node
serving static sites too, the same restriction hid 606 more lines in a single
day: probes answered 301 by the http→https redirect, and probes answered 200 by
a SPA. Dropping the status costs nothing, because these paths cannot be
legitimate on a Yuneta node.

Several hundred of those lines carry the user agent of Googlebot, GPTBot or
ClaudeBot. Every one is an impostor — the addresses reverse to
`googleusercontent.com` and to Cloudflare, and the real Googlebot does not ask
for `/.env.backup`. Banning them is the point.

**Both jails ship disabled.** If none of a jail's `logpath` globs resolves to a
file, fail2ban does not skip the jail: it refuses to configure and the whole
server exits 255, taking every other jail with it, `sshd` included. A node with
this package whose web server has not run yet is exactly that case. Enable them
once `access.log` exists:

```bash
printf '[yuneta-nginx-probe]\nenabled = true\n[nginx-botsearch]\nenabled = true\n' \
    | sudo tee /etc/fail2ban/jail.d/zz-yuneta-nginx-enabled.conf
sudo systemctl reload fail2ban && sudo fail2ban-client status
```

A separate file, because `jail.d/yuneta-nginx.conf` is a conffile and editing it
earns a prompt on every upgrade. `jail.d` is read in alphabetical order.

Two failures worth knowing, because neither says what is wrong:

- **A jail can watch nothing and still report healthy.** If the node's
  `[DEFAULT]` sets `backend = systemd`, every jail reads the journal and ignores
  `logpath`. `fail2ban-client status` looks normal; only
  `fail2ban-client get <jail> logpath` shows *"No file is currently
  monitored"*. The shipped jails pin `backend = auto` for that reason.
- **A ban can be recorded and never applied.** `banaction` names a command; if
  that command is not installed, fail2ban logs the failure to
  `/var/log/fail2ban.log` and carries on counting bans that do not exist. Check
  the firewall, not the jail: `nft list table inet f2b-table`, or
  `iptables -L f2b-<jail> -n`, and confirm the addresses match
  `fail2ban-client get <jail> banip`.

On a node with **SELinux enforcing** the globs do not work at all: `fail2ban_t`
cannot list `/yuneta/bin`, the denial is dontaudited so no AVC is written, and
the only symptom is the fatal *"Have not found any log file"*. There, override
`logpath` with a literal path in the same `zz-` file and label the directory:

```bash
semanage fcontext -a -t var_log_t "/yuneta/bin/nginx/logs(/.*)?"
restorecon -R /yuneta/bin/nginx/logs
```

### Shell Environment (`/etc/profile.d/yuneta.sh`)

- Adds `/yuneta/bin` and `/yuneta/agent` to `PATH`
- Sets `ulimit -c unlimited`, `ulimit -n unlimited` and `ulimit -l unlimited`
  (the last one so a yuno launched straight from a shell — `ycommand` included —
  gets the same pinned-memory budget as the ones started by the agent)
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
| **Depends** | `adduser`, `lsb-base`, `rsync`, `locales`, `rsyslog`, `init-system-helpers`, `gdb` |
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
sudo /yuneta/bin/install-certbot.sh
```

## Post-Install Checklist

After installing the `.deb`, the `postinst` script reminds you to run:

1. `sudo /yuneta/bin/install-yuneta-dev-deps.sh` - Install build toolchain (only needed for development)
2. `sudo /yuneta/bin/install-certbot.sh` - Install certbot for ACME TLS certificates

## Versioning

Two numbers feed the package name:

- **Framework version** — read from `../../YUNETA_VERSION` (e.g. `YUNETA_VERSION=7.3.4`). Bumped when the framework changes.
- **Package release** — read from `../../RELEASE` (a single number, e.g. `1`). Bumped when re-packaging the **same** framework version (build script changed, deps changed, conffile fixed, etc.). Shared across all four architectures.

The final package name follows the pattern:

```
yuneta-agent-<version>-<release>-<arch>.deb
```

Example: `yuneta-agent-7.3.4-1-amd64.deb`
