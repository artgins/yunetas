# sshd scripts

Five scripts under `tools/sshd/` audit sshd, keep it reachable under a
connection flood, and stop or start it safely. The installation packages ship
them at `/yuneta/development/yunetas/tools/sshd/`. They are not on the `PATH`.

**The packages do not run them.** How sshd behaves is the operating-system
policy of the operator of the node. A package that installs the agent can land
on a node of another company, so it does not change that policy. Run these
scripts by hand, on a node that you operate. On a node of another company, run
them only when its operator agrees.

Each script asks for root through `sudo` if you start it as another user.

```bash
T=/yuneta/development/yunetas/tools/sshd
```

| Script | What it does |
|---|---|
| `audit-sshd.sh` | Reports failures and possible improvements. Changes nothing. |
| `install-sshd-flood-guard.sh` | Keeps sshd reachable under a connection flood. |
| `install-fail2ban-sshd-jail.sh` | Makes fail2ban watch sshd where the distribution does not. |
| `stop-sshd.sh` | Stops sshd and arms a timer that starts it again. |
| `start-sshd.sh` | Starts sshd and cancels that timer. |

## Why a flood guard

On 2026-09-13 a password botnet sent 2,000-3,000 login attempts an hour to
each node. Password login was off, so no attempt got in. But each connection
held one of the sshd slots for unauthenticated connections. The stock sshd
allows 10 of them. Past 10, sshd drops new connections at random, and the
operator's logins and the deploy tools' `rsync` were dropped too: up to 1,464
drops an hour on one node. With the flood guard, the drops went to zero.

