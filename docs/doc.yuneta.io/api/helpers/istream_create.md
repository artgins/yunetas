

<!-- ============================================================== -->
(istream_create())=
# `istream_create()`
<!-- ============================================================== -->


The `istream_create` function initializes and allocates an `istream` instance. This instance provides an input stream buffer (`gbuffer_t`) with specified sizes for handling data efficiently.

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
PUBLIC istream_h istream_create(
    hgobj       gobj,
    size_t      data_size,
    size_t      max_size
);
```

**Parameters**

::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - **Parameter**
  - **Type**
  - **Description**

* - `gobj`
  - [`hgobj`](hgobj)
  - The GObj associated with the input stream.

* - `data_size`
  - `size_t`
  - The initial size of the data buffer.

* - `max_size`
  - `size_t`
  - The maximum allowable size of the buffer.
:::

---

**Return Value**

- Returns a pointer to the newly created `istream` instance on success.
- Returns `NULL` if memory allocation fails for the `istream` instance or its internal buffer.

**Notes**

- **Memory Management:**
  - The `istream` instance and its internal buffer are dynamically allocated. Ensure to call `istream_destroy` to free resources when the stream is no longer needed.
- **Error Handling:**
  - Logs an error if memory allocation for the `istream` instance or its internal `gbuffer_t` fails.
- **Internal Attributes:**
  - The function initializes:
    - `gobj`: Links the stream to the specified GObj.
    - `data_size` and `max_size`: Define the buffer's initial and maximum sizes.
    - `gbuf`: Allocates a `gbuffer_t` for managing the input stream.

**Use Case**

The `istream_create` function is useful for creating an input stream buffer tied to a GObj. For instance, it can be used to handle incoming data streams in a controlled manner, ensuring that memory constraints are respected.

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
