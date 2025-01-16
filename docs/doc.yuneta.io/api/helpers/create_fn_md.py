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
        "name": "newdir",
        "description": '''
Create a new directory at the specified path.
        ''',
        "prototype": '''
PUBLIC int newdir(
    const char  *path,
    int         mode
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `path`
  - `const char *`
  - The path of the directory to create.

* - `mode`
  - `int`
  - The permissions mode for the new directory.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on failure.
        '''
    },
    {
        "name": "newfile",
        "description": '''
Create a new file at the specified path.
        ''',
        "prototype": '''
PUBLIC int newfile(
    const char  *path,
    int         mode
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `path`
  - `const char *`
  - The path of the file to create.

* - `mode`
  - `int`
  - The permissions mode for the new file.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on failure.
        '''
    },
    {
        "name": "open_exclusive",
        "description": '''
Open a file in exclusive mode, creating it if it does not exist.
        ''',
        "prototype": '''
PUBLIC int open_exclusive(
    const char  *path,
    int         flags,
    int         mode
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `path`
  - `const char *`
  - The path of the file to open.

* - `flags`
  - `int`
  - The flags for opening the file.

* - `mode`
  - `int`
  - The permissions mode for the file, used if the file is created.
:::
        ''',
        "return_value": '''
Returns the file descriptor on success, or a negative value on failure.
        '''
    },
    {
        "name": "filesize",
        "description": '''
Get the size of a file in bytes.
        ''',
        "prototype": '''
PUBLIC off_t filesize(
    const char  *path
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `path`
  - `const char *`
  - The path of the file to query.
:::
        ''',
        "return_value": '''
Returns the size of the file in bytes, or a negative value on failure.
        '''
    },
    {
        "name": "filesize2",
        "description": '''
Get the size of a file in bytes using a file descriptor.
        ''',
        "prototype": '''
PUBLIC off_t filesize2(
    int         fd
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `fd`
  - `int`
  - The file descriptor of the file to query.
:::
        ''',
        "return_value": '''
Returns the size of the file in bytes, or a negative value on failure.
        '''
    },
    {
        "name": "lock_file",
        "description": '''
Lock a file using a file descriptor.
        ''',
        "prototype": '''
PUBLIC int lock_file(
    int         fd,
    int         lock_type
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `fd`
  - `int`
  - The file descriptor of the file to lock.

* - `lock_type`
  - `int`
  - The type of lock to apply (e.g., shared or exclusive).
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on failure.
        '''
    }
])

functions_documentation.extend([
    {
        "name": "unlock_file",
        "description": '''
Unlock a file using a file descriptor.
        ''',
        "prototype": '''
PUBLIC int unlock_file(
    int         fd
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `fd`
  - `int`
  - The file descriptor of the file to unlock.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on failure.
        '''
    },
    {
        "name": "is_regular_file",
        "description": '''
Check if the specified path points to a regular file.
        ''',
        "prototype": '''
PUBLIC BOOL is_regular_file(
    const char  *path
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `path`
  - `const char *`
  - The path to check.
:::
        ''',
        "return_value": '''
Returns `TRUE` if the path points to a regular file, otherwise returns `FALSE`.
        '''
    },
    {
        "name": "is_directory",
        "description": '''
Check if the specified path points to a directory.
        ''',
        "prototype": '''
PUBLIC BOOL is_directory(
    const char  *path
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `path`
  - `const char *`
  - The path to check.
:::
        ''',
        "return_value": '''
Returns `TRUE` if the path points to a directory, otherwise returns `FALSE`.
        '''
    },
    {
        "name": "file_size",
        "description": '''
Retrieve the size of a file at the specified path in bytes.
        ''',
        "prototype": '''
PUBLIC off_t file_size(
    const char  *path
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `path`
  - `const char *`
  - The path of the file to query.
:::
        ''',
        "return_value": '''
Returns the size of the file in bytes, or a negative value on failure.
        '''
    },
    {
        "name": "file_permission",
        "description": '''
Check the permissions of a file at the specified path.
        ''',
        "prototype": '''
PUBLIC int file_permission(
    const char  *path,
    int         mode
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `path`
  - `const char *`
  - The path of the file to check.

* - `mode`
  - `int`
  - The permission mode to check (e.g., readable, writable).
:::
        ''',
        "return_value": '''
Returns `0` if the file has the specified permissions, otherwise returns a negative value.
        '''
    },
    {
        "name": "file_exists",
        "description": '''
Check if a file exists at the specified path.
        ''',
        "prototype": '''
PUBLIC BOOL file_exists(
    const char  *path
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `path`
  - `const char *`
  - The path to check.
:::
        ''',
        "return_value": '''
Returns `TRUE` if the file exists, otherwise returns `FALSE`.
        '''
    }
])

functions_documentation.extend([
    {
        "name": "subdir_exists",
        "description": '''
Check if a subdirectory exists within a specified path.
        ''',
        "prototype": '''
PUBLIC BOOL subdir_exists(
    const char  *path,
    const char  *subdir
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `path`
  - `const char *`
  - The parent directory to search in.

* - `subdir`
  - `const char *`
  - The name of the subdirectory to check for.
:::
        ''',
        "return_value": '''
Returns `TRUE` if the subdirectory exists, otherwise returns `FALSE`.
        '''
    },
    {
        "name": "file_remove",
        "description": '''
Remove a file at the specified path.
        ''',
        "prototype": '''
PUBLIC int file_remove(
    const char  *path
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `path`
  - `const char *`
  - The path of the file to remove.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on failure.
        '''
    },
    {
        "name": "mkrdir",
        "description": '''
Create a directory and all its parent directories if they do not exist.
        ''',
        "prototype": '''
PUBLIC int mkrdir(
    const char  *path,
    int         mode
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `path`
  - `const char *`
  - The path of the directory to create.

* - `mode`
  - `int`
  - The permissions mode for the directory.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on failure.
        '''
    },
    {
        "name": "rmrdir",
        "description": '''
Remove a directory and all its contents.
        ''',
        "prototype": '''
PUBLIC int rmrdir(
    const char  *path
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `path`
  - `const char *`
  - The path of the directory to remove.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on failure.
        '''
    },
    {
        "name": "rmrcontentdir",
        "description": '''
Remove the contents of a directory without deleting the directory itself.
        ''',
        "prototype": '''
PUBLIC int rmrcontentdir(
    const char  *path
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `path`
  - `const char *`
  - The path of the directory whose contents will be removed.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on failure.
        '''
    },
    {
        "name": "delete_right_char",
        "description": '''
Delete a specified character from the right end of a string.
        ''',
        "prototype": '''
PUBLIC char *delete_right_char(
    char        *string,
    char         c
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
  - `char *`
  - The string to modify.

* - `c`
  - `char`
  - The character to remove from the right end of the string.
:::
        ''',
        "return_value": '''
Returns the modified string.
        '''
    }
])

