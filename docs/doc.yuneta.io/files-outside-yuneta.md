(files-outside-yuneta)=
# Files outside `/yuneta`

Yuneta keeps its files in `/yuneta`. The binaries, the configurations, the
stores and the logs of the yunos are all there. The `yuneta-agent` package
(`.deb` or `.rpm`) also puts a small set of files in the system directories,
and its install scripts change some system files. This page lists all of
them.

The set must stay small. Some sites do not let the user `yuneta` use `sudo`.
On such a site, the operator can install the package as root and do nothing
more. Each file outside `/yuneta` is a dependency on the administrator of the
machine, and each feature that needs root fails on such a site.

To see the list on a node, run one of these commands on that node:

```bash
# Debian / Ubuntu
dpkg -L yuneta-agent | grep -v '^/yuneta'
dpkg-query -W -f='${Conffiles}\n' yuneta-agent

# RHEL / Rocky / Alma
rpm -ql yuneta-agent | grep -v '^/yuneta'
rpm -qc yuneta-agent
```

The first command of each pair lists the files and the directories. The
second command lists the configuration files that an upgrade does not
replace.

(files-outside-yuneta-package)=
## Files in the package

The package contains no compiled binary outside `/yuneta`. The files below
are configuration files, systemd units, shell scripts, and two files of
license text.

| File | `.deb` | `.rpm` | Purpose |
|---|---|---|---|
| `/etc/profile.d/yuneta.sh` | conffile | config | Puts `/yuneta/bin` on the `PATH` of a login shell. |
| `/etc/sudoers.d/90-yuneta` | conffile | config | Gives the user `yuneta` `NOPASSWD: ALL`. |
| `/etc/init.d/yuneta_agent` | conffile | config | The SysV script. Under systemd, it drives the two agent units. |
| `/etc/letsencrypt/renewal-hooks/deploy/reload-certs` | conffile | config | The certbot hook. It copies the new certificates into `/yuneta/store/certs` and tells the agent. |
| `/etc/logrotate.d/yuneta` | conffile | config | Rotates the logs of the web server and `/var/log/yuneta/*.log`. |
| `/etc/fail2ban/filter.d/yuneta-nginx-probe.conf` | conffile | config | The fail2ban filter for clients that probe the web server. |
| `/etc/fail2ban/jail.d/yuneta-nginx.conf` | conffile | config | Two fail2ban jails, with `enabled = false`. |
| `/etc/sysctl.d/99-yuneta-core.conf` | conffile | config | `kernel.core_pattern = /var/crash/core.%e` and the kernel limits. |
| `/etc/security/limits.d/99-yuneta-core.conf` | conffile | config | The limits of the user `yuneta`: `core`, `nofile` and `memlock`. |
| `/etc/yuneta/authorized_keys` | conffile | config | Only in a package built with SSH keys. |
| `/usr/lib/systemd/system/yuneta_agent.service` | file | file | The unit of the main agent. |
| `/usr/lib/systemd/system/yuneta_agent22.service` | file | file | The unit of `yuneta_agent22`, the second agent. |
| `/usr/lib/systemd/system/yuneta-webserver.service` | file | file | The unit of the web server (nginx or openresty). |
| `/usr/lib/systemd/system/yuneta-core-pattern.service` | file | no | Sets `core_pattern` again after apport (Ubuntu). |
| `/usr/lib/tmpfiles.d/yuneta-crash.conf` | file | file | Makes `/var/crash` writable by the group `yuneta` at each boot. |
| `/usr/libexec/yuneta/yuneta-node-uuid` | file | file | Script, run as root. It makes the node uuid from the hardware. |
| `/usr/libexec/yuneta/yuneta-agent-cert` | file | file | Script, run as root. It makes the certificate of the agent. |
| `/usr/libexec/yuneta/yuneta-node-label` | file | file | Script, run by hand. It prints the label of the box. |
| `/usr/libexec/yuneta/install-package` | file | file | Script, run as root by `install-package` of `yuneta_agent22`. It downloads, checks and installs a `yuneta-agent` package. |
| `/usr/share/doc/yuneta-agent/copyright` | file | no | The license. The `.rpm` puts it in `/usr/share/licenses/yuneta-agent/LICENSE.txt`. |
| `/usr/share/doc/yuneta-agent/THIRD-PARTY-NOTICES` | file | no | The licenses of the third-party code. The `.rpm` puts it in `/usr/share/licenses/yuneta-agent/`. |

The package also makes two directories: `/etc/yuneta` and `/var/crash`.

