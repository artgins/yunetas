<!-- ============================================================== -->
(gclass_create())=
# `gclass_create()`
<!-- ============================================================== -->

Creates and register a GClass, defining its core structure, behavior, and configuration.

<!------------------------------------------------------------>
<!--                    Prototypes                          -->
<!------------------------------------------------------------>

``````{tab-set}

`````{tab-item} C

<!--====================================================-->
<!--                    Tab C                           -->
<!--====================================================-->

<!---------------------------------------------------->
<!--                C Prototype                     -->
<!---------------------------------------------------->

**Prototype**

````C
PUBLIC hgclass gclass_create(
    gclass_name_t       gclass_name,
    event_type_t        *event_types,
    states_t            *states,
    const GMETHODS      *gmt,
    const LMETHOD       *lmt,
    const sdata_desc_t  *tattr_desc,
    size_t              priv_size,
    const sdata_desc_t  *authz_table,
    const sdata_desc_t  *command_table,
    const trace_level_t *s_user_trace_level,
    gclass_flag_t       gclass_flag
);
````

<!---------------------------------------------------->
<!--                C Parameters                    -->
<!---------------------------------------------------->

**Parameters**

````{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gclass_name`
  - [`gclass_name_t`](gclass_name_t)
  - The unique name of the GClass being created.

* - `event_types`
  - [`event_type_t *`](event_type_t)
  - Pointer to the table of events supported by the GClass, defining input and output events.

* - `states`
  - [`states_t *`](states_t)
  - Pointer to the finite state machine (FSM) definition for the GClass, specifying states and transitions.

* - `gmt`
  - [`const GMETHODS *`](GMETHODS)
  - Pointer to the table of global methods for the GClass, implementing its core behavior.

* - `lmt`
  - [`const LMETHOD *`](LMETHOD)
  - Pointer to the table of internal methods for the GClass, invoked explicitly as needed (optional).

* - `tattr_desc`
  - [`const sdata_desc_t *`](sdata_desc_t)
  - Pointer to the table defining the GClass attributes (type, name, flags, default value, and description).

* - `priv_size`
  - `size_t`
  - The size of the private data buffer allocated for each GObj instance of the GClass.

* - `authz_table`
  - [`const sdata_desc_t *`](sdata_desc_t)
  - Pointer to the table defining authorization rules for the GClass (optional).

* - `command_table`
  - [`const sdata_desc_t *`](sdata_desc_t)
  - Pointer to the table defining commands available in the GClass (optional).

* - `s_user_trace_level`
  - [`const trace_level_t *`](trace_level_t)
  - Pointer to the table of trace levels for monitoring and debugging the GClass (optional).

* - `gclass_flag`
  - [`gclass_flag_t`](gclass_flag_t)
  - Flags modifying the GClass behavior (e.g., `gcflag_manual_start`, `gcflag_singleton`).

````

<!---------------------------------------------------->
<!--                C Return                        -->
<!---------------------------------------------------->

---

**Return Value**

- Returns a handle to the created GClass [](hgclass).


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
