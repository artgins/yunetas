# c_resource2_names test

Tests the resource names a persistent `C_RESOURCE2` takes.

A resource of a persistent store is the file `<name>.json` of
`store/resources/<service>/<yuno role^name>/<database>/`, and its name is
usually a peer's: an MQTT gate keeps its clients by client id. A name is
taken only when it is ONE file name: no `/`, no leading `.`, no `..`, and
`<name>.json` within `NAME_MAX`.

## What it pins

1. `gobj_create_resource()` of `a/b`, `/`, `..`, `.hidden` and a name of
   `NAME_MAX` bytes answers NULL, with ONE warning each (*"Resource name
   refused: not one file name (a '/', a leading '.', '..', too long)"*), and
   none of them is kept in memory.
2. `client-1` is created and saved: `client-1.json` is the only file of the
   store.

Up to 7.30.16 each of the five was made in memory, each save of it logged an
ERROR (*"Invalid resource name: '/' and '..' not allowed"*), and `.hidden` was
taken and wrote `.hidden.json` (red: 5 names taken, the store held
`client-1.json .hidden.json`).

## Run

```bash
ctest -R test_c_resource2_names --output-on-failure --test-dir build
```
