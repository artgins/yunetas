<!-- ============================================================== -->
(formatdate())=
# `formatdate()`
<!-- ============================================================== -->


The `formatdate()` function formats a given timestamp (`t`) into a human-readable string based on the specified `format`. 
The formatted string is stored in the buffer `bf`, which must have a size of at least `bfsize` bytes. 
This function is useful for converting timestamps into custom date and time representations.


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

char *formatdate(
    time_t      t,
    char        *bf,
    int         bfsize,
    const char  *format
);

```

**Parameters**


::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `t`
  - `time_t`
  - The timestamp to be formatted.

* - `bf`
  - `char *`
  - Pointer to the buffer where the formatted date string will be stored.

* - `bfsize`
  - `int`
  - The size of the buffer `bf` in bytes.

* - `format`
  - `const char *`
  - The format string specifying how the date should be formatted. It follows the standard `strftime` format specifiers.
:::


---

**Return Value**


Returns a pointer to the buffer `bf` containing the formatted date string. 
If an error occurs (e.g., the buffer is too small), the contents of `bf` are undefined.


**Notes**


- The `format` string must follow the conventions of the `strftime` function.
- Ensure that the `bfsize` is large enough to hold the formatted string, including the null terminator.
- This function does not allocate memory for the buffer; the caller must provide a pre-allocated buffer.


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

