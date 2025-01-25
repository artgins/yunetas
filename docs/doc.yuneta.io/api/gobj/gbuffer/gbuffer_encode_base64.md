

<!-- ============================================================== -->
(gbuffer_encode_base64())=
# `gbuffer_encode_base64()`
<!-- ============================================================== -->

The `gbuffer_encode_base64` function encodes the content of the input GBuffer (`gbuf_input`) into Base64 format and returns a new GBuffer containing the encoded result. The input GBuffer is decremented and released during the process.

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
PUBLIC gbuffer_t *gbuffer_encode_base64(
    gbuffer_t   *gbuf_input  // decref
);

```

**Parameters**

::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - **Parameter**
  - **Type**
  - **Description**

* - `gbuf_input`
  - [`gbuffer_t *`](gbuffer_t)
  - The input GBuffer containing the data to encode. It is decremented and released by the function.
:::

---

**Return Value**

- Returns a new GBuffer containing the Base64-encoded content.
- Returns `NULL` if the input GBuffer is invalid or the encoding fails.

**Notes**

- **Memory Management:**
  - The caller is responsible for freeing the returned GBuffer when no longer needed.
  - The input GBuffer (`gbuf_input`) is decremented and cannot be used after calling this function.
- **Base64 Encoding:**
  - Internally, the function uses `gbuffer_string_to_base64` to perform the encoding.
- **Use Case:**
  - This function is useful for encoding binary data into Base64 for transmission or storage in text-based systems.

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