functions_documentation.extend([
    {
        "name": "delete_left_char",
        "description": '''
Delete a specified character from the left end of a string.
        ''',
        "prototype": '''
PUBLIC char *delete_left_char(
    char        *string,
    char         c
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
  - `char *`
  - The string to modify.

* - `c`
  - `char`
  - The character to remove from the left end of the string.
:::
        ''',
        "return_value": '''
Returns the modified string.
        '''
    },
    {
        "name": "build_path",
        "description": '''
Build a path string by combining multiple segments.
        ''',
        "prototype": '''
PUBLIC char *build_path(
    const char  *separator,
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

* - `separator`
  - `const char *`
  - The separator to use between path segments.

* - `...`
  - `varargs`
  - The path segments to combine, ending with `NULL`.
:::
        ''',
        "return_value": '''
Returns the combined path string, or `NULL` on failure.
        '''
    },
    {
        "name": "get_last_segment",
        "description": '''
Get the last segment of a path string.
        ''',
        "prototype": '''
PUBLIC const char *get_last_segment(
    const char  *path,
    char         separator
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `path`
  - `const char *`
  - The path string to analyze.

* - `separator`
  - `char`
  - The character used to separate path segments.
:::
        ''',
        "return_value": '''
Returns a pointer to the last segment in the path string.
        '''
    },
    {
        "name": "pop_last_segment",
        "description": '''
Remove the last segment from a path string.
        ''',
        "prototype": '''
PUBLIC char *pop_last_segment(
    char        *path,
    char         separator
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `path`
  - `char *`
  - The path string to modify.

* - `separator`
  - `char`
  - The character used to separate path segments.
:::
        ''',
        "return_value": '''
Returns the modified path string with the last segment removed.
        '''
    },
    {
        "name": "helper_quote2doublequote",
        "description": '''
Convert single quotes in a string to double quotes.
        ''',
        "prototype": '''
PUBLIC char *helper_quote2doublequote(
    char        *string
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
  - `char *`
  - The string to modify.
:::
        ''',
        "return_value": '''
Returns the modified string with single quotes converted to double quotes.
        '''
    },
    {
        "name": "helper_doublequote2quote",
        "description": '''
Convert double quotes in a string to single quotes.
        ''',
        "prototype": '''
PUBLIC char *helper_doublequote2quote(
    char        *string
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
  - `char *`
  - The string to modify.
:::
        ''',
        "return_value": '''
Returns the modified string with double quotes converted to single quotes.
        '''
    }
])

functions_documentation.extend([
    {
        "name": "all_numbers",
        "description": '''
Check if a string contains only numeric characters.
        ''',
        "prototype": '''
PUBLIC BOOL all_numbers(
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
  - The string to check.
:::
        ''',
        "return_value": '''
Returns `TRUE` if the string contains only numeric characters, otherwise returns `FALSE`.
        '''
    },
    {
        "name": "nice_size",
        "description": '''
Convert a file size in bytes to a human-readable string with appropriate units (e.g., KB, MB).
        ''',
        "prototype": '''
PUBLIC const char *nice_size(
    off_t       size
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `size`
  - `off_t`
  - The file size in bytes to convert.
:::
        ''',
        "return_value": '''
Returns a human-readable string representation of the file size.
        '''
    },
    {
        "name": "delete_right_blanks",
        "description": '''
Remove trailing whitespace characters from a string.
        ''',
        "prototype": '''
PUBLIC char *delete_right_blanks(
    char        *string
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
  - `char *`
  - The string to modify.
:::
        ''',
        "return_value": '''
Returns the modified string with trailing whitespace removed.
        '''
    },
    {
        "name": "delete_left_blanks",
        "description": '''
Remove leading whitespace characters from a string.
        ''',
        "prototype": '''
PUBLIC char *delete_left_blanks(
    char        *string
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
  - `char *`
  - The string to modify.
:::
        ''',
        "return_value": '''
Returns the modified string with leading whitespace removed.
        '''
    },
    {
        "name": "left_justify",
        "description": '''
Remove both leading and trailing whitespace characters from a string.
        ''',
        "prototype": '''
PUBLIC char *left_justify(
    char        *string
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
  - `char *`
  - The string to modify.
:::
        ''',
        "return_value": '''
Returns the modified string with leading and trailing whitespace removed.
        '''
    },
    {
        "name": "strntoupper",
        "description": '''
Convert a string to uppercase up to a specified length.
        ''',
        "prototype": '''
PUBLIC char *strntoupper(
    char        *dest,
    const char  *src,
    int         n
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
  - `char *`
  - The destination buffer for the uppercase string.

* - `src`
  - `const char *`
  - The source string to convert.

* - `n`
  - `int`
  - The maximum number of characters to convert.
:::
        ''',
        "return_value": '''
Returns the destination buffer containing the uppercase string.
        '''
    }
])

functions_documentation.extend([
    {
        "name": "strntolower",
        "description": '''
Convert a string to lowercase up to a specified length.
        ''',
        "prototype": '''
PUBLIC char *strntolower(
    char        *dest,
    const char  *src,
    int         n
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
  - `char *`
  - The destination buffer for the lowercase string.

* - `src`
  - `const char *`
  - The source string to convert.

* - `n`
  - `int`
  - The maximum number of characters to convert.
:::
        ''',
        "return_value": '''
Returns the destination buffer containing the lowercase string.
        '''
    },
    {
        "name": "translate_string",
        "description": '''
Translate all occurrences of a specified character in a string to another character.
        ''',
        "prototype": '''
PUBLIC char *translate_string(
    char        *string,
    char         from,
    char         to
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
  - `char *`
  - The string to modify.

* - `from`
  - `char`
  - The character to replace.

* - `to`
  - `char`
  - The character to replace it with.
:::
        ''',
        "return_value": '''
Returns the modified string with characters replaced.
        '''
    },
    {
        "name": "change_char",
        "description": '''
Change a specific character in a string to another character.
        ''',
        "prototype": '''
PUBLIC void change_char(
    char        *string,
    char         from,
    char         to
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
  - `char *`
  - The string to modify.

* - `from`
  - `char`
  - The character to replace.

* - `to`
  - `char`
  - The character to replace it with.
:::
        ''',
        "return_value": '''
No return value. The function modifies the string in place.
        '''
    },
    {
        "name": "get_parameter",
        "description": '''
Extract a parameter value from a query string.
        ''',
        "prototype": '''
PUBLIC const char *get_parameter(
    const char  *query_string,
    const char  *parameter_name
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `query_string`
  - `const char *`
  - The query string to search.

* - `parameter_name`
  - `const char *`
  - The name of the parameter to retrieve.
:::
        ''',
        "return_value": '''
Returns a pointer to the value of the specified parameter, or `NULL` if not found.
        '''
    },
    {
        "name": "get_key_value_parameter",
        "description": '''
Parse a key-value pair parameter from a string.
        ''',
        "prototype": '''
PUBLIC BOOL get_key_value_parameter(
    const char  *parameter,
    char        *key,
    int          key_size,
    char        *value,
    int          value_size
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `parameter`
  - `const char *`
  - The string containing the key-value pair.

* - `key`
  - `char *`
  - The buffer to store the extracted key.

* - `key_size`
  - `int`
  - The size of the key buffer.

* - `value`
  - `char *`
  - The buffer to store the extracted value.

* - `value_size`
  - `int`
  - The size of the value buffer.
:::
        ''',
        "return_value": '''
Returns `TRUE` if the key-value pair was successfully parsed, otherwise returns `FALSE`.
        '''
    },
    {
        "name": "split2",
        "description": '''
Split a string into two parts based on a delimiter.
        ''',
        "prototype": '''
PUBLIC BOOL split2(
    const char  *string,
    char         delimiter,
    char        *part1,
    int          part1_size,
    char        *part2,
    int          part2_size
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
  - The string to split.

* - `delimiter`
  - `char`
  - The character used to split the string.

* - `part1`
  - `char *`
  - The buffer to store the first part of the split string.

* - `part1_size`
  - `int`
  - The size of the buffer for the first part.

* - `part2`
  - `char *`
  - The buffer to store the second part of the split string.

* - `part2_size`
  - `int`
  - The size of the buffer for the second part.
:::
        ''',
        "return_value": '''
Returns `TRUE` if the string was successfully split into two parts, otherwise returns `FALSE`.
        '''
    }
])

