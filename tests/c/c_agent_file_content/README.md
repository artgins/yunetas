# c_agent_file_content test

Tests how the agent reads a file whole for its `read-file` and `read-json`
commands, on the agent's own source (`yunos/c/yuno_agent/src/file_content.c`,
compiled into the test), against a maximum block of 1 MiB (`MEM_MAX_BLOCK` in
`main.c`).

Up to 7.28.9 the agent read at most the largest block a yuno may allocate and
cut the rest without saying so, and a `read()` that failed was taken for an
empty file.

## What it pins

1. a small file, and one of exactly the most that fits (its NUL is the last
   byte of the block), come back whole;
2. a file a byte longer is refused, with the reason, not cut;
3. an empty file is an empty string;
4. a file whose size the kernel does not tell (`/proc/self/status`, size 0)
   is read whole all the same, by growing the block;
5. a directory (the `read()` fails) and a file that does not exist are
   refused, with the reason.

## Run

```bash
ctest -R test_c_agent_file_content --output-on-failure --test-dir build
```
