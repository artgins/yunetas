# c_tcp test

Unit tests for the `C_TCP` client GClass. Exercises connect/disconnect cycles, I/O with a local echo server (`pepon`), and timeout handling.

## Run

```bash
ctest -R test_c_tcp --output-on-failure --test-dir build
```
