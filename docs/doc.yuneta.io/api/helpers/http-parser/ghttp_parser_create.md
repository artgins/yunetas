<!-- ============================================================== -->
(ghttp_parser_create())=
# `ghttp_parser_create()`
<!-- ============================================================== -->


Creates a new instance of `GHTTP_PARSER`, which is used to parse HTTP messages. This function initializes the parser with the specified event handlers for processing headers, body, and completed messages. The `gobj` parameter associates the parser with a specific object, while the `type` parameter specifies the type of HTTP parser to use (e.g., request or response). The event handlers are called when the respective parts of the HTTP message are received.


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
  - The object associated with this parser instance.

* - `type`
  - `enum http_parser_type`
  - Specifies the type of HTTP parser (request or response).

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


Returns a pointer to a newly created `GHTTP_PARSER` instance, or NULL if the creation fails.


**Notes**


Ensure that the created parser is properly destroyed using `ghttp_parser_destroy()` when it is no longer needed to avoid memory leaks.


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

