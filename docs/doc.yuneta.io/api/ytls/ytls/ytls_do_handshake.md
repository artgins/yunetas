# `ytls_do_handshake()`

## Description
Performs a TLS handshake on a secure connection.

## Prototype
```c
PUBLIC int ytls_do_handshake(hytls ytls, hsskt sskt);
```

## Parameters
- `ytls` (`hytls`): The TLS context.
- `sskt` (`hsskt`): The secure socket.

## Return Value
- Returns 0 on success, or an error code on failure.

---
