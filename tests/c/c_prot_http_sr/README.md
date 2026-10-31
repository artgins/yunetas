# c_prot_http_sr test

Tests `C_PROT_HTTP_SR`, the HTTP server protocol, against the peers it
refuses.

`test1` (`main_test1.c` + `c_test1.c`) plays a `C_IOGATE` whose channels are
`C_PROT_HTTP_SR` over `C_TCP` (`max_body_size` 1024, `max_tx_queue` 4,
`timeout_linger` 500). The test gobj is the application behind it. Four
steps, each on a connection of its own:

| Step | The peer | Expected |
|---|---|---|
| 1 | sends 600 requests in one write; the application drops the connection at the 3rd | the application gets 3 requests, and nothing is logged |
| 2 | sends a request whose head is 70 KB (over `max_header_size`, 64 KB) | it reads `431 Request Header Fields Too Large`; the connection is dropped after `timeout_linger`; ONE warning |
| 3 | sends a request whose body is 5000 bytes | it reads `413 Content Too Large`, the same way; ONE warning |
| 4 | sends 600 requests and never reads; the application answers each with 100 KB | the connection is dropped at `max_tx_queue`; ONE warning |

Up to 7.29.3 the requests of step 1 after the drop went to the gobj already
disconnected (red: an ERROR with a stack, *"Event NOT DEFINED in state"*, for
each one); a head had no limit and `max_body_size` did not exist, so the
requests of steps 2 and 3 reached the application (red: *"A refused request
reached the application"*), and a body over half the max block was dropped
with no answer.

```c
/*  in a channel tree  */
{
    "name": "http-1",
    "gclass": "C_PROT_HTTP_SR",
    "kw": {
        "max_header_size": 16384,
        "max_body_size": 1048576
    }
}
```

## Run

```bash
ctest -R '^c_prot_http_sr/' --output-on-failure --test-dir build
```
