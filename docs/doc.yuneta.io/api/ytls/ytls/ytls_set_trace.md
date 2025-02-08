# `ytls_set_trace()`

## Description
Enables or disables TLS debugging trace.

## Prototype
```c
PUBLIC void ytls_set_trace(hytls ytls, hsskt sskt, BOOL set);
```

## Parameters
- `ytls` (`hytls`): The TLS context.
- `sskt` (`hsskt`): The secure socket.
- `set` (`BOOL`): Whether to enable (`TRUE`) or disable (`FALSE`) tracing.

## Return Value
- None.

---
