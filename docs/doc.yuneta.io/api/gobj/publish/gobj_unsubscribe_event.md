<!-- ============================================================== -->
(gobj_unsubscribe_event)=
# `gobj_unsubscribe_event()`
<!-- ============================================================== -->

Removes a subscription from a publisher to a subscriber for a specific event in the GObj system.

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
int gobj_unsubscribe_event(
    hgobj         publisher,
    gobj_event_t  event,
    json_t       *kw,
    hgobj         subscriber
);
```

**Parameters**

::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `publisher`
  - `hgobj`
  - The GObj acting as the publisher from which the subscription should be removed.

* - `event`
  - `gobj_event_t`
  - The event name for which the subscription should be removed.

* - `kw`
  - `json_t *`
  - A JSON object containing additional parameters for filtering the subscription removal. Owned by the function.

* - `subscriber`
  - `hgobj`
  - The GObj acting as the subscriber that should be unsubscribed from the event.
:::

---

**Return Value**

Returns 0 on success, or -1 if the subscription was not found or an error occurred.

**Notes**

If the `event` is not found in the publisher's output event list, the function will return an error unless the publisher has the `gcflag_no_check_output_events` flag set.
If multiple subscriptions match the given parameters, all of them will be removed.
If no matching subscription is found, an error is logged.
The function decrements the reference count of `kw` before returning.

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

