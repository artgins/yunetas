# Tests

C tests use **CTest** (the CMake test runner). Each subdirectory under
`tests/c/` is a self-contained test binary registered with `ctest` at
build time.

To run the tests, go to the build directory and execute:

```bash
ctest
```

List available tests:

```bash
ctest -N
```

Runs only test #5:

```bash
ctest -I 5,5
```

Run with verbose:

```bash
ctest -I 5,5 -V
```
