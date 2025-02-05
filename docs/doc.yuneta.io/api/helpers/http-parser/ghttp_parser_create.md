<!-- ============================================================== -->
(ghttp_parser_create())=
# `ghttp_parser_create()`
<!-- ============================================================== -->


The `ghttp_parser_create` function initializes a new `GHTTP_PARSER` instance for parsing HTTP messages. It sets up the parser with the specified event handlers for header, body, and message completion events, allowing for custom processing of incoming HTTP data.


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
  - The gobj associated with the parser, used for event handling.

* - `type`
  - `enum http_parser_type`
  - The type of HTTP parser to create (e.g., request or response).

* - `on_header_event`
  - `gobj_event_t`
  - Event to publish or send when the header is completed.

* - `on_body_event`
  - `gobj_event_t`
  - Event to publish or send when the body is receiving.

* - `on_message_event`
  - `gobj_event_t`
  - Event to publish or send when the message is completed.

* - `send_event`
  - `BOOL`
  - Indicates whether to use `gobj_send_event()` to parent (TRUE) or `gobj_publish_event()` (FALSE).
:::


---

**Return Value**


Returns a pointer to the newly created `GHTTP_PARSER` instance, or NULL if the creation fails.


**Notes**


The `on_header_event`, `on_body_event`, and `on_message_event` parameters allow for flexible handling of different stages of HTTP message processing. Ensure that the event handlers are properly defined to handle the expected data.


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

