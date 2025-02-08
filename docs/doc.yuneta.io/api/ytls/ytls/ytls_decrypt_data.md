# `ytls_decrypt_data()`

## Description
Decrypts received data.

## Prototype
```c
PUBLIC int ytls_decrypt_data(
    hytls ytls,
    hsskt sskt,
    gbuffer_t *gbuf // owned
);
```

## Parameters
- `ytls` (`hytls`): The TLS context.
- `sskt` (`hsskt`): The secure socket.
- `gbuf` (`gbuffer_t *`): The encrypted data buffer. Owned.

## Return Value
- Returns 0 on success, or an error code on failure.

---
