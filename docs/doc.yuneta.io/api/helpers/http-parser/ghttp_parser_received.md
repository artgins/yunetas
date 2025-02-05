<!-- ============================================================== -->
(ghttp_parser_received())=
# `ghttp_parser_received()`
<!-- ============================================================== -->


This function processes incoming data for an HTTP parser. It takes a buffer containing the data received and its length, and attempts to parse the data according to the HTTP protocol. The function updates the state of the `GHTTP_PARSER` instance based on the received data and returns the number of bytes consumed from the buffer. If an error occurs during parsing, it returns -1.


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
    char        *bf,
    size_t      len
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
  - A pointer to the `GHTTP_PARSER` instance that is processing the incoming data.

* - `bf`
  - `char *`
  - A pointer to the buffer containing the data to be parsed.

* - `len`
  - `size_t`
  - The length of the data in the buffer to be parsed.

:::


---

**Return Value**


Returns the number of bytes consumed from the buffer if successful, or -1 if an error occurs during parsing.


**Notes**


Ensure that the `GHTTP_PARSER` instance is properly initialized before calling this function. The function may modify the state of the parser, so it should be called in the correct sequence as per the HTTP parsing workflow.


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

