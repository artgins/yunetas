<!-- ============================================================== -->
(gobj_create2())=
# `gobj_create2()`
<!-- ============================================================== -->

Creates a new `gobj` (generic object) with the specified name, class, attributes, and parent. The function initializes the object, sets its attributes, and registers it if it is a service.

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
hgobj gobj_create2(
    const char      *gobj_name,
    gclass_name_t   gclass_name,
    json_t          *kw,    // owned
    hgobj           parent,
    gobj_flag_t     gobj_flag
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
  - A JSON object containing the attributes and configuration for the `gobj`. The function takes ownership of this parameter.

* - `parent`
  - `hgobj`
  - The parent `gobj` under which the new `gobj` will be created. Must be non-null unless the `gobj` is a Yuno.

* - `gobj_flag`
  - `gobj_flag_t`
  - Flags that define the behavior of the `gobj`, such as whether it is a service, volatile, or a pure child.
:::

---

**Return Value**

Returns a handle to the newly created `gobj` (`hgobj`). Returns `NULL` if creation fails due to invalid parameters or memory allocation failure.

**Notes**

['If the `gobj` is marked as a service, it is registered globally.', 'If the `gobj` is a Yuno, it is stored as the global Yuno instance.', 'The function checks for required attributes and applies global configuration variables.', 'If the `gobj` has a `mt_create2` method, it is called with the provided attributes.', "If the `gobj` has a parent, it is added to the parent's child list."]

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
