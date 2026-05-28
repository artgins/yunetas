(module-test)=
# Test

**Kconfig:** `CONFIG_MODULE_TEST` — **GClasses:** `C_PEPON`, `C_TESTON`

Paired test server/client for functional testing and traffic generation.

## C_PEPON / C_TESTON

Paired test components: `C_PEPON` is a test server that responds to
requests, `C_TESTON` is a test client that generates traffic
(send it `EV_START_TRAFFIC` to begin). Together they exercise the Yuneta
transport and protocol stacks for functional tests and benchmarks (see the
suites under `tests/c/` and `performance/c/`).

**Trace levels (both):** `messages`.
