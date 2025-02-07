<!-- ============================================================== -->
(gclass_add_ev_action)=
# `gclass_add_ev_action()`
<!-- ============================================================== -->

Adds an event-action pair to a specified state in the given `hgclass`. The function associates an event with an action function and a next state transition.

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
int gclass_add_ev_action(
    hgclass         gclass,
    gobj_state_t    state_name,
    gobj_event_t    event,
    gobj_action_fn  action,
    gobj_state_t    next_state
);
```

**Parameters**

::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `gclass`
  - `hgclass`
  - The handle to the `gclass` where the event-action pair will be added.

* - `state_name`
  - `gobj_state_t`
  - The name of the state to which the event-action pair will be added.

* - `event`
  - `gobj_event_t`
  - The event that triggers the action.

* - `action`
  - `gobj_action_fn`
  - The function to be executed when the event occurs in the specified state.

* - `next_state`
  - `gobj_state_t`
  - The state to transition to after executing the action. If `NULL`, the state remains unchanged.
:::

---

**Return Value**

Returns `0` on success, or `-1` if an error occurs (e.g., if the state does not exist or the event is already defined).

**Notes**

This function ensures that an event-action pair is uniquely associated with a state. If the event is already defined in the state, an error is logged and the function returns `-1`.

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

