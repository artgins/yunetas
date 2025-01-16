#!/usr/bin/python

from string import Template
import os

template = Template("""

<!-- ============================================================== -->
($_name_())=
# `$_name_()`
<!-- ============================================================== -->

$_description_

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
$_prototype_

```

**Parameters**

$_parameters_

---

**Return Value**

$_return_value_


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

""")

functions_documentation = [
]
functions_documentation.extend([
    {
        "name": "gbuffer_create",
        "description": '''
Create a new gbuffer with the specified initial and maximum sizes.
        ''',
        "prototype": '''
PUBLIC gbuffer_t *gbuffer_create(
    size_t      initial_size,
    size_t      max_size
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `initial_size`
  - `size_t`
  - The initial size of the gbuffer in bytes.

* - `max_size`
  - `size_t`
  - The maximum allowable size of the gbuffer in bytes.
:::
        ''',
        "return_value": '''
Returns a pointer to the newly created [`gbuffer_t *`](gbuffer_t), or `NULL` on failure.
        '''
    },
    {
        "name": "gbuffer_remove",
        "description": '''
Remove and free a gbuffer from memory.
        ''',
        "prototype": '''
PUBLIC int gbuffer_remove(
    gbuffer_t   *gbuffer
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer to be removed and freed.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on error.
        '''
    },
    {
        "name": "gbuffer_incref",
        "description": '''
Increase the reference count of a gbuffer.
        ''',
        "prototype": '''
PUBLIC void gbuffer_incref(
    gbuffer_t   *gbuffer
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer whose reference count will be increased.
:::
        ''',
        "return_value": '''
No return value. This function modifies the reference count.
        '''
    },
    {
        "name": "gbuffer_decref",
        "description": '''
Decrease the reference count of a gbuffer and free it if the count reaches zero.
        ''',
        "prototype": '''
PUBLIC void gbuffer_decref(
    gbuffer_t   *gbuffer
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer whose reference count will be decreased.
:::
        ''',
        "return_value": '''
No return value. This function modifies the reference count and may free the gbuffer.
        '''
    },
    {
        "name": "gbuffer_cur_rd_pointer",
        "description": '''
Get the current read pointer of the gbuffer.
        ''',
        "prototype": '''
PUBLIC const void *gbuffer_cur_rd_pointer(
    gbuffer_t   *gbuffer
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer whose current read pointer will be retrieved.
:::
        ''',
        "return_value": '''
Returns a pointer to the current read position in the gbuffer.
        '''
    },
    {
        "name": "gbuffer_reset_rd",
        "description": '''
Reset the read pointer of the gbuffer to the beginning.
        ''',
        "prototype": '''
PUBLIC void gbuffer_reset_rd(
    gbuffer_t   *gbuffer
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer whose read pointer will be reset.
:::
        ''',
        "return_value": '''
No return value. This function modifies the read pointer of the gbuffer.
        '''
    }
])

functions_documentation.extend([
    {
        "name": "gbuffer_set_rd_offset",
        "description": '''
Set the read offset of the gbuffer to a specific position.
        ''',
        "prototype": '''
PUBLIC int gbuffer_set_rd_offset(
    gbuffer_t   *gbuffer,
    size_t      offset
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer whose read offset will be set.

* - `offset`
  - `size_t`
  - The new read offset in the gbuffer.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value if the offset is invalid.
        '''
    },
    {
        "name": "gbuffer_ungetc",
        "description": '''
Move the read pointer of the gbuffer backward by one character.
        ''',
        "prototype": '''
PUBLIC int gbuffer_ungetc(
    gbuffer_t   *gbuffer
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer whose read pointer will be moved backward.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value if the operation fails (e.g., already at the beginning).
        '''
    },
    {
        "name": "gbuffer_get_rd_offset",
        "description": '''
Get the current read offset of the gbuffer.
        ''',
        "prototype": '''
PUBLIC size_t gbuffer_get_rd_offset(
    gbuffer_t   *gbuffer
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer whose current read offset will be retrieved.
:::
        ''',
        "return_value": '''
Returns the current read offset of the gbuffer.
        '''
    },
    {
        "name": "gbuffer_get",
        "description": '''
Read a block of data from the gbuffer and advance the read pointer.
        ''',
        "prototype": '''
PUBLIC int gbuffer_get(
    gbuffer_t   *gbuffer,
    void        *data,
    size_t      size
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer from which data will be read.

* - `data`
  - `void *`
  - The buffer where the data will be copied.

* - `size`
  - `size_t`
  - The number of bytes to read from the gbuffer.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value if the operation fails (e.g., insufficient data).
        '''
    },
    {
        "name": "gbuffer_getchar",
        "description": '''
Read a single character from the gbuffer and advance the read pointer.
        ''',
        "prototype": '''
PUBLIC int gbuffer_getchar(
    gbuffer_t   *gbuffer
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer from which a character will be read.
:::
        ''',
        "return_value": '''
Returns the character read from the gbuffer, or a negative value if the operation fails.
        '''
    },
    {
        "name": "gbuffer_chunk",
        "description": '''
Retrieve a pointer to the current chunk of data in the gbuffer.
        ''',
        "prototype": '''
PUBLIC const void *gbuffer_chunk(
    gbuffer_t   *gbuffer
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer whose current data chunk will be retrieved.
:::
        ''',
        "return_value": '''
Returns a pointer to the current data chunk in the gbuffer.
        '''
    }
])

