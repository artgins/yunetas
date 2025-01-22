

<!-- ============================================================== -->
(gobj_create_resource())=
# `gobj_create_resource()`
<!-- ============================================================== -->


The `gobj_create_resource` function creates a new resource associated with a GObj. The resource is initialized using the provided attributes (`kw`) and options (`jn_options`). This function delegates its operation to the `mt_create_resource` method of the GObj's GClass.

If the `mt_create_resource` method is not implemented in the GClass, the function logs an error and returns `NULL`.
        

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

PUBLIC json_t *gobj_create_resource(
    hgobj       gobj,
    const char  *resource,
    json_t      *kw,  // owned
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
  - The GObj instance where the resource will be created.

* - `resource`
  - `const char *`
  - The name of the resource to create.

* - `kw`
  - `json_t *`
  - JSON object containing the attributes for the resource (owned).

* - `jn_options`
  - `json_t *`
  - Additional options for creating the resource (owned).
:::

        

---

**Return Value**


- Returns a JSON object representing the created resource (not owned by the caller).
- If the GObj is invalid or the `mt_create_resource` method is not defined, the function logs an error and returns `NULL`.
        


**Notes**


- **Ownership:**
  - The `kw` and `jn_options` parameters are owned by the function and will be decremented internally.
  - The returned JSON object is not owned by the caller and must not be decremented.
- **Error Handling:**
  - If the GObj is `NULL` or destroyed, the function logs an error and returns `NULL`.
  - If the `mt_create_resource` method is missing, the function logs an error and returns `NULL`.
        


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

