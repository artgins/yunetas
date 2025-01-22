

<!-- ============================================================== -->
(gobj_get_resource())=
# `gobj_get_resource()`
<!-- ============================================================== -->


The `gobj_get_resource` function retrieves a specific resource associated with a GObj that matches the given filter. Additional options for retrieving the resource can be provided via `jn_options`. This function delegates its operation to the `mt_get_resource` method of the GObj's GClass.

If the `mt_get_resource` method is not implemented in the GClass, the function logs an error and returns `NULL`.

        

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

PUBLIC json_t *gobj_get_resource(
    hgobj       gobj_,
    const char  *resource,
    json_t      *jn_filter,  // owned
    json_t      *jn_options // owned
);
        

```

**Parameters**


::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - **Parameter**
  - **Type**
  - **Description**

* - `gobj_`
  - `hgobj`
  - The GObj instance associated with the resource.

* - `resource`
  - `const char *`
  - The name of the resource to retrieve.

* - `jn_filter`
  - `json_t *`
  - JSON object defining the filter criteria for identifying the resource (owned).

* - `jn_options`
  - `json_t *`
  - Additional options for retrieving the resource (owned).
:::
        

---

**Return Value**


- Returns a JSON object representing the resource (not owned by the caller).
- Returns `NULL` if the GObj is invalid or the `mt_get_resource` method is not implemented.
        


**Notes**


- **Ownership:**
  - The `jn_filter` and `jn_options` parameters are owned by the function and will be decremented internally.
  - The returned JSON object is not owned by the caller and must not be decremented.
- **Error Handling:**
  - If the GObj is `NULL` or destroyed, the function logs an error and returns `NULL`.
  - If the `mt_get_resource` method is missing, the function logs an error and returns `NULL`.
        


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

