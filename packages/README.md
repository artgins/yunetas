# packages

This directory holds the OS packaging for the Yuneta Agent. It makes a `.deb`
and an `.rpm` of the same `/yuneta` runtime payload.

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
package release number from `../../RELEASE`. For the details of each format
(install actions, file layout, packaged CLI tools, system configuration), see
[`deb/README.md`](deb/README.md) and [`rpm/README.md`](rpm/README.md).

The files that the packages put outside `/yuneta`, the system files that their
scripts change, and the features that need `sudo` are listed on the page
[Files outside `/yuneta`](https://doc.yuneta.io/files-outside-yuneta)
(`docs/doc.yuneta.io/files-outside-yuneta.md`). Keep that list as short as
possible: some sites install the package as root and do not let `yuneta` use
`sudo`. When a change adds, moves or removes a file outside `/yuneta`, update
the page in the same commit.

## Release packages and SHA256SUMS

The workflow `.github/workflows/release-packages.yml` builds the AMD64
`.deb` and the x86_64 `.rpm` when a GitHub release is published. It uploads
each package with its `<package>.sha256`. A last job joins every `.sha256`
of the release into `SHA256SUMS`. `install.sh` and the operators of
`install-package` read the sha256 of a package there.

A manual run of the workflow (`workflow_dispatch`) on a tag that is already
published builds the packages again and replaces them. The new packages
have other bytes, so each sha256 that an operator copied before does not
match any more. The run builds the revision of the `RELEASE` file of the
tag (`-1`), never a `-2`.

A package that you upload by hand, for example a `-2` on the same tag or a
riscv64 `.deb`, needs its line in `SHA256SUMS`. Upload the package with its
`.sha256` (made with `sha256sum` in the directory of the package, so that
the line has no directory), then make `SHA256SUMS` again. Do not rebuild
the other packages. Use one of these two ways:

```bash
# 1. The workflow, with the builds skipped
gh workflow run release-packages.yml -f release_tag=7.31.29 -f checksums_only=true

# 2. By hand
mkdir sums && cd sums
gh release download 7.31.29 --pattern '*.sha256'
cat *.sha256 | sort -k2 > SHA256SUMS
gh release upload 7.31.29 --clobber SHA256SUMS
```

## The two packagers

Both packagers stage the same `/yuneta` runtime payload. The payload has the
agent binaries, CLI tools, web server, init scripts, kernel tuning, TLS
certificate management, and a dedicated `yuneta` system user. The packagers
differ only in the packaging format and the install glue of each distro.

| Format | Directory            | Distros                          | Service / sudo glue            |
|--------|----------------------|----------------------------------|--------------------------------|
| `.deb` | [`deb/`](deb/README.md) | Debian / Ubuntu               | `update-rc.d`, `sudo` group    |
| `.rpm` | [`rpm/`](rpm/README.md) | RHEL / Rocky / Alma / Fedora  | `chkconfig`, `wheel` group     |

```
packages/
├── README.md       <- this index
├── templates/      <- shared files: agent config samples, agent units, yuneta-node-uuid, yuneta-node-label, yuneta-agent-cert
├── install-yuneta-node-uuid.sh <- the node uuid by hand, on a node without the package
├── install-yuneta-agent-cert.sh <- the agent's own certificate by hand, on a node without the package
├── tests/          <- tests of the shared scripts (run them from anywhere)
├── deb/            <- Debian packaging (AMD64/ARM32/ARMhf/RISCV64 wrappers)
└── rpm/            <- RPM packaging (x86_64/aarch64/riscv64 wrappers)
```

Both packagers share `templates/`. They reference it by absolute path from
`$YUNETAS_BASE/packages/templates/`. For this reason, `templates/` stays at
this level and not inside `deb/` or `rpm/`.

## The node uuid

`templates/yuneta-node-uuid` writes the node uuid from the hardware (see its
header). `tests/test-yuneta-node-uuid.sh` tests it against a fake `/sys`.

`install-yuneta-node-uuid.sh` installs it by hand on a node that builds from
source and does not take the package. The script is idempotent (a second run
changes nothing). To make sure that a node is correct, run it again.

`templates/yuneta-node-label` prints the label of a box: its node uuid as a
QR code, the model and the MAC of each wired port, as text or as an SVG (see
its header). It reads `yuneta-node-uuid show` and refuses a uuid that the
hardware does not give. The units do not run it. On a node that builds from
source, run it from the tree. `tests/test-yuneta-node-label.sh` tests it
against a fake `/sys`.

`templates/yuneta-label-print` sends a label image (a PNG) to a Brother QL
label printer, through usbfs and without a driver (see its header).
`yuneta-node-label --print` uses it. `tests/test-yuneta-label-print.sh`
compares the bytes that it sends with the bytes of the Brother raster
reference, and gives it fake printer statuses. It needs no printer.

## install-package

`templates/install-package` is the root part of `install-package` of
`yuneta_agent22`. The packages install it as
`/usr/libexec/yuneta/install-package`. `agent22` writes the URL and the
sha256 in `/yuneta/agent/install-package/` and runs
`sudo -n /usr/libexec/yuneta/install-package start`. The program checks the
data, copies it to `/var/lib/yuneta/install-package/`, and starts a
transient unit that downloads, checks and installs the package. Its header
gives the layout, the checks and a `sudoers` line for a site whose `yuneta`
has no `sudo`.

`tests/test-install-package.sh` tests it as a normal user, in a fake root,
with stubs for `curl`, `systemd-run`, `systemctl`, `apt-get`, `dpkg-deb`,
`dnf`, `rpm` and `journalctl`:

```bash
packages/tests/test-install-package.sh
```

## The certificates of Let's Encrypt

`templates/copy-certs.sh` is installed as `/yuneta/store/certs/copy-certs.sh`,
and `templates/reload-certs` as the certbot deploy hook
`/etc/letsencrypt/renewal-hooks/deploy/reload-certs`. Both packagers install
the same two files. Root runs both. They read the keys of `/etc/letsencrypt`
as root, and they run everything else as `yuneta` (`setpriv`). Their headers
and the page
[Files outside `/yuneta`](https://doc.yuneta.io/files-outside-yuneta) give
the limit: root still runs files of `/yuneta`.

`tests/test-copy-certs.sh` tests both as a normal user, in a fake root, with
a stub of `setpriv` that writes down the user it runs a command as:

```bash
packages/tests/test-copy-certs.sh
```

## The certificate of the agent

`templates/yuneta-agent-cert` makes the own pair of the agent
(`/yuneta/agent/certs/yuneta_agent.{crt,key}`) on the node. It makes a pair in
these cases:

- There is no pair.
- The pair is the pair published with Yuneta up to 7.30.5.
- The script made the pair for another node uuid, for example on a cloned
  disk. `yuneta_agent.uuid` holds the uuid that each pair was made for. The
  script removes the key of the other node. It does not keep it.

For the details, see the header of the script. The units of both agents run
it, after `yuneta-node-uuid`.

`install-yuneta-agent-cert.sh` installs it by hand on a node without the
package, after `install-yuneta-node-uuid.sh` ran. The packages carry no agent
pair. They carry only `certs/controlcenter.crt`.

`tests/test-yuneta-agent-cert.sh` tests the script in a private directory: a
cloned disk against a fake `/sys`, with `yuneta-node-uuid`. The test also
makes sure of these points:

- `yunos/c/yuno_agent/service/` (`yuneta_agent`, `install-yuneta-service.sh`,
  `remove-yuneta-service.sh`) holds the files of the `.deb`. A node that
  builds from source installs that copy.
- The helper, the postinsts, the `%pre` of the `.rpm`, and the SysV script
  find an agent outside its unit by its `argv[0]`. They find it when its
  executable is replaced, renamed, behind a symlinked `/yuneta`, or
  unreadable. They leave the SysV script alone. For this point, the test runs
  the `install-yuneta-service.sh` of both packagers in a fake root, with stubs
  for `systemctl`, `pgrep` and `su`, and a fake `/proc`. The cases are systemd
  with and without the units, a unit that does not restart, and an old
  `/etc/init.d/yuneta_agent`.
- The build of the agent gives its warning about the installed scripts
  (`cmake -P`).
