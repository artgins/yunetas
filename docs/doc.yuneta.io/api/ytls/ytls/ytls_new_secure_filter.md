<!-- ============================================================== -->
(ytls_new_secure_filter())=
# `ytls_new_secure_filter()`
<!-- ============================================================== -->

`ytls_new_secure_filter()` creates a new secure filter for handling encrypted communication using a TLS context.

<!------------------------------------------------------------>
<!--                    Prototypes                          -->
<!------------------------------------------------------------>

``````{tab-set}

`````{tab-item} C

<!--====================================================-->
<!--                    Tab C                           -->
<!--====================================================-->

**Prototype**

```C
hsskt ytls_new_secure_filter(
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

**Parameters**

::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `ytls`
  - `hytls`
  - The TLS context used to create the secure filter.

* - `on_handshake_done_cb`
  - `int (*)(void *user_data, int error)`
  - Callback function invoked when the handshake process completes. The `error` parameter indicates success (0) or failure (non-zero).

* - `on_clear_data_cb`
  - `int (*)(void *user_data, gbuffer_t *gbuf)`
  - Callback function invoked when decrypted data is available. The `gbuf` parameter must be decremented (`decref`) after use.

* - `on_encrypted_data_cb`
  - `int (*)(void *user_data, gbuffer_t *gbuf)`
  - Callback function invoked when encrypted data is available. The `gbuf` parameter must be decremented (`decref`) after use.

* - `user_data`
  - `void *`
  - User-defined data passed to the callback functions.
:::

---

**Return Value**

Returns an `hsskt` handle representing the newly created secure filter, or `NULL` on failure.

**Notes**

The secure filter manages encrypted communication and invokes the provided callbacks for handshake completion, clear data reception, and encrypted data transmission.

<!--====================================================-->
<!--                    End Tab C                       -->
<!--====================================================-->

`````

`````{tab-item} JS

<!--====================================================-->
<!--                    Tab JS                          -->
<!--====================================================-->

<!---------------------------------------------------->
<!--                JS Prototype                    -->
<!---------------------------------------------------->

**Prototype**

````JS
// Not applicable in JS
````

<!--====================================================-->
<!--                    EndTab JS                       -->
<!--====================================================-->

`````

`````{tab-item} Python

<!--====================================================-->
<!--                    Tab Python                      -->
<!--====================================================-->

<!---------------------------------------------------->
<!--                Python Prototype                -->
<!---------------------------------------------------->

**Prototype**

````Python
# Not applicable in Python
````

<!--====================================================-->
<!--                    End Tab Python                   -->
<!--====================================================-->

`````

``````

<!------------------------------------------------------------>
<!--                    Examples                            -->
<!------------------------------------------------------------>

```````{dropdown} Examples

``````{tab-set}

`````{tab-item} C

<!--====================================================-->
<!--                    Tab C                           -->
<!--====================================================-->

<!---------------------------------------------------->
<!--                C examples                      -->
<!---------------------------------------------------->

````C
// TODO C examples
````

<!--====================================================-->
<!--                    End Tab C                       -->
<!--====================================================-->

`````

`````{tab-item} JS

<!--====================================================-->
<!--                    Tab JS                          -->
<!--====================================================-->

<!---------------------------------------------------->
<!--                JS examples                     -->
<!---------------------------------------------------->

````JS
// TODO JS examples
````

<!--====================================================-->
<!--                    EndTab JS                       -->
<!--====================================================-->

`````

`````{tab-item} Python

<!--====================================================-->
<!--                    Tab Python                      -->
<!--====================================================-->

<!---------------------------------------------------->
<!--                Python examples                 -->
<!---------------------------------------------------->

````python
# TODO Python examples
````

<!--====================================================-->
<!--                    End Tab Python                  -->
<!--====================================================-->

`````

``````

```````
