<!-- ============================================================== -->
(bits2gbuffer())=
# `bits2gbuffer()`
<!-- ============================================================== -->


Converts a set of bit values represented by a string table into a gbuffer with concatenated string values. The function takes a set of bit values and their corresponding string representations in a table and generates a gbuffer containing the concatenated string values.

**Example:**
Assume a sample_flag_t enum with string representations:
```
typedef enum {
    case1 = 0x0001,
    case2 = 0x0002,
    case3 = 0x0004,
} sample_flag_t;

const char *sample_flag_names[] = {
    "case1",
    "case2",
    "case3",
    0
};
```
The `bits2gbuffer` function will convert the bit values into a gbuffer with concatenated string representations.


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

gbuffer_t *bits2gbuffer(
    const char **strings_table,
    uint64_t bits
);

```

**Parameters**


::: {list-table}
:widths: 30 70
:header-rows: 1

* - Key
  - Type
  - Description
* - `strings_table`
  - `const char **`
  - Table of string representations corresponding to bit values.
* - `bits`
  - `uint64_t`
  - Bit values to be converted.
:::


---

**Return Value**


Returns a gbuffer containing the concatenated string values of the bit values represented in the `strings_table`.


**Notes**


- The function is useful for converting bit values into a readable format based on string representations.
- Ensure that the `strings_table` contains all possible bit values and their corresponding string representations.


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