functions_documentation.extend([
    {
        "name": "split_free2",
        "description": '''
Free the memory allocated for two strings that were split.
        ''',
        "prototype": '''
PUBLIC void split_free2(
    char        *part1,
    char        *part2
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `part1`
  - `char *`
  - The first string to free.

* - `part2`
  - `char *`
  - The second string to free.
:::
        ''',
        "return_value": '''
No return value. This function frees the memory for the two strings.
        '''
    },
    {
        "name": "split3",
        "description": '''
Split a string into three parts based on two delimiters.
        ''',
        "prototype": '''
PUBLIC BOOL split3(
    const char  *string,
    char         delimiter1,
    char         delimiter2,
    char        *part1,
    int          part1_size,
    char        *part2,
    int          part2_size,
    char        *part3,
    int          part3_size
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
  - The string to split.

* - `delimiter1`
  - `char`
  - The first delimiter used to split the string.

* - `delimiter2`
  - `char`
  - The second delimiter used to split the string.

* - `part1`
  - `char *`
  - The buffer to store the first part of the split string.

* - `part1_size`
  - `int`
  - The size of the buffer for the first part.

* - `part2`
  - `char *`
  - The buffer to store the second part of the split string.

* - `part2_size`
  - `int`
  - The size of the buffer for the second part.

* - `part3`
  - `char *`
  - The buffer to store the third part of the split string.

* - `part3_size`
  - `int`
  - The size of the buffer for the third part.
:::
        ''',
        "return_value": '''
Returns `TRUE` if the string was successfully split into three parts, otherwise returns `FALSE`.
        '''
    },
    {
        "name": "split_free3",
        "description": '''
Free the memory allocated for three strings that were split.
        ''',
        "prototype": '''
PUBLIC void split_free3(
    char        *part1,
    char        *part2,
    char        *part3
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `part1`
  - `char *`
  - The first string to free.

* - `part2`
  - `char *`
  - The second string to free.

* - `part3`
  - `char *`
  - The third string to free.
:::
        ''',
        "return_value": '''
No return value. This function frees the memory for the three strings.
        '''
    },
    {
        "name": "str_concat",
        "description": '''
Concatenate two strings into a newly allocated string.
        ''',
        "prototype": '''
PUBLIC char *str_concat(
    const char  *str1,
    const char  *str2
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `str1`
  - `const char *`
  - The first string to concatenate.

* - `str2`
  - `const char *`
  - The second string to concatenate.
:::
        ''',
        "return_value": '''
Returns a newly allocated string containing the concatenation of `str1` and `str2`, or `NULL` on failure.
        '''
    },
    {
        "name": "str_concat3",
        "description": '''
Concatenate three strings into a newly allocated string.
        ''',
        "prototype": '''
PUBLIC char *str_concat3(
    const char  *str1,
    const char  *str2,
    const char  *str3
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `str1`
  - `const char *`
  - The first string to concatenate.

* - `str2`
  - `const char *`
  - The second string to concatenate.

* - `str3`
  - `const char *`
  - The third string to concatenate.
:::
        ''',
        "return_value": '''
Returns a newly allocated string containing the concatenation of `str1`, `str2`, and `str3`, or `NULL` on failure.
        '''
    },
    {
        "name": "str_concat_free",
        "description": '''
Concatenate two strings into a newly allocated string and free the input strings.
        ''',
        "prototype": '''
PUBLIC char *str_concat_free(
    char        *str1,
    char        *str2
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `str1`
  - `char *`
  - The first string to concatenate and free.

* - `str2`
  - `char *`
  - The second string to concatenate and free.
:::
        ''',
        "return_value": '''
Returns a newly allocated string containing the concatenation of `str1` and `str2`, or `NULL` on failure.
        '''
    }
])

