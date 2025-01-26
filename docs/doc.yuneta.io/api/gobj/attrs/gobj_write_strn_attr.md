<!-- ============================================================== -->
(gobj_write_strn_attr())=
# `gobj_write_strn_attr()`
<!-- ============================================================== -->


The `gobj_write_strn_attr` function writes a string value (with a specified length) to a named attribute of a GObj. It ensures that the string is safely duplicated and updates the attribute in the GObj's hierarchical data store (HDS). If the GClass of the GObj implements the `mt_writing` method, it is invoked to handle any post-write actions.

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
PUBLIC int gobj_write_strn_attr(
    hgobj       gobj,
    const char  *name,
    const char  *value,
    size_t      len
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
  - `hgobj`
  - The GObj instance where the attribute will be written.

* - `name`
  - `const char *`
  - The name of the attribute to write.

* - `value`
  - `const char *`
  - The string value to write to the attribute.

* - `len`
  - `size_t`
  - The length of the string to write.
:::

**Return Value**

- Returns `0` on success.
- Returns `-1` if:
  - The attribute does not exist.
  - Memory allocation for the string duplication fails.

**Notes**

### Notes
- **Memory Management:**
  - The function duplicates the input string using `GBMEM_STRNDUP`, ensuring safety when handling the string.
  - The duplicated string is freed after being used to update the attribute.
- **Lifecycle Handling:**
  - The `mt_writing` method of the GClass is called if the GObj is fully created and not destroyed.
- **Error Logging:**
  - If the attribute is not found or memory allocation fails, an error is logged.


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
