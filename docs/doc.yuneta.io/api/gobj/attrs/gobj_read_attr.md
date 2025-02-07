

<!-- ============================================================== -->
(gobj_read_attr)=
# `gobj_read_attr()`
<!-- ============================================================== -->

The `gobj_read_str_attr` function retrieves the value of a string attribute from a GObj. If the GClass of the GObj implements the `mt_reading` method, it is invoked to retrieve the value. If the `mt_reading` method is not defined or does not provide a value, the function retrieves the value directly from the GObj's hierarchical data store (HDS).

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
PUBLIC json_t *gobj_read_attr(
    hgobj       gobj,
    const char  *name
);
```

**Parameters**

:::{list-table}
:widths: 20 20 60
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - Handle to the GObj whose attribute is being read.

* - `attr_name`
  - `const char *`
  - The name of the attribute to read.

:::

**Return Value**

- Returns the value of the specified attribute.
- Returns `NULL` if the attribute does not exist or is unreadable.

**Notes**
- **Custom Read Logic:**
  - If the GClass defines an `mt_reading` method, it is called to retrieve the value. If the method indicates that a value was found, it is returned.
  - If `mt_reading` is not defined or does not return a value, the function directly accesses the attribute from the HDS.
- **Error Logging:**
  - Logs a warning if the specified attribute is not found in the GObj.
- **Memory Management:**
  - The returned value is not owned by the caller and must not be modified or freed.


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