functions_documentation.extend([
    {
        "name": "idx_in_list",
        "description": '''
Check if a value exists in a list and return its index.
        ''',
        "prototype": '''
PUBLIC int idx_in_list(
    const char  *list[],
    const char  *value
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `list`
  - `const char *[]`
  - The list of strings to search.

* - `value`
  - `const char *`
  - The value to search for in the list.
:::
        ''',
        "return_value": '''
Returns the index of the value in the list, or `-1` if the value is not found.
        '''
    },
    {
        "name": "str_in_list",
        "description": '''
Check if a value exists in a list of strings.
        ''',
        "prototype": '''
PUBLIC BOOL str_in_list(
    const char  *list[],
    const char  *value
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `list`
  - `const char *[]`
  - The list of strings to search.

* - `value`
  - `const char *`
  - The value to search for in the list.
:::
        ''',
        "return_value": '''
Returns `TRUE` if the value exists in the list, otherwise returns `FALSE`.
        '''
    },
    {
        "name": "json_config",
        "description": '''
Load a JSON configuration file and return its contents. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC json_t *json_config(
    const char  *path,
    int          verbose
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `path`
  - `const char *`
  - The path to the JSON configuration file.

* - `verbose`
  - `int`
  - The verbosity level for logging messages.
:::
        ''',
        "return_value": '''
Returns a [`json_t *`](json_t) object containing the configuration data, or `NULL` on failure.
        '''
    },
    {
        "name": "load_persistent_json",
        "description": '''
Load a persistent JSON object from a file. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC json_t *load_persistent_json(
    const char  *path,
    int          verbose
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `path`
  - `const char *`
  - The path to the persistent JSON file.

* - `verbose`
  - `int`
  - The verbosity level for logging messages.
:::
        ''',
        "return_value": '''
Returns a [`json_t *`](json_t) object containing the persistent data, or `NULL` on failure.
        '''
    },
    {
        "name": "load_json_from_file",
        "description": '''
Load a JSON object from a file. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC json_t *load_json_from_file(
    const char  *path,
    int          verbose
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `path`
  - `const char *`
  - The path to the JSON file.

* - `verbose`
  - `int`
  - The verbosity level for logging messages.
:::
        ''',
        "return_value": '''
Returns a [`json_t *`](json_t) object containing the data from the file, or `NULL` on failure.
        '''
    },
    {
        "name": "save_json_to_file",
        "description": '''
Save a JSON object to a file. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC int save_json_to_file(
    const char  *path,
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

* - `path`
  - `const char *`
  - The path to save the JSON file.

* - `json`
  - [`json_t *`](json_t)
  - The JSON object to save.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on failure.
        '''
    }
])

functions_documentation.extend([
    {
        "name": "create_json_record",
        "description": '''
Create a JSON object record with predefined fields. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC json_t *create_json_record(
    const char  *schema,
    const char  *data
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `schema`
  - `const char *`
  - The schema defining the structure of the record.

* - `data`
  - `const char *`
  - The data to populate the record with.
:::
        ''',
        "return_value": '''
Returns a [`json_t *`](json_t) object representing the JSON record, or `NULL` on failure.
        '''
    },
    {
        "name": "json_record_to_schema",
        "description": '''
Convert a JSON object record to its corresponding schema format. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC json_t *json_record_to_schema(
    json_t      *record
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `record`
  - [`json_t *`](json_t)
  - The JSON record to convert.
:::
        ''',
        "return_value": '''
Returns a [`json_t *`](json_t) object representing the schema of the record, or `NULL` on failure.
        '''
    },
    {
        "name": "bits2jn_strlist",
        "description": '''
Convert a bitmask to a JSON array of strings based on a predefined mapping. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC json_t *bits2jn_strlist(
    uint64_t    bits,
    const char *bit_names[]
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `bits`
  - `uint64_t`
  - The bitmask to convert.

* - `bit_names`
  - `const char *[]`
  - The array of strings corresponding to bit positions.
:::
        ''',
        "return_value": '''
Returns a [`json_t *`](json_t) array of strings corresponding to the active bits in the bitmask.
        '''
    },
    {
        "name": "bits2gbuffer",
        "description": '''
Convert a bitmask to a gbuffer containing a string representation of the active bits. Works with [`gbuffer_t *`](gbuffer_t).
        ''',
        "prototype": '''
PUBLIC gbuffer_t *bits2gbuffer(
    uint64_t    bits,
    const char *bit_names[]
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `bits`
  - `uint64_t`
  - The bitmask to convert.

* - `bit_names`
  - `const char *[]`
  - The array of strings corresponding to bit positions.
:::
        ''',
        "return_value": '''
Returns a [`gbuffer_t *`](gbuffer_t) containing the string representation of the active bits.
        '''
    },
    {
        "name": "strings2bits",
        "description": '''
Convert a JSON array of strings to a bitmask based on a predefined mapping. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC uint64_t strings2bits(
    json_t      *jn_strlist,
    const char *bit_names[]
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `jn_strlist`
  - [`json_t *`](json_t)
  - The JSON array of strings to convert.

* - `bit_names`
  - `const char *[]`
  - The array of strings corresponding to bit positions.
:::
        ''',
        "return_value": '''
Returns a `uint64_t` bitmask representing the active bits from the JSON array.
        '''
    },
    {
        "name": "json_list_str_index",
        "description": '''
Find the index of a string in a JSON array. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC int json_list_str_index(
    json_t      *jn_list,
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

* - `jn_list`
  - [`json_t *`](json_t)
  - The JSON array to search.

* - `string`
  - `const char *`
  - The string to find in the array.
:::
        ''',
        "return_value": '''
Returns the index of the string in the JSON array, or `-1` if not found.
        '''
    }
])

functions_documentation.extend([
    {
        "name": "jn2real",
        "description": '''
Convert a JSON value to a real (floating-point) number. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC double jn2real(
    json_t      *jn,
    double      default_value
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `jn`
  - [`json_t *`](json_t)
  - The JSON value to convert.

* - `default_value`
  - `double`
  - The default value to return if the JSON value cannot be converted to a real.
:::
        ''',
        "return_value": '''
Returns the real value, or `default_value` if the conversion fails.
        '''
    },
    {
        "name": "jn2integer",
        "description": '''
Convert a JSON value to an integer. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC int jn2integer(
    json_t      *jn,
    int         default_value
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `jn`
  - [`json_t *`](json_t)
  - The JSON value to convert.

* - `default_value`
  - `int`
  - The default value to return if the JSON value cannot be converted to an integer.
:::
        ''',
        "return_value": '''
Returns the integer value, or `default_value` if the conversion fails.
        '''
    },
    {
        "name": "jn2string",
        "description": '''
Convert a JSON value to a string. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC const char *jn2string(
    json_t      *jn,
    const char  *default_value
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `jn`
  - [`json_t *`](json_t)
  - The JSON value to convert.

* - `default_value`
  - `const char *`
  - The default value to return if the JSON value cannot be converted to a string.
:::
        ''',
        "return_value": '''
Returns the string value, or `default_value` if the conversion fails.
        '''
    },
    {
        "name": "jn2bool",
        "description": '''
Convert a JSON value to a boolean. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC BOOL jn2bool(
    json_t      *jn,
    BOOL        default_value
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `jn`
  - [`json_t *`](json_t)
  - The JSON value to convert.

* - `default_value`
  - `BOOL`
  - The default value to return if the JSON value cannot be converted to a boolean.
:::
        ''',
        "return_value": '''
Returns the boolean value, or `default_value` if the conversion fails.
        '''
    },
    {
        "name": "cmp_two_simple_json",
        "description": '''
Compare two simple JSON values for equality. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC BOOL cmp_two_simple_json(
    json_t      *jn1,
    json_t      *jn2
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `jn1`
  - [`json_t *`](json_t)
  - The first JSON value to compare.

* - `jn2`
  - [`json_t *`](json_t)
  - The second JSON value to compare.
:::
        ''',
        "return_value": '''
Returns `TRUE` if the JSON values are equal, otherwise returns `FALSE`.
        '''
    },
    {
        "name": "json_is_identical",
        "description": '''
Check if two JSON objects or arrays are identical. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC BOOL json_is_identical(
    json_t      *jn1,
    json_t      *jn2
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `jn1`
  - [`json_t *`](json_t)
  - The first JSON object or array to compare.

* - `jn2`
  - [`json_t *`](json_t)
  - The second JSON object or array to compare.
:::
        ''',
        "return_value": '''
Returns `TRUE` if the JSON objects or arrays are identical, otherwise returns `FALSE`.
        '''
    }
])

