<!-- ============================================================== -->
(gclass_create())=
# `gclass_create()`
<!-- ============================================================== -->

The `gclass_create` function initializes and registers a new GClass with the specified attributes, including event types, states, methods, and flags. It ensures that the GClass is unique and properly structured before adding it to the global registry.

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
hgclass gclass_create(
    gclass_name_t        gclass_name,
    event_type_t        *event_types,
    states_t            *states,
    const GMETHODS      *gmt,
    const LMETHOD       *lmt,
    const sdata_desc_t  *tattr_desc,
    size_t               priv_size,
    const sdata_desc_t  *authz_table,
    const sdata_desc_t  *command_table,
    const trace_level_t *s_user_trace_level,
    gclass_flag_t        gclass_flag
);
```

**Parameters**

::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `gclass_name`
  - `gclass_name_t`
  - The unique name of the GClass.

* - `event_types`
  - `event_type_t *`
  - A list of event types associated with the GClass.

* - `states`
  - `states_t *`
  - A list of states defining the finite state machine of the GClass.

* - `gmt`
  - `const GMETHODS *`
  - A pointer to the structure containing global methods for the GClass.

* - `lmt`
  - `const LMETHOD *`
  - A pointer to the structure containing local methods for the GClass.

* - `tattr_desc`
  - `const sdata_desc_t *`
  - A pointer to the structure describing the attributes of the GClass.

* - `priv_size`
  - `size_t`
  - The size of the private data structure allocated for each instance of the GClass.

* - `authz_table`
  - `const sdata_desc_t *`
  - A pointer to the structure defining authorization levels for the GClass.

* - `command_table`
  - `const sdata_desc_t *`
  - A pointer to the structure defining available commands for the GClass.

* - `s_user_trace_level`
  - `const trace_level_t *`
  - A pointer to the structure defining user trace levels for debugging.

* - `gclass_flag`
  - `gclass_flag_t`
  - Flags defining special behaviors of the GClass.
:::

---

**Return Value**

Returns a handle (`hgclass`) to the newly created GClass, or `NULL` if an error occurs.

**Notes**

['The function checks for duplicate GClass names and returns an error if the name is already registered.', 'It initializes the finite state machine (FSM) and event types for the GClass.', 'The function ensures that the GClass is properly structured before adding it to the global registry.', 'If the GClass has a `gcflag_singleton` flag, only one instance of it can exist.']

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
