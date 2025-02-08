<!-- ============================================================== -->
(ghttp_parser_create())=
# `ghttp_parser_create()`
<!-- ============================================================== -->

Creates and initializes a new `GHTTP_PARSER` instance for parsing HTTP messages. The parser processes incoming HTTP data and triggers events when headers, body, or complete messages are received.

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
GHTTP_PARSER *ghttp_parser_create(
    hgobj gobj,
    enum http_parser_type type,
    gobj_event_t on_header_event,
    gobj_event_t on_body_event,
    gobj_event_t on_message_event,
    BOOL send_event
);
```

**Parameters**

::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `gobj`
  - `hgobj`
  - The parent GObj that will handle the parsed HTTP events.

* - `type`
  - `enum http_parser_type`
  - The type of HTTP parser (`HTTP_REQUEST`, `HTTP_RESPONSE`, or `HTTP_BOTH`).

* - `on_header_event`
  - `gobj_event_t`
  - The event triggered when HTTP headers are fully received.

* - `on_body_event`
  - `gobj_event_t`
  - The event triggered when a portion of the HTTP body is received.

* - `on_message_event`
  - `gobj_event_t`
  - The event triggered when the entire HTTP message is received.

* - `send_event`
  - `BOOL`
  - If `TRUE`, events are sent using [`gobj_send_event()`](#gobj_send_event()); otherwise, they are published using [`gobj_publish_event()`](#gobj_publish_event()).
:::

---

**Return Value**

A pointer to the newly created `GHTTP_PARSER` instance, or `NULL` if memory allocation fails.

**Notes**

The returned `GHTTP_PARSER` instance must be destroyed using [`ghttp_parser_destroy()`](#ghttp_parser_destroy()) when no longer needed.

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