functions_documentation.extend([
    {
        "name": "anystring2json",
        "description": '''
Convert any string to a JSON object. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC json_t *anystring2json(
    const char  *string,
    int          verbose
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
  - The string to convert to JSON.

* - `verbose`
  - `int`
  - The verbosity level for logging.
:::
        ''',
        "return_value": '''
Returns a [`json_t *`](json_t) object created from the string, or `NULL` on failure.
        '''
    },
    {
        "name": "string2json",
        "description": '''
Convert a JSON-formatted string to a JSON object. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC json_t *string2json(
    const char  *string,
    int          verbose
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
  - The JSON-formatted string to parse.

* - `verbose`
  - `int`
  - The verbosity level for logging.
:::
        ''',
        "return_value": '''
Returns a [`json_t *`](json_t) object created from the string, or `NULL` on failure.
        '''
    },
    {
        "name": "set_real_precision",
        "description": '''
Set the precision for converting real numbers to strings.
        ''',
        "prototype": '''
PUBLIC void set_real_precision(
    int         precision
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `precision`
  - `int`
  - The number of decimal places for real number precision.
:::
        ''',
        "return_value": '''
No return value. This function sets the global precision for real numbers.
        '''
    },
    {
        "name": "get_real_precision",
        "description": '''
Get the current precision setting for converting real numbers to strings.
        ''',
        "prototype": '''
PUBLIC int get_real_precision(void);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `-`
  - `-`
  - This function does not take any parameters.
:::
        ''',
        "return_value": '''
Returns the current precision setting for real numbers.
        '''
    },
    {
        "name": "json2str",
        "description": '''
Convert a JSON object to a formatted JSON string. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC char *json2str(
    json_t      *jn
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `jn`
  - [`json_t *`](json_t)
  - The JSON object to convert to a string.
:::
        ''',
        "return_value": '''
Returns a newly allocated string containing the formatted JSON representation, or `NULL` on failure.
        '''
    },
    {
        "name": "json2uglystr",
        "description": '''
Convert a JSON object to a compact, unformatted JSON string. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC char *json2uglystr(
    json_t      *jn
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `jn`
  - [`json_t *`](json_t)
  - The JSON object to convert to a string.
:::
        ''',
        "return_value": '''
Returns a newly allocated string containing the compact JSON representation, or `NULL` on failure.
        '''
    }
])

functions_documentation.extend([
    {
        "name": "json_check_refcounts",
        "description": '''
Check the reference counts of JSON objects to detect anomalies. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC void json_check_refcounts(void);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `-`
  - `-`
  - This function does not take any parameters.
:::
        ''',
        "return_value": '''
No return value. This function checks the reference counts of all JSON objects.
        '''
    },
    {
        "name": "json_print_refcounts",
        "description": '''
Print the reference counts of all JSON objects for debugging purposes. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC void json_print_refcounts(void);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `-`
  - `-`
  - This function does not take any parameters.
:::
        ''',
        "return_value": '''
No return value. This function prints the reference counts of JSON objects to the output.
        '''
    },
    {
        "name": "json_str_in_list",
        "description": '''
Check if a string exists in a JSON array of strings. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC BOOL json_str_in_list(
    json_t      *jn_list,
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

* - `jn_list`
  - [`json_t *`](json_t)
  - The JSON array to search.

* - `string`
  - `const char *`
  - The string to find in the array.
:::
        ''',
        "return_value": '''
Returns `TRUE` if the string exists in the JSON array, otherwise returns `FALSE`.
        '''
    },
    {
        "name": "walk_dir_tree",
        "description": '''
Recursively walk through a directory tree and process each file and subdirectory.
        ''',
        "prototype": '''
PUBLIC int walk_dir_tree(
    const char  *path,
    int (*callback)(const char *path, void *user_data),
    void        *user_data
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `path`
  - `const char *`
  - The root directory to start walking.

* - `callback`
  - `int (*)(const char *, void *)`
  - A function pointer to process each file or subdirectory.

* - `user_data`
  - `void *`
  - User-defined data passed to the callback function.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on failure.
        '''
    },
    {
        "name": "get_number_of_files",
        "description": '''
Get the number of files in a specified directory.
        ''',
        "prototype": '''
PUBLIC int get_number_of_files(
    const char  *path
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `path`
  - `const char *`
  - The path to the directory to count files in.
:::
        ''',
        "return_value": '''
Returns the number of files in the directory, or a negative value on failure.
        '''
    },
    {
        "name": "get_ordered_filename_array",
        "description": '''
Retrieve an ordered array of filenames from a directory.
        ''',
        "prototype": '''
PUBLIC char **get_ordered_filename_array(
    const char  *path,
    int         *file_count
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `path`
  - `const char *`
  - The path to the directory.

* - `file_count`
  - `int *`
  - A pointer to store the number of files in the array.
:::
        ''',
        "return_value": '''
Returns an ordered array of filenames, or `NULL` on failure. The caller is responsible for freeing the array.
        '''
    }
])

functions_documentation.extend([
    {
        "name": "free_ordered_filename_array",
        "description": '''
Free the memory allocated for an ordered array of filenames.
        ''',
        "prototype": '''
PUBLIC void free_ordered_filename_array(
    char        **filename_array,
    int           file_count
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `filename_array`
  - `char **`
  - The array of filenames to free.

* - `file_count`
  - `int`
  - The number of files in the array.
:::
        ''',
        "return_value": '''
No return value. This function frees the memory allocated for the filename array.
        '''
    },
    {
        "name": "tdump",
        "description": '''
Dump the contents of memory in a human-readable format.
        ''',
        "prototype": '''
PUBLIC void tdump(
    const void  *data,
    size_t       size
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `data`
  - `const void *`
  - The memory block to dump.

* - `size`
  - `size_t`
  - The size of the memory block in bytes.
:::
        ''',
        "return_value": '''
No return value. This function outputs the memory dump to the standard output or log.
        '''
    },
    {
        "name": "tdump2json",
        "description": '''
Convert a memory dump into a JSON object. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC json_t *tdump2json(
    const void  *data,
    size_t       size
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `data`
  - `const void *`
  - The memory block to convert.

* - `size`
  - `size_t`
  - The size of the memory block in bytes.
:::
        ''',
        "return_value": '''
Returns a [`json_t *`](json_t) object representing the memory dump, or `NULL` on failure.
        '''
    },
    {
        "name": "print_json2",
        "description": '''
Pretty-print a JSON object to the console. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC void print_json2(
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
  - [`json_t *`](json_t)
  - The JSON object to print.
:::
        ''',
        "return_value": '''
No return value. This function outputs the JSON object to the console in a readable format.
        '''
    },
    {
        "name": "debug_json",
        "description": '''
Debug and print the contents of a JSON object. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC void debug_json(
    json_t      *json,
    const char  *title
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
  - [`json_t *`](json_t)
  - The JSON object to debug.

* - `title`
  - `const char *`
  - An optional title for the debug output.
:::
        ''',
        "return_value": '''
No return value. This function outputs debug information for the JSON object.
        '''
    },
    {
        "name": "current_timestamp",
        "description": '''
Get the current timestamp in seconds since the epoch.
        ''',
        "prototype": '''
PUBLIC double current_timestamp(void);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `-`
  - `-`
  - This function does not take any parameters.
:::
        ''',
        "return_value": '''
Returns the current timestamp as a `double` value in seconds since the epoch.
        '''
    }
])

