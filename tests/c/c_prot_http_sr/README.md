# c_prot_http_sr test

Tests `C_PROT_HTTP_SR`, the HTTP server protocol, against the peers it
refuses.

`test1` (`main_test1.c` + `c_test1.c`) plays a `C_IOGATE` whose channels are
`C_PROT_HTTP_SR` over `C_TCP` (`max_body_size` 1024, `max_tx_queue` 4,
`timeout_tx_stall` 500, `max_rx_held` 65536, `timeout_linger` 500). The test
gobj is the application behind it. Thirteen steps, each on a connection of its
own:

| Step | The peer | Expected |
|---|---|---|
| 1 | sends 600 requests in one write; the application drops the connection at the 3rd | the application gets 3 requests, and nothing is logged |
| 2 | sends a request whose head is 70 KB (over `max_header_size`, 64 KB), and keeps sending once it reads the answer | it reads `431 Request Header Fields Too Large` once; the connection is dropped `timeout_linger` after the answer, not before and not later; ONE warning |
| 3 | sends a request whose body is 5000 bytes | it reads `413 Content Too Large`, the same way; ONE warning |
| 4 | sends 600 requests and never reads; the application answers each with 100 KB | the requests of its first read reach the application, the rest is held; the connection is dropped after `timeout_tx_stall`; ONE warning |
| 5 | reads: sends 500 requests in one write, one more with each of their answers (they find those before them held, and the queue at any length), and 1000 more when it has all those answers | 2000 answers, in the order of the requests, nothing logged |
| 6 | sends 100 requests whose answers have 100 KB, and reads | 100 answers, in their order, nothing logged |
| 7 | as step 1, but the application answers the first two, so the disconnection comes after the write in flight | the application gets 3 requests, and nothing is logged |
| 8 | sends six requests and a head over the limit in one write; the application answers them later, all in one turn | it reads the six `200` and then the `431`, and the connection is dropped after `timeout_linger`; ONE warning |
| 9 | sends one request at a time, each when the one before reached the application, and never reads; answers of 1 MB | the transport never holds more than `max_tx_queue` answers; the connection is dropped; ONE warning |
| 10 | does not read and sends 128 KB of requests | past `max_rx_held` bytes held the connection is dropped; ONE warning |
| 11 | sends 30 requests whose answers have 1 MB, and reads 6 MB/s | it reads them all in more than `timeout_tx_stall`, nothing logged |
| 12 | fills the queue with 10 requests, reads the answers and goes silent; `timeout_inactivity` 3 | it is dropped: the timer that watched the queue is back on the inactivity |
| 13 | never reads; the application goes on answering after the drop, which waits for the write in flight | ONE warning for the peer, ONE for those answers |

Up to 7.29.3 the requests of step 1 after the drop went to the gobj already
disconnected (red: an ERROR with a stack, *"Event NOT DEFINED in state"*, for
each one); a head had no limit and `max_body_size` did not exist, so the
requests of steps 2 and 3 reached the application (red: *"A refused request
reached the application"*), and a body over half the max block was dropped
with no answer. With a limit of the queue that counted the answers of the read
being parsed, steps 5 and 6 are red (*"HTTP peer does not read its answers,
connection dropped"* for a peer that reads, *"A peer that reads did not get
all its answers"*), and with the refusal sent at once step 8 is red (*"The
refused request was not told why"*).

```c
/*  in a channel tree  */
{
    "name": "http-1",
    "gclass": "C_PROT_HTTP_SR",
    "kw": {
        "max_header_size": 16384,
        "max_body_size": 1048576,
        "max_tx_queue": 64,
        "timeout_tx_stall": 5000,
        "max_rx_held": 1048576
    }
}
```

## Run

```bash
ctest -R '^c_prot_http_sr/' --output-on-failure --test-dir build
```
