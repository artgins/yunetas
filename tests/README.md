# README

C test is using Criterion. YET NO!

To run test, go to build directory and execute:

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
