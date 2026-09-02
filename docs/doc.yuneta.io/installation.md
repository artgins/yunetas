# **Installation**

> **Prerequisites:** Linux, Python 3.7+, `sudo` access.
> The full lists of dependencies and licenses are in [Reference](#reference).

Yunetas has two installation methods. Your task decides which method you use:

- **[Quick install](#quick-install)** — a pre-built package. It is a `.deb` on
  Debian and Ubuntu, and a `.rpm` on RHEL, Rocky and Alma. One installer
  supports the two distro families. The package installs the runtime: the
  agent, the CLI tools and the bundled web server. It also installs the Yuneta
  libraries, the headers and the CMake toolchain under
  `/yuneta/development/yunetas/`. By default it installs the developer
  toolchain too. You can then *run* yunos, and you can *compile your own
  projects* against the published runtime. The package does not include the
  source tree of Yuneta.
- **[Build from source](#build-from-source)** — the full source tree. Use this
  method to develop the framework. Use it also to make a runtime with different
  build options. The options are the TLS backend, the modules, the static or
  dynamic link, and the build type. See
  [`menuconfig`](#configure-menuconfig).

> ℹ️ The PyPI package `yunetas` (`pipx install yunetas`) is the CLI for
> management and builds. Its current version is 0.19.1. It is not the C
> framework runtime, which is at 7.x. The `.deb` contains the two of them. A
> build from source uses the CLI to drive the build.

---

(quick-install)=
## Quick install

:::{important}
**One command installs all the software.** It supports the two distro
families: Debian and Ubuntu, and RHEL, Rocky and Alma.

```bash
curl -fsSL https://raw.githubusercontent.com/artgins/yunetas/main/install.sh | sudo sh
```
:::

(install-on-a-build-node)=
### Not on a node that builds from source

The installer **refuses** a node that keeps the sources in
`/yuneta/development/yunetas`. It looks for `kernel/` and `.git`. The package
makes the same refusal when you install it by hand.

The package contains libraries, headers and binaries from a different machine.
It puts them in `outputs/` and `outputs_ext/`, the tree that `yunetas build`
owns. After that, the tree is not consistent. New objects link against archives
that a **different glibc** built.

A dynamic link fails with a clear error. A **static** link is worse, because it
resolves the archives with no message. The binary then corrupts its heap at run
time. You get a SIGABRT inside `malloc` some seconds after the start. There is
no framework error before it, and the stack shows unrelated code. This is the
same fault that
[`libc_guard.cmake`](https://github.com/artgins/yunetas/blob/7.17.2/tools/cmake/libc_guard.cmake)
catches at configure time.

You can install the package on such a node. But it must be a decision:

```bash
curl -fsSL https://raw.githubusercontent.com/artgins/yunetas/main/install.sh \
    | sudo YUNETAS_FORCE_OVER_SOURCE=1 sh
```

Put the variable **after** `sudo`. The `sudo` command resets the environment,
so `VAR=1 sudo …` does not reach the scripts of the package. Some nodes are
deliberately a build machine and a package node. For those nodes, make the
permission permanent with a marker file:

```bash
sudo touch /etc/yuneta/allow-package-over-source
```

:::{warning}
After you force the install, **build all the software on that node again.
Start with the external libraries**:

```bash
cd /yuneta/development/yunetas
source yunetas-env.sh
cd kernel/c/linux-ext-libs && ./extrae.sh && ./configure-libs.sh
cd /yuneta/development/yunetas
yunetas init
yunetas clean && yunetas build
```

Build the external libraries **first**. If you build the SDK only, the external
libraries of the package stay on the node. This gives the same glibc mismatch.
:::

(version-on-a-build-node)=
### Which version a build node shows

On a node that builds from source, read the version from the **binary**. Do not
read it from the package database.

`dpkg -l` or `rpm -q` can show an older release than `<binary> --version`. On a
build node, this is the correct state. It is not an upgrade that you must do.

The node builds its own SDK, and it already has that code. The package adds
only a version string to the package database.

:::{important}
Do not install the package to make the two numbers agree.

The package contains full trees from the build machine: `/yuneta/bin/nginx`,
`/yuneta/bin/openresty`, `outputs/`, `outputs_ext/` and `tools/`. The build
machine can have a different glibc. Then the package installs a web server that
cannot start on this node. It also installs archives that this node did not
build. After that, you must build all the software again, external libraries
first.

The marker file `/etc/yuneta/allow-package-over-source` removes the refusal. It
does not remove this effect.
:::

### Tested on

The command above gets a complete test on a new installation of the OS:

| Distro | Package | Tested |
|---|---|---|
| **Rocky Linux 9** (Blue Onyx) | `.rpm` (EL9, x86_64) | 7.9.11-3 · 2026-08-06 |
| **Debian 13** (trixie) | `.deb` (amd64) | 7.9.11-3 · 2026-08-06 |

The test of Debian 13 uses **a VM and a dedicated server**. This is deliberate.
The faster machine loses a start-up race that the slower machine wins. A snapd
bug in the certbot step stayed hidden for this reason. The test of Rocky also
includes **a reboot**. At that moment firewalld starts for the first time, and
the agent must start without help.

Other releases of the same families can also work, because the script selects
`apt` or `dnf` and does not read the version. There is no test for them.

The test at 7.9.11-3 installed the OS again on the two nodes and did a complete
test. Only for this reason it found two defects. First, the installer took the
**oldest** package of a release, not the newest. Second, the fail2ban filter
matched nothing on a node whose vhost is a single-page app. A node that is
already configured shows neither defect.

A container of the target distro builds each package: **`debian:13`** for the
`.deb` (glibc 2.41) and **`rockylinux:9`** for the `.rpm` (glibc 2.34). This
build base decides if a node can *compile* against the SDK of the package. See
the glibc warning below. It has no effect on the binaries of the package, which
run on any node.

The script does all the work in one run. There is no second step:

- It finds the distro (`apt` or `dnf`) and the architecture.
- **On RHEL, Rocky and Alma**, it enables **EPEL and CRB** first. Those repos
  hold mercurial, ninja-build, pipx and the `-devel` and `-static` packages.
  `dnf` cannot install from a repo that it enables in the same transaction.
- It gets the correct package (`.deb` or `.rpm`) from the most recent
  [GitHub Release](https://github.com/artgins/yunetas/releases) and installs
  it. The dependencies then resolve correctly.
- It installs the **full developer toolchain**: git, mercurial, clang, gcc,
  cmake, ninja, wget, pipx and more. The node can then build yunos
  immediately. The script asks no questions and does not stop. It runs from the
  start to the end. To omit the toolchain on a deployment node, use
  `--runtime-only` (`… | sudo sh -s -- --runtime-only`).

Select a version. It must exist as a published Release:

```bash
curl -fsSL https://raw.githubusercontent.com/artgins/yunetas/main/install.sh | sudo sh -s -- 7.5.7
```

> ⚠️ **RHEL, Rocky and Alma must have io_uring enabled.** The `.rpm` contains
> `kernel.io_uring_disabled=0` and applies it in `%post`. If SELinux
> (`Enforcing`) or a host policy keeps io_uring disabled, the agent does not
> start. See the [io_uring requirement](#io_uring-required) below. If the agent
> does not run after the installation, `%post` prints a warning. The warning
> names the cause, io_uring or SELinux. It also gives the `systemctl status`
> and `journalctl` commands for the diagnosis.

You can also download the package from the
[Releases page](https://github.com/artgins/yunetas/releases) and install it.
This installs the runtime and its declared dependencies only. To add the
developer toolchain after that, run
`sudo /yuneta/bin/install-yuneta-dev-deps.sh`, or use the one-line command
above:

```bash
# Debian / Ubuntu
sudo apt install ./yuneta-agent-<version>-<release>-<arch>.deb

# RHEL / Rocky / Alma  (enable EPEL + CRB first so the deps resolve)
sudo dnf -y install epel-release && sudo crb enable
sudo dnf -y install ./yuneta-agent-<version>-<release>.x86_64.rpm
```

You can also build the package yourself, for a different architecture or with
different options. First [build from source](#build-from-source). Then run
`packages/deb/<ARCH>.sh` or `packages/rpm/<arch>.sh`. See
[`packages/README.md`](https://github.com/artgins/yunetas/tree/7.17.2/packages).

The package installs the agent, the CLI tools and the bundled openresty under
`/yuneta/`. It creates the `yuneta` system user. It applies the kernel tuning
and the PAM limits. Then it starts the SysV service.

The package also installs a sparse SDK under `/yuneta/development/yunetas/`.
This sparse SDK holds the Yuneta libraries, the headers, the CMake toolchain
and the build `.config` (`outputs/`, `outputs_ext/`, `tools/` and `.config`,
with no sources). This base path is the SAME path as a full source checkout.
Your projects thus compile against the published runtime with no source tree
and with no difference of layout. The full inventory is in
[`packages/README.md`](https://github.com/artgins/yunetas/tree/7.17.2/packages).

> 🔴 **The sparse SDK compiles only on the glibc that built the package.** The
> `outputs/lib/*.a` files of the package are static archives. They reference
> internals of glibc, and the layout of those internals changes between
> releases. If you link new objects against them on a node with a *different*
> glibc, the binary corrupts its heap at run time. You get a SIGABRT in
> `_int_malloc` some seconds after the start, with no framework error before it
> and a stack that shows unrelated code. The **binaries of the package continue
> to work**, because they are self-contained. Only the *build* fails. Before
> you build on a node, do this test:
>
> ```bash
> cat /yuneta/development/yunetas/outputs/lib/yuneta_libc.stamp   # what built the package
> ldd --version | head -1                                          # what the node has
> ```
>
> If the two values are different, that node is **runtime-only**. Build on a
> different machine and push the binaries with `yunetas sync-binaries`.
> `tools/cmake/libc_guard.cmake` applies this rule at configure time.
> `-DYUNETA_ALLOW_LIBC_MISMATCH=ON` removes the message only. It does not make
> the link safe.
>
> A container of the target distro builds each package. Each package thus
> matches one glibc only:
>
> | Package | Built in | glibc | Nodes that can build on-node |
> |---|---|---|---|
> | `.rpm` (EL9) | `rockylinux:9` | 2.34 | Rocky/Alma 9 |
> | `.deb` (AMD64) | `debian:13` | 2.41 | Debian 13 (trixie) |
>
> Thus **Ubuntu nodes are runtime-only**. Ubuntu 24.04 has glibc 2.39 and
> Ubuntu 26.04 has glibc 2.43, and neither value is 2.41. Debian 12 is
> runtime-only for the same reason. Build for those nodes on a different
> machine and push the binaries. Until **7.8.6-3**, an `ubuntu-22.04` runner
> (glibc 2.35) built the `.deb`. If you have an older package, read its stamp
> and do not use this table.

> ℹ️ **Build options of the published `.deb`.** The Kconfig defaults
> (`alldefconfig`) compile the release asset. These defaults are **GCC**,
> **RelWithDebInfo**, **fully static** binaries and the **OpenSSL** TLS backend
> (mbedTLS is off). All the modules are enabled: console, mqtt, postgres, test
> and modbus. The installation puts the exact configuration at
> `/yuneta/development/yunetas/.config`. Read that file to know the options of
> a given package. For a different combination, build from source and select
> your options with [`menuconfig`](#configure-menuconfig). Two examples are
> mbedTLS for smaller binaries, and a smaller set of modules.

> ⚠️ **The agent is a SysV service. Control it with the `--start` and `--stop`
> options of the agent binary. Do not use `systemctl` or systemd.** Yuneta runs
> its own daemon and watchdog. Thus `systemctl restart yuneta_agent` does
> **nothing**: the process keeps its old PID and its old binary. To start, stop
> or restart the agent, use these commands:
>
> ```bash
> /yuneta/agent/yuneta_agent --config-file=/yuneta/agent/yuneta_agent.json --stop
> /yuneta/agent/yuneta_agent --config-file=/yuneta/agent/yuneta_agent.json --start
> ```
>
> You can also use the init script
> `/etc/init.d/yuneta_agent {start|stop|restart}`. It also controls the bundled
> web server. To install
> a new agent binary, write it over `/yuneta/agent/yuneta_agent`. Then use
> `--stop` and `--start`.

> ℹ️ **You can build the `.deb` yourself** and not use the published asset.
> See `packages/README.md` for the four wrapper scripts, one for each
> architecture
> ([`AMD64.sh`](https://github.com/artgins/yunetas/blob/7.17.2/packages/deb/AMD64.sh), [`ARM32.sh`](https://github.com/artgins/yunetas/blob/7.17.2/packages/deb/ARM32.sh), [`ARMhf.sh`](https://github.com/artgins/yunetas/blob/7.17.2/packages/deb/ARMhf.sh), [`RISCV64.sh`](https://github.com/artgins/yunetas/blob/7.17.2/packages/deb/RISCV64.sh)). The SDK must be built
> first (next section).

### Tests after a fresh install

There are five tests, and they apply to the two distro families. Run them
immediately after the one-line command ends. They find all the faults that
occurred on a new node:

```bash
# 1. The three processes are up (agent, agent22, bundled web server)
ps -ef | grep -E 'yuneta_agent|nginx' | grep -v grep

# 2. They come back after a reboot (SysV, enabled — there is no native unit
#    for agent22/nginx, the init script starts all three)
systemctl is-enabled yuneta_agent

# 3. The control channel answers (empty list on a fresh node is correct)
sudo -u yuneta ycommand -c 'list-yunos'

# 4. The CLI runs
sudo -u yuneta yunetas --help

# 5. Core dumps land where the debugger looks for them
cat /proc/sys/kernel/core_pattern      # -> /var/crash/core.%e
```

If the node must **build** software, do the glibc-stamp test above too.

Two results look like a fault and are correct. First,
`systemctl is-active yuneta_agent22` and `nginx` report `inactive` or
`not-found`. Neither has a systemd unit, and the init script controls them. Second, on RHEL,
`certbot-renew.timer` is `enabled` but `inactive` until the next boot.

### Logs and banning

The package also configures the logs of the web server. nginx does not rotate
its logs, and it does not ban an attacker.

**The rotation is automatic.** `/etc/logrotate.d/yuneta` rotates the access log
and the error log of the web server of the node, one time each day. It keeps 30
compressed files. Without this rotation, those files increase in size for the
full life of the node. nginx has no rotation function. It can only open its
files again when it receives a signal.

**The ban function is installed, but it is OFF.**
`/etc/fail2ban/jail.d/yuneta-nginx.conf` contains two jails. They ban a client
that probes the node for `.php` paths, `.env`, `.git` or the WordPress paths. A
Yuneta node serves none of these, thus such a request cannot be an error.

The jails are **disabled** deliberately. If the log path of a jail matches no
file, fail2ban does not omit that jail. It refuses to start, and all the other
jails stop with it, `sshd` included. A node that has the package and has not
started its web server is in this condition.

When `access.log` exists, enable the jails:

```bash
printf '[yuneta-nginx-probe]\nenabled = true\n[nginx-botsearch]\nenabled = true\n' \
    | sudo tee /etc/fail2ban/jail.d/zz-yuneta-nginx-enabled.conf
sudo systemctl reload fail2ban && sudo fail2ban-client status
```

Then test the **firewall**, not the jail. If `banaction` names a command that
the node does not have, the jail reports a good condition but bans nothing. On
Debian that command is `nft`. The `nftables` package supplies it, and a minimal
installation does not.

```bash
sudo nft list table inet f2b-table     # or: sudo iptables -L f2b-yuneta-nginx-probe -n
```

> ⚠️ **With SELinux in `Enforcing` mode, the log paths of the package do not
> work.** The paths are globs, and `fail2ban_t` cannot list `/yuneta/bin`.
> SELinux *dontaudits* this denial, thus it writes no AVC. fail2ban then reports
> *"Have not found any log file"* and stops. No message names SELinux. Give the
> jails a literal path in the same `zz-` file. Then label the directory with
> `semanage fcontext -a -t var_log_t "/yuneta/bin/nginx/logs(/.*)?"` and
> `restorecon -R /yuneta/bin/nginx/logs`.

The full detail is in
[`packages/deb/README.md`](https://github.com/artgins/yunetas/blob/7.17.2/packages/deb/README.md).

---

(build-from-source)=
## Build from source

The seven steps below install the full SDK under `~/yunetaprojects/`. The SDK
holds the sources, the build dependencies and the tools.

### 1. Create the `yuneta` user

```bash
sudo adduser yuneta
sudo mkdir /yuneta
sudo chown yuneta:yuneta /yuneta
```

Close the session. Then start a new session as the `yuneta` user for the other
steps.

### 2. Install OS packages

The repo contains a helper script. It installs all the packages below and the
`kconfiglib` backend for `menuconfig`. It reads `/etc/os-release` to find the
distro family: Debian and Ubuntu, or RHEL, Rocky, Alma and Fedora:

```bash
cd ~/yunetaprojects/yunetas
./install-dependencies.sh
```

To run the package manager yourself, select your distro below.

::::{tab-set}

:::{tab-item} Debian / Ubuntu

```bash
sudo apt -y install --no-install-recommends \
  git mercurial make cmake ninja-build \
  gcc clang g++ \
  python3-dev python3-pip python3-setuptools \
  python3-tk python3-wheel python3-venv \
  libjansson-dev libpcre2-dev liburing-dev \
  zlib1g-dev libssl-dev \
  perl dos2unix tree curl wget \
  postgresql-server-dev-all libpq-dev \
  kconfig-frontends telnet pipx \
  patch gettext fail2ban rsync \
  build-essential pkg-config ca-certificates linux-libc-dev

pipx install kconfiglib
```

:::

:::{tab-item} RHEL / Rocky / Alma / Fedora

Some packages are in **EPEL**: mercurial, ninja-build, telnet, pipx, fail2ban
and python3-wheel. One package is in **CRB (CodeReady Builder)**:
liburing-devel. Enable the two repos first. Fedora has neither repo, thus omit
this step on Fedora:

```bash
sudo dnf -y install epel-release
sudo crb enable        # or: sudo dnf config-manager --set-enabled crb
```

Then install the packages. The names below are the RHEL names for the Debian
list:

```bash
sudo dnf -y install \
  git mercurial make cmake ninja-build \
  gcc clang gcc-c++ \
  python3-devel python3-pip python3-setuptools \
  python3-tkinter python3-wheel \
  jansson-devel pcre2-devel liburing-devel \
  pcre-devel zlib-devel openssl-devel \
  perl dos2unix tree wget \
  libpq-devel \
  telnet pipx \
  patch gettext fail2ban rsync \
  pkgconf-pkg-config ca-certificates glibc-devel kernel-headers \
  glibc-static libstdc++-static libxcrypt-static

pipx install kconfiglib
```

(io_uring-required)=

> ⚠️ **RHEL and Rocky disable io_uring. Yuneta does not run until you enable
> it.** The event loop of Yuneta (`yev_loop`) uses Linux **io_uring** for all
> its work. RHEL 9, Rocky 9 and Alma 9 set `kernel.io_uring_disabled=2`, which
> disables io_uring completely. This value is a hardening default. Thus every
> yuno aborts at the start, and all the tests fail with
> *"Subprocess aborted"*. Enable io_uring:
>
> ```bash
> # Persist across reboots (production):
> echo 'kernel.io_uring_disabled = 0' | sudo tee /etc/sysctl.d/99-yuneta-iouring.conf
> sudo sysctl --system
>
> # Or just for the current boot:
> sudo sysctl -w kernel.io_uring_disabled=0
> ```
>
> The values are `0` for all users, `1` for `CAP_SYS_ADMIN` or the members of
> the `io_uring` group, and `2` for fully disabled. `2` is the default on RHEL
> and Rocky. Debian and Ubuntu set `0`, thus only the RHEL family needs this
> step. To make sure that the value is correct, run
> `sysctl kernel.io_uring_disabled`.
>
> **SELinux is a second and independent control.** The sysctl can be `0`, and an
> `Enforcing` policy can still deny `io_uring_setup(2)` to a confined service.
> The agent then aborts. If the agent does not start and io_uring is enabled,
> read `getenforce` and the audit log (`ausearch -m AVC -ts recent`).

> ℹ️ **A static build needs static archives.** The default configuration is
> `CONFIG_FULLY_STATIC=y`. The link then needs `libc.a`, `libstdc++.a` and
> `libcrypt.a`. On RHEL, `glibc-static`, `libstdc++-static` and
> `libxcrypt-static` supply them, all in CRB. On Debian they are inside `libc6-dev` and
> `build-essential`, thus the Debian list does not show them.

```{dropdown} Debian → RHEL package name mapping
| Debian / Ubuntu                  | RHEL / Rocky / Alma / Fedora            |
|----------------------------------|-----------------------------------------|
| `g++`                            | `gcc-c++`                               |
| `python3-dev`                    | `python3-devel`                         |
| `python3-tk`                     | `python3-tkinter`                       |
| `python3-venv`                   | *(ships with `python3`, no package)*    |
| `libjansson-dev`                 | `jansson-devel`                         |
| `libpcre2-dev`                   | `pcre2-devel`                           |
| `liburing-dev`                   | `liburing-devel` *(CRB)*                |
| `zlib1g-dev`                     | `zlib-devel`                            |
| `libssl-dev`                     | `openssl-devel`                         |
| `postgresql-server-dev-all` / `libpq-dev` | `libpq-devel`                  |
| `kconfig-frontends`              | *(none; use `pipx install kconfiglib`)* |
| `build-essential`                | `gcc gcc-c++ make` *(or "Development Tools" group)* |
| `pkg-config`                     | `pkgconf-pkg-config`                    |
| `linux-libc-dev`                 | `glibc-devel kernel-headers`            |
| *(static archives in `libc6-dev`)* | `glibc-static libstdc++-static libxcrypt-static` *(CRB)* |
| `curl`                           | *(already present as `curl-minimal`)*   |

`jansson-devel`, `liburing-devel`, `pcre2-devel` and `openssl-devel` give the
development headers only. Yunetas builds its own static copies under
`kernel/c/linux-ext-libs` (step 7). Thus on RHEL these packages are necessary
only for the dynamic link of nginx and openresty.
```

:::

::::

```{dropdown} What each non-obvious package is for
- `libjansson-dev` / `jansson-devel` — required for libjwt
- `libpcre2-dev` / `pcre2-devel`      — required by openresty
- `perl dos2unix mercurial wget`      — required by openresty (wget fetches its tarballs)
- `pipx kconfiglib`                   — yunetas configuration tool
- `kconfig-frontends`                 — alternative configuration tool (Debian only)
- `telnet`                            — required by tests
```

### 3. Install the `yunetas` CLI

::::{tab-set}

:::{tab-item} With `pipx`

```bash
sudo apt install pipx     # Ubuntu 23.04+
pipx ensurepath
pipx install yunetas
```

````{dropdown} Older Ubuntu (≤22.04)
```bash
python3 -m pip install --user pipx
python3 -m pipx ensurepath
```
````

````{dropdown} Update / uninstall
```bash
pipx upgrade yunetas
pipx uninstall yunetas
```
````

:::

:::{tab-item} With `conda`

```bash
pip install yunetas
```

````{dropdown} Bootstrap miniconda from scratch
```bash
mkdir -p ~/miniconda3
wget https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-x86_64.sh -O ~/miniconda3/miniconda.sh
bash ~/miniconda3/miniconda.sh -b -u -p ~/miniconda3
rm -rf ~/miniconda3/miniconda.sh
~/miniconda3/bin/conda init bash
exit   # close and re-open the shell

conda config --add channels conda-forge
conda create -y -n conda_yunetas pip
conda config --set auto_activate_base false
echo 'conda activate conda_yunetas' >> ~/.bashrc
source ~/.bashrc
```
````

:::

::::

### 4. Clone the repo

```bash
mkdir ~/yunetaprojects
cd ~/yunetaprojects
git clone --recurse-submodules https://github.com/artgins/yunetas.git
```

````{dropdown} Pin a specific version
```bash
git clone -b <version> --recurse-submodules https://github.com/artgins/yunetas.git <version>
```
````

### 5. Activate the environment

```bash
cd ~/yunetaprojects/yunetas
source yunetas-env.sh
```

[`yunetas-env.sh`](https://github.com/artgins/yunetas/blob/7.17.2/yunetas-env.sh) exports four variables. It also puts `/yuneta/bin` and
`$YUNETAS_BASE/scripts` at the start of `PATH`:

| Variable              | Value                                       |
|-----------------------|---------------------------------------------|
| `YUNETAS_BASE`        | The yunetas repo root (the dir you sourced from). |
| `YUNETAS_OUTPUTS`     | `$YUNETAS_BASE/outputs`                     |
| `YUNETAS_OUTPUTS_EXT` | `$YUNETAS_BASE/outputs_ext`                 |
| `YUNETAS_YUNOS`       | `$YUNETAS_OUTPUTS/yunos`                    |

> ℹ️ **Layout contract.** The build artifacts (`outputs/` and `outputs_ext/`)
> are INSIDE `$YUNETAS_BASE`. Git ignores the two directories. The base is the
> SAME path on every node: `/yuneta/development/yunetas/`. On a development
> node it is a full source checkout. On a runtime-only node it is the sparse
> SDK from the `.deb` or the `.rpm` (`outputs/`, `outputs_ext/`, `tools/` and
> `.config`, with no sources). Your own project repos can be at any path.
> Register them with `yunetas register-project` (below).

> ⚠️ **Source the file again in each shell.** A new SSH session, a cron job
> and CI must source [`yunetas-env.sh`](https://github.com/artgins/yunetas/blob/7.17.2/yunetas-env.sh) again. Without it, [`ybatch`](#util-ybatch), [`ycommand`](#util-ycommand)
> and [`yshutdown`](#util-yshutdown) are not on `PATH`. The deploy scripts then
> fail with "command not found".

**To make this permanent**, add these lines to `~/.bashrc`:

```bash
cd ~/yunetaprojects/yunetas
source yunetas-env.sh
```

If `~/.yunetasrc` exists, the script sources it too. Use that file for your
own additions.

(configure-menuconfig)=
### 6. Configure (`menuconfig`)

```bash
cd ~/yunetaprojects/yunetas
menuconfig
```

Select the compiler, the build type and the modules. Then **save**. This
writes `.config`, and the build needs that file.

~~~~{dropdown} Full menuconfig options
```text
(Top) → Compiler Selection
    (X) GCC compiler (default)
    ( ) Clang compiler

(Top) → Build Configuration
    Build Type
        (X) RelWithDebInfo (default)
        ( ) Release
        ( ) Debug
        ( ) MinSizeRel
    [*] Build fully static binaries (default y)
         Produces binaries with no shared library dependencies.
         Copy to any Linux machine of the same architecture and run — nothing to install.

(Top) → TLS Library
    (X) OpenSSL (default)
    ( ) Mbed-TLS

(Top) → Debug Options
    [*] Use backtrace library (default y)
    [ ] Enable track memory
    [ ] Print times of yev_loop

(Top) → Modules
    [*] C_CONSOLE support
    [*] C_MQTT support
    [*] C_MODBUS support
    [*] C_POSTGRES support
    [*] C_TEST support
```
~~~~

> ⚠️ **Save `.config`, or the build fails.** If you change the compiler, run [`./set_compiler.sh`](https://github.com/artgins/yunetas/blob/7.17.2/set_compiler.sh) again. It builds the external
> libraries with the correct toolchain.

### 7. Build and test

First, build the bundled external libraries. You do this one time only:

```bash
cd ~/yunetaprojects/yunetas/kernel/c/linux-ext-libs
./extrae.sh         # clone libraries
./configure-libs.sh # configure, build and install
```

Then build, install and test yunetas with the **`yunetas` CLI**. This CLI is
the standard build interface, with the commands `init`, `build`, `clean` and
`test`. Use it, and do not call `cmake` directly. The CLI does the installation
to `$YUNETAS_OUTPUTS/lib` and the relink of each yuno in the correct order:

```bash
cd ~/yunetaprojects/yunetas
yunetas init     # configure build dirs + compiler/build-type from .config (menuconfig)
yunetas build    # regenerate yuneta_version.h + `make install` everything (kernel + yunos)
yunetas test     # ctest
# yunetas clean  # wipe the build dirs
```

The artifacts go to `$YUNETAS_OUTPUTS/`, which is `$YUNETAS_BASE/outputs/`.
The subdirectories are `include/`, `lib/`, `bin/` and `yunos/`.

#### External projects

The `yunetas` CLI can also build your own projects. A project is any repo
whose `yunos/CMakeLists.txt` includes `tools/cmake/project.cmake`. Register a
project one time. Then `init`, `build` and `clean` process it immediately after
the SDK:

```bash
yunetas register-project /yuneta/development/projects/myproject
yunetas list-projects
yunetas build                 # SDK + every registered project
yunetas build myproject       # only that project (SDK skipped)
yunetas build --sdk-only      # only the SDK
yunetas unregister-project myproject
```

The registry is user state of the local machine (`~/.yuneta/projects.json`).
It stays outside the source tree.

A deploy has **two steps. First push the artifacts. Then promote them.** The
helpers below are an interface to `tools/agent/`. They forward the shared
arguments without a change: `-n` for a dry run, `-a` for all, and the OAuth2
options. [The `yunetas` CLI](yunetas-cli.md) gives the full CLI reference, with
the build commands and the project-management commands:

```bash
# 1. Push binaries AND configs together (recommended)
yunetas sync -n               # = sync-binaries + sync-configs, dry-run

# 2. Promote the freshly pushed releases to primary and restart
yunetas upgrade-yunos -n      # preview the agent commands without running them
yunetas upgrade-yunos         # snapshot -> find-new-yunos -> deactivate-snap
```

Push the two kinds of artifact with **`sync`**. Do not push them one at a
time. A new binary must never go to a node without its new configuration.

One example shows the risk. A new fail-closed runtime verifies TLS by default.
Against an old configuration with no CA, the OIDC login fails. `sync` connects
the two steps, thus you cannot forget one. If the push of the binaries fails,
`sync` stops before the configurations. There is no half-deploy.

The individual helpers stay available. Use them when you need one kind only, or
a flag of one tool:

```bash
yunetas sync-binaries -n      # outputs/yunos vs the local agent
yunetas sync-configs -n       # each project's yunos/batches/<host>/, auto-matched to local realms
yunetas sync-configs -n --host my.host.com   # or target one batches dir explicitly
```

`sync-configs` reads the registered projects, and the configuration pass of
`sync` does the same. Without `--host`, it asks the local agent for its realms
(`*list-realms`). It then syncs every `batches/<host>/` directory whose name is
a realm_id of that agent. A node with several realms thus deploys all of them
in one pass. The name of a batches directory is its realm_id, which is the
deploy FQDN. If the agent does not answer, the tool matches one hostname only.

`upgrade-yunos` does four operations:

1. It makes an optional snapshot for a rollback. The name makes it idempotent,
   and the default name is `pre-upgrade-<YYYYMMDD>`. Use `--no-snap` to omit
   the snapshot. If a snap is already active, the tool uses that one.
2. It shows the new yuno rows that `find-new-yunos` creates, and it asks for a
   confirmation. Use `--yes` to omit the question.
3. It registers those rows with `find-new-yunos create=1`.
4. It runs `deactivate-snap`. This starts `restart_nodes()` in the agent, which
   does a SIGKILL and a treedb reload. The agent then promotes the newest
   release of every yuno.

A hot-patch of the same version has no change of `APP_VERSION`. For that case
you can omit `upgrade-yunos`. Run `sync`. Then restart the affected yunos with
`kill-yuno` and `run-yuno` or `play-yuno`.

> ℹ️ **Fully static builds** (`CONFIG_FULLY_STATIC=y`) use the same
> `configure-libs.sh` with GCC or with Clang. They need no different toolchain.
> The build of OpenSSL uses `no-dso` and `no-sock`. This keeps the glibc
> resolver stubs out of the static binary. See
> `kernel/c/linux-ext-libs/HACKS.md` for the details.

---

## Troubleshooting

````{dropdown} Restore /var/log/syslog
```bash
sudo apt-get update
sudo apt-get install rsyslog
sudo systemctl enable --now rsyslog
```
````

````{dropdown} Correct the "Setting locale failed" warnings
```bash
dpkg-reconfigure locales
```
````

---

(reference)=
## Reference

### License

MIT, except for `kernel/c/libjwt/*` which is **MPL-2.0**.

### Build dependencies (C / Linux)

- [Jansson](https://jansson.readthedocs.io/en/latest/) — MIT
- [libjwt](https://github.com/benmcollins/libjwt) — MPL-2.0
- [liburing](https://github.com/axboe/liburing) — MIT, LGPL-2.1, GPL-2.0
- [mbedtls](https://www.trustedfirmware.org/projects/mbed-tls/) — Apache-2.0 or GPL-2.0
- [openssl](https://www.openssl.org/) — Apache-2.0
- [pcre2](https://github.com/PCRE2Project/pcre2) — BSD and others
- [libbacktrace](https://github.com/ianlancetaylor/libbacktrace) — BSD 3-Clause
- [argp-standalone](https://github.com/artgins/argp-standalone.git) — LGPL-2.1
- [ncurses](https://github.com/mirror/ncurses.git) — MIT
- [llhttp](https://github.com/nodejs/llhttp) — MIT

### Runtime / deploy dependencies

- [nginx](https://nginx.org) — BSD-2-Clause
- [openresty](https://openresty.org/) — BSD 2-Clause, BSD 3-Clause, MIT, OpenSSL, Zlib, SSLeay

[pipx]:     https://pipx.pypa.io/stable/
[yunetas]:  https://pypi.org/project/yunetas/
[conda]:    https://docs.anaconda.com/free/miniconda/#miniconda