The flood guard does not stop brute force. Password login off stops brute
force, and `audit-sshd.sh` reports a node where it is on. The real exposure is
port 22 open to the world. The fix for that is a source allowlist (the
provider's firewall, or nftables), and in the end a sealed node with no inbound
SSH.

## `audit-sshd.sh`

Reads `sshd -T`, which prints the values that sshd uses, and reports what is
wrong or can be better. It reads `sshd -T` and not `sshd_config`, because
drop-ins, distribution defaults and crypto policies set the rest.

```bash
$T/audit-sshd.sh          # failures and improvements
$T/audit-sshd.sh --all    # also what is already correct
```

It checks:

- **Authentication:** password and keyboard-interactive login, root login,
  empty passwords, `AllowUsers` / `AllowGroups`, and more.
- **The flood settings:** `LoginGraceTime`, `MaxStartups`,
  `PerSourceMaxStartups`, `PerSourcePenalties`.
- **Algorithms and host keys:** weak ciphers, MACs and key exchange, DSA and
  short RSA host keys, host keys that other users can read.
- **Files:** the owner and mode of `sshd_config` and its drop-ins, and of the
  `authorized_keys` of `root` and `yuneta`.
- **The last hour of the journal:** connections that sshd dropped, and failed
  logins.
- **fail2ban:** is it installed, running, and watching sshd?

Each finding has a level:

| Level | Meaning |
|---|---|
| `FAIL` | Lets an attacker in, or makes it easier. Fix it. |
| `WARN` | A real weakness, or a flood that occurs now. |
| `INFO` | A possible improvement. Compare it with how the node is used. |
| `OK` | Correct. Shown only with `--all`. |

The exit status is `2` if there is a `FAIL`, `1` if there is a `WARN`, and `0`
if not. You can use it in a script:

```bash
if ! $T/audit-sshd.sh > /tmp/sshd-audit.txt; then
    echo "sshd audit found problems: see /tmp/sshd-audit.txt"
fi
```

Example output:

```
== Authentication
[FAIL] PasswordAuthentication yes: a botnet can try passwords. Fix: PasswordAuthentication no, once key login is confirmed to work
[INFO] No AllowUsers/AllowGroups: every account with a shell can try. Listing the accounts that log in (e.g. AllowUsers yuneta) closes the rest

== Availability under a connection flood
[WARN] MaxStartups 10:30:100: past 10 unauthenticated connections sshd drops new ones AT RANDOM, yours included. See install-sshd-flood-guard.sh (50:30:200)

== Last hour in sshd's journal
[WARN] 412 connections dropped by MaxStartups/penalties in the last hour: legitimate logins lose that lottery too. See install-sshd-flood-guard.sh

1 FAIL, 2 WARN, 1 INFO.
```

`sshd -T` prints the global values. A `Match` block can change them for some
users or addresses. The script tells you when there is a `Match` block. To see
the values for one connection, give its user, host and address:

```bash
sudo sshd -T -C user=yuneta,host=deploy.example.com,addr=203.0.113.7
```

## `install-sshd-flood-guard.sh`

Writes `/etc/ssh/sshd_config.d/10-yuneta-ssh-flood.conf`:

```
LoginGraceTime 20
MaxStartups 50:30:200
PerSourceMaxStartups 10
```

| Directive | Stock | Effect |
|---|---|---|
| `LoginGraceTime 20` | `120` | sshd closes a connection that has not authenticated in 20 s, so its slot is free sooner. |
| `MaxStartups 50:30:200` | `10:30:100` | sshd starts to drop at 50 unauthenticated connections, not 10, and drops all of them at 200. |
| `PerSourceMaxStartups 10` | no limit | One address can hold 10 slots at most. This stops the few addresses that send many connections, not a flood from many addresses. Needs OpenSSH 8.5 or later. |

```bash
$T/install-sshd-flood-guard.sh            # install or update
$T/install-sshd-flood-guard.sh --check    # show the values sshd uses, change nothing
$T/install-sshd-flood-guard.sh --remove   # remove the file
```

What it makes sure of:

- It runs `sshd -t` before it changes anything. If sshd rejects the new file
  (for example, OpenSSH older than 8.5), the previous file comes back and sshd
  is not reloaded.
- It reloads sshd. It never restarts sshd.
- It reads `sshd -T` after the change. If a value is not the value in effect,
  it tells you why: a drop-in that sshd reads first wins.
- It warns when password login is on. It never turns password login off,
  because that can lock you out of a node that you reach only with a password.

sshd keeps the first value that it reads. To use a different value on one
node, add a file with a lower number, for example
`/etc/ssh/sshd_config.d/05-local.conf`:

```
MaxStartups 100:30:300
```

Then run `sudo systemctl reload ssh` (Debian) or `sudo systemctl reload sshd`
(RHEL).

## `install-fail2ban-sshd-jail.sh`

Writes `/etc/fail2ban/jail.d/yuneta-sshd.conf`:

```ini
[sshd]
enabled = true
backend = systemd
```

Debian enables the `sshd` jail in its own `jail.d/defaults-debian.conf`.
RHEL and Rocky enable no jail. Thus a Rocky node can run fail2ban with nothing
that watches sshd. If the distribution already runs an `sshd` jail, the script
tells you and changes nothing.

`backend = systemd` is necessary because sshd logs to the journal. On EL9 it
needs `python3-systemd`.

```bash
$T/install-fail2ban-sshd-jail.sh            # install
$T/install-fail2ban-sshd-jail.sh --check    # show the sshd jail
$T/install-fail2ban-sshd-jail.sh --remove   # remove the jail
```

The script runs `fail2ban-client -t` before it reloads fail2ban. A jail that
fail2ban cannot configure stops the whole fail2ban server, with all its other
jails. If the new jail causes the failure, the previous state comes back.

A jail does not stop a botnet, which uses thousands of addresses and a small
number of attempts from each. It stops one address that sends many attempts.

## `stop-sshd.sh` and `start-sshd.sh`

When sshd stops, port 22 closes, and a connection flood cannot get past a closed
port. But on a node that you reach only with SSH, a stopped sshd can also lock
you out. Thus `stop-sshd.sh`:

1. Runs `sshd -t`. If it fails, the script stops nothing, because a timer
   cannot start an sshd that does not start.
2. Arms `yuneta-sshd-autostart.timer`, a transient systemd timer that starts
   sshd again.
3. Stops sshd only after the timer is armed. If the timer cannot be armed, the
   script stops nothing.

```bash
$T/stop-sshd.sh                 # stop now, start again in 60 minutes
$T/stop-sshd.sh 15              # stop now, start again in 15 minutes
$T/stop-sshd.sh --no-restart    # stop now, arm nothing
systemctl list-timers yuneta-sshd-autostart.timer   # when does it start again?
```

Use `--no-restart` only when you can get to the provider's console.

Sessions that are already open stay open. Debian's `ssh.service` and RHEL's
`sshd.service` stop only the listener (`KillMode=process`). The script warns
you if the unit has a different `KillMode`.

While sshd is stopped, you can get to the node only without SSH: through the
provider's console, or through the agent's control plane. From the console,
`start-sshd.sh` starts sshd before the timer does:

```bash
$T/start-sshd.sh
```

It runs `sshd -t` first and shows the errors if there are errors. It starts
sshd (and its socket unit, if that unit is enabled), makes sure that sshd runs,
and cancels the timer.
