# c_yuno_signal_read test

Tests that a yuno keeps reading its signals when the read of its signalfd
ends with no stop asked (`read_signals_again()` in `c_yuno.c`).

- `test1`: `C_TEST_SIGNAL` puts `/dev/null` on the number of the yuno's
  signalfd, as a wrong close of a stale fd would, and sends a SIGPIPE (in the
  mask, ignored). The pending read still holds the signalfd and gets it; the
  read armed again after it is on `/dev/null` and ends at once. The yuno must
  log the ERROR *"the read of the signals ended while the yuno runs: read
  again"*, read its signals on a new signalfd, and stop at the SIGQUIT sent
  at the next timeout (*"Exit to die"*). Up to 7.28.1 the read was not armed again
  and nothing was said: the yuno was deaf to every signal, and the test hung: its
  own auto-kill is a SIGALRM, read the same way (SIGQUIT and SIGALRM stay
  pending for ever). Seen in production as an agent whose unit's stop
  timed out into a SIGKILL.
  The driver's `mt_stop()` then asks the stop again, as a stop that reaches
  a daemon twice does (its watcher passes its SIGQUIT on, and whoever stops
  it sends one too): *"Exit to die"* must be logged once. Up to 7.29.3 it was
  logged a second time, after *"Pausing yuno"*.

## Run

```bash
ctest -R c_yuno_signal_read --output-on-failure --test-dir build
```
