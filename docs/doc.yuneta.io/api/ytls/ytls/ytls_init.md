### `ytls_init()`

#### Description
Initializes a new TLS context.

#### Prototype
```c
PUBLIC hytls ytls_init(
    hgobj gobj,
    json_t *jn_config,  // not owned
    BOOL server
);
```

#### Parameters
- `gobj` (`hgobj`): The GObj that manages the TLS instance.
- `jn_config` (`json_t *`): JSON configuration for the TLS context. Not owned.
- `server` (`BOOL`): Indicates if the context is for a server (`TRUE`) or a client (`FALSE`).

#### Return Value
- Returns a handle to the TLS context (`hytls`).

---

### `ytls_cleanup()`

#### Description
Cleans up and releases resources for the TLS context.

#### Prototype
```c
PUBLIC void ytls_cleanup(hytls ytls);
```

#### Parameters
- `ytls` (`hytls`): The TLS context to clean up.

#### Return Value
- None.

---

### `ytls_version()`

#### Description
Retrieves the version of the TLS implementation.

#### Prototype
```c
PUBLIC const char * ytls_version(hytls ytls);
```

#### Parameters
- `ytls` (`hytls`): The TLS context.

#### Return Value
- Returns a string representing the TLS version.

---

### `ytls_new_secure_filter()`

#### Description
Creates a new secure filter for a TLS connection.

#### Prototype
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

#### Parameters
- `ytls` (`hytls`): The TLS context.
- `on_handshake_done_cb` (`int (*)(void *, int)`): Callback for handshake completion.
- `on_clear_data_cb` (`int (*)(void *, gbuffer_t *)`): Callback for receiving decrypted data.
- `on_encrypted_data_cb` (`int (*)(void *, gbuffer_t *)`): Callback for receiving encrypted data.
- `user_data` (`void *`): User-defined data passed to callbacks.

#### Return Value
- Returns a handle to the secure socket (`hsskt`).

---

### `ytls_shutdown()`

#### Description
Shuts down the TLS connection.

#### Prototype
```c
PUBLIC void ytls_shutdown(hytls ytls, hsskt sskt);
```

#### Parameters
- `ytls` (`hytls`): The TLS context.
- `sskt` (`hsskt`): The secure socket to shut down.

#### Return Value
- None.

---

### `ytls_free_secure_filter()`

#### Description
Frees a secure filter instance.

#### Prototype
```c
PUBLIC void ytls_free_secure_filter(hytls ytls, hsskt sskt);
```

#### Parameters
- `ytls` (`hytls`): The TLS context.
- `sskt` (`hsskt`): The secure socket to free.

#### Return Value
- None.

---

### `ytls_do_handshake()`

#### Description
Performs a TLS handshake on a secure connection.

#### Prototype
```c
PUBLIC int ytls_do_handshake(hytls ytls, hsskt sskt);
```

#### Parameters
- `ytls` (`hytls`): The TLS context.
- `sskt` (`hsskt`): The secure socket.

#### Return Value
- Returns 0 on success, or an error code on failure.

---

### `ytls_encrypt_data()`

#### Description
Encrypts data for transmission.

#### Prototype
```c
PUBLIC int ytls_encrypt_data(
    hytls ytls,
    hsskt sskt,
    gbuffer_t *gbuf // owned
);
```

#### Parameters
- `ytls` (`hytls`): The TLS context.
- `sskt` (`hsskt`): The secure socket.
- `gbuf` (`gbuffer_t *`): The data buffer to encrypt. Owned.

#### Return Value
- Returns 0 on success, or an error code on failure.

---

### `ytls_decrypt_data()`

#### Description
Decrypts received data.

#### Prototype
```c
PUBLIC int ytls_decrypt_data(
    hytls ytls,
    hsskt sskt,
    gbuffer_t *gbuf // owned
);
```

#### Parameters
- `ytls` (`hytls`): The TLS context.
- `sskt` (`hsskt`): The secure socket.
- `gbuf` (`gbuffer_t *`): The encrypted data buffer. Owned.

#### Return Value
- Returns 0 on success, or an error code on failure.

---

### `ytls_get_last_error()`

#### Description
Retrieves the last TLS error message.

#### Prototype
```c
PUBLIC const char *ytls_get_last_error(hytls ytls, hsskt sskt);
```

#### Parameters
- `ytls` (`hytls`): The TLS context.
- `sskt` (`hsskt`): The secure socket.

#### Return Value
- Returns a string describing the last TLS error.

---

### `ytls_set_trace()`

#### Description
Enables or disables TLS debugging trace.

#### Prototype
```c
PUBLIC void ytls_set_trace(hytls ytls, hsskt sskt, BOOL set);
```

#### Parameters
- `ytls` (`hytls`): The TLS context.
- `sskt` (`hsskt`): The secure socket.
- `set` (`BOOL`): Whether to enable (`TRUE`) or disable (`FALSE`) tracing.

#### Return Value
- None.

---

### `ytls_flush()`

#### Description
Flushes any pending TLS data.

#### Prototype
```c
PUBLIC int ytls_flush(hytls ytls, hsskt sskt);
```

#### Parameters
- `ytls` (`hytls`): The TLS context.
- `sskt` (`hsskt`): The secure socket.

#### Return Value
- Returns 0 on success, or an error code on failure.
