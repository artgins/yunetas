<!-- ============================================================== -->
(gobj_create())=
# `gobj_create()`
<!-- ============================================================== -->

Creates a new `gobj` (generic object) instance of the specified `gclass` and assigns it to a parent `gobj` if provided.

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
hgobj gobj_create(
    const char      *gobj_name,
    gclass_name_t   gclass_name,
    json_t          *kw, // owned
    hgobj           parent
);
```

**Parameters**

::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `gobj_name`
  - `const char *`
  - The name of the `gobj` to be created. It is case-insensitive and converted to lowercase.

* - `gclass_name`
  - `gclass_name_t`
  - The name of the `gclass` to which the `gobj` belongs.

* - `kw`
  - `json_t *`
  - A JSON object containing configuration parameters for the `gobj`. The ownership of this object is transferred to the function.

* - `parent`
  - `hgobj`
  - The parent `gobj` to which the new `gobj` will be attached. If `NULL`, the `gobj` is created without a parent.
:::

---

**Return Value**

Returns a handle to the newly created `gobj` (`hgobj`). Returns `NULL` if the creation fails due to invalid parameters or memory allocation failure.

**Notes**

['The function internally calls [`gobj_create2()`](#gobj_create2) with default flags.', 'If `gobj_name` is longer than 15 characters on ESP32, the function will abort execution.', 'If `gclass_name` is empty or not found, an error is logged and `NULL` is returned.', "The function ensures that `gobj_name` does not contain invalid characters such as '`' or '^'.", 'If `parent` is `NULL`, an error is logged unless the `gobj` is a Yuno instance.']

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