functions_documentation.extend([
    {
        "name": "gbuffer_getline",
        "description": '''
Read a line of text from the gbuffer, stopping at a newline character or end of buffer.
        ''',
        "prototype": '''
PUBLIC char *gbuffer_getline(
    gbuffer_t   *gbuffer,
    char        *line,
    size_t      line_size
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer from which the line will be read.

* - `line`
  - `char *`
  - The buffer where the line will be stored.

* - `line_size`
  - `size_t`
  - The size of the buffer for the line.
:::
        ''',
        "return_value": '''
Returns the line read from the gbuffer, or `NULL` if no line can be read.
        '''
    },
    {
        "name": "gbuffer_cur_wr_pointer",
        "description": '''
Get the current write pointer of the gbuffer.
        ''',
        "prototype": '''
PUBLIC void *gbuffer_cur_wr_pointer(
    gbuffer_t   *gbuffer
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer whose current write pointer will be retrieved.
:::
        ''',
        "return_value": '''
Returns a pointer to the current write position in the gbuffer.
        '''
    },
    {
        "name": "gbuffer_reset_wr",
        "description": '''
Reset the write pointer of the gbuffer to the beginning.
        ''',
        "prototype": '''
PUBLIC void gbuffer_reset_wr(
    gbuffer_t   *gbuffer
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer whose write pointer will be reset.
:::
        ''',
        "return_value": '''
No return value. This function modifies the write pointer of the gbuffer.
        '''
    },
    {
        "name": "gbuffer_set_wr",
        "description": '''
Set the write pointer of the gbuffer to a specific position.
        ''',
        "prototype": '''
PUBLIC int gbuffer_set_wr(
    gbuffer_t   *gbuffer,
    size_t      offset
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer whose write pointer will be set.

* - `offset`
  - `size_t`
  - The new write offset in the gbuffer.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value if the offset is invalid.
        '''
    },
    {
        "name": "gbuffer_append",
        "description": '''
Append raw data to the gbuffer.
        ''',
        "prototype": '''
PUBLIC int gbuffer_append(
    gbuffer_t   *gbuffer,
    const void  *data,
    size_t      size
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer to which data will be appended.

* - `data`
  - `const void *`
  - The raw data to append to the gbuffer.

* - `size`
  - `size_t`
  - The size of the data to append, in bytes.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value if the operation fails.
        '''
    },
    {
        "name": "gbuffer_append_string",
        "description": '''
Append a null-terminated string to the gbuffer.
        ''',
        "prototype": '''
PUBLIC int gbuffer_append_string(
    gbuffer_t   *gbuffer,
    const char  *string
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer to which the string will be appended.

* - `string`
  - `const char *`
  - The null-terminated string to append.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value if the operation fails.
        '''
    }
])

