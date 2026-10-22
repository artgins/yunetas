# Performance audit of the C SDK (2026-10)

A review of the C code of the SDK in search of speed, done in rounds. The rule
of the audit: **no change of functionality**. A change is applied on this
branch only when what the code does stays the same (same results, same files,
same bytes on the wire, same events, same logs, same errors); an idea that
would change behaviour, format or API is written down as a proposal, with its
expected gain, and is not applied.

Every round says what was looked at, what the profile showed, what was changed
(each change a commit of its own, so it can be taken or left alone), what it
gained, measured, and what was found and not done.

| Round | Scope | Document |
|-------|-------|----------|
| 1 | `gobj-c`: the allocator, the publication of events, subscriptions, `gobj_change_state()`, `gobj_match_gobj()`. TCP echo **+32%**, TLS echo +26%, BFF logins +12% | [round-1-gobj-core.md](round-1-gobj-core.md) |
| 2 | `timeranger2` and `tr_treedb` | (pending) |
| 3 | The network path: `yev_loop`, `C_TCP`, the protocols, TLS | (pending) |

## Method

- **Base**: the release 7.27.7 (`1d827e4`), built with the `.config` of a node
  (GCC 13.3, RelWithDebInfo, fully static, OpenSSL, no
  `CONFIG_DEBUG_TRACK_MEMORY`), `linux-ext-libs` 1.24, glibc 2.39 (Ubuntu
  24.04), on a 4-core cloud VM. The figures of this machine are not those of
  the reports in `performance/reports/` (another machine, another glibc, track
  memory on): only the A/B on the same machine is compared.
- **Where the time goes**: `valgrind --tool=callgrind` on the benchmarks of
  `performance/c/`, chiefly `perf_tcp_test4` (TCP echo through the whole gobj
  stack) and `perf_tr_treedb` (treedb writes). The instruction count is
  deterministic, so a change is also measured by how many instructions it
  takes away. It does not count the time inside the kernel (the syscalls).
- **How much it gains**: the binaries of the base and of the change run
  alternated, several rounds, the order turned every round
  (`performance/c/README.md`); mean, standard deviation and t (the change over
  its standard error, |t| < 3 is noise). This VM is noisy, so the instruction
  count is the steadier figure and the wall-clock A/B confirms its direction.
- **That nothing changed**: the whole ctest suite, before and after.
