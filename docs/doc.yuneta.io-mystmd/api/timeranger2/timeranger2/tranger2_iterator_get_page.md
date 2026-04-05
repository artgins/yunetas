<!-- ============================================================== -->
(tranger2_iterator_get_page())=
# `tranger2_iterator_get_page()`
<!-- ============================================================== -->

Retrieves a page of records from an iterator in the TimeRanger database. The function returns a JSON object containing the total number of rows, the number of pages based on the specified limit, and the list of retrieved records.

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
json_t *tranger2_iterator_get_page(
    json_t *tranger,
    json_t *iterator,
    json_int_t from_rowid,    // based 1
    size_t limit,
    BOOL backward
);
```

**Parameters**

::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `tranger`
  - `json_t *`
  - A pointer to the TimeRanger database instance.

* - `iterator`
  - `json_t *`
  - A pointer to the iterator from which records will be retrieved.

* - `from_rowid`
  - `json_int_t`
  - The starting row ID for retrieving records, based on a 1-based index.

* - `limit`
  - `size_t`
  - The maximum number of records to retrieve in the page.

* - `backward`
  - `BOOL`
  - If `TRUE`, records are retrieved in reverse order; otherwise, they are retrieved in forward order.
:::

---

**Return Value**

A JSON object containing the retrieved records, total row count, and pagination details. The caller owns the returned JSON object and must manage its memory.

**Notes**

This function is useful for paginating through records in an iterator. The `from_rowid` parameter determines the starting point, and the `limit` parameter controls the number of records retrieved per page.

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
