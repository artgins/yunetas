# yev_loop test

Integration tests for the io_uring-based event loop: timers, TCP client/server echo, TLS client/server echo (both OpenSSL and mbedTLS), event delivery ordering and clean shutdown.

`yev_events/test_yevent_sq_full` fills the submission queue with NOPs that are not submitted. A timer starts and stops, and the loop stops, with the queue full: the loop flushes the queue and asks again. Then the kernel is made to take nothing (the ring fd is made invalid for the call, so `io_uring_enter()` answers `EBADF`): the submission is kept, the start answers `0`, and the timer fires at the next cycle; a timer stopped while its start is still kept gets the callback `STOPPED` with `-ECANCELED`; a stop of a second loop is kept and stops it.

## Run

```bash
ctest -R test_yev_loop --output-on-failure --test-dir build
```

Benchmarks live under `performance/c/perf_yev_ping_pong*`.
