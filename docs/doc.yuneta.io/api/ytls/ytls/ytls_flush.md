# `ytls_flush()`

## Description
Flushes any pending TLS data.

## Prototype
```c
PUBLIC int ytls_flush(hytls ytls, hsskt sskt);
```

## Parameters
- `ytls` (`hytls`): The TLS context.
- `sskt` (`hsskt`): The secure socket.

## Return Value
- Returns 0 on success, or an error code on failure.