In the `.deb`, a "conffile" is a configuration file that dpkg does not
replace when the operator changed it. In the `.rpm`, "config" is a file
marked `%config(noreplace)`. When the operator changed such a file, rpm
writes the new version next to it as `.rpmnew`. Every file that the package
puts in `/etc` is in one of these two groups. Up to 7.31.28, the `.deb` did
not mark `sysctl.d/99-yuneta-core.conf` and `limits.d/99-yuneta-core.conf`.
Each upgrade replaced them and removed the changes of the operator.

The scripts in `/usr/libexec/yuneta` run as root, so they are not in
`/yuneta`. The user `yuneta` owns `/yuneta`, and it must not be able to change
a script that root runs.

(files-outside-yuneta-install)=
## Changes that the install scripts make

The `postinst` (`.deb`) and `%post` (`.rpm`) scripts change these system
files and objects. These files are not in the package, so the package
manager does not remove them.

| What | `.deb` | `.rpm` |
|---|---|---|
| The user and the group `yuneta`, with `/home/yuneta`. | yes | yes |
| The user `yuneta` in a list of groups. The list contains `sudo` (`.deb`) or `wheel` (`.rpm`). | yes | yes |
| `/home/yuneta/.ssh/authorized_keys`, from `/etc/yuneta/authorized_keys`. | if present | if present |
| `/etc/yuneta/webserver`: `nginx` or `openresty`. Written once, and kept on an upgrade. | yes | yes |
| `session required pam_limits.so` added to `/etc/pam.d/common-session` and `common-session-noninteractive`. | yes | no |
| `pam_limits.so` added to `/etc/pam.d/system-auth` and `password-auth`. | no | yes |
| `en_US.UTF-8` and `es_ES.UTF-8` enabled in `/etc/locale.gen`. | yes | no |
| `LANG=en_US.UTF-8` in `/etc/locale.conf`, if that file is empty. | no | yes |
| `enabled=0` in `/etc/default/apport`, if that file exists. | yes | no |
| `/var/crash` owned by `root:yuneta`, mode `0775`. | yes | yes |
| The kernel settings of `99-yuneta-core.conf`, applied at once with `sysctl`. | yes | yes |
| rsyslog enabled and started. | yes | yes |
| The units enabled: links in `/etc/systemd/system/`. | yes | yes |
| `/run/reboot-required`, a hint only. The install never reboots. | yes | yes |
| SELinux local policy: a file-context rule `bin_t` for four files of `/yuneta` (see below), when SELinux is on. `%postun` removes the rules at a full uninstall. | no | yes |

`/yuneta` is outside the policy of SELinux, so SELinux labels its files
`default_t`, and systemd cannot start a `default_t` file (the unit fails
with `203/EXEC`). So the `%post` of the `.rpm` adds a local rule `bin_t`
(`semanage fcontext`) for each file of `/yuneta` that systemd starts, and
applies it (`restorecon`):

- `/yuneta/bin/yuneta-webserver`, started by `yuneta-webserver.service`.
- `/yuneta/agent/yuneta_agent` and `/yuneta/agent/yuneta_agent22`, started
  by their units.
- `/yuneta/bin/restart-yuneta`, started by the certbot hook in a transient
  unit (after 7.31.29).

To see the rules and the labels on a node:

```bash
semanage fcontext -l -C | grep /yuneta
ls -Z /yuneta/bin/yuneta-webserver /yuneta/agent/yuneta_agent \
    /yuneta/agent/yuneta_agent22 /yuneta/bin/restart-yuneta
```

Before an upgrade, the install scripts copy `/etc/yuneta/webserver` to
`/etc/yuneta/webserver.pkgsave`, and put it back after the upgrade. The
`.deb` `preinst` also moves a `/etc/logrotate.d/yuneta` that dpkg does not
own to `/etc/logrotate.d/yuneta.dpkg-bak`. An operator
can give a different list of groups with the variable `YUNETA_GROUPS`
at install time.

Two helpers in `/yuneta/bin` change the system, but only when an operator
runs them as root:

- `install-certbot.sh` installs certbot from snap and makes the link
  `/usr/bin/certbot` to `/snap/bin/certbot`.
- `install-yuneta-dev-deps.sh` installs the developer packages of the distro,
  and installs `kconfiglib` and `yunetas` with pipx into
  `/home/yuneta/.local/bin`.

(files-outside-yuneta-sudo)=
## What needs root at run time

The agents and the yunos run as the user `yuneta`. These features use
`sudo -n`. On a node where `yuneta` cannot use `sudo`, they fail, and the
rest of Yuneta works.

