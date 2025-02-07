<!-- ============================================================== -->
(gobj_subscribe_event)=
# `gobj_subscribe_event()`
<!-- ============================================================== -->

The `gobj_subscribe_event` function subscribes a `subscriber` GObj to an `event` emitted by a `publisher` GObj, with optional configuration parameters.

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
json_t *gobj_subscribe_event(
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
  - The GObj that emits the event.

* - `event`
  - `gobj_event_t`
  - The event to subscribe to.

* - `kw`
  - `json_t *`
  - A JSON object containing subscription options, including `__config__`, `__global__`, `__local__`, and `__filter__`.

* - `subscriber`
  - `hgobj`
  - The GObj that will receive the event notifications.
:::

---

**Return Value**

Returns a JSON object representing the subscription if successful, or `NULL` on failure.

**Notes**

The `event` must be in the publisher's output event list unless the `gcflag_no_check_output_events` flag is set.
If a subscription with the same parameters already exists, it will be overridden.
The `__config__` field in `kw` can include options such as `__hard_subscription__` (permanent subscription) and `__own_event__` (prevents further propagation if the subscriber handles the event).
The function calls [`gobj_unsubscribe_event()`](#gobj_unsubscribe_event) to remove duplicate subscriptions before adding a new one.

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
