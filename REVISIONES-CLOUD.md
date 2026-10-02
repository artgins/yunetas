# Cloud review of main

Reviewed up to `ee940e48a` (2026-10-02, release 7.25.22, packaging revision
-3). What is resolved is removed from this file. Since `fc023c0c5` main
carries only packaging and docs (`5728d7bc3`, `ee940e48a`): no item below is
fixed yet, and no C file changed, so the last suite still holds (on
`fc023c0c5`: clean build with no warning, 287/287 as user `yuneta` under
`ulimit -Sn 1024`).

## Open

### High

**1. dbsimple: the trust of a root yuno can still be planted** (item 4,
`76a7db735`).

`trusted_dir_owner()` (`dbsimple.c:77-118`) returns the owner of the FIRST
directory upward that has no group/other write bits. Nothing checks the
directories above it, and it uses `stat()`, which follows a symlink.

Scenario for a root yuno, by a member of the `yuneta` group (the parent
`<role^id>` is 02775):

1. `mv data data.old`.
2. `mkdir data; chmod 0755 data` and write the file there, mode 0600.
3. The walk stops at that `data` and returns the attacker's uid: the file is
   trusted and loaded.
4. On the next save `give_back` (`dbsimple.c:752`) `fchown`s the new file,
   with the yuno's secrets, to the attacker.

A symlink in place of `data` does the same. The walk is by path and the file
is checked by fd, so there is also a TOCTOU window.

Preferred fix: start from a fixed anchor (the yuneta root, or the realm
directory the agent makes) and require EVERY directory of the chain to be
closed, with `lstat()`/`openat(O_NOFOLLOW)` along the path; or simply trust
only root's file and the euid's own. A test as root.

### Medium

**2. `write-attr` writes any `SDF_PERSIST` attr** (new).

`ATTR_WRITABLE` is `SDF_WR|SDF_PERSIST` and is tested with `&`
(`gobj.c:3608`), so an attr flagged `SDF_PERSIST` alone is writable at run
time through `write-attr`, which is `SDF_AUTHZ_X` only (nothing while
`enable_command_authz` is off). Among them:

- `C_AUTHZ.max_sessions_per_user` (`c_authz.c:339`): bypasses the
  always-on check of `set-max-sessions`.
- `C_IDP_KEYCLOAK` `kc_base_url` / `kc_admin_client_secret`: a changed base
  url sends the admin client secret to another server.
- `C_YUNO` `allowed_ips` / `denied_ips`, `C_TCP_S.crypto`,
  `C_MQTT_BROKER.enable_acl`, emailsender `url` / `password`.

Apart from `max_sessions_per_user`, each is as open through its own
command; together they are the reason to enable the gate.

Preferred fix: `write-attr` needs `SDF_WR` (an `SDF_PERSIST` attr without
it is set by config or by its own checked command); review which of the
attrs above should be `SDF_WR` at all.

**3. rpm: the SELinux label of the agents is not durable without
`semanage`** (`fc023c0c5`).

`%post` falls back to `chcon` when `semanage` is missing, and the spec does
not require `policycoreutils-python-utils` (`make-yuneta-agent-rpm.sh:1551`).
A `chcon` label is lost on an autorelabel or a `restorecon -R /yuneta`:
both agent units then fail at boot with 203/EXEC. The CHANGELOG's advice
("a binary moved in by hand takes the rule back with restorecon") is wrong
on such a node, and a `mv` keeps the source's label.

Preferred fix: `Requires(post): policycoreutils-python-utils` (EL9) so the
rule is always written; `%postun` on erase removes the three fcontext rules.

**4. C_UDP / C_UDP_S: a subscriber's -1 stops the reading for good**
(predates; the same defect `d64ac9ff3` fixed in C_TCP).

`c_udp.c:746` and `c_udp_s.c:1150` re-arm the read only if the publish
answered 0. One subscriber answering -1 (or "Event NOT DEFINED", which the
publish sums) leaves the server deaf, with nothing said.

Preferred fix: the same as C_TCP: re-arm when the event is still idle,
whatever the publish answered.

**5. A frame of exactly the DEFAULT max block still never completes**
(item 7, `ff7ffd484`).

With `max_payload_size=0` (websocket) or `max_pkt_size=0` (tcp4h) the max
is `gbmem_get_maximum_block()` (`__max_block__ - TRACK_MEM`). The buffer of
such a frame grows to `frame_length+1`, which `_mem_realloc` refuses
(`gbmem.c:612`); the append truncates, `istream_consume` ignores it
(`istream.c:187/191`), and the frame waits for its timeout. The tests use
explicit maxima only.

Preferred fix: with the default, refuse a frame `>=` the max block (or cap
the default one byte lower); `istream_consume` checks what
`gbuffer_append` answers.

**6. `--stop`** (`5337b15bb`).

- `kill()` is never checked: on EPERM (an agent of another user) it waits
  10 s, prints a false "killed (SIGKILL)" and exits 0.
- An agent that crashes during its orderly stop is relaunched by its
  watcher after 2 s; at 10 s the watcher is SIGKILLed and the relaunched
  agent is left alive, an orphan.

Preferred fix: check and say each `kill()`; SIGQUIT the watcher first (it
then does not relaunch) and collect the agent's pid again before the
SIGKILL.

**7. Controlcenter `mt_stop` can stop the yuno's loop** (predates).