| Feature | Command | Without `sudo` |
|---|---|---|
| Certificate sync of the agent, every 15 minutes | `cert_sync_copy_cmd`: `/usr/bin/sudo -n /yuneta/store/certs/copy-certs.sh` | The copy fails and the agent logs a WARNING at each sync. The certbot hook runs as root and copies the certificates. To stop the sync, set `cert_sync_enabled` to `0`. |
| `install-package` of `yuneta_agent22` | `sudo -n /usr/libexec/yuneta/install-package start` | The node cannot install a package through the agent. Root must install it. |
| `install-package-status` of `yuneta_agent22` | `sudo -n /usr/libexec/yuneta/install-package journal <lines>` | The answer shows the error of `sudo` in place of the journal. |
| `upgrade-agent` of `yuneta_agent22` (source nodes only) | `sudo -n systemctl restart yuneta_agent.service` | The new binary is on disk, but the agent does not restart. |
| `restart-yuneta`, and the restart action of `logcenter` | `sudo -n systemctl restart yuneta_agent.service` | The script refuses and says so. |

The package gives `sudo` to `yuneta` with the file `/etc/sudoers.d/90-yuneta`
(`NOPASSWD: ALL`). The package also puts `yuneta` in the group `sudo` (or
`wheel`), but `yuneta` has no password, so the group alone does not give
`sudo`. The standard package keeps this design. A site that does not let
`yuneta` use `sudo` has two choices:

- The site removes `NOPASSWD: ALL` from `/etc/sudoers.d/90-yuneta`. The file
  is a configuration file of the package, so an upgrade keeps the change.
- ArtGins supplies a package made for that site.

On such a site, the features of the table above fail as the table says.

Such a site can give `sudo` for one feature only. For `install-package` and
`install-package-status`, grant exactly the program and its two subcommands:

```text
# /etc/sudoers.d/91-yuneta-install-package (edit it with visudo -f)
yuneta ALL=(root) NOPASSWD: /usr/libexec/yuneta/install-package start, /usr/libexec/yuneta/install-package journal *
```

The program checks every value that `yuneta` wrote, and it reads its own
data only from a directory of root. Do not grant `systemd-run`: its options
can run any command. This grant is still equivalent to root, because the
operator chooses the package, and the scripts of the package run as root.

(files-outside-yuneta-root-run)=
### Files of `/yuneta` that root runs

Removing `sudo` from `yuneta` does not make `yuneta` a privilege boundary
today. The user `yuneta` owns `/yuneta` (the install scripts run
`chown -R yuneta:yuneta /yuneta`), and root runs these files of
`/yuneta`:

| File | Run as root by | When |
|---|---|---|
| `/yuneta/bin/yuneta-webserver`, and the nginx or openresty under `/yuneta/bin/nginx/` or `/yuneta/bin/openresty/` | `yuneta-webserver.service` (it has no `User=`) | At each boot, and at each start of the unit. |
| `/yuneta/bin/nginx/sbin/nginx -s reload` (or `/yuneta/bin/openresty/bin/openresty -s reload`) | The certbot deploy hook | After each renewal of a certificate. |
| `/yuneta/store/certs/copy-certs.sh` | The certbot deploy hook, and `sudo` from the agent's cert sync | After each renewal of a certificate, and every 15 minutes. |
| `/yuneta/bin/yuneta-webserver type` | `/etc/init.d/yuneta_agent status` | Each time root asks for the status. |
| `/yuneta/bin/yuneta-webserver quit` and `stop` | The `postinst` (`.deb`) and the `%post` (`.rpm`) | At each install and upgrade of the package. |

Root also works in two places of `yuneta` without running a file there:

- logrotate rotates `/yuneta/bin/nginx/logs/*.log` and
  `/yuneta/bin/openresty/nginx/logs/*.log` as root, with no `su`, in a
  directory that `yuneta` owns (`/etc/logrotate.d/yuneta`). The stanza
  keeps it that way for now: with `su yuneta yuneta`, logrotate cannot
  give a rotated file the owner `root` of the openresty nodes, and its
  `postrotate` signals a master that runs as root.
- A child that `setpriv` (or `su`) starts as `yuneta` keeps the
  controlling terminal of root when root runs the hook from a terminal.
  Where the kernel allows `TIOCSTI`, such a child can type into that
  terminal. The hook and `copy-certs.sh` start their children with
  `setsid`, so they have no terminal.

So `yuneta` can change a file that root runs later, and it gets the rights
of root. This breaks rule 5 below. To find these places on a node, run:

