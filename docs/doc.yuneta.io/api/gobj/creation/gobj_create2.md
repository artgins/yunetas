<!-- ============================================================== -->
(gobj_create2())=
# `gobj_create2()`
<!-- ============================================================== -->

The `gobj_create2` function creates a new GObj (Generic Object) with the specified parameters. It initializes the GObj with attributes (`kw`), associates it with a parent GObj, and sets specific flags (`gobj_flag`) to define its behavior or role. The function supports advanced use cases such as creating services, default services, and volatile objects.

This function validates the input parameters, allocates memory for the GObj, initializes its internal attributes, and invokes lifecycle methods such as `mt_create2` or `mt_create` defined by the GClass. The created GObj is then registered in the appropriate context, such as a parent GObj or global registries (for services or Yunos).

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
PUBLIC hgobj gobj_create2(
    const char      *gobj_name,
    gclass_name_t   gclass_name,
    json_t          *kw, // owned
    hgobj           parent,
    gobj_flag_t     gobj_flag
);
```

**Parameters**

:::{list-table}
:widths: 20 20 60
:header-rows: 1

* - **Parameter**
  - **Type**
  - **Description**

* - `gobj_name`
  - `const char *`
  - The name of the GObj. If `NULL`, defaults to an empty string.

* - `gclass_name`
  - `gclass_name_t`
  - The name of the GClass defining the behavior of the GObj.

* - `kw`
  - `json_t *`
  - JSON object containing configuration attributes for the GObj (owned).

* - `parent`
  - `hgobj`
  - The parent GObj. Must not be `NULL` unless creating a Yuno.

* - `gobj_flag`
  - `gobj_flag_t`
  - Flags defining the role and behavior of the GObj (e.g., service, volatile, etc.).

:::

**Return Value**

- Returns a handle to the newly created GObj ([`hgobj`](hgobj)).
- Returns `NULL` if an error occurs (e.g., invalid parameters, memory allocation failure).

**Notes**
- **Flags Behavior:**
  - `gobj_flag_yuno`: Marks the GObj as a Yuno. Only one Yuno can exist.
  - `gobj_flag_service`: Registers the GObj as a service.
  - `gobj_flag_default_service`: Registers the GObj as the default service.
  - `gobj_flag_volatil`: Marks the GObj as volatile.
- **Lifecycle Methods:**
  - If the GClass implements `mt_create2`, it is called with the attributes (`kw`).
  - If `mt_create2` is not implemented, the older `mt_create` method is invoked.
- **Parent Relationship:**
  - If the GObj is not a Yuno, it is added as a child of the specified parent GObj.

**Error Handling**
- Logs an error and returns `NULL` in the following cases:
  - `gclass_name` is empty or invalid.
  - A service or default service with the same name already exists.
  - Memory allocation fails.
  - `gobj_flag_yuno` is set, but a Yuno already exists.
- Input parameters (`kw`) are decremented if an error occurs.

**Example Use Case**
This function is typically used to create and configure complex hierarchical GObj trees, where each GObj performs a specific role within the system. For example:
- Creating a service GObj to manage resources.
- Initializing a default service for a Yuno.
- Adding child Gobjs to a parent for modular behavior.


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