It calls `clear_timeout0()` then `gobj_stop()` of `rates_timer`. The
cancel's completion arrives with the gobj stopped, C_TIMER0's callback
answers -1 (`c_timer0.c:224/246`), and `yev_loop` sets `running=false`
(`yev_loop.c:1928`). Harmless at the yuno's shutdown; if the service is
ever stopped alone (a stop of the service, a restart of the tree), the
whole yuno stops. Not reproduced.

Preferred fix: C_TIMER0's callback answers -1 only for the yuno's own
timers (or never); a stopped child timer is not a reason to end the loop.

**8. Restarts that do not go through root start the agent OUTSIDE its unit**
(new, `5728d7bc3` makes it visible).

With the units, an agent started by hand with `--start` runs where systemd
does not see it, and the next boot does not start it (CLAUDE.md, *Deploy
conventions*). Two paths still do exactly that:

- `/yuneta/bin/restart-yuneta` (`make-yuneta-agent-deb.sh:1545-1578`, same
  in the rpm), run by the certbot deploy hook as its fallback: as a non-root
  user it does `yshutdown` and then `yuneta_agent --start`.
- logcenter's `restart_yuneta_command` default (`c_logcenter.c:127`):
  `/yuneta/bin/yshutdown -s; sleep 1; /yuneta/agent/yuneta_agent --start ...`,
  run by the logcenter (user `yuneta`) after a queue alarm.

After either, the agent runs outside `yuneta_agent.service` (and `yshutdown`
also took agent22 down, which `restart-yuneta` does not start again). The
init script now puts it back in its unit at its next `start`, but nothing
runs that until a reboot or an operator.

Preferred fix: both ask systemd (`sudo -n systemctl restart
yuneta_agent.service` with a sudoers rule for the `yuneta` user limited to
that command, or a unit the agent can trigger), and fall back to `--start`
only on a node without the units; `yshutdown` must not take agent22 down
with it.

### Low

- **The init script** (`5728d7bc3`): `stop` and `start` answer 0 whatever
  the units answered (only logged), so `service yuneta_agent start` reports
  success when the main agent's unit failed; `status` looks only at the
  units, so an agent running outside its unit reads "not running".
- **`restart_nodes()` after its 10 s** relaunches with
  `spare_the_living=FALSE`, so a yuno still alive (D state) gets a second
  instance: the original failure, now logged. Passing TRUE skips it with the
  existing warning.
- **`kill-yuno` of a yuno found only by the scan answers at once**; a
  `run-yuno` right after it can find it still exiting and skip it ("not
  launched again"), and the yuno ends up down. Answer when it has gone, or
  say it in the answer.
- **ExecStopPost** reads a cgroup v2 path written by hand
  (`/sys/fs/cgroup/system.slice/%n/cgroup.procs`): a no-op, unsaid, on a v1
  or hybrid host; and it kills with nothing in the journal. Read the path
  from `/proc/self/cgroup`, and `logger` the pid it kills.
- **C_AUTHZ with no treedb** (`d70e02d8b`): any valid JWT may run
  `add-jwk` / `remove-jwk` there (pre-7.25.22 behaviour, in memory only).
- **`jwks` persisted at run time is dropped with nothing said**, and stays
  in the file for ever (`json_object_update_missing`). A node whose keys came
  only from `add-jwk` loses its JWT logins at the first restart; only the
  upgrade note warns. Say it once at load.
- **CHANGELOG upgrade steps** do not name the -403 of `register-idp-user`
  with a role, nor the immediate answer of `kill-yuno` for a yuno not
  connected.
- **ytls:** `flush_clear_data` sums the subscribers' answers into the same
  number space as -2222 and the "< -1000 TLS error" band (more than 1000
  records answered -1 in one read become a TLS error; a sum of exactly
  -2222 hangs the connection). OpenSSL `encrypt_data` loops on
  WANT_READ/WRITE with no bound (mbedTLS stops at 5); `flush_clear_data`
  (OpenSSL) does not check `gbuffer_create`.
- **`gbuffer_vprintf`** grows by `written`, with no room for the NUL
  (`gbuffer.c:533`): an exact fit writes one character less and logs "NOT
  ENOUGH SPACE". Pass `written+1`.
- **`set_disconnected()`** publishes `EV_DISCONNECTED` and then touches the
  gobj (`gobj_reset_volatil_attrs`): a host that destroys it on that event
  would be a use-after-free (no such host in the tree).
- **fs_watcher:** `watch_unwatched_again()` watches the directory alone,
  not its subtree, so under `FS_FLAG_RECURSIVE_PATHS` a subdirectory made
  during the outage is never watched; the half-limit warning has no
  hysteresis; `kernel_queue_bound()` takes 16384 silently when `fscanf`
  fails.
- **C_TIMER0:** `gobj_stop()` then `set_timeout0()` in one turn leaves the
  re-arm flag set on a stopped gobj, and no `EV_STOPPED` is published.
- **`close_range()`** needs glibc 2.34 at compile time: a source build on an
  older glibc does not compile.
- **The tcp4h memory assertion** of the new test reads
  `get_cur_system_memory()`, which is 0 without `CONFIG_DEBUG_TRACK_MEMORY`:
  vacuous on the nodes.

Items 9a/9b (a delete sequence in the master's signal) and 26 (project
repos) are in TODO.md and the projects' TODOs by decision; not repeated
here.

## Order I would fix them in

1. The dbsimple chain (1), with a root test.
2. `write-attr` and `SDF_PERSIST` (2); the rpm SELinux requirement (3).
3. The UDP read stall (4) and the default-max frame (5).
4. `--stop` (6), C_TIMER0's -1 (7) and the restarts outside the unit (8).
5. The low items as their area is touched.