functions_documentation.extend([
    {
        "name": "tm2timestamp",
        "description": '''
Convert a `struct tm` to a timestamp in seconds since the epoch.
        ''',
        "prototype": '''
PUBLIC double tm2timestamp(
    const struct tm *tm
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `tm`
  - `const struct tm *`
  - A pointer to the `struct tm` to convert.
:::
        ''',
        "return_value": '''
Returns the timestamp as a `double` value in seconds since the epoch.
        '''
    },
    {
        "name": "t2timestamp",
        "description": '''
Convert a time value in seconds since the epoch to a `double` timestamp.
        ''',
        "prototype": '''
PUBLIC double t2timestamp(
    time_t t
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `t`
  - `time_t`
  - The time value in seconds since the epoch.
:::
        ''',
        "return_value": '''
Returns the timestamp as a `double` value in seconds since the epoch.
        '''
    },
    {
        "name": "start_sectimer",
        "description": '''
Start a timer to measure elapsed time in seconds.
        ''',
        "prototype": '''
PUBLIC double start_sectimer(void);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `-`
  - `-`
  - This function does not take any parameters.
:::
        ''',
        "return_value": '''
Returns the current timestamp as a `double` value in seconds.
        '''
    },
    {
        "name": "test_sectimer",
        "description": '''
Test the elapsed time since `start_sectimer()` was called, in seconds.
        ''',
        "prototype": '''
PUBLIC double test_sectimer(
    double start
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `start`
  - `double`
  - The start timestamp returned by `start_sectimer()`.
:::
        ''',
        "return_value": '''
Returns the elapsed time in seconds as a `double` value.
        '''
    },
    {
        "name": "start_msectimer",
        "description": '''
Start a timer to measure elapsed time in milliseconds.
        ''',
        "prototype": '''
PUBLIC double start_msectimer(void);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `-`
  - `-`
  - This function does not take any parameters.
:::
        ''',
        "return_value": '''
Returns the current timestamp as a `double` value in milliseconds.
        '''
    },
    {
        "name": "test_msectimer",
        "description": '''
Test the elapsed time since `start_msectimer()` was called, in milliseconds.
        ''',
        "prototype": '''
PUBLIC double test_msectimer(
    double start
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `start`
  - `double`
  - The start timestamp returned by `start_msectimer()`.
:::
        ''',
        "return_value": '''
Returns the elapsed time in milliseconds as a `double` value.
        '''
    }
])

functions_documentation.extend([
    {
        "name": "time_in_miliseconds_monotonic",
        "description": '''
Get the current monotonic time in milliseconds. This time is not affected by system clock changes.
        ''',
        "prototype": '''
PUBLIC uint64_t time_in_miliseconds_monotonic(void);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `-`
  - `-`
  - This function does not take any parameters.
:::
        ''',
        "return_value": '''
Returns the current monotonic time as a `uint64_t` value in milliseconds.
        '''
    },
    {
        "name": "time_in_miliseconds",
        "description": '''
Get the current system time in milliseconds since the epoch.
        ''',
        "prototype": '''
PUBLIC uint64_t time_in_miliseconds(void);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `-`
  - `-`
  - This function does not take any parameters.
:::
        ''',
        "return_value": '''
Returns the current system time as a `uint64_t` value in milliseconds since the epoch.
        '''
    },
    {
        "name": "time_in_seconds",
        "description": '''
Get the current system time in seconds since the epoch.
        ''',
        "prototype": '''
PUBLIC time_t time_in_seconds(void);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `-`
  - `-`
  - This function does not take any parameters.
:::
        ''',
        "return_value": '''
Returns the current system time as a `time_t` value in seconds since the epoch.
        '''
    },
    {
        "name": "htonll",
        "description": '''
Convert a 64-bit integer from host byte order to network byte order.
        ''',
        "prototype": '''
PUBLIC uint64_t htonll(
    uint64_t hostlonglong
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `hostlonglong`
  - `uint64_t`
  - The 64-bit integer in host byte order.
:::
        ''',
        "return_value": '''
Returns the 64-bit integer in network byte order.
        '''
    },
    {
        "name": "ntohll",
        "description": '''
Convert a 64-bit integer from network byte order to host byte order.
        ''',
        "prototype": '''
PUBLIC uint64_t ntohll(
    uint64_t netlonglong
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `netlonglong`
  - `uint64_t`
  - The 64-bit integer in network byte order.
:::
        ''',
        "return_value": '''
Returns the 64-bit integer in host byte order.
        '''
    },
    {
        "name": "list_open_files",
        "description": '''
List all open file descriptors for the current process.
        ''',
        "prototype": '''
PUBLIC char **list_open_files(
    int *file_count
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `file_count`
  - `int *`
  - A pointer to store the number of open files.
:::
        ''',
        "return_value": '''
Returns an array of strings containing the paths of open files, or `NULL` on failure. The caller is responsible for freeing the array.
        '''
    }
])

