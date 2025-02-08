# TLS Functions

Source code in:

- [ytls.c](https://github.com/artgins/yunetas/blob/main/kernel/c/ytls/src/ytls.c)
- [ytls.h](https://github.com/artgins/yunetas/blob/main/kernel/c/ytls/src/ytls.h)

- [openssl.c](https://github.com/artgins/yunetas/blob/main/kernel/c/ytls/src/tls/openssl.c)
- [openssl.h](https://github.com/artgins/yunetas/blob/main/kernel/c/ytls/src/tls/openssl.h)

- [mbedtls.c](https://github.com/artgins/yunetas/blob/main/kernel/c/ytls/src/tls/mbedtls.c)
- [mbedtls.h](https://github.com/artgins/yunetas/blob/main/kernel/c/ytls/src/tls/mbedtls.h)


```{toctree}
:caption: TLS Functions
:maxdepth: 1

ytls/ytls_init
ytls/ytls_cleanup
ytls/ytls_version
ytls/ytls_new_secure_filter
ytls/ytls_shutdown
ytls/ytls_free_secure_filter
ytls/ytls_do_handshake
ytls/ytls_encrypt_data
ytls/ytls_decrypt_data
ytls/ytls_get_last_error
ytls/ytls_set_trace
ytls/ytls_flush

```
