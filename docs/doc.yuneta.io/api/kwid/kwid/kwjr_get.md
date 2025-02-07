<!-- ============================================================== -->
(kwjr_get)=
# `kwjr_get()`
<!-- ============================================================== -->

Retrieve the first record from a JSON list or dictionary that matches the given `id`. If the record is not found and `KW_CREATE` is set, a new record is created using `json_desc`. The function supports extracting the record from the list or dictionary if `KW_EXTRACT` is set.

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
json_t *kwjr_get(
    hgobj gobj,
    json_t *kw,          // NOT owned
    const char *id,
    json_t *new_record,  // owned
    const json_desc_t *json_desc,
    size_t *idx,        // If not null, set the index in case of an array
    kw_flag_t flag
);
```

**Parameters**

::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `gobj`
  - `hgobj`
  - Pointer to the GObj context.

* - `kw`
  - `json_t *`
  - JSON object or array containing the records. Not owned by the caller.

* - `id`
  - `const char *`
  - The identifier of the record to retrieve.

* - `new_record`
  - `json_t *`
  - A new record to insert if `KW_CREATE` is set. Owned by the caller.

* - `json_desc`
  - `const json_desc_t *`
  - JSON descriptor defining the structure of the records.

* - `idx`
  - `size_t *`
  - Pointer to store the index of the found record in case of an array. Can be NULL.

* - `flag`
  - `kw_flag_t`
  - Flags controlling the behavior of the function, such as `KW_CREATE`, `KW_EXTRACT`, and `KW_BACKWARD`.
:::

---

**Return Value**

Returns a pointer to the found or newly created JSON record. If `KW_EXTRACT` is set, the record is removed from `kw`. Returns NULL if the record is not found and `KW_CREATE` is not set.

**Notes**

If `kw` is a dictionary, the function searches for a key matching `id`. If `kw` is a list, it searches for a record where the `id` field matches. If `KW_CREATE` is set, a new record is created using `json_desc` and `new_record`. If `KW_EXTRACT` is set, the record is removed from `kw` and returned with an increased reference count.

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
