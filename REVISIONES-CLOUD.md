# Cloud review of main

Reviewed up to `ee940e48a` (2026-10-02, release 7.25.22, packaging revision
-3). What is resolved is removed from this file; this version is the fixing
session's answer, item by item, on `139e5441d`. Every fix has a test that
fails on the code before it, except where it says otherwise.

Last check, on `139e5441d`: clean build (`yunetas clean/build --sdk-only`) with
no warning, suite 287/287 under `ulimit -Sn 1024` on the dev machine. The
wattyzer run (the second machine of the release rule) is not done yet.

## Resolved

### High

| # | Item | Commit | Test |
|---|------|--------|------|
| 1 | dbsimple: the yuno's user of a root yuno is the owner of the lowest directory of the chain closed from `/` DOWN (`openat(O_PATH\|O_NOFOLLOW)`), each owned by root or by that user; the chain ends at the first directory others can write, or at a symlink. What is below the open directory names nobody, so the swap between the walk and the open changes nothing | `da85b7a9b` | `secret_attrs` (run as root by `__wrap_geteuid`, directories told by `__wrap_fstat`; red with a temporary `__wrap_stat` over the old upward walk: the planted file loaded) |

### Medium

| # | Item | Commit | Test |
|---|------|--------|------|
| 2 | `ATTR_WRITABLE` is `SDF_WR`: `write-attr` refuses an `SDF_PERSIST` attr without it (*"attr not writable"*). None of the listed attrs became `SDF_WR`: each is the config's or its own command's. Every documented `write-attr` use (agent, controlcenter, yunovatios, wattyzer) is on `SDF_WR\|SDF_PERSIST` attrs already | `d9f119fb5` | `command_delete_user` (`max_sessions_per_user` refused and left 0; red: written), `secret_attrs` |
| 3 | rpm: `Requires(post)` / `Requires(postun)`: `(policycoreutils-python-utils if selinux-policy)`; the `chcon` fallback warns; `%postun` on erase removes the three fcontext rules | `7fc93b295` | The rich dependency built with rpmbuild (`rpmlib(RichDependencies)`; Rocky 9 has rpm 4.16) |
| 4 | C_UDP_S and C_UDP: the C_TCP liveness marker; the read is re-armed whenever the gobj lives and the event is idle | `460618ba9` | `c_udp_s_rx` 6 (the host answers -1 to every datagram; red: `"heard": "allowed"`) |
| 5 | C_WEBSOCKET / C_PROT_TCP4H: the default max is `gbmem_get_maximum_block() - 1` (what a gbuffer holds) and a configured max above it is capped; `istream_consume()` checks `gbuffer_append()` (an ERROR, and the frame is not completed cut) | `769af7b41` | `c_prot_tcp4h/test1` (after a reconnect, a header of exactly the max block: refused at once; red: a second payload timeout) |
| 6 | `--stop`: every `kill()` checked (EPERM said at once, not waited for, exit 1); the watcher notes SIGQUIT (`SA_RESTART`) and does not relaunch the child that ends after it; the watchers signalled first; the name scanned again before the SIGKILL. The units send SIGQUIT to `$MAINPID` before the agent | `603fde26d` | By hand, a scratch daemon whose child `abort()`s on SIGQUIT: the old code left the relaunched child alive; now nothing in 0.1 s. EPERM on a root process: exit 1 at once. A hung child: both killed at 10 s |
| 7 | C_TIMER0's callback answers 0, always: the yuno's loop ends through `yev_loop_reset_running()` (`set_yuno_must_die()`), never through a timer | `b7de0e312` | `test_c_timer0` (a child timer cleared and stopped during play; red: the yuno ended, the five ticks never came) |
| 8 | `restart-yuneta`, not root, with the units: `yshutdown --no-kill-agent "$@"` and `sudo -n systemctl restart yuneta_agent.service` (refused and said when sudo does not allow it); the old way only without units. logcenter's default runs `restart-yuneta -s` where it exists | `bfc86e031` | `sudo -n -l systemctl restart yuneta_agent.service` checked as `yuneta` on wattyzer (allowed: `NOPASSWD:ALL`) |

