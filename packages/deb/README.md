# packages/deb

Debian packaging for the Yuneta Agent. It builds `.deb` packages that install the complete Yuneta runtime under the `/yuneta` hierarchy: agent binaries, CLI tools, web server, init scripts, kernel tuning, TLS certificate management, and a dedicated `yuneta` system user.

> For RHEL/Rocky/Alma, the RPM counterpart is in [`../rpm/`](../rpm/README.md).
> It stages the same `/yuneta` payload and builds an `.rpm` with `rpmbuild`.
> See [`../rpm/README.md`](../rpm/README.md) for the differences on RHEL
> (SysV through `chkconfig`, `wheel` group, langpacks, EPEL certbot, and the
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

Each wrapper reads the framework version from `../../YUNETA_VERSION` and the package release number from `../../RELEASE`. Then it calls `make-yuneta-agent-deb.sh` with the correct parameters. All four architectures share the same release counter. When you package the same framework version again, bump `RELEASE` one time.

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

Generated at build time:

```
packages/deb/
├── build/deb/<arch>/<package>/  <- staging tree
└── dist/<package>.deb           <- final .deb output
```

## How It Works

### Build Pipeline

1. Architecture wrapper (`AMD64.sh` and the others) parses `YUNETA_VERSION` and calls `make-yuneta-agent-deb.sh`
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
- You must execute the wrapper scripts from this directory (`cd packages/deb/`)

## It refuses a node that builds from source

`preinst` aborts the install when `/yuneta/development/yunetas` holds the sources
(`kernel/` and `.git`). The package carries artifacts built on another machine.
It puts them into the tree that `yunetas build` owns (`outputs/`, `outputs_ext/`).
Thus new objects link against archives from a different glibc. A static link
accepts those archives silently and corrupts the heap at run time.

It is a refusal, not a warning. Thus somebody must take the decision, and an
install cannot take it for a build node in passing. To go ahead:

```bash
sudo YUNETAS_FORCE_OVER_SOURCE=1 apt install ./<package>
# or, to make it stick for this node:
sudo touch /etc/yuneta/allow-package-over-source
```

Then rebuild everything on that node, external libraries first
(`kernel/c/linux-ext-libs`: `extrae.sh` + `configure-libs.sh`, then
`yunetas init` and `yunetas clean && yunetas build`). If you rebuild only the
SDK, the packaged externals stay in place. That is the same mismatch by a
shorter road.

## What the .deb Installation Does

When you run `sudo apt install ./yuneta-agent-*.deb`, these steps occur, in order:

### 1. File Extraction

The package manager extracts the full `/yuneta/` tree, the configuration files under `/etc/`, and the `/var/crash/` directory onto the filesystem. It overwrites no existing configuration file. The files under `/etc/` are marked as `conffiles`. The web server configuration is outside `/etc/`, where `conffiles` cannot reach it, so the package does not ship it (see below).

### 2. Post-Installation Script (`postinst`)

The `postinst` script runs automatically after file extraction. It does these actions, in order:

#### 2.1. User and Group Setup
- Creates the `yuneta` login user (if it does not exist) with:
  - Home directory: `/home/yuneta`
  - Shell: `/bin/bash`
  - UID >= 1000 (non-system user, regular login)
  - Disabled password (SSH key or sudo access)
- Adds `yuneta` to these system groups: `adm`, `sudo`, `tty`, `dialout`, `cdrom`, `audio`, `video`, `plugdev`, `netdev`, `render`, `input`, `gpio`, `i2c`, `spi`, `uucp`, `wireshark`, `bluetooth`, `scanner`, `lp`, `lpadmin`, `sambashare`, `docker`, `libvirt`, `kvm`, `lxd`
  - Missing groups are skipped silently (override with `YUNETA_CREATE_MISSING_GROUPS=1`)
  - To change the group list, set the `YUNETA_GROUPS` env var

#### 2.2. Agent Configuration
- Creates `/yuneta/agent/yuneta_agent.json` from the bundled `.sample` file (only if the file does not exist yet)
- Creates `/yuneta/agent/yuneta_agent22.json` from the bundled `.sample` file (only if the file does not exist yet)
- An upgrade never overwrites existing configuration files

#### 2.3. Locale Configuration
- Enables `en_US.UTF-8` and `es_ES.UTF-8` in `/etc/locale.gen`
- Runs `locale-gen` and sets `LANG=en_US.UTF-8`

#### 2.4. Ownership and Permissions
- Sets `/yuneta/` tree ownership recursively to `yuneta:yuneta`
- Sets `/yuneta/store/certs/private/` to mode `0700` (only the owner can access private keys)
- Sets `/var/crash/` to mode `0775` with group `yuneta` (for core dumps). It
  also installs `/usr/lib/tmpfiles.d/yuneta-crash.conf`, so systemd applies it
  again on every boot. Another package can co-own `/var/crash` (`kexec-tools`
  of kdump declares it `root:root 0755`). A one-shot postinst fix holds only
  until the next transaction of that package. After that, cores stop being
  written, with no diagnostic.

- Disables `apport` (`enabled=0` in `/etc/default/apport` +
  `systemctl disable --now`). It is the crash-telemetry client of Ubuntu. It
  exists to ship deduplicated reports to `errors.ubuntu.com`. It discards
  crashes of binaries outside its packaging allowlist (`/yuneta` is not on it).
  Thus, on a Yuneta node, it only costs us core dumps. Note that
  `apport --stop` leaves `core_pattern` as the bare word `core` (dumps land in
  the CWD of the crashing process). Thus the installer applies the sysctl
  again after it stops apport.
- Installs and enables `yuneta-core-pattern.service`, which applies
  `kernel.core_pattern` again after `apport.service`. It is a backstop in case
  an apport upgrade enables apport again. On Ubuntu, apport takes that sysctl
  over at every boot and discards the cores of non-distro binaries. Without
  this service, the yunos silently stop producing core dumps after the first
  reboot. To make sure, run `cat /proc/sys/kernel/core_pattern`. A value that
  starts with `|` means that a crash handler owns it.

#### 2.5. Kernel Parameter Tuning
- Applies `/etc/sysctl.d/99-yuneta-core.conf` via `sysctl --system`

#### 2.5bis. sshd is left alone
- The package does not touch sshd or the `sshd` jail of fail2ban. The
  behavior of sshd is the policy of the operator of the node, and the package
  can be installed on a node of another company. 7.20.0-3 shipped a flood
  drop-in here. The next release removed it from the package.
- The same measures are scripts that an operator runs by hand, shipped at
  `/yuneta/development/yunetas/tools/sshd/`: `audit-sshd.sh`,
  `install-sshd-flood-guard.sh`, `install-fail2ban-sshd-jail.sh`,
  `stop-sshd.sh`, `start-sshd.sh`. See [`tools/README.md`](../../tools/README.md).

#### 2.6. SSH Keys (Optional)
- If `/etc/yuneta/authorized_keys` exists (bundled at build time), the script installs it to `/home/yuneta/.ssh/authorized_keys` with the correct permissions (directory `0700`, file `0600`)

#### 2.7. Syslog Setup
- Enables and starts `rsyslog`, to make `/var/log/syslog` available

#### 2.8. The agents' services
- Installs the systemd units `/usr/lib/systemd/system/yuneta_agent.service`
  and `yuneta_agent22.service` (from `packages/templates/`), and enables both.
  On `configure`/`reconfigure`, it restarts them: the main agent first, and
  agent22 only when the main agent runs. Thus both are never down at the same
  time. It is a restart, not a start, for two reasons. An agent that the SysV
  script started left its generated unit `active (exited)`, and there a start
  does nothing. Also, a restart moves an agent22 that runs outside its unit
  into it. The comments of the units tell why each line is there
  (`Type=forking` + the `--pid-file` of the watcher, `KillMode=process`,
  `Restart=no`).
- `yuneta_agent.service` has the name of the SysV script. Thus, under systemd,
  it replaces the unit generated from `/etc/init.d/yuneta_agent`. The script is
  still installed (with its `update-rc.d` links) for an init that is not
  systemd. Up to 7.25.21 it was the only service, and agent22 had no unit. An
  agent started outside it was invisible to `systemctl`.

#### 2.8bis. Web server service

- Installs `/usr/lib/systemd/system/yuneta-webserver.service` and
  `/yuneta/bin/yuneta-webserver`, and enables and starts the unit.
- The init script no longer starts the web server. Before,
  `/etc/init.d/yuneta_agent` ran nginx and let it daemonize, so nothing owned
  the process afterwards. A later `start` found nothing to look at and tried
  again. The second master died with *"Address already in use"*, while the
  first one kept serving.
- The unit runs the server with `daemon off`, so `$MAINPID` is the real master,
  and stop and reload reach it. `systemctl reload` sends HUP, which reloads the
  configuration. The signal to reopen the log files is USR1. It stays where it
  was, in the `postrotate` of `/etc/logrotate.d/yuneta`.
- On upgrade, the scriptlet asks the old, daemonized server to finish before it
  starts the unit. The reason is that the unit cannot take `:80` and `:443`
  while the old master holds them. That is a short interruption of the web
  server of the node, at upgrade time only.

#### 2.9. PAM Limits
- Adds `session required pam_limits.so` to `/etc/pam.d/common-session` and `/etc/pam.d/common-session-noninteractive` (if not already present)

#### 2.10. Reboot
- The installer never reboots, in either mode. It applies the kernel tuning
  live with `sysctl --system`, so the node does not need a reboot to run. It
  only writes the `/run/reboot-required` flag and recommends a reboot, to make
  sure that the agent comes up at boot.
- Before, this paragraph said that a non-interactive install reboots
  automatically. That is not true since the removal of the auto-reboot. To read
  it before an install on the node of a client is a bad minute to have.

### The node's web server choice

`/etc/yuneta/webserver` holds one word, `nginx` or `openresty`. It is
node state, not package content. Neither package ships it.

The reason is the lesson of nginx.conf in 7.9.1. A file that the package ships
carries the choice of the BUILD machine. The build machine has no choice, so
the file says `nginx`. On 7.11.0 that pushed `nginx` onto two nodes that run
openresty. Both came back up and served from the wrong tree with a default
configuration.

The handling is the same as for nginx.conf, and it is symmetric between the two
flavors:

| Step | What happens |
|---|---|
| `preinst` / `%pre` | Copies the node's value to `/etc/yuneta/webserver.pkgsave` |
| unpack | A file the package no longer provides is removed by the package manager |
| `postinst` / `%posttrans` | Absent → restore from `.pkgsave`. No `.pkgsave` → seed the build default. Present → leave it alone |

Thus a node that already chose keeps its choice. Only a node that never had
one gets a default.

### 3. Package Removal

- `prerm`: on an upgrade, it stops the main agent only (agent22 stays up). On a removal, it stops and disables every unit that the package brought: both agents, `yuneta-webserver.service`, `yuneta-core-pattern.service`. It does this inline, because by `postrm` the files of the package are gone. Up to 7.25.21, `postrm` did this through a script of the package, which no longer existed, so agent22 kept running from a deleted binary.
- `postrm remove`: Removes SysV runlevel symlinks (keeps conffiles) and reloads systemd
- `postrm purge`: Also deletes `/etc/init.d/yuneta_agent`

### 4. Configuration Files Preserved on Upgrade

These files are marked as `conffiles`, and an upgrade does not overwrite them:
- `/etc/profile.d/yuneta.sh`
- `/etc/sysctl.d/99-yuneta-core.conf` (since the release after 7.31.28)
- `/etc/security/limits.d/99-yuneta-core.conf` (since the release after 7.31.28)
- `/etc/sudoers.d/90-yuneta`
- `/etc/init.d/yuneta_agent`
- `/etc/letsencrypt/renewal-hooks/deploy/reload-certs`
- `/etc/logrotate.d/yuneta`
- `/etc/fail2ban/filter.d/yuneta-nginx-probe.conf`
- `/etc/fail2ban/jail.d/yuneta-nginx.conf`
- `/etc/yuneta/authorized_keys` (if bundled)

Every file that the package puts in `/etc` must be in this list. The package
does not ship `/etc/yuneta/webserver`: `postinst` writes it once and keeps it.
The full list of what the package puts outside `/yuneta` is the page
[Files outside `/yuneta`](https://doc.yuneta.io/files-outside-yuneta)
(`docs/doc.yuneta.io/files-outside-yuneta.md`). Update that page in the same
commit when you add a file outside `/yuneta`.

The web server configuration is preserved by a different mechanism, because it
is under `/yuneta/bin/`, where `conffiles` does not apply. The `.rpm` cannot
tag it either: `%files` lists `/yuneta` as a directory, so rpmbuild rejects a
second entry for a file inside it. Instead, these files are stripped from the
payload. The package cannot replace a file that it does not contain:

- `/yuneta/bin/nginx/conf/nginx.conf` and `conf/conf.d/`
- `/yuneta/bin/openresty/nginx/conf/nginx.conf` and `conf/conf.d/`

But not to ship them is only half the job. dpkg deletes files that an upgrade
no longer provides. Thus an upgrade from a package that still owned
`nginx.conf` (7.9.0 or older) removes it, if nothing keeps a copy. Since 7.9.2,
`preinst` copies it to `nginx.conf.pkgsave` before the unpack, and `postinst`
restores from that copy.

Thus `postinst` picks, in order:

1. `nginx.conf` already there → leave it alone.
2. `nginx.conf.pkgsave` → restore the configuration of the node (an upgrade
   that dropped it).
3. `nginx.conf.default` → seed a first install.

It also creates `conf.d/` when it is missing. Until 7.9.1 the package carried
the copies of the build machine and overwrote the copies of the node on every
upgrade. 7.9.1 fixed the overwrite but deleted the file on the way, and 7.9.2
fixes that.

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
│   ├── yuneta_agent22                   <- secondary agent (yuno_agent22)
│   ├── yuneta_agent44                   <- agent22 variant (main44.c)
│   ├── yuneta_agent.json                <- agent config (created by postinst)
│   ├── yuneta_agent22.json              <- agent22 config (created by postinst)
│   └── service/                         <- service management helpers
│       ├── install-yuneta-service.sh
│       ├── remove-yuneta-service.sh
│       ├── restart-yuneta
│       ├── install-certbot.sh
│       ├── install-yuneta-dev-deps.sh
│       ├── check-certs-validity.sh
│       └── colas2.sh
├── gui/                                 <- web UI
├── realms/                              <- configuration realms
├── repos/                               <- repositories
├── store/
│   ├── certs/                           <- SSL certificates
│   │   ├── copy-certs.sh                <- run by the certbot deploy hook
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
├── fail2ban/filter.d/yuneta-nginx-probe.conf <- fail2ban filter
├── fail2ban/jail.d/yuneta-nginx.conf    <- fail2ban jails (disabled)
├── letsencrypt/renewal-hooks/deploy/
│   └── reload-certs                     <- certbot deploy hook
└── yuneta/
    ├── authorized_keys                  <- (optional) SSH keys for yuneta user
    └── webserver                        <- web server selection, written by postinst

/usr/lib/systemd/system/                 <- yuneta_agent, yuneta_agent22, yuneta-webserver, yuneta-core-pattern units
/usr/lib/tmpfiles.d/yuneta-crash.conf    <- /var/crash ownership at each boot
/usr/libexec/yuneta/                     <- yuneta-node-uuid, yuneta-agent-cert, yuneta-node-label, yuneta-label-print (run as root)
/usr/share/doc/yuneta-agent/             <- copyright, THIRD-PARTY-NOTICES

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
| `tr2check` | Check a timeranger2 topic filled by a load test |
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

Each agent has a corresponding `.json` configuration file, which `postinst` generates on the first install.

## Service Management Helper Scripts

| Script | Location | Purpose |
|--------|----------|---------|
| `install-yuneta-service.sh` | `/yuneta/agent/service/` | Replace an `/etc/init.d/yuneta_agent` that differs (the old one kept as `yuneta_agent.<date>.bak`, not executable). Where `yuneta_agent.service` exists, enable and restart the systemd units of both agents (exit 1 when a restart fails). If not, install, enable and start the SysV init script |
| `remove-yuneta-service.sh` | `/yuneta/agent/service/` | Disable and stop the units of both agents and the unit of the web server. Stop the SysV service and remove its runlevel symlinks (the init script stays: it is a conffile) |
| `restart-yuneta` | `/yuneta/bin/` | Restart the Yuneta stack (`service` first, if not available `yshutdown` + direct start) |
| `install-certbot.sh` | `/yuneta/bin/` | Install certbot through snap, and symlink it to `/usr/bin/certbot` |
| `install-yuneta-dev-deps.sh` | `/yuneta/bin/` | Install full build toolchain (gcc, cmake, libs, gh, pipx packages) |
| `check-certs-validity.sh` | `/yuneta/bin/` | Scan `*.crt` files and show expiry status |
| `copy-certs.sh` | `/yuneta/store/certs/` | Copy Let's Encrypt certificates from `/etc/letsencrypt/live/` to `/yuneta/store/certs/` (from `packages/templates/copy-certs.sh`). Root reads the keys, and `yuneta` writes the copies. It checks the names of `certs.list` and the paths of the keys. The certbot hook (`packages/templates/reload-certs`) and the agent's cert sync run it as root |
| `colas2.sh` | `/yuneta/bin/` | Scan and list two-level message queue directories |
| `yuneta-node-uuid` | `/usr/libexec/yuneta/` | Write the node uuid (`/yuneta/store/agent/uuid/uuid.json`) as a function of the machine. On a box that has an identifier burned into it (SoC serial, devicetree `serial-number`), the source is the chip. On any other machine (x86, a VM), the source is the DMI uuid AND the lowest permanent on-board MAC. The uuid is the same after a reinstall, and two machines never share it. The `postinst`, both agent units (`ExecStartPre=-+`) and the SysV start run it as root. It reads the hardware as root and drops to `yuneta` for the files. If the hardware no longer gives a stored uuid, the tool replaces it (and keeps a copy), unless something is only missing at that start. In a container the uuid is random (the `/sys` there is the host's), and it belongs to the hostname of the container. There, the `postinst` (`ensure --installing`, exit 3) stores no uuid and, without systemd, does not start the agents. Thus an image built with the package carries no uuid. A chroot (`/` is not the root of PID 1, or no `/proc`: an image built over an exported or a debootstrapped rootfs) is treated as a container. Two copies with the same hostname and the same stored uuid share it (an image committed from a container that has one). Thus delete `uuid.json` before a `docker commit` / `docker export`. Anything out of the initial pid namespace is a container, with or without a marker file (a BuildKit `RUN` step, containerd). So is a `--pid=host` container (Toolbx, distrobox). The test is that `/` is not the root of PID 1. Where the tool cannot look at the root of PID 1, the test is that the mount on `/` is not the mount of PID 1 (`/proc/1/mountinfo`). That happens when the root of Docker has no `CAP_SYS_PTRACE`, when the root of Toolbx is in a user namespace, or when a security module refuses any root. A root without `CAP_SYS_PTRACE` on the box itself has the mount of PID 1 on `/` and reads the hardware. Where the tool cannot read `/proc/1/mountinfo` either, a root in a user namespace or without `CAP_SYS_PTRACE` is a container. A board written from a `docker export` keeps `/.dockerenv`, which counts only out of the initial pid namespace. In the initial namespace, only what this boot made counts: the `container=` of PID 1, `/run/systemd/container` (an Ubuntu image ships it with `docker`) or `/run/host/container-manager` only on a tmpfs `/run`, and of `systemd-detect-virt` only `wsl`, `openvz` and `proot`. Such a board reads its hardware. With no identifier there, it gets a new random uuid, not the uuid of the container. `show` tells the source, `set <uuid>` pins one by hand, `unset` goes back to the uuid of the hardware. After a change, restart both agents and every yuno, because they read it one time, when they start. The tool is root-owned and outside `/yuneta` on purpose, because root runs it |
| `yuneta-node-label` | `/usr/libexec/yuneta/` | Print the label of the box: its node uuid as a QR code (in upper case, no URL), the first 8 characters in big type, the model, and the MAC of each on-board wired port. The output is text, or an SVG 62 mm wide with `--svg <file>` (`-` for stdout, and then the text goes to stderr). `--title` adds a line on top. `--print` prints the label on a Brother QL label printer on a USB port (`--printer <printer>` names it). The tool reads `yuneta-node-uuid show`, and it refuses to make a label when the stored uuid is not the uuid of this hardware, or when the uuid is `random` or `manual`. Nothing runs it: an installer runs it by hand when the box is installed. The SVG needs `qrencode`, and `--print` also needs `rsvg-convert` and `python3` (all are `Recommends`) |
| `yuneta-label-print` | `/usr/libexec/yuneta/` | Send a PNG 696 dots wide or narrower to a Brother QL label printer (the QL-700 is tested) with a 62 mm roll. It uses the raster protocol of the printer, through usbfs (`/dev/bus/usb`), so it needs no driver and no `usblp`. It asks the printer for its status first, and refuses to print on an error or on another roll. `--status` gives the status only. `yuneta-node-label --print` runs it |
| `ylabel` | `/yuneta/bin/` | The short command of the operator: print the label of the box on its Brother QL printer, with the title `CONTROLLER` (`ylabel <title>` for another). It runs `/usr/libexec/yuneta/yuneta-node-label --title <title> --print` with `sudo`. Run it as `yuneta`, without `sudo`: it asks `sudo` for the root-owned tool only |

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

`nofile unlimited` is not infinite. Linux refuses that value for open files,
and pam_limits gives `fs.nr_open` instead (4000000, see the sysctl table above).
Do not set a number of your own in a shell startup file. `ulimit -n N` also
sets the HARD limit, and a user process can never raise it again. Thus that
shell, and every agent or yuno started from it, stays at N.

`memlock` is not optional. io_uring rings are pinned memory charged against
`RLIMIT_MEMLOCK`. The budget is for each user, shared by every yuno that runs
as `yuneta`. A yuno with `io_uring_entries=32768` pins ~3.1 MB (SQEs
`32768*64` + CQEs `65536*16` + SQ array). Thus the usual 8 MB default admits
only two of them. From the third yuno on, `yev_loop_create()` fails with
`ENOMEM`, the yuno aborts at startup, and the ydaemon watcher relaunches it
indefinitely. The symptom is a node with gigabytes of free RAM whose yunos die
of "out of memory". It is a limit, never a shortage.

### Log Rotation (`/etc/logrotate.d/yuneta`)

nginx has no rotation of its own. It only reopens its files when it gets
`USR1`. Thus, without this drop-in, `access.log` and `error.log` grow for the
life of the node. When the drop-in was added, no node had ever rotated a log.
The busiest node wrote 3.5 MB a day.

| Path | Period | Keeps |
|------|--------|-------|
| `/yuneta/bin/nginx/logs/*.log` | daily | 30, compressed |
| `/yuneta/bin/openresty/nginx/logs/*.log` | daily | 30, compressed |
| `/var/log/yuneta/*.log` | monthly | 12, compressed |

Both web server trees are listed. A node runs one or the other (the choice is
in `/etc/yuneta/webserver`), and `missingok` covers the tree that is absent.
The `postrotate` reads each `nginx.pid` and sends `USR1` only to a live
process named `nginx` (asked through `/proc/<pid>/comm`). Thus a stale pid file
cannot make it signal an unrelated process. When it finds no live master, it
writes that in the logrotate output (the journal of `logrotate.service`), and
it does not skip in silence.

It does not use `kill -0` for that check, and the reason is RHEL/Rocky. There,
logrotate runs in the SELinux domain `logrotate_t`. That domain can send the
master `USR1` (`signal`), but not the null signal (`signull`). The policy does
not even audit that denial: no AVC appears unless the `dontaudit` rules are
switched off (`semodule -DB`). The first version guarded the signal with
`if kill -0 "$pid" 2>/dev/null`. Thus, on a Rocky 9 test node, the signal was
skipped every night, and the service still ended "successfully". nginx
continued to write to the rotated `access.log.1` until the next restart of the
web server. The symptom is a live file that does not grow while the `.1` does.
The `webstats` yuno warns about exactly that (*"Log rotation STALLED"*). To
examine a node after the nightly rotation, run:

```bash
ls -l --time-style=+%F_%T /yuneta/bin/nginx/logs/access.log /yuneta/bin/nginx/logs/access.log.1
# the live access.log must be the one with the newest mtime
```

The yunos do not rotate here. Each one writes numbered files under
`/yuneta/realms/<realm>/<yuno>/logs/` and rotates them itself.

The package `Depends` on `logrotate` (`Requires` on RPM), and the drop-in is a
conffile. Thus a node that edits it keeps its version across upgrades.

The owner of the log can change on the first rotation, and that is correct.
`create` gives the new file the owner of the old one. Then nginx reopens it and
gives it to the `user` of its own configuration, so that the workers can write.
Measured on the openresty nodes: logrotate leaves `root:root`, and after the
`USR1` the file is `yuneta:root`.

### Intrusion Banning (`/etc/fail2ban/`)

Two files: the filter `filter.d/yuneta-nginx-probe.conf` and the jails
`jail.d/yuneta-nginx.conf`. `fail2ban` is a `Recommends`. A `Requires` on RPM
is too strong for a package that also runs on nodes with no web server.

The filter does not ban on 404s. Search engines collect 404s honestly all day,
and a rate rule bans Googlebot before it bans anybody worth banning. It
bans on what was asked for: any `.php` path (no node runs PHP), the
dot-directories that hold source control or credentials (`.env`, `.git`,
`.aws`, `.ssh`, `.svn`, `.hg`), and the WordPress surface `/wp-*`. Measured
against 15 days of the `access.log` of one node: 52 383 matching lines from 972
addresses, and not one of them was a real crawler or another node of the fleet.

The filter does not look at the response status, and a fresh node taught that.
The first version matched only 404, 403 and 444. The reason was that a path
that some app really serves stops matching when it answers 200. That holds for
a static site and fails on a SPA. With `try_files $uri $uri/ /index.html`,
every unknown path answers 200 with `index.html`. Thus `/wp-login.php` came
back 200, and the filter never fired. On a node that also serves static sites,
the same restriction hid 606 more lines in a single day: probes answered 301 by
the http→https redirect, and probes answered 200 by a SPA. To drop the status
costs nothing, because these paths cannot be legitimate on a Yuneta node.

Several hundred of those lines carry the user agent of Googlebot, GPTBot or
ClaudeBot. Every one is an impostor. The addresses reverse to
`googleusercontent.com` and to Cloudflare, and the real Googlebot does not ask
for `/.env.backup`. To ban them is the purpose.

Both jails ship disabled. If none of the `logpath` globs of a jail resolves to
a file, fail2ban does not skip the jail. It refuses to configure, and the whole
server exits 255, with every other jail, `sshd` included. A node with this
package whose web server did not run yet is exactly that case. When
`access.log` exists, enable them:

```bash
printf '[yuneta-nginx-probe]\nenabled = true\n[nginx-botsearch]\nenabled = true\n' \
    | sudo tee /etc/fail2ban/jail.d/zz-yuneta-nginx-enabled.conf
sudo systemctl reload fail2ban && sudo fail2ban-client status
```

The enable goes in a separate file, because `jail.d/yuneta-nginx.conf` is a
conffile, and an edit to it earns a prompt on every upgrade. fail2ban reads
`jail.d` in alphabetical order.

Two failures are worth knowing, because neither says what is wrong:

- A jail can watch nothing and still report healthy. If the `[DEFAULT]` of the
  node sets `backend = systemd`, every jail reads the journal and ignores
  `logpath`. `fail2ban-client status` looks normal. Only
  `fail2ban-client get <jail> logpath` shows *"No file is currently
  monitored"*. The shipped jails pin `backend = auto` for that reason.
- A ban can be recorded and never applied. `banaction` names a command. If
  that command is not installed, fail2ban logs the failure to
  `/var/log/fail2ban.log` and continues to count bans that do not exist.
  Examine the firewall, not the jail: `nft list table inet f2b-table`, or
  `iptables -L f2b-<jail> -n`. Make sure that the addresses match
  `fail2ban-client get <jail> banip`.

On a node with SELinux enforcing, the globs do not work at all. `fail2ban_t`
cannot list `/yuneta/bin`, and the denial is dontaudited, so no AVC is written.
The only symptom is the fatal *"Have not found any log file"*. There, override
`logpath` with a literal path in the same `zz-` file, and label the directory:

```bash
semanage fcontext -a -t var_log_t "/yuneta/bin/nginx/logs(/.*)?"
restorecon -R /yuneta/bin/nginx/logs
```

### Shell Environment (`/etc/profile.d/yuneta.sh`)

- Adds `/yuneta/bin` and `/yuneta/agent` to `PATH`
- Sets `ulimit -c unlimited` and `ulimit -l unlimited`. The second one gives a
  yuno launched straight from a shell (`ycommand` included) the same
  pinned-memory budget as the yunos that the agent starts
- Raises the soft limit of open files to the hard one (`ulimit -Sn "$(ulimit -Hn)"`).
  pam_limits sets the hard limit to `fs.nr_open` (4000000). A shell cannot raise
  it, and Linux refuses `unlimited` for open files. systemd starts a desktop
  terminal with a soft limit of 1024.
- Defines aliases: `y` (cd to yunetas), `salidas` / `outputs` (cd to outputs), `logs` (cd to logs)

### Sudoers (`/etc/sudoers.d/90-yuneta`)

```
yuneta ALL=(ALL) NOPASSWD:ALL
```

Grants the `yuneta` user passwordless sudo for all commands.

### SysV Init Script (`/etc/init.d/yuneta_agent`)

- Starts `yuneta_agent` and `yuneta_agent22` on boot (runlevels 2 3 4 5)
- Controls the selected web server (nginx or openresty, per `/etc/yuneta/webserver`)
- Raises resource limits (`ulimit`) before launching agents: open files to
  `fs.nr_open` (4000000), the same value pam_limits gives a login
- Supports standard `start`, `stop`, `restart`, `force-reload`, `status` operations
- Runs agents as user `yuneta` through `su -s /bin/sh`

Under systemd (since 7.25.22-3), the script drives the units of the two
agents, and it does not start the agents by hand:

- `start` starts `yuneta_agent.service`. When it is up, it starts
  `yuneta_agent22.service`. Thus a broken binary never takes both down.
- `stop` stops the main agent alone. It leaves agent22, the escape hatch,
  running, as the SysV `stop` always did.
- `restart` restarts the main agent, and starts agent22 only if it was not
  running.
- `status` reports both.

An agent that runs outside its unit (started by hand with `--start`) is
stopped first and started in its unit. The script recognizes it by the base
name of its `argv[0]` (`/proc/<pid>/cmdline`), as its own `--stop` does. The
`argv[0]` of the script is its shell. Up to 7.30.17 the script used
`/proc/<pid>/exe`, which misses an agent behind a symlinked `/yuneta`, a
renamed binary, or a root without `CAP_SYS_PTRACE`.
`service yuneta_agent22 stop` or `systemctl` drive one unit alone. All of it
needs root, as `su - yuneta` always did.

```bash
sudo /etc/init.d/yuneta_agent restart     # the main agent; agent22 untouched
sudo systemctl restart yuneta_agent22     # agent22 alone
```

## Package Metadata

### Dependencies

| Type | Packages |
|------|----------|
| Depends | `adduser`, `lsb-base`, `rsync`, `locales`, `rsyslog`, `init-system-helpers`, `gdb` |
| Recommends | `curl`, `vim`, `sudo`, `tree`, `pipx`, `fail2ban`, `net-tools`, `locate`, `qrencode` (for `yuneta-node-label --svg`), `librsvg2-bin` and `python3` (for `yuneta-node-label --print`) |
| Suggests | Full development toolchain (`git`, `cmake`, `gcc`, `clang`, `python3-dev`, crypto/compression libs, and others) |

## Optional Build-Time Configuration

### SSH Keys

Put SSH public keys in `authorized_keys/authorized_keys` before you build. The package installation copies them to the `~/.ssh/authorized_keys` of the `yuneta` user.

```bash
mkdir -p authorized_keys
cp ~/.ssh/id_rsa.pub authorized_keys/authorized_keys
```

### Web Server Selection

To choose between nginx and openresty (default: nginx), create a `webserver/webserver` file:

```bash
mkdir -p webserver
echo "openresty" > webserver/webserver
```

### Let's Encrypt Integration

The package installs a certbot deploy hook at `/etc/letsencrypt/renewal-hooks/deploy/reload-certs` that:

1. Copies renewed certificates from `/etc/letsencrypt/live/` to `/yuneta/store/certs/`
2. Reloads the selected web server
3. Restarts the Yuneta stack in the background

If `/yuneta/store/certs/certs.list` exists, the hook updates only the listed certificates. If not, it discovers the certificates automatically.

To install certbot after you install the .deb, run:

```bash
sudo /yuneta/bin/install-certbot.sh
```

## Post-Install Checklist

After installing the `.deb`, the `postinst` script reminds you to run:

1. `sudo /yuneta/bin/install-yuneta-dev-deps.sh` - Install the build toolchain (only for development)
2. `sudo /yuneta/bin/install-certbot.sh` - Install certbot for ACME TLS certificates

## Versioning

Two numbers feed the package name:

- Framework version: read from `../../YUNETA_VERSION` (for example `YUNETA_VERSION=7.3.4`). Bumped when the framework changes.
- Package release: read from `../../RELEASE` (a single number, for example `1`). Bumped when you package the same framework version again (build script changed, deps changed, conffile fixed, and similar). Shared across all four architectures.

The final package name follows the pattern:

```
yuneta-agent-<version>-<release>-<arch>.deb
```

Example: `yuneta-agent-7.3.4-1-amd64.deb`
