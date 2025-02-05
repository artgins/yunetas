<!-- ============================================================== -->
(ghttp_parser_received())=
# `ghttp_parser_received()`
<!-- ============================================================== -->


The `ghttp_parser_received()` function processes a chunk of HTTP data received by the parser. 
It parses the provided buffer `bf` of size `len` using the specified `GHTTP_PARSER` instance. 
The function is responsible for consuming the data, updating the parser's state, and triggering 
events (if configured) when headers, body, or the complete message are processed. 


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

int ghttp_parser_received(
    GHTTP_PARSER *parser,
    char         *bf,
    size_t       len
);

```

**Parameters**


::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `parser`
  - `GHTTP_PARSER *`
  - Pointer to the `GHTTP_PARSER` instance that will process the data.

* - `bf`
  - `char *`
  - Pointer to the buffer containing the HTTP data to be parsed.

* - `len`
  - `size_t`
  - Length of the data in the buffer `bf`.
:::


---

**Return Value**


Returns the number of bytes successfully consumed from the buffer. 
If an error occurs during parsing, the function returns `-1`.


**Notes**


- Ensure that the `parser` is properly initialized using `ghttp_parser_create()` before calling this function.
- The function may trigger events such as `on_header_event`, `on_body_event`, or `on_message_event` 
  based on the parser's configuration and the data processed.
- If the function returns `-1`, it indicates a parsing error, and the parser's state may need to be reset 
  using `ghttp_parser_reset()`.


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

