<!-- ============================================================== -->
(ghttp_parser_received())=
# `ghttp_parser_received()`
<!-- ============================================================== -->


The `ghttp_parser_received` function processes incoming data for an HTTP parser. It takes a buffer containing the data and its length, and attempts to parse it according to the HTTP protocol. The function returns the number of bytes consumed from the buffer or -1 if an error occurs during parsing.


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
    char *bf,
    size_t len
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
  - A pointer to the `GHTTP_PARSER` structure that holds the state of the HTTP parser.

* - `bf`
  - `char *`
  - A pointer to the buffer containing the data to be parsed.

* - `len`
  - `size_t`
  - The length of the data in the buffer.
:::


---

**Return Value**


The function returns the number of bytes consumed from the buffer if parsing is successful. If an error occurs, it returns -1.


**Notes**


This function is part of the HTTP parser implementation and is designed to handle the parsing of HTTP messages. Ensure that the `GHTTP_PARSER` instance is properly initialized before calling this function.


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

