# `ytls_get_last_error()`

## Description
Retrieves the last TLS error message.

## Prototype
```c
PUBLIC const char *ytls_get_last_error(hytls ytls, hsskt sskt);
```

## Parameters
- `ytls` (`hytls`): The TLS context.
- `sskt` (`hsskt`): The secure socket.

## Return Value
- Returns a string describing the last TLS error.
