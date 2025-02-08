<!-- ============================================================== -->
(gobj_publish_event())=
# `gobj_publish_event()`
<!-- ============================================================== -->

The `gobj_publish_event` function publishes an event from a given publisher to all its subscribers, applying optional filters and transformations before dispatching the event.

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
int gobj_publish_event(
    hgobj        publisher,
    gobj_event_t event,
    json_t       *kw  // this kw extends kw_request.
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
  - The gobj (generic object) that is publishing the event.

* - `event`
  - `gobj_event_t`
  - The event to be published.

* - `kw`
  - `json_t *`
  - A JSON object containing additional data for the event. This object is extended with the subscription's global parameters.
:::

---

**Return Value**

Returns the sum of the return values from [`gobj_send_event()`](#gobj_send_event) calls to all subscribers. A return value of -1 indicates that an event was owned and should not be further published.

**Notes**

If the publisher has a `mt_publish_event` method, it is called first. If it returns <= 0, the function returns immediately.
Each subscriber's `mt_publication_pre_filter` method is called before dispatching the event, allowing for filtering or modification of the event data.
If a subscriber has a `mt_publication_filter` method, it is used to determine whether the event should be sent to that subscriber.
Local and global keyword modifications are applied before sending the event.
If the event is a system event, it is only sent to subscribers that support system events.

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