functions_documentation.extend([
    {
        "name": "formatdate",
        "description": '''
Format a timestamp into a human-readable date string.
        ''',
        "prototype": '''
PUBLIC char *formatdate(
    double      timestamp,
    const char  *format
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `timestamp`
  - `double`
  - The timestamp to format, in seconds since the epoch.

* - `format`
  - `const char *`
  - The format string for the date (e.g., `"%Y-%m-%d %H:%M:%S"`).
:::
        ''',
        "return_value": '''
Returns a string containing the formatted date, or `NULL` on failure. The caller is responsible for freeing the string.
        '''
    },
    {
        "name": "count_char",
        "description": '''
Count the occurrences of a specific character in a string.
        ''',
        "prototype": '''
PUBLIC int count_char(
    const char  *string,
    char         c
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
  - The string to search.

* - `c`
  - `char`
  - The character to count.
:::
        ''',
        "return_value": '''
Returns the number of times the character `c` appears in the string.
        '''
    },
    {
        "name": "get_hostname",
        "description": '''
Retrieve the hostname of the current system.
        ''',
        "prototype": '''
PUBLIC char *get_hostname(void);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `-`
  - `-`
  - This function does not take any parameters.
:::
        ''',
        "return_value": '''
Returns a string containing the hostname, or `NULL` on failure. The caller is responsible for freeing the string.
        '''
    },
    {
        "name": "create_uuid",
        "description": '''
Generate a new universally unique identifier (UUID).
        ''',
        "prototype": '''
PUBLIC char *create_uuid(void);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `-`
  - `-`
  - This function does not take any parameters.
:::
        ''',
        "return_value": '''
Returns a string containing the newly generated UUID. The caller is responsible for freeing the string.
        '''
    },
    {
        "name": "node_uuid",
        "description": '''
Retrieve the UUID of the current node.
        ''',
        "prototype": '''
PUBLIC const char *node_uuid(void);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `-`
  - `-`
  - This function does not take any parameters.
:::
        ''',
        "return_value": '''
Returns a constant string containing the node's UUID.
        '''
    },
    {
        "name": "comm_prot_register",
        "description": '''
Register a communication protocol handler.
        ''',
        "prototype": '''
PUBLIC int comm_prot_register(
    const char  *protocol_name,
    void        (*callback)(void *data, int size),
    void        *user_data
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `protocol_name`
  - `const char *`
  - The name of the protocol to register.

* - `callback`
  - `void (*)(void *, int)`
  - The function to handle protocol events.

* - `user_data`
  - `void *`
  - User-defined data to pass to the callback function.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on failure.
        '''
    }
])

functions_documentation.extend([
    {
        "name": "comm_prot_get_gclass",
        "description": '''
Retrieve the GClass (Generic Class) associated with a communication protocol.
        ''',
        "prototype": '''
PUBLIC const char *comm_prot_get_gclass(
    const char  *protocol_name
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `protocol_name`
  - `const char *`
  - The name of the communication protocol.
:::
        ''',
        "return_value": '''
Returns the name of the GClass associated with the protocol, or `NULL` if not found.
        '''
    },
    {
        "name": "comm_prot_free",
        "description": '''
Free resources associated with a registered communication protocol.
        ''',
        "prototype": '''
PUBLIC int comm_prot_free(
    const char  *protocol_name
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `protocol_name`
  - `const char *`
  - The name of the communication protocol to free.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on failure.
        '''
    },
    {
        "name": "launch_daemon",
        "description": '''
Launch a process as a daemon.
        ''',
        "prototype": '''
PUBLIC int launch_daemon(
    const char  *command,
    const char  *working_dir,
    const char  *output_file
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `command`
  - `const char *`
  - The command to execute as a daemon.

* - `working_dir`
  - `const char *`
  - The working directory for the daemon process.

* - `output_file`
  - `const char *`
  - The file to redirect the daemon's output to.
:::
        ''',
        "return_value": '''
Returns the PID of the launched daemon on success, or a negative value on failure.
        '''
    },
    {
        "name": "parse_url",
        "description": '''
Parse a URL into its components.
        ''',
        "prototype": '''
PUBLIC int parse_url(
    const char  *url,
    char        *schema,
    int          schema_size,
    char        *host,
    int          host_size,
    char        *port,
    int          port_size,
    char        *path,
    int          path_size
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `url`
  - `const char *`
  - The URL to parse.

* - `schema`
  - `char *`
  - Buffer to store the URL schema.

* - `schema_size`
  - `int`
  - The size of the schema buffer.

* - `host`
  - `char *`
  - Buffer to store the URL host.

* - `host_size`
  - `int`
  - The size of the host buffer.

* - `port`
  - `char *`
  - Buffer to store the URL port.

* - `port_size`
  - `int`
  - The size of the port buffer.

* - `path`
  - `char *`
  - Buffer to store the URL path.

* - `path_size`
  - `int`
  - The size of the path buffer.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on failure.
        '''
    },
    {
        "name": "get_url_schema",
        "description": '''
Extract the schema from a URL.
        ''',
        "prototype": '''
PUBLIC char *get_url_schema(
    const char  *url
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `url`
  - `const char *`
  - The URL to extract the schema from.
:::
        ''',
        "return_value": '''
Returns a newly allocated string containing the URL schema, or `NULL` on failure. The caller is responsible for freeing the string.
        '''
    },
    {
        "name": "ghttp_parser_received",
        "description": '''
Handle received HTTP data for parsing.
        ''',
        "prototype": '''
PUBLIC int ghttp_parser_received(
    const char  *data,
    size_t       size
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `data`
  - `const char *`
  - The HTTP data received.

* - `size`
  - `size_t`
  - The size of the HTTP data.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on failure.
        '''
    }
])

functions_documentation.extend([
    {
        "name": "ghttp_parser_destroy",
        "description": '''
Destroy an HTTP parser and free associated resources.
        ''',
        "prototype": '''
PUBLIC void ghttp_parser_destroy(void);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `-`
  - `-`
  - This function does not take any parameters.
:::
        ''',
        "return_value": '''
No return value. This function releases resources associated with the HTTP parser.
        '''
    },
    {
        "name": "ghttp_parser_reset",
        "description": '''
Reset an HTTP parser to its initial state.
        ''',
        "prototype": '''
PUBLIC void ghttp_parser_reset(void);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `-`
  - `-`
  - This function does not take any parameters.
:::
        ''',
        "return_value": '''
No return value. This function resets the parser's state.
        '''
    },
    {
        "name": "istream_create",
        "description": '''
Create an input stream for reading data.
        ''',
        "prototype": '''
PUBLIC istream_t *istream_create(
    size_t buffer_size
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `buffer_size`
  - `size_t`
  - The size of the buffer to allocate for the input stream.
:::
        ''',
        "return_value": '''
Returns a pointer to the newly created input stream, or `NULL` on failure.
        '''
    },
    {
        "name": "istream_destroy",
        "description": '''
Destroy an input stream and free associated resources.
        ''',
        "prototype": '''
PUBLIC void istream_destroy(
    istream_t *istream
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `istream`
  - `istream_t *`
  - The input stream to destroy.
:::
        ''',
        "return_value": '''
No return value. This function frees resources associated with the input stream.
        '''
    },
    {
        "name": "istream_read_until_num_bytes",
        "description": '''
Read data from an input stream until a specified number of bytes is read.
        ''',
        "prototype": '''
PUBLIC size_t istream_read_until_num_bytes(
    istream_t *istream,
    char      *buffer,
    size_t     num_bytes
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `istream`
  - `istream_t *`
  - The input stream to read from.

* - `buffer`
  - `char *`
  - The buffer to store the read data.

* - `num_bytes`
  - `size_t`
  - The number of bytes to read from the stream.
:::
        ''',
        "return_value": '''
Returns the number of bytes successfully read, or `0` on failure.
        '''
    },
    {
        "name": "istream_read_until_delimiter",
        "description": '''
Read data from an input stream until a specified delimiter is encountered.
        ''',
        "prototype": '''
PUBLIC size_t istream_read_until_delimiter(
    istream_t *istream,
    char      *buffer,
    size_t     buffer_size,
    char       delimiter
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `istream`
  - `istream_t *`
  - The input stream to read from.

* - `buffer`
  - `char *`
  - The buffer to store the read data.

* - `buffer_size`
  - `size_t`
  - The size of the buffer.

* - `delimiter`
  - `char`
  - The delimiter to stop reading at.
:::
        ''',
        "return_value": '''
Returns the number of bytes successfully read, or `0` on failure.
        '''
    }
])

