<!-- ============================================================== -->
(gobj_create_service())=
# `gobj_create_service()`
<!-- ============================================================== -->

The `gobj_create_service()` function creates a new service GObj instance with the specified name, gclass, and configuration, and assigns it to the given parent GObj.

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
hgobj gobj_create_service(
    const char      *gobj_name,
    gclass_name_t   gclass_name,
    json_t          *kw,  // owned
    hgobj parent
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
  - The name of the GObj instance to be created.

* - `gclass_name`
  - `gclass_name_t`
  - The name of the GClass to which the new GObj belongs.

* - `kw`
  - `json_t *`
  - A JSON object containing the configuration parameters for the new GObj. This parameter is owned and will be consumed by the function.

* - `parent`
  - `hgobj`
  - The parent GObj under which the new service GObj will be created.
:::

---

**Return Value**

Returns a handle to the newly created service GObj (`hgobj`). If the creation fails, it returns `NULL`.

**Notes**

The created GObj is marked as a service using the `gobj_flag_service` flag. If the `gclass_name` is not found, an error is logged, and the function returns `NULL`. The function internally calls [`gobj_create2()`](#gobj_create2) with the appropriate flags.

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
