<!-- ============================================================== -->
(gclass_create())=
# `gclass_create()`
<!-- ============================================================== -->

Creates and registers a new `gclass`, defining its event types, states, methods, and attributes.

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
    gclass_name_t         gclass_name,
    event_type_t         *event_types,
    states_t             *states,
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
  - The name of the `gclass` to be created.

* - `event_types`
  - `event_type_t *`
  - A list of event types associated with the `gclass`.

* - `states`
  - `states_t *`
  - A list of states defining the finite state machine of the `gclass`.

* - `gmt`
  - `const GMETHODS *`
  - A pointer to the global methods table defining the behavior of the `gclass`.

* - `lmt`
  - `const LMETHOD *`
  - A pointer to the local methods table for internal method handling.

* - `tattr_desc`
  - `const sdata_desc_t *`
  - A pointer to the attribute description table defining the attributes of the `gclass`.

* - `priv_size`
  - `size_t`
  - The size of the private data structure allocated for each instance of the `gclass`.

* - `authz_table`
  - `const sdata_desc_t *`
  - A pointer to the authorization table defining access control rules.

* - `command_table`
  - `const sdata_desc_t *`
  - A pointer to the command table defining available commands for the `gclass`.

* - `s_user_trace_level`
  - `const trace_level_t *`
  - A pointer to the trace level table defining user-defined trace levels.

* - `gclass_flag`
  - `gclass_flag_t`
  - Flags defining special properties of the `gclass`.
:::

---

**Return Value**

Returns a handle to the newly created `gclass`, or `NULL` if an error occurs.

**Notes**

The `gclass_name` must be unique and cannot contain the characters `.` or `^`.
If the `gclass` already exists, an error is logged and `NULL` is returned.
The function initializes the finite state machine (FSM) and event list for the `gclass`.
If the FSM is invalid, the `gclass` is unregistered and `NULL` is returned.

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
