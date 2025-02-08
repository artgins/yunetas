# `ytls_init()`

## Description
Initializes a new TLS context.

## Prototype
```c
PUBLIC hytls ytls_init(
    hgobj gobj,
    json_t *jn_config,  // not owned
    BOOL server
);
```

## Parameters
- `gobj` (`hgobj`): The GObj that manages the TLS instance.
- `jn_config` (`json_t *`): JSON configuration for the TLS context. Not owned.
- `server` (`BOOL`): Indicates if the context is for a server (`TRUE`) or a client (`FALSE`).

## Return Value
- Returns a handle to the TLS context (`hytls`).
