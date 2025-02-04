<!-- ============================================================== -->
(ghttp_parser_create())=
# `ghttp_parser_create()`
<!-- ============================================================== -->


This function creates a GHTTP_PARSER object for parsing HTTP messages. It allows for handling headers, body, and complete messages separately.

The parser can be configured to handle different types of HTTP messages and trigger events when specific parts of the message are received.


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
  - The gobj instance associated with the parser.

* - `type`
  - `enum http_parser_type`
  - The type of HTTP parser to create.

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
  - Flag indicating whether to use `gobj_send_event()` to the parent (`TRUE`) or `gobj_publish_event()` (`FALSE`).
:::


---

**Return Value**


The function returns a pointer to the created GHTTP_PARSER object.


**Notes**


- The `on_header_event` should provide a key-value pair dictionary with information about the headers received.
- The `on_body_event` should provide a partial body buffer as a key-value pair.
- The `on_message_event` should provide information about the complete message, including headers and body.
- The parser should be destroyed using `ghttp_parser_destroy()` when no longer needed.


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

