# c_prot_http_cl test

Tests `C_PROT_HTTP_CL`, the HTTP client protocol.

`test1` (`main_test1.c` + `c_test1.c`) is about a GET with parameters. A GET
carries them in the query of its resource, and `C_PROT_HTTP_CL` builds that
query from `data` (a dict) or from `query` (a text) when the `Content-Type` is
`application/x-www-form-urlencoded`. A raw server (`C_PROT_RAW`) collects the
bytes of four GETs and checks each request line:

| The client sends | The request line |
|---|---|
| `data` `{"a": "1", "b": "xyz"}` | `GET /path?a=1&b=xyz HTTP/1.1` |
| `query` `"q=hello"` | `GET /search?q=hello HTTP/1.1` |
| neither | `GET /plain HTTP/1.1` (no `?`) |
| `data` `{"user": "user+1@x.com", "pass": "p a%41&b=c~-._*"}` | `GET /form?user=user%2B1%40x.com&pass=p+a%2541%26b%3Dc~-._%2A HTTP/1.1` |

Up to 7.26.9 the buffer of the new resource had no room for the `?`, and the
query lost its last character with no log (red: *"The request did not carry
its query whole"*, `GET /path?a=1&b=xy`).

Each key and each value of `data` is form-urlencoded: the unreserved bytes of
RFC 3986 (`A-Z a-z 0-9 - . _ ~`) as they are, a space as `+`, every other byte
as `%XX`. Up to 7.29.3 they went as they are, so a `+` was a space for the
server and a `&` began another field (red: *"The request did not carry its
query whole"*, `GET /form?user=user+1@x.com&pass=p a%41&b=c~-._* HTTP/1.1`).

A fifth request, a POST whose `data` is a json string (`"hello"`), must carry
the body `"hello"` (with its quotes). Up to 7.26.9 `json_dumps()` refused a
json that is not a dict or a list, and the client crashed on the `NULL` it
answered (red: the test dies with SIGSEGV).

```c
json_t *kw = json_pack("{s:s, s:s, s:{s:s}, s:{s:s, s:s}}",
    "method", "GET",
    "resource", "/path",
    "headers",
        "Content-Type", "application/x-www-form-urlencoded",
    "data",
        "a", "1",
        "b", "xyz"
);
gobj_send_event(gobj_http_cl, EV_SEND_MESSAGE, kw, gobj);   // GET /path?a=1&b=xyz
```

`test2` (`main_test2.c` + `c_test2.c`) is about forms and the responses the
client refuses. The raw server answers with bytes the test writes. Twelve
steps, a client each:

| Step | The client | Expected |
|---|---|---|
| 1 | POSTs `data` `{"empty": "", "n": 5, "r": 1.5, "t": true, "nul": "a\0b", "big": <6 MB>}` | the body `empty=&n=5&r=1.5&t=true&nul=a%00b&big=xx...`, its `Content-Length` exact |
| 1 | POSTs `data` `{}` | `Content-Length: 0`, no error |
| 1 | POSTs `data` `{"bad": null}` | not sent, the action answers `-1`, ONE error |
| 2 | gets a response whose head is 70 KB | `EV_ON_MESSAGE` with `headers_too_large` 65536, no body and its repeated headers joined, ONE warning, the connection dropped |
| 3 | gets a response with a CR alone in a header | `EV_ON_MESSAGE` with `protocol_error`, its repeated headers joined, ONE warning, the connection dropped, the request sent once |
| 4 | sends to a socket that listens and never accepts, `max_tx_queue` 3, `timeout_tx_stall` 500: a POST of 8 MB that never leaves, two short ones, and one more each second | the 6th refused (`-1`): the 4th found 2 waiting, the 5th found 3 and started the wait, the 6th found it over. ONE warning, the connection dropped; a request after the drop is no second warning of the server |
| 5 | sends 100 requests in one turn to a server that reads | all answered, nothing logged |
| 6 | gets a whole response and then garbage | ONE `EV_ON_MESSAGE`, the good one, ONE warning, the connection dropped |
| 7 | gets garbage alone | no `EV_ON_MESSAGE`, ONE warning, the connection dropped |
| 8 | gets a chunked response whose trailers repeat `X-A` and `Content-Type` | strings in `headers`, the body read as json |
| 9 | `raw_body_data`, gets two responses on one connection | `CONTENT-LENGTH` is `2` and then `4`, never `2, 4` |
| 10 | `raw_body_data`, gets a head of 70 KB | no `EV_ON_MESSAGE`, ONE warning, the connection dropped |
| 11 | sends a POST of 8 MB, gets three responses in one read, and drops at the first with the write in flight | the other two are not published |
| 12 | sends four POSTs of 4 MB to a server that reads 6 MB/s, `max_tx_queue` 1, `timeout_tx_stall` 500, and one more each second | none refused, all read, nothing logged |

Up to 7.29.3 an empty value was left out with an ERROR (an empty password
could not be sent), a NUL cut its value, a value of 6 MB was refused (the form
was sized for three times its bytes), a form with no fields logged *"len is
ZERO"* with a stack, a head had no limit, a broken response answered nothing,
and the requests waiting for a server that does not read had no limit.
With a limit of the queue that counted the requests of the turn, step 5 is red
(*"A request to a server that reads was refused"* at the 66th).

```c
json_t *kw = json_pack("{s:s, s:s, s:{s:s}, s:{s:s, s:i}}",
    "method", "POST",
    "resource", "/token",
    "headers",
        "Content-Type", "application/x-www-form-urlencoded",
    "data",
        "password", "",     // goes as password=
        "n", 5              // goes as n=5
);
gobj_send_event(gobj_http_cl, EV_SEND_MESSAGE, kw, gobj);
```

## Run

```bash
ctest -R '^c_prot_http_cl/' --output-on-failure --test-dir build
```
