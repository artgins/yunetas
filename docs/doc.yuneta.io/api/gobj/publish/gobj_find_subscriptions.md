<!-- ============================================================== -->
(gobj_find_subscriptions)=
# `gobj_find_subscriptions()`
<!-- ============================================================== -->

Retrieves a list of event subscriptions for a given publisher, filtering by event, keyword parameters, and subscriber.

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
json_t *gobj_find_subscriptions(
    hgobj       publisher,
    gobj_event_t event,
    json_t      *kw,
    hgobj       subscriber
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
  - The publisher object whose subscriptions are being queried.

* - `event`
  - `gobj_event_t`
  - The event name to filter subscriptions. If NULL, all events are considered.

* - `kw`
  - `json_t *`
  - A JSON object containing filtering parameters such as `__config__`, `__global__`, `__local__`, and `__filter__`. If NULL, no additional filtering is applied.

* - `subscriber`
  - `hgobj`
  - The subscriber object to filter subscriptions. If NULL, all subscribers are considered.
:::

---

**Return Value**

A JSON array containing the matching subscriptions. Each subscription is represented as a JSON object. The caller is responsible for freeing the returned JSON object.

**Notes**

This function is useful for inspecting active subscriptions and can be used in conjunction with [`gobj_unsubscribe_list()`](#gobj_unsubscribe_list) to remove subscriptions.

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
