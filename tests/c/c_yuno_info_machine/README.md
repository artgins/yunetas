# c_yuno_info_machine test

Tests the answer of the `info-machine` command of `C_YUNO`
(`cmd_info_machine()` in `c_yuno.c`).

- `test1`: `C_TEST_INFO_MACHINE` asks its yuno for `info-machine` two times.
  - With the hostname of the machine, the answer is `0`. Its `ifs` has one
    entry for each interface name, in the order that `getifaddrs()` first
    gives that name. Each entry has the IPv4 and IPv6 addresses that
    `getifaddrs()` gives for it.
  - With a hostname that is not UTF-8, the answer is `-1` and the yuno logs
    one ERROR, *"info-machine: cannot build the answer"*. The test links with
    `-Wl,--wrap=get_hostname`, and its `__wrap_get_hostname()` returns that
    hostname for one call only.

  Up to 7.31.29 the second answer was `0` with no data and no log.
  `json_pack()` frees its `o` values when it fails, and the command then
  read the os info that was freed.

## Run

```bash
ctest -R c_yuno_info_machine --output-on-failure --test-dir build
```
