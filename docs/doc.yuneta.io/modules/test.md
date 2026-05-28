(module-test)=
# Test

**Kconfig:** `CONFIG_MODULE_TEST` — **GClasses:** `C_PEPON`, `C_TESTON`

Paired test server/client for functional testing and traffic generation.

## C_PEPON / C_TESTON

Paired test components: `C_PEPON` is a test server that responds to
requests, `C_TESTON` is a test client that generates traffic
(via `EV_START_TRAFFIC`). Used for functional testing and validation
of Yuneta transport and protocol stacks.
