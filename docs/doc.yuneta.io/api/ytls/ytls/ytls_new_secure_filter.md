# `ytls_new_secure_filter()`

## Description
Creates a new secure filter for a TLS connection.

## Prototype
```c
PUBLIC hsskt ytls_new_secure_filter(
    hytls ytls,
    int (*on_handshake_done_cb)(void *user_data, int error),
    int (*on_clear_data_cb)(
        void *user_data,
        gbuffer_t *gbuf  // must be decref
    ),
    int (*on_encrypted_data_cb)(
        void *user_data,
        gbuffer_t *gbuf  // must be decref
    ),
    void *user_data
);
```

## Parameters
- `ytls` (`hytls`): The TLS context.
- `on_handshake_done_cb` (`int (*)(void *, int)`): Callback for handshake completion.
- `on_clear_data_cb` (`int (*)(void *, gbuffer_t *)`): Callback for receiving decrypted data.
- `on_encrypted_data_cb` (`int (*)(void *, gbuffer_t *)`): Callback for receiving encrypted data.
- `user_data` (`void *`): User-defined data passed to callbacks.

## Return Value
- Returns a handle to the secure socket (`hsskt`).
