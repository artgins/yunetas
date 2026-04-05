<!-- ============================================================== -->
(gobj_find_subscribings())=
# `gobj_find_subscribings()`
<!-- ============================================================== -->

Returns a list of subscriptions where the given `subscriber` is subscribed to events from various publishers.

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
json_t *gobj_find_subscribings(
    hgobj subscriber,
    gobj_event_t event,
    json_t *kw,
    hgobj publisher
);
```

**Parameters**

::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `subscriber`
  - `hgobj`
  - The subscriber gobj whose subscriptions are being queried.

* - `event`
  - `gobj_event_t`
  - The event name to filter subscriptions. If NULL, all events are considered.

* - `kw`
  - `json_t *`
  - A JSON object containing additional filtering criteria, such as `__config__`, `__global__`, `__local__`, and `__filter__`. Owned by the function.

* - `publisher`
  - `hgobj`
  - The publisher gobj to filter subscriptions. If NULL, all publishers are considered.
:::

---

**Return Value**

A JSON array containing the matching subscriptions. The caller owns the returned JSON object and must free it using `json_decref()`.

**Notes**

This function searches for subscriptions where `subscriber` is subscribed to events from `publisher`. The filtering criteria in `kw` allow for fine-grained selection of subscriptions.

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
