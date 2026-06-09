# packages/rpm

RPM packaging for the Yuneta Agent (RHEL / Rocky / Alma / CentOS Stream 9+).
This is the counterpart of the Debian packaging in [`../`](../README.md): it
stages the **same** `/yuneta` runtime tree and the same helpers/configs, then
packages it with `rpmbuild` instead of `dpkg-deb`.

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

> ℹ️ **Published automatically on release.** The
> [`release-packages.yml`](../../.github/workflows/release-packages.yml)
> GitHub workflow runs `x86_64.sh` on every published Release and uploads the
> resulting `.rpm` as a release asset, next to the AMD64 `.deb`. It builds on
> the Ubuntu runner: the framework is fully static by default, so those
> binaries also run on RHEL/Rocky. This local flow is for other arches or
> custom build options.

Each wrapper reads the framework version from `../../YUNETA_VERSION` and the
package release number from `../../RELEASE` (shared with the `.deb`), then calls
`make-yuneta-agent-rpm.sh`. As with the `.deb`, the `<arch>` argument is only a
label over whatever binaries currently sit in `/yuneta/bin` and `/yuneta/agent`
— a genuine cross-arch package needs those dirs populated with cross-built
binaries.

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

**Generated at build time:**

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

The payload (`/yuneta` tree, init script, helpers, configs) is identical. Only
the packaging metadata and the install/remove logic differ:

| Concern            | `.deb` (Debian)                       | `.rpm` (RHEL/Rocky)                          |
|--------------------|---------------------------------------|----------------------------------------------|
| Package metadata   | `DEBIAN/control`                      | `.spec` (`Requires` / `Recommends`)          |
| Install hook       | `postinst`                            | `%post` (`$1`: 1 install, 2 upgrade)         |
| Pre-remove hook    | `prerm`                               | `%preun` (`$1`: 0 remove, 1 upgrade)         |
| Post-remove hook   | `postrm`                              | `%postun` (`$1`: 0 remove, 1 upgrade)        |
| Create user        | `adduser --disabled-password`         | `useradd -m` + `passwd -l`                    |
| sudo group         | `sudo`                                | `wheel`                                       |
| Locales            | `locale-gen` / `update-locale`        | `glibc-langpack-{en,es}` + `/etc/locale.conf` |
| SysV enable        | `update-rc.d` / `invoke-rc.d`         | `chkconfig` + `service`                       |
| Conffiles          | `DEBIAN/conffiles`                    | `%config(noreplace)`                          |
| certbot helper     | `install-certbot-snap.sh` (snap)      | `install-certbot.sh` (EPEL `dnf`)            |
| dev-deps helper    | `install-yuneta-dev-deps.sh` (apt)    | same name, `dnf` + EPEL + CRB                 |
| PAM limits         | `common-session*`                     | `system-auth` / `password-auth` (default-on) |

### io_uring (critical on RHEL)

Yuneta's event loop (`yev_loop`) is built on Linux **io_uring**. RHEL 9 /
Rocky 9 / Alma 9 ship `kernel.io_uring_disabled=2` (fully disabled, a hardening
default), so every yuno aborts at startup. The `.rpm` therefore ships
`kernel.io_uring_disabled = 0` inside `/etc/sysctl.d/99-yuneta-core.conf` and
applies it via `sysctl --system` in `%post`. If your security policy forbids
re-enabling io_uring, remove that line before building and Yuneta will not run.

**SELinux is a second, independent gate.** Even with
`kernel.io_uring_disabled=0`, an `Enforcing` policy can deny `io_uring_setup(2)`
to a confined service, so the agent still aborts. If the agent will not start
while io_uring is enabled, check `getenforce` and the audit log
(`ausearch -m AVC -ts recent`); a permissive domain or an explicit policy
allowance may be required.

### No automatic reboot

The `.deb` postinst offers/forces a reboot. The `.rpm` does **not**: `dnf` runs
`%post` non-interactively, so an auto-reboot on every install/upgrade would be
surprising and un-idiomatic. The kernel tuning is applied live with
`sysctl --system`; reboot only if your kernel needs it for other reasons.

## What `%post` does (in order)

1. Creates the `yuneta` login user (locked password) if missing; ensures
   `/home/yuneta`.
2. Adds `yuneta` to a broad set of access groups (`wheel`, `dialout`, `tty`,
   `kvm`, `libvirt`, `docker`, …); missing groups are skipped (override with
   `YUNETA_GROUPS`, create missing with `YUNETA_CREATE_MISSING_GROUPS=1`).
3. Creates `/yuneta/agent/yuneta_agent*.json` from the bundled `.sample`
   (never overwrites; optional `YUNETA_OWNER=` substitutes `node_owner`).
4. Sets `LANG=en_US.UTF-8` in `/etc/locale.conf` if unset.
5. `chown -R yuneta:yuneta /yuneta`; private keys dir `0700`; `/var/crash`
   `0775 root:yuneta`.
6. `sysctl --system` (applies the tuning **including io_uring**).
7. Installs bundled `authorized_keys` for `yuneta` (if present).
8. Enables `rsyslog`.
9. Installs + enables the SysV service via `chkconfig`. It then starts the
   agent **only when io_uring is actually enabled** (re-checked after the
   `sysctl --system` of step 6) and captures the real result — a disabled
   io_uring, or any other start failure, is reported, **not** hidden behind
   RPM's always-"Complete" transaction.
10. Ensures `pam_limits.so` in `system-auth`/`password-auth` (already default
    on RHEL; only appended if genuinely missing and not authselect-managed).

> ℹ️ **If the agent is not running after install**, `%post` ends with a
> `Yuneta files installed, but the AGENT IS NOT RUNNING` warning that names the
> cause. The usual ones on RHEL: io_uring still disabled
> (`kernel.io_uring_disabled` ≠ 0 — enable it, then
> `sudo service yuneta_agent start`), or **SELinux** denying io_uring to the
> confined service (`getenforce`). Diagnose with
> `systemctl status yuneta_agent.service` and
> `journalctl -xeu yuneta_agent.service`. The package files are installed
> regardless — only the agent start is gated.

## Inspecting the package (without installing)

```bash
rpm -qlp   dist/yuneta-agent-*.rpm    # list files
rpm -qp --scripts dist/yuneta-agent-*.rpm   # show %post/%preun/%postun
rpm -qp --requires dist/yuneta-agent-*.rpm  # runtime deps
rpmlint    dist/yuneta-agent-*.rpm    # lint (some warnings are expected)
```

## Versioning

Same two-number scheme as the `.deb`: framework version from
`../../YUNETA_VERSION`, package release from `../../RELEASE`. RPM `Version`
cannot contain `-`, so a pre-release like `7.0.0-b10` is mapped to `7.0.0~b10`.
The final name follows:

```
yuneta-agent-<version>-<release>.<dist>.<arch>.rpm
# e.g. yuneta-agent-7.5.6-1.el9.x86_64.rpm
```

## Optional build-time configuration

Identical to the `.deb`: drop SSH keys in `authorized_keys/authorized_keys`,
choose the web server with `webserver/webserver` (`nginx` or `openresty`,
default `nginx`). See [`../README.md`](../README.md) for the full description of
the runtime layout, CLI tools, service helpers and certbot integration — all
shared with this package.