functions_documentation.extend([
    {
        "name": "istream_consume",
        "description": '''
Consume a specified number of bytes from the input stream, advancing the read pointer.
        ''',
        "prototype": '''
PUBLIC size_t istream_consume(
    istream_t *istream,
    size_t     num_bytes
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `istream`
  - `istream_t *`
  - The input stream to consume data from.

* - `num_bytes`
  - `size_t`
  - The number of bytes to consume.
:::
        ''',
        "return_value": '''
Returns the number of bytes successfully consumed, or `0` on failure.
        '''
    },
    {
        "name": "istream_cur_rd_pointer",
        "description": '''
Get the current read pointer for the input stream.
        ''',
        "prototype": '''
PUBLIC const char *istream_cur_rd_pointer(
    istream_t *istream
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `istream`
  - `istream_t *`
  - The input stream to query.
:::
        ''',
        "return_value": '''
Returns a pointer to the current read position in the input stream.
        '''
    },
    {
        "name": "istream_length",
        "description": '''
Get the length of data available in the input stream.
        ''',
        "prototype": '''
PUBLIC size_t istream_length(
    istream_t *istream
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `istream`
  - `istream_t *`
  - The input stream to query.
:::
        ''',
        "return_value": '''
Returns the number of bytes currently available in the input stream.
        '''
    },
    {
        "name": "istream_get_gbuffer",
        "description": '''
Retrieve the current gbuffer from the input stream. Works with [`gbuffer_t *`](gbuffer_t).
        ''',
        "prototype": '''
PUBLIC gbuffer_t *istream_get_gbuffer(
    istream_t *istream
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `istream`
  - `istream_t *`
  - The input stream to retrieve the gbuffer from.
:::
        ''',
        "return_value": '''
Returns a pointer to the current [`gbuffer_t *`](gbuffer_t) in the input stream, or `NULL` on failure.
        '''
    },
    {
        "name": "istream_pop_gbuffer",
        "description": '''
Pop and remove the current gbuffer from the input stream. Works with [`gbuffer_t *`](gbuffer_t).
        ''',
        "prototype": '''
PUBLIC gbuffer_t *istream_pop_gbuffer(
    istream_t *istream
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `istream`
  - `istream_t *`
  - The input stream to pop the gbuffer from.
:::
        ''',
        "return_value": '''
Returns a pointer to the removed [`gbuffer_t *`](gbuffer_t), or `NULL` on failure. The caller is responsible for freeing the gbuffer.
        '''
    },
    {
        "name": "istream_new_gbuffer",
        "description": '''
Create and attach a new gbuffer to the input stream. Works with [`gbuffer_t *`](gbuffer_t).
        ''',
        "prototype": '''
PUBLIC gbuffer_t *istream_new_gbuffer(
    istream_t *istream
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `istream`
  - `istream_t *`
  - The input stream to attach the new gbuffer to.
:::
        ''',
        "return_value": '''
Returns a pointer to the newly created [`gbuffer_t *`](gbuffer_t), or `NULL` on failure.
        '''
    }
])

functions_documentation.extend([
    {
        "name": "istream_extract_matched_data",
        "description": '''
Extract data from the input stream that matches a specified pattern or condition.
        ''',
        "prototype": '''
PUBLIC size_t istream_extract_matched_data(
    istream_t   *istream,
    char        *buffer,
    size_t       buffer_size,
    BOOL         (*match_fn)(const char *data, size_t size, void *user_data),
    void        *user_data
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `istream`
  - `istream_t *`
  - The input stream to extract data from.

* - `buffer`
  - `char *`
  - The buffer to store the matched data.

* - `buffer_size`
  - `size_t`
  - The size of the buffer.

* - `match_fn`
  - `BOOL (*)(const char *, size_t, void *)`
  - A function pointer to evaluate matches for the data.

* - `user_data`
  - `void *`
  - User-defined data passed to the match function.
:::
        ''',
        "return_value": '''
Returns the number of bytes of matched data extracted, or `0` if no match was found.
        '''
    },
    {
        "name": "istream_reset_wr",
        "description": '''
Reset the write pointer of the input stream to the beginning.
        ''',
        "prototype": '''
PUBLIC void istream_reset_wr(
    istream_t   *istream
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `istream`
  - `istream_t *`
  - The input stream whose write pointer will be reset.
:::
        ''',
        "return_value": '''
No return value. This function resets the write pointer of the input stream.
        '''
    },
    {
        "name": "istream_reset_rd",
        "description": '''
Reset the read pointer of the input stream to the beginning.
        ''',
        "prototype": '''
PUBLIC void istream_reset_rd(
    istream_t   *istream
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `istream`
  - `istream_t *`
  - The input stream whose read pointer will be reset.
:::
        ''',
        "return_value": '''
No return value. This function resets the read pointer of the input stream.
        '''
    },
    {
        "name": "istream_clear",
        "description": '''
Clear the input stream, removing all data and resetting pointers.
        ''',
        "prototype": '''
PUBLIC void istream_clear(
    istream_t   *istream
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `istream`
  - `istream_t *`
  - The input stream to clear.
:::
        ''',
        "return_value": '''
No return value. This function clears all data from the input stream.
        '''
    },
    {
        "name": "istream_is_completed",
        "description": '''
Check if the input stream has completed reading all data.
        ''',
        "prototype": '''
PUBLIC BOOL istream_is_completed(
    istream_t   *istream
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `istream`
  - `istream_t *`
  - The input stream to check.
:::
        ''',
        "return_value": '''
Returns `TRUE` if the input stream has completed reading all data, otherwise returns `FALSE`.
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
