# c_prot_http_cl test

Tests `C_PROT_HTTP_CL`, the HTTP client protocol.

`test1` (`main_test1.c` + `c_test1.c`) is about a GET with parameters. A GET
carries them in the query of its resource, and `C_PROT_HTTP_CL` builds that
query from `data` (a dict) or from `query` (a text) when the `Content-Type` is
`application/x-www-form-urlencoded`. A raw server (`C_PROT_RAW`) collects the
bytes of three GETs and checks each request line:

| The client sends | The request line |
|---|---|
| `data` `{"a": "1", "b": "xyz"}` | `GET /path?a=1&b=xyz HTTP/1.1` |
| `query` `"q=hello"` | `GET /search?q=hello HTTP/1.1` |
| neither | `GET /plain HTTP/1.1` (no `?`) |

Up to 7.26.9 the buffer of the new resource had no room for the `?`, and the
query lost its last character with no log (red: *"The request did not carry
its query whole"*, `GET /path?a=1&b=xy`).

A fourth request, a POST whose `data` is a json string (`"hello"`), must carry
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

## Run

```bash
ctest -R '^c_prot_http_cl/' --output-on-failure --test-dir build
```
