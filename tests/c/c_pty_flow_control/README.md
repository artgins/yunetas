# c_pty_flow_control test

A `C_PTY` running `bash` (`C_TEST_PTY` drives it, `src/c_test_pty.c`) prints
`seq 1 200000` (1.3 MB), and its reading is paused and resumed with
`rx_paused`:

1. Paused INSIDE its `EV_TTY_DATA` (the read that just ended is not
   re-armed): no `EV_TTY_DATA` comes in 500 ms.
2. Resumed: the output comes again. After 50 more `EV_TTY_DATA`, on the next
   turn of the loop (a read in flight): paused, resumed while that read is
   still in flight (it must start no second read), paused again: at most
   that read comes in 500 ms.
3. Resumed: every line of `seq` comes, in order, none lost (a second read
   started over the one in flight loses or mixes them).
4. Paused inside its `EV_TTY_DATA` again, and stopped: its `EV_TTY_CLOSE` and
   `EV_STOPPED` come at once, and it is destroyed.
5. A `C_PTY` created with `rx_paused` `TRUE` and sent `echo HELLO`: no
   `EV_TTY_DATA` in 500 ms; once resumed, its output comes.

A failure is an ERROR *"TEST FAIL"*, which the strict FIFO of expected logs
(`main.c`) does not hold. Up to 7.30.26 `C_PTY` had no `rx_paused` (the
writes of the attribute were *"GClass Attribute NOT FOUND"*, and the output
never stopped), and a `C_PTY` that is not volatile published `EV_STOPPED`
without declaring it.

## Run

```bash
ctest -R test_c_pty_flow_control --output-on-failure --test-dir build
```