```bash
systemctl cat yuneta-webserver.service | grep -E '^(User|ExecStart)='
grep -n '/yuneta/' /etc/letsencrypt/renewal-hooks/deploy/reload-certs \
    /etc/init.d/yuneta_agent /etc/logrotate.d/yuneta
# The install scripts of the package
grep -n '/yuneta/bin/' /var/lib/dpkg/info/yuneta-agent.postinst   # Debian
rpm -q --scripts yuneta-agent | grep -n '/yuneta/bin/'             # RHEL
```

These files no longer run as root (after 7.31.29):

- `/yuneta/bin/check-certs-validity.sh`. `copy-certs.sh` runs it as
  `yuneta`.
- `/yuneta/bin/restart-yuneta` and `/yuneta/bin/ycommand`. The certbot
  hook runs them as `yuneta`: `ycommand` with `setpriv --reset-env`, in a
  session of its own (`setsid`), and `restart-yuneta` in a transient unit
  of its own (`systemd-run --uid=yuneta`), so that it outlives the unit
  of certbot.

`copy-certs.sh` opens the keys of `/etc/letsencrypt/live/` as root, and
`yuneta` compares them with its copies and writes only the files that
changed, in `/yuneta/store/certs/`. No key is written anywhere else. The
script checks the names of `certs.list` and the paths of the keys, and it
refuses a symbolic link in place of a destination directory.

The full fix is a task for a package without `sudo`, and it is designed:

- `copy-certs` becomes a program of the package, owned by root, in
  `/usr/libexec/yuneta/`, like `install-package`. It reads `certs.list`
  as data.
- `yuneta-webserver.service` runs the web server as `User=yuneta` with
  `AmbientCapabilities=CAP_NET_BIND_SERVICE`, and the hook reloads it with
  `systemctl reload yuneta-webserver`. The `ssl_certificate` lines of the
  web server must then read `/yuneta/store/certs/`, not
  `/etc/letsencrypt/live/`, which only root can read.
- The SysV status, the install scripts and logrotate stop running or
  working as root in `/yuneta`.

(files-outside-yuneta-runtime)=
## Files that the software uses at run time

The yunos are static binaries. Static glibc has no NSS and no system
resolver, so Yuneta reads these files itself:

- `/etc/passwd` and `/etc/group`, for users and groups.
- `/etc/resolv.conf` and `/etc/hosts`, for the DNS resolver.

The yunos also read these files. They do not write them:

- `/etc/hostname`, and `/etc/os-release` (or `/usr/lib/os-release`).
- `/usr/share/zoneinfo`, for time zones.
- The CA bundle of the system, for TLS. Yuneta tries
  `/etc/ssl/certs/ca-certificates.crt`, `/etc/pki/tls/certs/ca-bundle.crt`,
  `/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem`,
  `/etc/ssl/ca-bundle.pem` and `/etc/ssl/cert.pem`.
- `/var/lib/yuneta/last-deploy-hook-run`, read by the agent. The certbot hook
  writes it.
- `/var/log/fail2ban.log`, read by `webstats`. The attribute
  `fail2ban_log_path` changes the path, and an empty value turns it off.
- `/usr/bin/rsync`, run by `archive_sender`.

These files are written at run time:

- `/var/crash/core.<binary>`: the core dump of a yuno that crashes.
- `/var/log/yuneta/deploy-hook.log` and `copy-certs.log`: written by the
  certbot hook, as root.
- `/run/yuneta-copy-certs.lock`: the lock of `copy-certs.sh`, made by
  root (empty). The certbot hook and the agent's cert sync never run it at
  the same time. `/run` is cleared at each boot.
- `/var/lib/yuneta/install-package/`: written by
  `/usr/libexec/yuneta/install-package`, as root. It holds the URL and the
  sha256 of the last install, its `result`, and the download while the
  install runs. Root owns the directory, mode `0755`, so `yuneta` can read
  `result` but cannot change a file there.

(files-outside-yuneta-rules)=
## Rules for a change

These rules keep the list short and keep this page correct:

1. Put a new file in `/yuneta` when you can. A file outside `/yuneta` needs a
   reason that `/yuneta` cannot meet, for example a path that the system
   reads (`/etc/sysctl.d`) or a script that root runs.
2. If you add, remove or move a file outside `/yuneta`, update this page in
   the same commit. Do the same for a system change in an install script.
3. Mark every file that the package puts in `/etc`: list it in
   `DEBIAN/conffiles` (`packages/deb/make-yuneta-agent-deb.sh`) and mark it
   `%config(noreplace)` (`packages/rpm/make-yuneta-agent-rpm.sh`).
4. Do not add a feature that needs `sudo` when a feature without root can do
   the work. If a feature needs `sudo`, add it to the
   [table of root features](#files-outside-yuneta-sudo), with what happens
   without `sudo`.
5. Do not let `yuneta` write a file that root runs.