functions_documentation.extend([
    {
        "name": "gbuffer_append_char",
        "description": '''
Append a single character to the gbuffer.
        ''',
        "prototype": '''
PUBLIC int gbuffer_append_char(
    gbuffer_t   *gbuffer,
    char        c
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer to which the character will be appended.

* - `c`
  - `char`
  - The character to append to the gbuffer.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value if the operation fails.
        '''
    },
    {
        "name": "gbuffer_append_gbuf",
        "description": '''
Append the contents of one gbuffer to another.
        ''',
        "prototype": '''
PUBLIC int gbuffer_append_gbuf(
    gbuffer_t   *dest,
    gbuffer_t   *src
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `dest`
  - [`gbuffer_t *`](gbuffer_t)
  - The destination gbuffer to which data will be appended.

* - `src`
  - [`gbuffer_t *`](gbuffer_t)
  - The source gbuffer whose contents will be appended.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value if the operation fails.
        '''
    },
    {
        "name": "gbuffer_printf",
        "description": '''
Write formatted data to the gbuffer using a printf-style format string.
        ''',
        "prototype": '''
PUBLIC int gbuffer_printf(
    gbuffer_t   *gbuffer,
    const char  *format,
    ...
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer where the formatted data will be written.

* - `format`
  - `const char *`
  - The printf-style format string.
:::
        ''',
        "return_value": '''
Returns the number of characters written on success, or a negative value if the operation fails.
        '''
    },
    {
        "name": "gbuffer_vprintf",
        "description": '''
Write formatted data to the gbuffer using a printf-style format string and a `va_list`.
        ''',
        "prototype": '''
PUBLIC int gbuffer_vprintf(
    gbuffer_t   *gbuffer,
    const char  *format,
    va_list     args
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer where the formatted data will be written.

* - `format`
  - `const char *`
  - The printf-style format string.

* - `args`
  - `va_list`
  - The list of arguments for the format string.
:::
        ''',
        "return_value": '''
Returns the number of characters written on success, or a negative value if the operation fails.
        '''
    },
    {
        "name": "gbuffer_head_pointer",
        "description": '''
Retrieve a pointer to the start of the gbuffer.
        ''',
        "prototype": '''
PUBLIC const void *gbuffer_head_pointer(
    gbuffer_t   *gbuffer
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer whose head pointer will be retrieved.
:::
        ''',
        "return_value": '''
Returns a pointer to the start of the gbuffer.
        '''
    },
    {
        "name": "gbuffer_clear",
        "description": '''
Clear the contents of the gbuffer and reset its read and write pointers.
        ''',
        "prototype": '''
PUBLIC void gbuffer_clear(
    gbuffer_t   *gbuffer
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer to clear.
:::
        ''',
        "return_value": '''
No return value. This function clears the contents of the gbuffer.
        '''
    }
])

functions_documentation.extend([
    {
        "name": "gbuffer_leftbytes",
        "description": '''
Get the number of unread bytes remaining in the gbuffer.
        ''',
        "prototype": '''
PUBLIC size_t gbuffer_leftbytes(
    gbuffer_t   *gbuffer
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer to query for remaining unread bytes.
:::
        ''',
        "return_value": '''
Returns the number of unread bytes left in the gbuffer.
        '''
    },
    {
        "name": "gbuffer_totalbytes",
        "description": '''
Get the total number of bytes in the gbuffer.
        ''',
        "prototype": '''
PUBLIC size_t gbuffer_totalbytes(
    gbuffer_t   *gbuffer
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer to query for the total number of bytes.
:::
        ''',
        "return_value": '''
Returns the total number of bytes in the gbuffer.
        '''
    },
    {
        "name": "gbuffer_freebytes",
        "description": '''
Get the number of free bytes remaining in the gbuffer.
        ''',
        "prototype": '''
PUBLIC size_t gbuffer_freebytes(
    gbuffer_t   *gbuffer
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer to query for the number of free bytes.
:::
        ''',
        "return_value": '''
Returns the number of free bytes remaining in the gbuffer.
        '''
    },
    {
        "name": "gbuffer_setlabel",
        "description": '''
Set a label for the gbuffer for identification or debugging purposes.
        ''',
        "prototype": '''
PUBLIC void gbuffer_setlabel(
    gbuffer_t   *gbuffer,
    const char  *label
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer to label.

* - `label`
  - `const char *`
  - The label to assign to the gbuffer.
:::
        ''',
        "return_value": '''
No return value. This function sets the label for the gbuffer.
        '''
    },
    {
        "name": "gbuffer_getlabel",
        "description": '''
Retrieve the label of the gbuffer.
        ''',
        "prototype": '''
PUBLIC const char *gbuffer_getlabel(
    gbuffer_t   *gbuffer
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer whose label will be retrieved.
:::
        ''',
        "return_value": '''
Returns the label of the gbuffer as a null-terminated string, or `NULL` if no label is set.
        '''
    },
    {
        "name": "gbuffer_setmark",
        "description": '''
Set a mark at the current read position of the gbuffer.
        ''',
        "prototype": '''
PUBLIC void gbuffer_setmark(
    gbuffer_t   *gbuffer
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer where the mark will be set at the current read position.
:::
        ''',
        "return_value": '''
No return value. This function sets a mark in the gbuffer.
        '''
    }
])

