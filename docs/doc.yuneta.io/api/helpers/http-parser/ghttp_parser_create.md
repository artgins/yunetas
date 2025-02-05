<!-- ============================================================== -->
(ghttp_parser_create())=
# `ghttp_parser_create()`
<!-- ============================================================== -->


The `ghttp_parser_create()` function initializes and returns a new HTTP parser instance (`GHTTP_PARSER`).
This parser is used to process HTTP messages, including headers and body content, and trigger events
when different parts of the message are received.

The function allows specifying event handlers for when the headers are completed, when body data is received,
and when the entire message is completed. The parser can either send events directly to the parent object
or publish them globally, depending on the `send_event` flag.


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
    hgobj           gobj,
    enum http_parser_type type,
    gobj_event_t    on_header_event,
    gobj_event_t    on_body_event,
    gobj_event_t    on_message_event,
    BOOL            send_event
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
  - The parent GObj that will receive the parsed HTTP events.

* - `type`
  - `enum http_parser_type`
  - The type of HTTP parser, either `HTTP_REQUEST` or `HTTP_RESPONSE`.

* - `on_header_event`
  - `gobj_event_t`
  - The event triggered when the HTTP headers are fully received.

* - `on_body_event`
  - `gobj_event_t`
  - The event triggered when a portion of the HTTP body is received.

* - `on_message_event`
  - `gobj_event_t`
  - The event triggered when the entire HTTP message is received.

* - `send_event`
  - `BOOL`
  - If `TRUE`, events are sent to the parent object using `gobj_send_event()`;
    if `FALSE`, events are published globally using `gobj_publish_event()`.
:::


---

**Return Value**


Returns a pointer to a newly allocated `GHTTP_PARSER` instance, or `NULL` if an error occurs.


**Notes**


- The caller is responsible for destroying the parser using [`ghttp_parser_destroy()`](#ghttp_parser_destroy).
- The `on_header_event` event receives a JSON object containing HTTP headers and metadata.
- The `on_body_event` event receives partial body data in a buffer.
- The `on_message_event` event receives the full message, including headers and body.


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

