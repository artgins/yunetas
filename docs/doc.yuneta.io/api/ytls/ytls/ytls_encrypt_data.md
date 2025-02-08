# `ytls_encrypt_data()`

## Description
Encrypts data for transmission.

## Prototype
```c
PUBLIC int ytls_encrypt_data(
    hytls ytls,
    hsskt sskt,
    gbuffer_t *gbuf // owned
);
```

## Parameters
- `ytls` (`hytls`): The TLS context.
- `sskt` (`hsskt`): The secure socket.
- `gbuf` (`gbuffer_t *`): The data buffer to encrypt. Owned.

## Return Value
- Returns 0 on success, or an error code on failure.
