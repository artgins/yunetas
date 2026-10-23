# c_yuno_cpu_core test

Tests the `cpu_core` of a yuno (`boost_process_performance()` in `c_yuno.c`).

- `test1`: `cpu_core` 1 and `sched_priority` 20, run as a plain user. The
  process must run on cpu 1 alone (`sched_getaffinity`), and the only log
  line of the pin must be the INFO *"Yuno pinned to a cpu"*: no real-time
  priority and no memory lock without `CAP_SYS_NICE` / `CAP_IPC_LOCK`, and
  no error. Up to 7.27.9 the capabilities were looked for in the bounding set,
  and the yuno logged *"sched_setscheduler() FAILED"* at each start.
- `test2`: `cpu_core` 4096, a cpu the machine does not have. It must be the
  ERROR *"cpu_core is not a cpu of this machine: the yuno is NOT pinned"*,
  and the process must keep every cpu it had. Up to 7.27.9 it was silent.

`test1` needs a machine with at least two cpus. Both are run as a plain user:
under root (or with the capabilities) `test1` would get `SCHED_RR`, and its
INFO would say so.

## Run

```bash
ctest -R '^c_yuno_cpu_core/' --output-on-failure --test-dir build
```
