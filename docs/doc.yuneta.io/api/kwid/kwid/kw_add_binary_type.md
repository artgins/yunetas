<!-- ============================================================== -->
(kw_add_binary_type)=
# `kw_add_binary_type()`
<!-- ============================================================== -->

Registers a new binary type for serialization and deserialization in the kw system. `kw_add_binary_type()` associates a binary field with its serialized representation and provides functions for handling reference counting.

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
int kw_add_binary_type(
    const char *binary_field_name,
    const char *serialized_field_name,
    serialize_fn_t serialize_fn,
    deserialize_fn_t deserialize_fn,
    incref_fn_t incref_fn,
    decref_fn_t decref_fn
);
```

**Parameters**

::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `binary_field_name`
  - `const char *`
  - The name of the binary field to be registered.

* - `serialized_field_name`
  - `const char *`
  - The name of the corresponding serialized field.

* - `serialize_fn`
  - `serialize_fn_t`
  - Function pointer for serializing the binary field.

* - `deserialize_fn`
  - `deserialize_fn_t`
  - Function pointer for deserializing the binary field.

* - `incref_fn`
  - `incref_fn_t`
  - Function pointer for incrementing the reference count of the binary field.

* - `decref_fn`
  - `decref_fn_t`
  - Function pointer for decrementing the reference count of the binary field.
:::

---

**Return Value**

Returns `0` on success, or `-1` if the maximum number of serialized fields has been reached.

**Notes**

The function maintains an internal table of registered binary types. If the table is full, an error is logged, and the function returns `-1`.

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