functions_documentation.extend([
    {
        "name": "gbuffer_getmark",
        "description": '''
Retrieve the mark previously set in the gbuffer.
        ''',
        "prototype": '''
PUBLIC size_t gbuffer_getmark(
    gbuffer_t   *gbuffer
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer whose mark will be retrieved.
:::
        ''',
        "return_value": '''
Returns the position of the mark in the gbuffer, or `0` if no mark is set.
        '''
    },
    {
        "name": "gbuffer_serialize",
        "description": '''
Serialize a gbuffer into a JSON-compatible format.
        ''',
        "prototype": '''
PUBLIC json_t *gbuffer_serialize(
    gbuffer_t   *gbuffer
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer to be serialized.
:::
        ''',
        "return_value": '''
Returns a JSON object representing the serialized gbuffer, or `NULL` on failure.
        '''
    },
    {
        "name": "gbuffer_deserialize",
        "description": '''
Deserialize a JSON object into a gbuffer.
        ''',
        "prototype": '''
PUBLIC gbuffer_t *gbuffer_deserialize(
    json_t      *json
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `json`
  - `json_t *`
  - The JSON object to deserialize into a gbuffer.
:::
        ''',
        "return_value": '''
Returns a newly created [`gbuffer_t *`](gbuffer_t) from the JSON object, or `NULL` on failure.
        '''
    },
    {
        "name": "gbuffer_string_to_base64",
        "description": '''
Convert a string into a gbuffer containing its Base64 representation.
        ''',
        "prototype": '''
PUBLIC gbuffer_t *gbuffer_string_to_base64(
    const char  *string
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `string`
  - `const char *`
  - The null-terminated string to encode into Base64.
:::
        ''',
        "return_value": '''
Returns a [`gbuffer_t *`](gbuffer_t) containing the Base64-encoded string, or `NULL` on failure.
        '''
    },
    {
        "name": "gbuffer_base64_to_string",
        "description": '''
Decode a Base64-encoded gbuffer into a string.
        ''',
        "prototype": '''
PUBLIC char *gbuffer_base64_to_string(
    gbuffer_t   *gbuffer
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer containing the Base64-encoded data to decode.
:::
        ''',
        "return_value": '''
Returns a null-terminated string containing the decoded data, or `NULL` on failure.
        '''
    },
    {
        "name": "json2gbuf",
        "description": '''
Convert a JSON object into a gbuffer.
        ''',
        "prototype": '''
PUBLIC gbuffer_t *json2gbuf(
    json_t      *json
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `json`
  - `json_t *`
  - The JSON object to convert into a gbuffer.
:::
        ''',
        "return_value": '''
Returns a newly created [`gbuffer_t *`](gbuffer_t) containing the serialized JSON data, or `NULL` on failure.
        '''
    }
])

functions_documentation.extend([
    {
        "name": "gbuf2json",
        "description": '''
Convert a gbuffer into a JSON object.
        ''',
        "prototype": '''
PUBLIC json_t *gbuf2json(
    gbuffer_t   *gbuffer,
    BOOL        expand
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer to convert into a JSON object.

* - `expand`
  - `BOOL`
  - If `TRUE`, the gbuffer will be expanded during conversion; otherwise, it will remain as is.
:::
        ''',
        "return_value": '''
Returns a JSON object created from the gbuffer, or `NULL` on failure.
        '''
    },
    {
        "name": "gobj_trace_dump_gbuf",
        "description": '''
Dump the contents of a gbuffer as trace information.
        ''',
        "prototype": '''
PUBLIC void gobj_trace_dump_gbuf(
    gbuffer_t   *gbuffer,
    const char  *format,
    ...
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer whose contents will be dumped.

* - `format`
  - `const char *`
  - A printf-style format string for the trace message.

* - `...`
  - `varargs`
  - Additional arguments for the format string.
:::
        ''',
        "return_value": '''
No return value. This function logs the contents of the gbuffer as trace information.
        '''
    },
    {
        "name": "gobj_trace_dump_full_gbuf",
        "description": '''
Dump the full contents of a gbuffer as trace information, including metadata.
        ''',
        "prototype": '''
PUBLIC void gobj_trace_dump_full_gbuf(
    gbuffer_t   *gbuffer,
    const char  *format,
    ...
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer whose full contents will be dumped.

* - `format`
  - `const char *`
  - A printf-style format string for the trace message.

* - `...`
  - `varargs`
  - Additional arguments for the format string.
:::
        ''',
        "return_value": '''
No return value. This function logs the full contents of the gbuffer, including metadata, as trace information.
        '''
    }
])


# Loop through the list of names and create a file for each
for fn in functions_documentation:
    # Substitute the variable in the template

    formatted_text = template.substitute(
        _name_          = fn['name'],
        _description_   = fn['description'],
        _prototype_     = fn['prototype'],
        _parameters_    = fn['parameters'],
        _return_value_  = fn['return_value']
    )
    # Create a unique file name for each name
    file_name = f"{fn['name'].lower()}.md"

    # Check if the file already exists
    if os.path.exists(file_name):
        print(f"File {file_name} already exists. =============================> Skipping...")
        continue  # Skip to the next name

    # Write the formatted text to the file
    with open(file_name, "w") as file:
        file.write(formatted_text)

    print(f"File created: {file_name}")
