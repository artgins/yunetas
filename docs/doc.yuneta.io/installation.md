# **Installation**

> **Prerequisites:** Linux, Python 3.7+, `sudo` access.
> Full dependency lists and licenses are in [Reference](#reference) below.

There are two ways to install Yunetas, depending on what you want to do:

- **[Quick install](#quick-install)** — pre-built `.deb`, no compiler needed.
  Use this to *run* yunos on a server.
- **[Build from source](#build-from-source)** — full SDK. Use this to
  *develop* with the framework or hack on it.

> ℹ️ The PyPI package `yunetas` (`pipx install yunetas`) is the
> management/build CLI (currently 0.x), **not** the C framework runtime
> (currently 7.x). The `.deb` bundles both; building from source uses
> the CLI to drive the build.

---

(quick-install)=
## Quick install

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
curl -fsSL https://raw.githubusercontent.com/artgins/yunetas/main/install.sh | sudo sh -s -- 7.3.1
```

Or download the `.deb` manually from the
[Releases page](https://github.com/artgins/yunetas/releases) and run:

```bash
sudo apt install ./yuneta-agent-<version>-<release>-<arch>.deb
```

The package installs the agent + CLI tools + bundled openresty under
`/yuneta/`, creates the `yuneta` system user, applies kernel tuning
and PAM limits, and starts the SysV service. Full inventory in
[`packages/README.md`](https://github.com/artgins/yunetas/tree/main/packages).

> ℹ️ **Build the `.deb` yourself** instead of using the published asset:
> see `packages/README.md` for the four arch wrapper scripts
> (`AMD64.sh`, `ARM32.sh`, `ARMhf.sh`, `RISCV64.sh`). Requires the
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

```bash
sudo apt -y install --no-install-recommends \
  git mercurial make cmake ninja-build \
  gcc clang g++ \
  python3-dev python3-pip python3-setuptools \
  python3-tk python3-wheel python3-venv \
  libjansson-dev libpcre2-dev liburing-dev \
  libpcre3-dev zlib1g-dev libssl-dev \
  perl dos2unix tree curl \
  postgresql-server-dev-all libpq-dev \
  kconfig-frontends telnet pipx \
  patch gettext fail2ban rsync \
  build-essential pkg-config ca-certificates linux-libc-dev

pipx install kconfiglib
```

```{dropdown} What each non-obvious package is for
- `libjansson-dev`          — required for libjwt
- `libpcre2-dev`            — required by openresty
- `perl dos2unix mercurial` — required by openresty
- `pipx kconfiglib`         — yunetas configuration tool
- `kconfig-frontends`       — alternative configuration tool
- `telnet`                  — required by tests
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

`yunetas-env.sh` exports three variables and prepends `/yuneta/bin`
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
> to source `yunetas-env.sh` again. Without it, `ybatch` / `ycommand` /
> `yshutdown` are not on `PATH` and deploy scripts fail with
> "command not found".

**Make it permanent** — add to `~/.bashrc`:

```bash
cd ~/yunetaprojects/yunetas
source yunetas-env.sh
```

The script also sources `~/.yunetasrc` if it exists — use that file
for your own personal additions.

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
> re-run `./set_compiler.sh` so the external libs are rebuilt with
> the matching toolchain.

### 7. Build and test

Build the bundled external libraries first (one-shot):

```bash
cd ~/yunetaprojects/yunetas/kernel/c/linux-ext-libs
./extrae.sh         # clone libraries
./configure-libs.sh # configure, build and install
```

Then build, install and test yunetas:

```bash
cd ~/yunetaprojects/yunetas
yunetas init
yunetas build
yunetas test
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

- [Jansson](http://jansson.readthedocs.io/en/latest/) — MIT
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