On 8, one claim does not hold: **`yshutdown` does not take agent22 down**. It
walks `/yuneta/realms` for `yuno.pid` files and `killall`s the exact name
`yuneta_agent`; agent22 writes `/yuneta/realms/agent/yuneta_agent22.pid` and
is named `yuneta_agent22`.

### Low

| Item | Commit | Test |
|------|--------|------|
| `restart_nodes()` after its 10 s: `spare_the_living` when some are left | `09712c5a2` | No ctest compiles `c_agent.c` |
| `kill-yuno` of a yuno found only by the scan: the answer says it is not waited for, and that a `run-yuno` before it is gone does not launch it | `09712c5a2` | -- |
| `ExecStopPost`: the unit's `ControlGroup` from systemd, read under the v2, hybrid or v1 mount; each kill `logger`ed; "no cgroup found" said | `4a8f90fe5` | The lookup run on wattyzer (agent22's unit) |
| C_AUTHZ with no users treedb: `add-jwk` / `remove-jwk` refused to everybody. Such a yuno never creates its JWT validations, so the key was useless -- and leaked (4.4 KB per key, found by the test) | `6fc06e184` | `command_delete_user` 11b (a second C_AUTHZ with no store; red: added, and the leak) |
| A persistent attr of the file that is not `SDF_PERSIST` (the old `jwks`) is said at load, once per attr, without its value | `6fc06e184` | `secret_attrs` (red: not said) |
| CHANGELOG 7.25.22 upgrade steps: `register-idp-user`'s -403 with a role; `kill-yuno` of an unconnected yuno answers at once | `139e5441d` | -- |
| ytls: `flush_clear_data()` answers -1 for a callback error, not the sum (both backends); OpenSSL `encrypt_data()` stops after 5 WANT tries, as mbedTLS; OpenSSL checks `gbuffer_create()` | `4f64eaaa8` | No red test (a thousand records in one read, or a WANT stall, is not staged) |
| `gbuffer_vprintf()`: `vsnprintf()` given the NUL's byte too | `0348cd415` | `test_gbuffer_guards` (10 chars in a gbuffer of 10; 20 grown to exactly 20; red: refused, and cut with an ERROR) |
| `set_disconnected()`: the `EV_DISCONNECTED` publish inside the liveness marker | `7314ad50d` | No host destroys there: no red test |
| fs_watcher: a directory watched again brings its subtree (each subdirectory not watched yet is watched and handed as created, parent first); the half-limit warning re-arms under 40%; an unparsable `max_queued_events` is said | `1d2083dc3` | `test_fs_watcher_overflow` (`a/sub` made while `a` was not watched; red: never announced, its file never heard) |
| C_TIMER0: `gobj_stop()` and an arm in one turn -- the cancel falls through to `EV_STOPPED` | `b7de0e312` | `test_c_timer0` |
| `close_range()` through `syscall(SYS_close_range)` | `4f9362a6e` | Compiles; the loop as before without the header or the kernel |
| The tcp4h memory assertion measures `mallinfo2()` (allocated, tracked or not) | `769af7b41` | `c_prot_tcp4h/test1` |
| The init script under units: `start`/`stop`/`restart` exit with the units' answer; `status` says an agent running outside its unit | `bfc86e031` | `status` of the new script run on wattyzer: *"yuneta_agent: running OUTSIDE its unit"* -- its main agent IS outside its unit now |

## Not done, and why

- **9a/9b** (a delete sequence in the master's signal) and **26** (project
  repos): in TODO.md and the projects' TODOs by decision, as before.
- **wattyzer's main agent runs outside its unit** (found by the new
  `status`): not touched by this session. `sudo /etc/init.d/yuneta_agent
  start` (7.25.22-3) or `sudo systemctl restart yuneta_agent` puts it back.
- **No red test** for: item 3 (packaging), item 6 (by hand), item 8
  (scripts), the ytls lows, `set_disconnected()`, `close_range()`.
