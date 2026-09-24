# command_binary_kw test

Regression test: a command or a stats request whose kw carries a binary field
(a `gbuffer`) keeps the gbuffer's reference count right.

`command_parser()` builds a new kw for the handler and copies into it the keys
of the caller's kw. Both kws are released with `KW_DECREF`, and `KW_DECREF`
releases the gbuffer of each one, so the copy must take a reference of its own
(`kw_incref`, not `json_incref`). Up to 7.25.4 it took none, and the gbuffer
was released once too often (*"BAD gbuf_decref()"*, or a gbuffer freed while
its owner still held it).

The test covers the three paths of `build_cmd_kw()` that copy the caller's kw:

1. a command without a parameter schema (`noschema`),
2. a command with a parameter schema (`withschema`),
3. a command with a parameter schema and `SDF_WILD_CMD` (`wild`),

and `build_stats()`, the default stats parser, which hands the kw to the stats
builder of the gobj and of each bottom gobj.

For each case the test holds two references of its own and gives the kw a
third one. The handler must see four (its copy has one), and after the command
the test must hold its two again:

```
ok   noschema: answers result=0                           (0)
ok   noschema: the handler gets the gbuffer               (1)
ok   noschema: the handler's copy holds a reference       (4)
ok   noschema: the test holds its two references          (2)
...
test_command_binary_kw: PASS
```

Run it:

```bash
ctest -R test_command_binary_kw --output-on-failure --test-dir build
```
