

<!-- ============================================================== -->
(gobj_delete_resource())=
# `gobj_delete_resource()`
<!-- ============================================================== -->


The `gobj_delete_resource` function deletes a specific resource associated with a GObj. The resource to be deleted is identified using the `record` parameter, and additional options can be provided via `jn_options`. This function delegates its operation to the `mt_delete_resource` method of the GObj's GClass.

If the `mt_delete_resource` method is not implemented in the GClass, the function logs an error and returns `-1`.

        

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

PUBLIC int gobj_delete_resource(
    hgobj       gobj_,
    const char  *resource,
    json_t      *record,  // owned
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
  - The name of the resource to delete.

* - `record`
  - `json_t *`
  - JSON object representing the resource to delete (owned).

* - `jn_options`
  - `json_t *`
  - Additional options for deleting the resource (owned).
:::

        

---

**Return Value**


- Returns `0` on success.
- Returns `-1` if the GObj is invalid, the `record` parameter is `NULL`, or the `mt_delete_resource` method is not implemented.
        


**Notes**


- **Ownership:**
  - The `record` and `jn_options` parameters are owned by the function and will be decremented internally.
- **Error Handling:**
  - If the GObj is `NULL` or destroyed, the function logs an error and returns `-1`.
  - If the `mt_delete_resource` method is missing, the function logs an error and returns `-1`.
        


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

