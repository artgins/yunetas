# packages/rpm

This directory makes the RPM package of the Yuneta Agent for RHEL, Rocky, Alma
and CentOS Stream 9 or later. It is the counterpart of the Debian packaging in
[`../`](../README.md). It stages the same `/yuneta` runtime tree and the same
helpers and configuration files. Then it packages them with `rpmbuild` instead
of `dpkg-deb`.

## Quick Start

```bash
# 1. Build yunetas first (see top-level CLAUDE.md)
source yunetas-env.sh
yunetas init && yunetas build

# 2. Run from THIS directory
cd packages/rpm/

# 3. Build for your architecture
./x86_64.sh         # x86_64 (Intel/AMD)
./aarch64.sh        # ARM64
./riscv64.sh        # RISC-V 64-bit

# 4. Install the resulting .rpm
sudo dnf -y install ./dist/yuneta-agent-7.5.6-1.el9.x86_64.rpm
```

Read these points before you install:

- On a node that builds from source, `%pre` stops the install. See
  [It refuses a node that builds from source](#it-refuses-a-node-that-builds-from-source).
- The package enables io_uring in the kernel (`kernel.io_uring_disabled = 0`).
  If your security policy forbids io_uring, read
  [io_uring](#io_uring-critical-on-rhel) before you build.
- If the agent does not run after the install, read the warning at the end of
  `%post`. See [What `%post` does](#what-post-does-in-order).

> ℹ️ The GitHub workflow
> [`release-packages.yml`](../../.github/workflows/release-packages.yml)
> publishes the package on each release. It runs `x86_64.sh` on every
> published Release and uploads the `.rpm` as a release asset, next to the
> AMD64 `.deb`. It builds on the Ubuntu runner. The framework is fully static
> by default, so those binaries also run on RHEL/Rocky. Use the local
> procedure above for other architectures or for custom build options.

Each wrapper reads the framework version from `../../YUNETA_VERSION` and the
package release number from `../../RELEASE`. The `.deb` uses the same two
files. Then the wrapper calls `make-yuneta-agent-rpm.sh`. As with the `.deb`,
the `<arch>` argument is only a label. The package holds the binaries that are
in `/yuneta/bin` and `/yuneta/agent` at that time. For a real package of
another architecture, those directories must hold cross-built binaries.

## File Layout

```
packages/rpm/
├── x86_64.sh                   <- arch wrapper (Intel/AMD)
├── aarch64.sh                  <- arch wrapper (ARM64)
├── riscv64.sh                  <- arch wrapper (RISC-V 64-bit)
├── make-yuneta-agent-rpm.sh    <- main packaging script
├── README.md                   <- this file
├── authorized_keys/            <- (optional) SSH keys to bundle
│   └── authorized_keys
└── webserver/                  <- (optional) web server selection
    └── webserver               <- contents: "nginx" or "openresty"
```

The build makes these files:

```
packages/rpm/
├── build/rpm/<arch>/stage/     <- staging tree (the /yuneta + /etc + /var payload)
├── build/rpm/<arch>/rpmbuild/  <- rpmbuild _topdir
├── build/rpm/<arch>/yuneta-agent.spec
└── dist/<package>.rpm          <- final .rpm output
```

## Prerequisites

```bash
sudo dnf install rpm-build       # rpmbuild
# optional, for inspection:
sudo dnf install rpmlint rpmdevtools
```

## Differences from the `.deb` (RHEL-specific)

The payload (the `/yuneta` tree, the init script, the helpers and the
configuration files) is the same in both packages. Only the packaging metadata
and the install and remove logic are different:

| Concern            | `.deb` (Debian)                       | `.rpm` (RHEL/Rocky)                          |
|--------------------|---------------------------------------|----------------------------------------------|
| Package metadata   | `DEBIAN/control`                      | `.spec` (`Requires` / `Recommends`)          |
| Pre-install hook   | `preinst`                             | `%pre` (`$1`: 1 install, 2 upgrade)          |
| Install hook       | `postinst`                            | `%post` (`$1`: 1 install, 2 upgrade)         |
| End-of-transaction | *(none: `postinst` runs late enough)*  | `%posttrans` (after the old files are erased) |
| Pre-remove hook    | `prerm`                               | `%preun` (`$1`: 0 remove, 1 upgrade)         |
| Post-remove hook   | `postrm`                              | `%postun` (`$1`: 0 remove, 1 upgrade)        |
| Create user        | `adduser --disabled-password`         | `useradd -m` + `passwd -l`                    |
| sudo group         | `sudo`                                | `wheel`                                       |
| Locales            | `locale-gen` / `update-locale`        | `glibc-langpack-{en,es}` + `/etc/locale.conf` |
| Agent services     | systemd units (+ `update-rc.d` for the SysV script) | systemd units (+ `chkconfig` for the SysV script) |
| Conffiles          | `DEBIAN/conffiles`                    | `%config(noreplace)`                          |
| certbot helper     | `install-certbot.sh` (snap)      | `install-certbot.sh` (EPEL `dnf`)            |
| dev-deps helper    | `install-yuneta-dev-deps.sh` (apt)    | same name, `dnf` + EPEL + CRB                 |
| PAM limits         | `common-session*`                     | `system-auth` / `password-auth` (default-on) |
| Log rotation       | `Depends: logrotate`                  | `Requires: logrotate` (base repo, no EPEL)   |
| Intrusion banning  | `Recommends: fail2ban`, banaction nftables/iptables | `Recommends: fail2ban` (EPEL), banaction firewalld |

The `/etc/logrotate.d/yuneta` drop-in is the same in both packages: the same
paths, the same periods and the same `postrotate`. The *Log Rotation* section
of [`../deb/README.md`](../deb/README.md) tells what it rotates and why.

The fail2ban filter and jails are also the same in both packages, and both
packages ship the jails disabled. See the *Intrusion Banning* section of
[`../deb/README.md`](../deb/README.md). On RHEL, read the SELinux note there
before you enable the jails. On RHEL the `logpath` globs cannot work, and they
fail with no message that names SELinux.

### io_uring (critical on RHEL)

The event loop of Yuneta (`yev_loop`) uses Linux io_uring. RHEL 9, Rocky 9
and Alma 9 ship `kernel.io_uring_disabled=2` as a hardening default. That value
disables io_uring fully, so every yuno aborts at startup. For this reason the
`.rpm` ships `kernel.io_uring_disabled = 0` in
`/etc/sysctl.d/99-yuneta-core.conf`, and `%post` applies it with
`sysctl --system`. If your security policy forbids io_uring, remove that line
before you build. Yuneta will then not run.

SELinux is a second gate, independent of the first. An `Enforcing` policy can
deny `io_uring_setup(2)` to a confined service, also with
`kernel.io_uring_disabled=0`. The agent then still aborts. If the agent does
not start while io_uring is enabled, read the output of `getenforce` and the
audit log (`ausearch -m AVC -ts recent`). The agent can need a permissive
domain or an explicit policy allowance.

SELinux also decides whether the web server unit can start at all. `/yuneta`
is outside the policy, so SELinux labels everything under it `default_t`. The
SysV script can execute such a file, because `initrc_t` has that permission.
systemd does not have it. `yuneta-webserver.service` then dies with `203/EXEC`
and *"Permission denied"* on a file whose Unix mode is `rwxr-xr-x`. For this
reason `%post` labels `bin_t` each file of `/yuneta` that systemd starts:
`/yuneta/bin/yuneta-webserver`, `/yuneta/agent/yuneta_agent`,
`/yuneta/agent/yuneta_agent22`, and `/yuneta/bin/restart-yuneta` (the certbot
hook runs it in a transient unit). It uses `semanage fcontext` +
`restorecon`, with a `chcon` fallback, and `%postun` takes the rules out.
If a node starts without a web server, read
`ls -Z /yuneta/bin/yuneta-webserver` first.

### The node's web server choice

`/etc/yuneta/webserver` holds one word, `nginx` or `openresty`. It is node
state, not package content. Neither package ships it.

The reason is the lesson that `nginx.conf` taught in 7.9.1. If the package
ships the file, the file carries the choice of the BUILD machine. The build
machine has no choice, so the file says `nginx`. In 7.11.0 that file put
`nginx` on two nodes that run openresty. Both nodes came back up and served
from the wrong tree with a default configuration.

The packages handle this file as they handle `nginx.conf`. The `.deb` and the
`.rpm` do the same steps:

| Step | What happens |
|---|---|
| `preinst` / `%pre` | Copies the value of the node to `/etc/yuneta/webserver.pkgsave` |
| unpack | The package manager removes a file that the package no longer provides |
| `postinst` / `%post` | Absent → restore from `.pkgsave`. No `.pkgsave` → seed the build default. Present → leave it alone. |

As a result, a node that already has a choice keeps it. Only a node that never
had one gets a default.

One upgrade does not keep the choice: an `.rpm` upgrade from 7.11.0-1/-2.
Those packages still shipped the file. At `%post` the file of the old package
is still there, so `%post` restores nothing. rpm removes the file after
`%post`, and `%posttrans` restores only `nginx.conf`. On such a node, restore
the choice by hand:

```bash
sudo cp -a /etc/yuneta/webserver.pkgsave /etc/yuneta/webserver   # or webserver.rpmsave
```

The scriptlets are the scripts that the package manager runs during an
install. The default is the choice of the build. The build writes it into the
scriptlets as TEXT. From 7.12.0 to 7.30.26 the scriptlets named the build
variable instead (`${WEB_CHOICE}`, inside a quoted heredoc). That variable does
not exist on the node. As a result, the first install on a fresh node died with
*"WEB_CHOICE: parameter not set"* and left the package unconfigured.
`install.sh` seeds `nginx` when the node has no choice, so those packages
install with it. To do the same by hand:

```bash
printf 'nginx\n' | sudo tee /etc/yuneta/webserver   # or openresty
sudo dpkg --configure -a                             # .deb; a .rpm: sudo dnf reinstall yuneta-agent
```

### No automatic reboot

The postinst of the `.deb` offers or forces a reboot. The `.rpm` does not.
`dnf` runs `%post` non-interactively. An automatic reboot on each install or
upgrade is a surprise there, and it is not the usual practice of an RPM.
`sysctl --system` applies the kernel tuning live. If your kernel needs a reboot
for other reasons, reboot.

## It refuses a node that builds from source

`%pre` aborts the install when `/yuneta/development/yunetas` holds the sources
(`kernel/` and `.git`). The package carries artifacts built on another machine.
It puts them into the tree that `yunetas build` owns (`outputs/`,
`outputs_ext/`). New objects then link against archives from a different glibc.
A static link accepts those archives silently and corrupts the heap at run
time.

It is a refusal, not a warning, because the install must be the decision of a
person. It must not be a side effect of an install on a build node. To
continue:

```bash
sudo YUNETAS_FORCE_OVER_SOURCE=1 dnf install ./<package>.rpm
# or, to make it stick for this node:
sudo touch /etc/yuneta/allow-package-over-source
```

Then rebuild everything on that node, external libraries first
(`kernel/c/linux-ext-libs`: `extrae.sh` + `configure-libs.sh`, then
`yunetas init` and `yunetas clean && yunetas build`). If you rebuild only the
SDK, the packaged external libraries stay in place. That is the same mismatch
by a shorter road.

## What `%post` does (in order)

1. Creates the `yuneta` login user (locked password) if it does not exist.
   Makes sure that `/home/yuneta` exists.
2. Adds `yuneta` to a broad set of access groups (`wheel`, `dialout`, `tty`,
   `kvm`, `libvirt`, `docker`, …). It skips the groups that do not exist. To
   change the set, use `YUNETA_GROUPS`. To create the missing groups, use
   `YUNETA_CREATE_MISSING_GROUPS=1`.
3. Creates `/yuneta/agent/yuneta_agent*.json` from the bundled `.sample`. It
   never overwrites a file. The optional `YUNETA_OWNER=` substitutes
   `node_owner`.
4. Sets `LANG=en_US.UTF-8` in `/etc/locale.conf` if it is not set.
5. Runs `chown -R yuneta:yuneta /yuneta`. Sets the private keys directory to
   `0700` and `/var/crash` to `0775 root:yuneta`. Then runs
   `systemd-tmpfiles --create` on the bundled
   `/usr/lib/tmpfiles.d/yuneta-crash.conf`. `kexec-tools` (kdump) also owns
   `/var/crash`, and it declares the directory `root:root 0755`. A later
   transaction that touches that package then silently reverts the group and
   the mode, and the kernel stops writing cores. The tmpfiles drop-in sets them
   again at each boot.
6. Runs `sysctl --system`. This applies the tuning, io_uring included.
6bis. The package leaves sshd alone. It does not touch sshd or the `sshd` jail
   of fail2ban, because that is the policy of the operator of the node.
   7.20.0-3 shipped a flood drop-in and enabled the jail. The next package
   removed both. The same measures are scripts that you run by hand, at
   `/yuneta/development/yunetas/tools/sshd/` (see
   [`tools/README.md`](../../tools/README.md)).
7. Installs the bundled `authorized_keys` for `yuneta`, if the file is present.
8. Enables `rsyslog`.
9. Installs the SysV script with `chkconfig` (for an init that is not
   systemd). Enables the units `yuneta_agent.service` and
   `yuneta_agent22.service`, and RESTARTS them: the main agent first, agent22
   only after the main agent runs. A start is not enough. `%pre` stopped the
   agent without the knowledge of the generated unit, and that left the unit
   `active (exited)`, where a start does nothing. `%pre` and `%post` find the
   running agent by the base name of its `argv[0]`, as its own `--stop` does.
   Up to 7.30.17 they used `/proc/<pid>/exe`. They then did not find an agent
   behind a symlinked `/yuneta`, on a renamed binary, or under a root without
   `CAP_SYS_PTRACE`, and `%pre` left the old agent running. The restart
   happens only when io_uring is enabled. `%post` tests this again after the
   `sysctl --system` of step 6. It captures the real result. It reports a
   disabled io_uring, or any other start failure. It does not hide them behind
   the RPM transaction, which always says "Complete". Under systemd the SysV
   script itself drives the pair of units (since 7.25.22-3). `start` starts
   both, agent22 after the main agent is up. `stop` stops the main agent
   alone. `status` shows both. Up to 7.25.22-2 the script was not redirected
   on RHEL at all. A `/etc/init.d/yuneta_agent start` then started the agents
   by hand, outside their units.
10. Makes sure that `pam_limits.so` is in `system-auth`/`password-auth`. RHEL
    has it there by default. The script appends it only if it is really
    missing and authselect does not manage the file.

## Web server service

- Installs `/usr/lib/systemd/system/yuneta-webserver.service` and
  `/yuneta/bin/yuneta-webserver`, then enables and starts the unit.
- The init script no longer starts the web server. It did before:
  `/etc/init.d/yuneta_agent` ran nginx and let it daemonize, so nothing owned
  the process after that. A later `start` found nothing to look at and tried
  again. The second master then died with *"Address already in use"*, and the
  first one continued to serve.
- The unit runs the server with `daemon off`, so `$MAINPID` is the real
  master, and stop and reload reach it. `systemctl reload` sends HUP, which
  reloads the configuration. USR1 reopens the log files, and it stays where it
  was, in the `postrotate` of `/etc/logrotate.d/yuneta`.
- On upgrade, the scriptlet asks the old, daemonized server to finish before it
  starts the unit. The unit cannot take `:80` and `:443` while the old master
  holds them. This interrupts the web server of the node for a short time, at
  upgrade time only.

## What `%pre` and `%posttrans` do (the web server configuration)

The web server configuration belongs to the node, so the package removes it
from the payload and does not ship it. `%files` lists `/yuneta` as a
directory, so a file in it cannot have the tag `%config(noreplace)` (rpmbuild:
*"file listed twice"*). But to not ship the file is only half of the job. rpm
erases the files that an upgrade no longer provides. Without the steps below,
an upgrade from a package that still owned `nginx.conf` (7.9.0 or older)
removes it.

- `%pre` copies `/yuneta/bin/{nginx,openresty/nginx}/conf/nginx.conf` to
  `nginx.conf.pkgsave`, before rpm touches anything.
- `%posttrans` restores it. It picks the first of these, in order: an existing
  `nginx.conf` (it leaves it alone) → `nginx.conf.pkgsave` (the file of the
  node, which the erase removed) → `nginx.conf.default` (a first install). It
  also creates `conf/conf.d/` if it does not exist.

The restore is in `%posttrans`, not in `%post`. On an upgrade, rpm runs the
new `%post` before it erases the files of the old package. A short time later
the erase removes anything that `%post` wrote there. That happened in 7.9.1,
and one node was left with no `nginx.conf` at all.

> ℹ️ If the agent does not run after the install, `%post` ends with a
> `Yuneta files installed, but the AGENT IS NOT RUNNING` warning that names the
> cause. There are two usual causes on RHEL. The first is that io_uring is
> still disabled (`kernel.io_uring_disabled` ≠ 0). Enable it, then run
> `sudo service yuneta_agent start`. The second is that SELinux denies io_uring
> to the confined service (`getenforce`). To find the cause, run
> `systemctl status yuneta_agent.service` and
> `journalctl -xeu yuneta_agent.service`. The package installs its files in all
> cases. Only the start of the agent depends on these conditions.

## Inspecting the package (without installing)

```bash
rpm -qlp   dist/yuneta-agent-*.rpm    # list files
rpm -qp --scripts dist/yuneta-agent-*.rpm   # show %post/%preun/%postun
rpm -qp --requires dist/yuneta-agent-*.rpm  # runtime deps
rpmlint    dist/yuneta-agent-*.rpm    # lint (some warnings are expected)
```

## Versioning

The `.rpm` uses the same scheme of two numbers as the `.deb`. The build reads
the framework version from `../../YUNETA_VERSION` and the package release from
`../../RELEASE`. The RPM `Version` cannot contain `-`, so a pre-release like
`7.0.0-b10` becomes `7.0.0~b10`. The final name has this form:

```
yuneta-agent-<version>-<release>.<dist>.<arch>.rpm
# e.g. yuneta-agent-7.5.6-1.el9.x86_64.rpm
```

## Optional build-time configuration

This is the same as for the `.deb`. To bundle SSH keys, put them in
`authorized_keys/authorized_keys`. To choose the web server, write `nginx` or
`openresty` in `webserver/webserver` (the default is `nginx`).
[`../README.md`](../README.md) gives the full description of the runtime
layout, the CLI tools, the service helpers and the certbot integration. This
package shares all of them.
