# **Installation**

> **Prerequisites:** Linux, Python 3.7+, `sudo` access.
> Full dependency lists and licenses are in [Reference](#reference) below.

There are two ways to install Yunetas, depending on what you want to do:

- **[Quick install](#quick-install)** — pre-built `.deb`. Installs the
  runtime (agent, CLI tools, bundled web server) **and** the Yuneta
  libraries, headers and CMake toolchain under `/yuneta/development/`,
  so you can both *run* yunos and *compile your own projects* against
  the published runtime. What it does **not** include is Yuneta's own
  source tree.
- **[Build from source](#build-from-source)** — the full source tree.
  Use this to develop the framework itself, or to produce a customised
  runtime with different build options (TLS backend, modules,
  static/dynamic, build type — see [`menuconfig`](#configure-menuconfig)).

> ℹ️ The PyPI package `yunetas` (`pipx install yunetas`) is the
> management/build CLI (currently 0.8.0), **not** the C framework runtime
> (currently 7.x). The `.deb` bundles both; building from source uses
> the CLI to drive the build.

---

(quick-install)=
## Quick install

> ⚠️ **The `curl | sh` one-liner is Debian/Ubuntu only** — it uses
> `dpkg`/`apt-get`. On RHEL/Rocky/Alma a `.rpm` is published as a release
> asset alongside the `.deb`; download it from the
> [Releases page](https://github.com/artgins/yunetas/releases) and install
> it directly:
>
> ```bash
> sudo dnf -y install ./yuneta-agent-<version>-<release>.x86_64.rpm
> ```
>
> **RHEL/Rocky/Alma need io_uring enabled.** The `.rpm` ships
> `kernel.io_uring_disabled=0` and applies it in `%post`, but if SELinux
> (`Enforcing`) or a host policy keeps io_uring disabled the agent will not
> start — see the [io_uring requirement](#io_uring-required) below. The
> `%post` is honest about it: if the agent is not running after install it
> prints a warning naming the cause (io_uring or SELinux) with the
> `systemctl status` / `journalctl` commands to diagnose it.
>
> Prefer to build the `.rpm` yourself (other arch, custom options)?
> [Build from source](#build-from-source) (fully supported on RHEL/Rocky,
> see step 2), then run `packages/rpm/x86_64.sh` — see
> [`packages/rpm/README.md`](https://github.com/artgins/yunetas/tree/main/packages/rpm).

One-liner:

```bash
curl -fsSL https://raw.githubusercontent.com/artgins/yunetas/main/install.sh | sudo sh
```

The script:

- detects the host architecture (`amd64`, `armhf`, `riscv64`);
- pulls the matching `.deb` from the latest [GitHub Release](https://github.com/artgins/yunetas/releases);
- installs it via `apt`/`dpkg` so dependencies resolve cleanly.

Pin a version (must exist as a published Release):

```bash
curl -fsSL https://raw.githubusercontent.com/artgins/yunetas/main/install.sh | sudo sh -s -- 7.5.1
```

Or download the `.deb` manually from the
[Releases page](https://github.com/artgins/yunetas/releases) and run:

```bash
sudo apt install ./yuneta-agent-<version>-<release>-<arch>.deb
```

The package installs the agent + CLI tools + bundled openresty under
`/yuneta/`, creates the `yuneta` system user, applies kernel tuning
and PAM limits, and starts the SysV service. It also lays down the
Yuneta libraries, headers, CMake toolchain and the build `.config`
under `/yuneta/development/` (`outputs/`, `outputs_ext/`, `tools/`),
so projects can compile against the published runtime without the
source tree. Full inventory in
[`packages/README.md`](https://github.com/artgins/yunetas/tree/main/packages).

> ℹ️ **Build options of the published `.deb`.** The release asset is
> compiled with the Kconfig defaults (`alldefconfig`): **GCC**,
> **RelWithDebInfo**, **fully static** binaries, **OpenSSL** TLS
> backend (mbedTLS off), and every module enabled (console, mqtt,
> postgres, test, modbus). The exact configuration is installed at
> `/yuneta/development/.config` — inspect it to confirm what a given
> package was built with. Need a different combination (e.g. mbedTLS
> for smaller binaries, or a leaner module set)? Build from source and
> pick your options with [`menuconfig`](#configure-menuconfig).

> ⚠️ **The agent is a SysV service — manage it with the agent binary's own
> `--start` / `--stop`, NOT `systemctl`/systemd.** Yuneta runs its own
> daemon + watchdog, so `systemctl restart yuneta_agent` is effectively a
> **no-op** (the process keeps its old PID and binary — it is not actually
> restarted). To start/stop/restart the agent:
>
> ```bash
> /yuneta/agent/yuneta_agent --config-file=/yuneta/agent/yuneta_agent.json --stop
> /yuneta/agent/yuneta_agent --config-file=/yuneta/agent/yuneta_agent.json --start
> ```
>
> or the init script `/etc/init.d/yuneta_agent {start|stop|restart}` (which
> also handles the bundled web server). To roll out a new agent binary:
> overwrite `/yuneta/agent/yuneta_agent`, then `--stop` and `--start`.

> ℹ️ **Build the `.deb` yourself** instead of using the published asset:
> see `packages/README.md` for the four arch wrapper scripts
> ([`AMD64.sh`](https://github.com/artgins/yunetas/blob/7.5.7/packages/deb/AMD64.sh), [`ARM32.sh`](https://github.com/artgins/yunetas/blob/7.5.7/packages/deb/ARM32.sh), [`ARMhf.sh`](https://github.com/artgins/yunetas/blob/7.5.7/packages/deb/ARMhf.sh), [`RISCV64.sh`](https://github.com/artgins/yunetas/blob/7.5.7/packages/deb/RISCV64.sh)). Requires the
> SDK already built (next section).

---

(build-from-source)=
## Build from source

The 7-step flow below installs the full SDK — sources, build deps,
tooling — under `~/yunetaprojects/`.

### 1. Create the `yuneta` user

```bash
sudo adduser yuneta
sudo mkdir /yuneta
sudo chown yuneta:yuneta /yuneta
```

Log out and log back in as `yuneta` for the rest of the steps.

### 2. Install OS packages

The repo ships a distro-aware helper that installs everything below and
the `kconfiglib` `menuconfig` backend. It auto-detects Debian/Ubuntu vs
RHEL/Rocky/Alma/Fedora from `/etc/os-release`:

```bash
cd ~/yunetaprojects/yunetas
./install-dependencies.sh
```

Prefer to run the package manager yourself? Pick your distro below.

::::{tab-set}

:::{tab-item} Debian / Ubuntu

```bash
sudo apt -y install --no-install-recommends \
  git mercurial make cmake ninja-build \
  gcc clang g++ \
  python3-dev python3-pip python3-setuptools \
  python3-tk python3-wheel python3-venv \
  libjansson-dev libpcre2-dev liburing-dev \
  libpcre3-dev zlib1g-dev libssl-dev \
  perl dos2unix tree curl wget \
  postgresql-server-dev-all libpq-dev \
  kconfig-frontends telnet pipx \
  patch gettext fail2ban rsync \
  build-essential pkg-config ca-certificates linux-libc-dev

pipx install kconfiglib
```

:::

:::{tab-item} RHEL / Rocky / Alma / Fedora

Several packages live in **EPEL** (mercurial, ninja-build, telnet,
pipx, fail2ban, python3-wheel) and **CRB / CodeReady Builder**
(liburing-devel). Enable both first (skip on Fedora — it has neither):

```bash
sudo dnf -y install epel-release
sudo crb enable        # or: sudo dnf config-manager --set-enabled crb
```

Then install the packages (RHEL names mapped from the Debian list):

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

> ⚠️ **RHEL/Rocky disable io_uring — Yuneta will not run until you
> re-enable it.** Yuneta's event loop (`yev_loop`) is built entirely on
> Linux **io_uring**. RHEL 9 / Rocky 9 / Alma 9 ship
> `kernel.io_uring_disabled=2` (io_uring fully disabled, a hardening
> default), so every yuno aborts at startup and the whole test suite
> fails with *"Subprocess aborted"*. Enable it:
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
> Values: `0` = enabled for all · `1` = only `CAP_SYS_ADMIN` or members
> of the `io_uring` group · `2` = fully disabled (the RHEL/Rocky default).
> Debian/Ubuntu ship `0`, so this step is RHEL-family only. Confirm with
> `sysctl kernel.io_uring_disabled`.
>
> **SELinux is a second, independent gate.** Even with the sysctl at `0`, an
> `Enforcing` policy can deny `io_uring_setup(2)` to a confined service, so the
> agent still aborts. If it will not start while io_uring is enabled, check
> `getenforce` and the audit log (`ausearch -m AVC -ts recent`).

> ℹ️ **Static build needs static archives.** The default config is
> `CONFIG_FULLY_STATIC=y`, so the link needs `libc.a` / `libstdc++.a` /
> `libcrypt.a` — provided on RHEL by `glibc-static`, `libstdc++-static`
> and `libxcrypt-static` (all in CRB). On Debian these ship inside
> `libc6-dev` / `build-essential`, so they are not listed separately
> there.

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
| `libpcre3-dev`                   | `pcre-devel`                            |
| `zlib1g-dev`                     | `zlib-devel`                            |
| `libssl-dev`                     | `openssl-devel`                         |
| `postgresql-server-dev-all` / `libpq-dev` | `libpq-devel`                  |
| `kconfig-frontends`              | *(none; use `pipx install kconfiglib`)* |
| `build-essential`                | `gcc gcc-c++ make` *(or "Development Tools" group)* |
| `pkg-config`                     | `pkgconf-pkg-config`                    |
| `linux-libc-dev`                 | `glibc-devel kernel-headers`            |
| *(static archives in `libc6-dev`)* | `glibc-static libstdc++-static libxcrypt-static` *(CRB)* |
| `curl`                           | *(already present as `curl-minimal`)*   |

`jansson-devel`, `liburing-devel`, `pcre2-devel` and `openssl-devel` are
dev headers only; Yunetas builds its own static copies under
`kernel/c/linux-ext-libs` (step 7), so on RHEL they are needed only for
the dynamically-linked nginx/openresty and as convenience.
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

````{dropdown} Optional: lib-yui end-to-end tests (Playwright)
The WebKit browser used by the lib-yui e2e suite needs two extra
packages on Debian/Ubuntu:

```bash
sudo apt -y install --no-install-recommends \
    libgstreamer-plugins-bad1.0-0 libavif16
```

The `kernel/js/lib-yui/install-e2e-deps.sh` helper installs them
along with all three Playwright browsers. Chromium and Firefox bundle
their own deps.
````

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

[`yunetas-env.sh`](https://github.com/artgins/yunetas/blob/7.5.7/yunetas-env.sh) exports three variables and prepends `/yuneta/bin`
plus `$YUNETAS_BASE/scripts` to `PATH`:

| Variable          | Value                                       |
|-------------------|---------------------------------------------|
| `YUNETAS_BASE`    | The yunetas repo root (the dir you sourced from). |
| `YUNETAS_OUTPUTS` | `$(dirname $YUNETAS_BASE)/outputs`          |
| `YUNETAS_YUNOS`   | `$YUNETAS_OUTPUTS/yunos`                    |

> ℹ️ **Layout contract.** `outputs/` and your own project repos sit as
> siblings of the `yunetas` repo (e.g. `~/yunetaprojects/myproject/`).
> Pick the parent dir freely; keep the sibling relationship.

> ⚠️ **Re-source per shell.** New SSH sessions, cron jobs and CI need
> to source [`yunetas-env.sh`](https://github.com/artgins/yunetas/blob/7.5.7/yunetas-env.sh) again. Without it, [`ybatch`](#util-ybatch) / [`ycommand`](#util-ycommand) /
> [`yshutdown`](#util-yshutdown) are not on `PATH` and deploy scripts fail with
> "command not found".

**Make it permanent** — add to `~/.bashrc`:

```bash
cd ~/yunetaprojects/yunetas
source yunetas-env.sh
```

The script also sources `~/.yunetasrc` if it exists — use that file
for your own personal additions.

(configure-menuconfig)=
### 6. Configure (`menuconfig`)

```bash
cd ~/yunetaprojects/yunetas
menuconfig
```

Pick compiler, build type and the modules you need, then **save** —
this writes `.config`, which the build needs.

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

> ⚠️ **Save `.config` or the build fails.** If you switch compiler,
> re-run [`./set_compiler.sh`](https://github.com/artgins/yunetas/blob/7.5.7/set_compiler.sh) so the external libs are rebuilt with
> the matching toolchain.

### 7. Build and test

Build the bundled external libraries first (one-shot):

```bash
cd ~/yunetaprojects/yunetas/kernel/c/linux-ext-libs
./extrae.sh         # clone libraries
./configure-libs.sh # configure, build and install
```

Then build, install and test yunetas with the **`yunetas` CLI** — this is
the standard build interface (`init` / `build` / `clean` / `test`); prefer
it over calling `cmake` directly, so the install to `$YUNETAS_OUTPUTS/lib`
and the per-yuno relinks happen in the right order:

```bash
cd ~/yunetaprojects/yunetas
yunetas init     # configure build dirs + compiler/build-type from .config (menuconfig)
yunetas build    # regenerate yuneta_version.h + `make install` everything (kernel + yunos)
yunetas test     # ctest
# yunetas clean  # wipe the build dirs
```

Artefacts land in `$YUNETAS_OUTPUTS/` (= `/yuneta/development/outputs/`
by default): `include/`, `lib/`, `bin/`, `yunos/`.

> ℹ️ **Fully static builds** (`CONFIG_FULLY_STATIC=y`) reuse the same
> `configure-libs.sh` with GCC or Clang — no separate toolchain.
> OpenSSL is built with `no-dso` and `no-sock` to avoid glibc
> resolver stubs in the static binary. See
> `kernel/c/linux-ext-libs/HACKS.md` for details.

---

## Troubleshooting

````{dropdown} Bring back /var/log/syslog
```bash
sudo apt-get update
sudo apt-get install rsyslog
sudo systemctl enable --now rsyslog
```
````

````{dropdown} Fix "Setting locale failed" warnings
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
