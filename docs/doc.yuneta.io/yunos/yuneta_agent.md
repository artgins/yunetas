(yuno-yuneta_agent)=
# `yuneta_agent`

The primary agent — the supervisor of a Yuneta node. It manages the lifecycle
of every other yuno (create / run / kill / update / delete), owns the realms
and their on-disk layout, exposes the control plane, and coordinates inter-yuno
communication. [`ycommand`](#util-ycommand), [`ystats`](#util-ystats), [`ylist`](#util-ylist) and [`ybatch`](#util-ybatch) talk to this yuno by
default.

(yuneta-agent-systemd)=
## Starting and stopping it: systemd

Since 7.25.22 the two agents run as native systemd units,
`yuneta_agent.service` and `yuneta_agent22.service`, independent of each
other. systemd starts them at boot and supervises the process that matters:
the agent's own watcher (which relaunches the agent if it crashes) is the
unit's main process, through `--pid-file`. The packages install and enable
both units.

```bash
systemctl status yuneta_agent            # state, main pid, last lines of its log
sudo systemctl restart yuneta_agent      # the main agent alone
sudo systemctl restart yuneta_agent22    # agent22 alone
journalctl -u yuneta_agent -n 50         # what systemd saw (start, stop, failures)
```

`service yuneta_agent ...` and `service yuneta_agent22 ...` reach the same
units. The SysV script is still installed, and under systemd it drives the
PAIR of units, as it always treated the two agents:

| `sudo /etc/init.d/yuneta_agent ...` | What it does |
|-------------------------------------|--------------|
| `start` | Starts the main agent's unit, then agent22's, once the main one is up (a broken binary never takes both down). |
| `stop` | Stops the main agent alone. agent22, the escape hatch, keeps running. |
| `restart` | Restarts the main agent, and starts agent22 if it was not running. |
| `status` | Reports both. |

What to know before touching them:

- **Starting a unit needs root**, as the SysV script always did (it ran
  `su - yuneta`). **Stopping does not**: `--stop` run as `yuneta` ends the
  agent in order, and its unit goes `inactive`.

  ```bash
  /yuneta/agent/yuneta_agent --config-file=/yuneta/agent/yuneta_agent.json --stop
  ```

- **An agent started by hand runs OUTSIDE its unit.** `yuneta_agent --start`
  launched from a shell is a process of that shell: systemd does not see it,
  `systemctl status` says `inactive`, and the next boot is the unit's, not
  yours. To put it back, `sudo /etc/init.d/yuneta_agent start` (it stops the
  agent found outside and starts the unit) or `sudo systemctl restart
  yuneta_agent`.
- **A new agent binary is deployed with the unit**: move the binary into place
  and restart that unit, one agent at a time, never both.

  ```bash
  cp yuneta_agent /yuneta/agent/yuneta_agent.new
  mv /yuneta/agent/yuneta_agent.new /yuneta/agent/yuneta_agent
  sudo restorecon /yuneta/agent/yuneta_agent    # only on SELinux (RHEL / Rocky)
  sudo systemctl restart yuneta_agent
  ```

  On RHEL and Rocky, systemd cannot exec a file labelled `default_t`, and
  everything under `/yuneta` is: the package writes a `bin_t` rule for both
  agents into the policy (`semanage fcontext`; it requires
  `policycoreutils-python-utils` where `selinux-policy` is installed), and
  `restorecon` gives a file moved in by hand that label back -- a `mv` keeps
  the label of the source. Without it the
  unit dies with `203/EXEC` (*"Failed to locate executable ... Permission
  denied"*).
- **The yunos outlive a restart of their agent.** They are in the unit's
  cgroup, but a stop takes only the agent (`KillMode=process`); the journal
  says *"Found left-over process"* for them at the next start, which is that
  rule working.
- **A node built from source** (no package) gets the units by hand, from the
  repo's templates:

  ```bash
  sudo install -m 0644 packages/templates/yuneta_agent.service \
                       packages/templates/yuneta_agent22.service /usr/lib/systemd/system/
  sudo systemctl daemon-reload
  sudo systemctl enable yuneta_agent yuneta_agent22
  /yuneta/agent/yuneta_agent22 --config-file=/yuneta/agent/yuneta_agent22.json --stop
  sudo systemctl restart yuneta_agent22                      # agent22 first, then:
  /yuneta/agent/yuneta_agent --config-file=/yuneta/agent/yuneta_agent.json --stop
  sudo systemctl restart yuneta_agent
  ```

  A development machine where the agents run as the developer adds a drop-in,
  `/etc/systemd/system/yuneta_agent.service.d/override.conf` (and the same for
  agent22), with `[Service]`, `User=<you>`, `Group=<you>`.

Why each line of the units is there (`Type=forking`, `KillMode=process`,
`Restart=no`, `ExecStop`, `ExecStopPost`) is in
[Entry point §4.7](#entry-point-systemd) and in the comments of the units themselves.

## Control plane ports

| URL | Purpose |
|-----|---------|
| `ws://127.0.0.1:1991` | Local, plaintext control plane (default for `ycommand`) |
| `wss://0.0.0.0:1993` | Secure control plane (TLS + OAuth2) |

## Connecting out to a control center

When the node has an owner (`node_owner` ≠ `"none"`), the agent starts an
outbound [`C_IEVENT_CLI`](#gclass-c-ievent-cli) client service named `controlcenter` and dials the
control center. The remote URL is assembled from config variables:

```
tcps://(^^__sys_machine__^^).(^^__node_owner__^^).(^^__output_url__^^)
```

that is, `tcps://<machine>.<node_owner>.<output_url>`, where `__output_url__`
defaults to `yunetacontrol.com:1988` (1994 up to 7.30.3), and the link trusts
only `/yuneta/agent/certs/controlcenter.crt`. With an empty/`none` owner the
agent does not dial out.

A node points its agent at another control center from its json file
(`/yuneta/agent/yuneta_agent.json`, 7.30.5+), with the same key the agent's
`main.c` uses for the default:

```json
"global": {
    "controlcenter.__json_config_variables__": {
        "__output_url__": "yunetacontrol.com:1988"
    }
}
```

A top-level `__json_config_variables__` in that file changes nothing: its scope
is the file itself, and the agent's urls are in `main.c`. The input urls
(`ws://127.0.0.1:1991`, `wss://0.0.0.0:1993`) cannot be changed from the file.

**The agent shows who it is with its own key** (7.30.10+). On the link to the
control center the agent presents its own pair,
`/yuneta/agent/certs/yuneta_agent.{crt,key}`, as the client certificate. Each
node makes that pair for itself (see *The agent's own certificate* in the
installation guide). The control center pins the node's uuid to the key once an
operator accepts it ([`controlcenter`](controlcenter.md), "Each node's key"). A
node with no pair cannot open that link: the connection fails and says so in
the log, and the agent's unit makes the pair again at its next start. Since
7.30.11 the accepted key is also the agent's login: the agent needs no token
(`controlcenter.jwt`) in its json, and the package's template carries none. A
new node connects with its key, and appears in the control center's
`list-nodes` as `knocking` until an operator accepts it with the key read on
the node.

**The agent verifies who asks** (7.30.9+). The control center forwards an
operator's command with an **operator assertion**: a short token that the
control center signs for this node (`aud` is the node uuid), and that names
the operator (`sub`). The agent verifies the assertion with the control
center's public key (`Authz.assertion_jwks` in its `main.c`). Then it takes
the `sub` as the operator, and ignores the `__username__` that the control
center wrote.

- A command with no assertion is of the principal `controlcenter`. Examples
  are a control center with no key, or a request with no user.
- A bad assertion is refused: another key, another node, or expired by more
  than the day of leeway (`Authz.assertion_time_leeway`).

The agent accepts the day of leeway because a box whose clock drifts must
still take its control center's commands. To take the control center's word
again, put this in the json file:

```json
"global": {
    "controlcenter.verify_operator": false
}
```

See [`C_IEVENT_CLI`](#gclass-c-ievent-cli) and
[`C_AUTHZ`](#gclass-c-authz). See [`controlcenter`](controlcenter.md) for the
signing side.

## Audit files

The agent writes every command that it runs to a daily audit file in
`/yuneta/realms/agent/agent/audit/` (`use_audit_command_file`, on by default).
**An agent whose commands cannot be audited at start does not run.** If the
agent cannot make the `audit` directory, or cannot create or open the file in
it, at start, it stops, as a yuno whose file log cannot open. The cause goes to
stdout and syslog, and the watcher relaunches the agent until the cause is
fixed: 2 s after the first stop, then at a growing interval, up to one
relaunch a minute ([Entry point §4.3](#entry-point-watcher)).
The causes are a disk with no space left (`ENOSPC`: the
`min_free_disk_percentage` threshold is not checked at start), a read-only
file system, the permissions or the owner of `/yuneta/realms/agent/agent/` or
of `audit/`, or a file with the name `audit` in place of the directory:

```text
rotatory_open(): Cannot create '/yuneta/realms/agent/agent/audit' directory, Read-only file system
rotatory_open(): Cannot create '/yuneta/realms/agent/agent/audit/302-29_10_2026.log' file, Permission denied
```

Up to 7.29.3 an `audit` directory that could not be made gave the agent no
path, and the agent ran with no audit of commands; the only trace was
*"path EMPTY"* in syslog. The path itself is always built: its root
(`/yuneta`) and domain (`realms/agent/agent`) are in the fixed configuration
of the agent. If it could not be built, the agent logs one ERROR, *"Command
audit: the path of the audit file cannot be built, the agent does not run"*,
with the parts of the path (`root_dir`, `domain_dir`, `subdir`, `filename`),
and stops the same way.

**Once the agent runs, a failure of the audit does not stop it**: the
commands go on running, without their records.

- When the free space of the disk goes below `min_free_disk_percentage`
  (default 20 %), the audit stops writing and drops the records. Syslog has
  one line when it stops and one when it writes again, with the number of
  records dropped. The free space is checked again every few records.
- When the file of a new day (or of a size rotation) cannot be made or
  opened, syslog has one line, and each next record tries again. Syslog has
  one more line when the file is open again.

To run the agent with no audit, set `agent.use_audit_command_file` to
`false` **inside the `global` that `/yuneta/agent/yuneta_agent.json` already
has**, next to the keys there, and restart the agent. Do not add a second
`global`: a JSON object keeps only one of two keys with the same name, so the
other keys (`agent.agent_environment`, and the log of the agent with it) are
lost. With the keys of the packaged file:

```json
{
    "global": {
        "agent.agent_environment": {
            "daemon_log_handlers": {
                "to_file": {
                    "filename_mask": "W.log"
                }
            }
        },
        "controlcenter.jwt": "",
        "authz.master": true,
        "authz.authz_service": "agent",
        "agent.use_audit_command_file": false
    },

    "environment": {
        "node_owner": "none"
    }
}
```

(The rest of the file, such as `__json_config_variables__`, stays as it is.)

- A read-only command (`list-*`, `view-*`, `stats`, `nodes`, `help`, …) is
  recorded with the command, the date and the user only. A command with a
  `__reset__` value (`stats-yuno stats=__reset__`) is not read-only.
- Any other command also gets its `source` (the console purpose and each
  inter-yuno hop: role, yuno, service, user, host) and its parameters.
- A `content64` (a binary, a config) is never written: only its size and the
  sha256 of the decoded content.
- A secret (a parameter named like `password`, `pwd`, `secret`, `token`, `jwt`,
  `api_key`, `cookie`, `authorization`, `private_key`, …) is never written:
  its value is `<redacted>`, whatever else the name holds: `api_key_max`,
  `token_mode` and `token_endpoint` are redacted here, while the traces show
  them (a name ABOUT a credential; `is_secret_name_any()`; up to 7.25.21 the
  record wrote them in clear). So is whatever `mask_secrets_inline()` masks in
  a string, by the same rules and the same function (`mark_secrets_inline()`):
  a JWT or a JWE (two parts or more), the credential after `Bearer` or
  `Basic`, a `name: value` of a secret's name (`X-Auth-Token: x` is
  `X-Auth-Token: <redacted>`), every word of a password written with blanks;
  and, stricter than the SDK, the word after `Bearer ` whatever it is. Up to
  7.26.9 the record's JWT was three parts exactly, and a JWE was written in
  clear. Wherever they are: also in the `user`, the hops of
  `source`, the console purpose and the console name, which come from a peer
  (a field of those longer than 1024 bytes is written as its size and
  sha256 only). For example a hop whose user is a JWT and whose host has
  4095 bytes is written `"user":"<redacted>"`,
  `"host":"<4095 bytes, not scanned, sha256:HEX>"`.
- The command word is taken as the command parser takes it (any case,
  quotes, aliases): `WRITE-TTY` and `EV_WRITE_TTY` are `write-tty`.
- The scan of a command is one pass in linear time, and one record scans at
  most 128 MB: a longer string is written as its size and sha256 only.
- Every record is flushed to the file before the command runs.
- A console keystroke (`write-tty`) keeps only the fact: who, when, which
  console, how many writes and bytes. Nothing of what was typed, not even a
  hash. The writes of one user into one console make one burst of up to 60
  seconds: its first write is recorded at once, the rest in one record when
  the burst ends.

```json
{"command":"list-yunos","date":"2026-09-24T10:00:00.000000000+0200","user":"operator@example.com"}
{"command":"install-binary id=auth_bff content64='<33554432 bytes sha256:401b36b9…>'","date":"…","user":"yuneta","kw":{}}
{"command":"set-user-pwd username=bob password=<redacted>","date":"…","user":"yuneta","kw":{}}
{"command":"write-tty","date":"…","user":"operator@example.com","console":"console-1","writes":1,"bytes":1}
{"command":"write-tty","date":"…","user":"operator@example.com","console":"console-1","writes":212,"bytes":230,"until":"…"}
```

Up to 7.25.4 an `install-binary` of a 32 MB yuno wrote 134 MB (the base64 three
times); now it writes about 500 bytes.

A day that crosses `max_megas_audit_file` (500 MB) continues in `.OLD.1`,
`.OLD.2`, …; no piece of a day is removed. The attribute `audit_keep_days`
(default `7`, `0` = keep all) is the retention: at start, and when a new audit
file begins (inside the write of its first record, also while the disk is
below `min_free_disk_percentage`), the agent removes the
audit files older than that and logs one INFO line with their names. Only
files with the name shape of the audit mask are removed. A clock set back
across midnight empties no audit file. For example, to keep 30 days, put this in
`/yuneta/agent/yuneta_agent.json` and restart the agent:

```json
{
    "global": {
        "agent.audit_keep_days": 30
    }
}
```

An old `audit/` directory from 7.25.4 or earlier needs no manual cleaning: the
first start removes what is older than `audit_keep_days`. Details and the full
list of read-only commands in [the agent's audit files](#agent-audit-files).

## Listing the node's directories

The `dir-*` commands list a directory tree of the node: `dir-yuneta`,
`dir-realms`, `dir-repos`, `dir-store` (`subdirectory=` below `/yuneta`,
`/yuneta/realms`, `/yuneta/repos`, `/yuneta/store`, and `match=` a regular
expression of the names), `dir-logs` and `dir-local-data` (`id=` of a yuno).
The data is the list of full paths, sorted:

```bash
ycommand -c 'dir-store subdirectory=mqtt_broker match=.*\.json'
```

A tree that cannot be listed -- the directory does not exist, cannot be
opened or cannot be read, a `match` that is not a regular expression, no
memory for an entry -- answers `-1`, and says which directory; the cause is
in the agent's log:

```text
-1: yuneta_agent^agent: cannot list '/yuneta/store/nope', see the log
```

Up to 7.25.4 each of them answered an EMPTY list with result `0`, which reads
as "the directory is empty". A SUBdirectory the agent cannot open is skipped,
as before. (`tests/c/helpers/test_dir_listing`.)

## Deleting records: the `delete-*` commands

Five commands delete records of the agent's treedb, and **each one needs an
`id=`**. Without it the filter is empty and selects every record of the
topic, so the command refuses (*"delete-config needs an id: without one it
selects every configuration"*). Up to 7.29.3 they ran: `delete-binary force=1`
with no id deleted every binary, those in use too.

| Command | Deletes | Also |
|---|---|---|
| `delete-binary` | a binary record and its directory `/yuneta/repos/<tags>/<role>/<version>`; without `version=`, or with the primary's, **every version** of the id | `version=`, `force=1`, `record_only=1` |
| `delete-config` | a configuration; without `version=`, or with the primary's, **every version** of the id | `version=`, `force=1` |
| `delete-yuno` | a release of a yuno, or the yuno; the release of the primary, when there are others, only with `whole=1` | `yuno_release=` or `whole=1` (one of them, not both), `force=1` |
| `delete-realm` | a realm with no yunos | `force=1` |
| `delete-public-service` | a public service | `force=1` |

```bash
ycommand -c 'delete-binary id=emailsender version=7.29.2'
ycommand -c 'delete-config id=emailsender version=3'
ycommand -c 'delete-yuno id=7601 whole=1'
ycommand -c 'delete-realm id=stress.example.com'
ycommand -c 'list-public-services'            # the id of a public service is a number
ycommand -c 'delete-public-service id=3'
ycommand -c 'delete-config version=3'         # refused: no id
```

`delete-binary` and `delete-config` without `version=` delete the record of
**every** version of the id, not only the primary's: the delete of the primary
takes the whole key, and so does `version=` with the primary's own version.
Then every version taken is checked by the guards, `delete-binary` removes the
directory of each, and the answer counts them all. Up to 7.29.3 the directories
of the versions that were not the primary stayed in `/yuneta/repos`. To remove
one version and keep the others, name a version that is not the primary
(`list-binaries-instances`, `list-configs-instances`).

The "in use" guard reads the `yunos` of **every** version taken. Each version
has its own list: a yuno is linked to the version it was created on
(`create-yuno … role_version=1.0`). The lists can overlap: a version installed
later starts with a copy of the list of the primary at its install (a new
version of a record takes the links of the primary), so it can name yunos that
use another version. A yuno that two lists name counts once. Up to 7.29.3 only
the list of the record given was read, so a yuno that used an older version
did not stop the delete of the primary, which took that version too:

```bash
ycommand -c 'delete-binary id=my_role'
# my_role 1.0 is used by a yuno, 2.0 (the primary) by none: refused
# "…: cannot delete binary 'my_role': the delete of its primary takes its 2 versions, and 1 yunos use it (use force=1 to override)"
```

The delete of ONE version (not the primary) counts only the yunos that use
THAT version (`role_version` for a binary, `name_version` for a
configuration). So a version whose list it took from the primary, and that no
yuno uses, is deleted:

```bash
# my_role 1.0 is used by yuno y9; 2.0 was installed after y9 was created, so
# its list names y9 too; 3.0 is the primary
ycommand -c 'delete-binary id=my_role version=2.0'   # deleted: y9 uses 1.0
ycommand -c 'delete-binary id=my_role version=1.0'   # refused: y9 uses it
```

`delete-yuno yuno_release=` with the release of the **primary** is not one
release: the delete of the primary takes every release of the yuno. When the
yuno has other releases, the command refuses it and asks for `whole=1`. Up to
7.29.3 it deleted every release. `yuno_release=` and `whole=1` together are
refused, whatever the release: the command does not guess which delete was
meant. Up to 7.29.3 `whole=1` with a release other than the primary's deleted
that release only:

```bash
ycommand -c 'delete-yuno id=7601 yuno_release=1.1.0-2'   # a release behind the primary: deleted
ycommand -c 'delete-yuno id=7601 yuno_release=1.2.0-2'   # the primary's, with others: refused
ycommand -c 'delete-yuno id=7601 whole=1'                 # the yuno and every release
ycommand -c 'delete-yuno id=7601 yuno_release=1.1.0-2 whole=1'   # refused: one or the other
```

A delete that takes the whole yuno also removes the attributes the yuno saved
of its own. That is `delete-yuno whole=1`, and `delete-yuno yuno_release=`
with the yuno's only release (since 7.30.24; up to 7.30.23 only `whole=1`
removed them). The attributes are the file of the persistent attrs of its
`C_YUNO`, `<domain_dir>/data/C_YUNO-<yuno_name>-persistent-attrs.json` (its trace
levels, and what was written to its `__yuno__` with persistence). An INFO
*"Saved attrs of a yuno deleted whole removed"* names the path. The rest of
its data stays. Up to 7.30.22 the file survived the delete, and a yuno created
again with the same id started with the old trace on and `traced` false:

```bash
ycommand -c 'trace-on-yuno id=7601 level=machine'
ycommand -c 'kill-yuno id=7601'
ycommand -c 'delete-yuno id=7601 whole=1'
# removed: /yuneta/realms/<owner>/<realm>/<role>^7601/data/C_YUNO-<name>-persistent-attrs.json
ycommand -c 'create-yuno id=7601 realm_id=<realm> yuno_role=<role> yuno_name=<name>'
ycommand -c 'run-yuno id=7601'      # starts with no trace
```

The same with the yuno's only release (`list-yunos id=7601` shows one row):

```bash
ycommand -c 'kill-yuno id=7601'
ycommand -c 'delete-yuno id=7601 yuno_release=1.2.0-2'   # its only release: the yuno is gone
# removed: /yuneta/realms/<owner>/<realm>/<role>^7601/data/C_YUNO-<name>-persistent-attrs.json
```

The agent knows the `domain_dir` it gives a yuno, not one fixed in the yuno's
binary: `logcenter` and `webstats` save in `realms/agent/<role>` whatever
their id, so their file is not removed. Reset their trace with
`trace-off-yuno` while the yuno runs, before `kill-yuno`. `delete-yuno`
refuses a running yuno, and `trace-off-yuno` of a stopped yuno changes
nothing (it answers -1, *"not connected to the agent"*):

```bash
ycommand -c 'trace-off-yuno id=7602'     # 0: the yuno runs, its saved trace is reset
ycommand -c 'kill-yuno id=7602'
ycommand -c 'delete-yuno id=7602 whole=1'
```

The `data` of a `delete-binary` answer has one item per version taken, with
its files and whether the agent removed them:

```json
[{"id": "emailsender", "version": "7.29.2", "files": "/yuneta/repos/yuneta/utils/emailsender/7.29.2", "removed": true}]
```

A directory the agent cannot remove has `"removed": false`, and so has one it
cannot look at (it cannot tell whether the directory is there, for example
without permission on a parent directory). A directory that is not there is
`"removed": true`: its path does not exist, or a part of it is not a
directory (`ENOTDIR`: then no directory can be at that path). The error is in the log, the record is deleted all the same, and the answer is an error
(`-1`) that counts the records deleted and names the directories to remove by
hand: *"…: 1 binaries deleted, 1 of them with files the agent could not remove
(see the log): remove them by hand: ["/yuneta/repos/yuneta/utils/emailsender/7.29.2"]"*.
Up to 7.29.3 a failed removal was not looked at, and the answer said only
*"1 binaries deleted"*; a directory the agent could not look at was not
removed and was not named.

A delete that the treedb refuses stops there, and the answer names it, with
its cause and what was deleted before it, which is in the `data`:
*"…: binary 'emailsender' version '7.29.2' NOT deleted: <cause>; before it,
0 binaries deleted"*. `delete-config`, `delete-realm` and
`delete-public-service` answer the same way, and their `data` lists only
what was deleted. Up to 7.29.3 the answer was the cause alone, the `data` of
`delete-realm` and `delete-public-service` named the record that failed as
if deleted, and `delete-config` lost its `data` (a leak in the agent).

The `<cause>` is what the delete logged as an ERROR. When it logged none,
the cause is *"(see the log)"*: the treedb refuses a record that still has
links with a WARNING (*"has down links"*, *"has up links"*), and the
WARNING is in the log. For example
*"…: realm 'r2' NOT deleted: (see the log); before it, 0 realms deleted"*.
Up to 7.29.3 that cause was empty, or the cause of an older, unrelated
failure. `delete-yuno` gives the same cause, and so do the creates and
updates that answer one: `create-realm`, `update-realm`,
`update-public-service`, `install-binary`, `update-binary`,
`create-config`, `update-config` and `create-yuno` (for example
*"…: Cannot create realm: (see the log)"*). Up to 7.29.3 those named an
empty cause, or the cause of an older failure, when the treedb refused
with a WARNING.

`delete-binary record_only=1` deletes the record and leaves its files, and
says where they are, in the comment and in the `data` (`"removed": false`):
*"… remove its files by hand: ["/yuneta/repos/yuneta/utils/emailsender/7.29.2"]"*. The guards of each
command (in use, tagged by a snap, running) are in
[the yuno lifecycle](../../../yunos/c/yuno_agent/YUNO_LIFECYCLE.md).

Other commands act on EVERY record when they have no filter, by design:
`kill-yuno`, `run-yuno`, `command-yuno` and the rest of the list in
[bulk when unfiltered](#agent-bulk-commands).

## A `content64` that is not what it must be

A `create-config` or `update-config` whose `content64` decodes to nothing,
or to text that is not json (its `#^^` comments removed), answers `-1`
*"…: Bad json in content64"*. The log gets ONE WARNING with the position of
the error and the length, and nothing of the content: a config holds keys
under names that no mask knows.

```text
create-config id=x content64=aGVsbG8=
-1 "yuneta_agent^node: Bad json in content64"
WARNING {"msgset": "Parameter", "msg": "content64 of a config is not json, refused",
         "command": "create-config", "error": "'[' or '{' expected", "line": 1, "column": 5, "len": 5}
```

Up to 7.30.26 these were ERRORs with a stack (*"len is ZERO"*, *"json_loads()
FAILED"*), and a dump of the whole config.

A `write-tty` to a `name` that is no open console, or to a service that is
not a console (`__yuno__`, `agent`), logs one WARNING (*"console not found"*,
*"not a console"*) and answers `EV_TTY_CLOSE` with that `name`. One without
`content64` logs one WARNING (*"content64 required"*) and is dropped. Up to
7.30.26 `name=__yuno__` gave *"Event NOT DEFINED in state"* with a stack,
`name=agent` made the agent send the write to itself, and a missing
`content64` was an ERROR.

## A yuno that runs but is not connected

`command-yuno`, `stats-yuno` and `authzs-yuno` go to every yuno that matches
and is running, through the channel the yuno opened to the agent. A yuno that
runs but has no channel -- starting, not yet connected, or going -- used to
get nothing and answer nothing, so the requester waited for ever (a control
center running a scenario, its step's whole deadline). Since 7.25.16 the
command is answered at once when it reached none of them,

```text
yuneta_agent^node-a: set-interval: yuno gen1 runs but is not connected to the agent (starting or stopping): nothing sent
```

and when it reached only some, the others are named in a warning of the
agent's log (*"yuno running but not connected to the agent, nothing sent to
it"*): the ones that got it answer on their own.

## Tracing a yuno (`trace-on-yuno`, `trace-off-yuno`)

`trace-on-yuno` sets GLOBAL trace levels in the yunos that match, and marks
them `traced` in `list-yunos`; `trace-off-yuno` resets every global level of
the yunos that match and marks them not traced. The levels are those of
`set-global-trace`, the same in every yuno (see
[Debugging](../../../yunos/c/yuno_agent/DEBUGGING.md)), and the agent sends
one `set-global-trace level=<level> set=1` to the yuno per level asked.

```bash
# trace the FSM of yuno 3 and the kw of each of its events
ycommand -c 'trace-on-yuno id=3 level=machine,ev_kw'

# what the yuno traces now
ycommand -c 'command-yuno id=3 service=__yuno__ command=get-global-trace'

# done: every global level of yuno 3 off, `traced` false
ycommand -c 'trace-off-yuno id=3'
```

- `trace-on-yuno` with no `level`, or with a name that is not a global level,
  answers `-1` (*"what global trace level? …"*), sets nothing, and its `data`
  lists the global levels.
- A yuno already traced takes more levels: `trace-on-yuno id=3 level=commands`
  adds `commands` to `machine` and `ev_kw`.
- `trace-off-yuno` resets ALL the global levels of the yuno, also ones set by
  hand with `set-global-trace`.
- The yuno saves its global levels (`__global_trace__`), and `traced` is a
  persistent column: both survive a restart of the yuno, until `trace-off-yuno`.
- With no filter (`id`, `yuno_role`, `realm_id`, ...) they act on every yuno.
- They reach only a yuno connected to the agent. A yuno that is stopped,
  starting or stopping gets nothing: its saved levels and its `traced` stay as
  they are, and the answer names it. When no yuno is reached the answer is
  `-1`; when some are, it is `0`, with the rows of those reached. The agent
  logs a WARNING *"yuno not connected to the agent, its trace not changed"*.

```bash
ycommand -c 'kill-yuno id=3'
ycommand -c 'trace-off-yuno id=3'
# -1: "…: trace-off-yuno: yuno 3 not connected to the agent (stopped,
#      starting or stopping): its trace not changed"   (traced stays true)
ycommand -c 'run-yuno id=3'          # it starts with its saved levels again
ycommand -c 'trace-off-yuno id=3'    # 0: now they are reset, traced false
```

**BREAKING in 7.30.23:** up to 7.30.22 `trace-off-yuno` of a yuno not
connected answered `0` and set `traced` false, but nothing reached the yuno:
it kept its saved levels and had them again at its next start, with `traced`
false. `trace-on-yuno` set `traced` true for a yuno it did not reach.

**BREAKING in 7.30.22:** up to 7.30.21 `trace-on-yuno` took no level and set
the user levels of the yuno's root gobj (`C_YUNO`), which has none: nothing
was traced, only `traced` changed. It also skipped a yuno already traced
(*"Yuno not found or already tracing"*), and `trace-off-yuno` one not traced.

## Stats pushed to a watcher (`watch-yuno-stats`)

Since 7.25.13 a client can ask the agent to SEND it the stats of some yunos
every period, instead of asking `stats-yuno` again and again. It is what the
Monitor workspace of gui_agent uses when it talks to an agent directly.

```bash
# every 2 s: the state, the cpu and the stats of each yuno's service
watch-yuno-stats ids=gen1,7601,7501 period=2000
# `id:service` reads another service than the one named as the role
watch-yuno-stats ids=7501:db_events,2 period=5000
# end it (closing the connection ends it too)
watch-yuno-stats stop=1
```

These lines show the parameters. A watch is accepted only from a client that
also says, in the kw of the command, that it takes the pushed event
(`"__relays__": ["EV_YUNO_STATS"]`, see below); `stop=1` needs no marker. So
typed in `ycommand`, which cannot take `EV_YUNO_STATS`, a watch is refused, and
that is the point: ask `stats-yuno` there.

The answer says what is watched:

```text
yuneta_agent^node-a: watching 3 yunos every 2000 ms
```

and from then on the agent sends `EV_YUNO_STATS` along the route the
request came from -- the way the Terminal's PTY mirror sends `EV_TTY_DATA`
-- three kinds per yuno and period, the event's `data` being:

```json
{"yuno_id": "7601", "kind": "state", "missing": false,
 "yuno_running": true, "yuno_playing": true, "yuno_disabled": false}
{"yuno_id": "7601", "kind": "cpu", "service": "", "result": 0, "comment": "",
 "data": {"cpu": 15, "start_date": "...", "uptime": 86400, "mem_rss_kb": 5324,
          "mem_node_available_percent": 35, "disk_min_free_percent": 73, "...": "..."}}
{"yuno_id": "7601", "kind": "app", "service": "", "result": 0, "comment": "",
 "data": {"txMsgs": 3966, "rxMsgs": 3966, "rxMsgsec": 500, "...": "..."}}
```

A yuno that does not run gets only its `state`. One watch per requester --
its connection AND the client at the far end of the route, because every web
client of a control center shares that control center's connection: a new
call replaces the previous one. The client is told by the hop the agent can
trust: directly connected, the hop of the agent's own input channel; through
a control center, the hop stamped by the control center -- the channel the
client came in by there (`input_channel`, since 7.25.15: before, two tabs of
one browser were one requester and shared a watch) and its connection
(`cc_connection`). Hops deeper in the stack are written by the client as it
likes and are never read: up to 7.25.20 the agent took the LAST one, so a
client could add a hop naming another user's watch, and stop or replace it. One yuno may be named with several services
(`ids=7501,7501:db_events`), and an id that is not a yuno of this agent no
longer refuses the whole watch: it is watched, its `state` says `missing`, and
the answer names it (`data.missing`,
*"watching 3 yunos every 2000 ms, 1 of them not found here"*). A watch lives `watch_ttl` (60000 ms) unless it
is asked again: behind a control center the agent never sees a browser leave,
so the requester renews it (the answer carries the `ttl`; gui_agent renews
every third of it), and one not renewed goes with *"watch-yuno-stats not
renewed, expired"* in the agent's log. The readings come ONCE per yuno and
service however many requesters watch it, at the shortest period asked. Since
7.28.11 the agent asks each watched yuno to send them itself
([`publish-stats`](#c_yuno_publish_stats),
one per service, with `watch_ttl` as its ttl, renewed at half of it, and
stopped when nobody watches that service any more), and only reads them on
loopback (`stats-yuno` to its own yunos) for the first readings of a new watch
and for a yuno that refuses it -- one built before 7.28.11 (*"publish-stats
refused, the agent asks for these stats"*, INFO, once per process); `period` is never
under the attribute `watch_min_period` (1000 ms), `max_watches` (30)
bounds the requesters, and `max_watch_ids` (256) the names in one `ids`
(*"too many yuno ids: 300, max_watch_ids is 256"*). A `max_watch_ids` under 1
is a configuration error: every watch is refused (*"max_watch_ids is 0, it
must be 1 or more: no watch taken"*) and the agent logs it, once per bad
value. The answer goes first:
the first readings of a new or renewed watch follow it, to that requester
only (up to 7.25.20 they went out before it, and every renewal read every
watch of every requester again). It is not a subscription on purpose: a subscription
travels only from a client to a server, and the agent is the server of its
yunos.

A client that asks for a watch must know `EV_YUNO_STATS`, and SAY it: a C
client that receives an event it does not know drops the connection, or logs
*"Event NOT DEFINED in state"* on every one (`ycommand`, until the watch
expired). So a watch is accepted only if its kw carries `__relays__` with the
event -- written by a control center in between (see
[the control center](controlcenter.md): it removes the web client's own, and
writes it only when the web client's request named the event), or by a client
connected directly:

```json
{"ids": "7601,7501", "period": 2000, "__relays__": ["EV_YUNO_STATS"]}
```

Otherwise it is refused (*"this client does not say it takes EV_YUNO_STATS
(__relays__), ask stats-yuno instead"*, or through a control center *"the
client, or the control center in between, does not say it takes EV_YUNO_STATS
(__relays__), ask stats-yuno instead"*) and the requester polls.

## Redundancy

Every node also runs [`yuneta_agent22`](yuneta_agent22.md), a minimal second
agent kept alive as an escape hatch so a node is never left unmanageable.

## Deep dive

The agent is documented at length under **Operating Yuneta**:

- [Entry point (main + watcher)](../../../yunos/c/yuno_agent/ENTRY_POINT.md)
- [Yuno lifecycle](../../../yunos/c/yuno_agent/YUNO_LIFECYCLE.md)
- [Debugging a yuno](../../../yunos/c/yuno_agent/DEBUGGING.md)
- [Inter-process communication](../../../yunos/c/yuno_agent/IPC.md)
- [Realms (multi-tenancy)](../../../yunos/c/yuno_agent/REALMS.md)
- [Scaffolding new yunos](../../../yunos/c/yuno_agent/SCAFFOLDING.md)
- [Auth, permissions, TLS](../../../yunos/c/yuno_agent/YUNO_AUTH.md)
- [gobj framework crash course](../../../yunos/c/yuno_agent/GOBJ.md)
- [timeranger2 + treedb crash course](../../../yunos/c/yuno_agent/YUNO_TREEDB.md)
