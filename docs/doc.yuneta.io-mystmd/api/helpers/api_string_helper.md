# String Functions

Source code in:

- [helpers.h](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.h)
- [helpers.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.c)

(all_numbers)=
## `all_numbers()`

`all_numbers()` checks if a given string consists entirely of numeric characters.

```C
BOOL all_numbers(const char *s);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `s` | `const char *` | The input string to be checked. |

**Returns**

Returns `TRUE` if the string contains only numeric characters and is not empty; otherwise, returns `FALSE`.

**Notes**

An empty string is considered non-numeric and will return `FALSE`.

---

(bin2hex)=
## `bin2hex()`

`bin2hex` converts a binary buffer into a hexadecimal string representation.

```C
char *bin2hex(
    char *bf,
    int bfsize,
    const uint8_t *bin,
    size_t bin_len
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Pointer to the output buffer where the hexadecimal string will be stored. |
| `bfsize` | `int` | Size of the output buffer in bytes. |
| `bin` | `const uint8_t *` | Pointer to the input binary data to be converted. |
| `bin_len` | `size_t` | Length of the input binary data in bytes. |

**Returns**

Returns a pointer to the output buffer `bf` containing the hexadecimal string.

**Notes**

The output buffer `bf` must be large enough to store the hexadecimal representation (2 * `bin_len` + 1 bytes for the null terminator).

---

(build_path)=
## `build_path()`

`build_path()` constructs a file path by concatenating multiple path segments, ensuring proper directory separators and removing redundant slashes.

```C
char *build_path(
    char *bf,
    size_t bfsize,
    ...
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Buffer to store the constructed path. |
| `bfsize` | `size_t` | Size of the buffer `bf`. |
| `...` | `variadic` | Variable number of path segments to concatenate, terminated by `NULL`. |

**Returns**

Returns a pointer to `bf` containing the constructed path.

**Notes**

Ensures that the resulting path does not have redundant slashes and properly formats directory separators.

---

(change_char)=
## `change_char()`

`change_char()` replaces all occurrences of a specified character in a string with another character and returns the count of replacements.

```C
int change_char(
    char *s,
    char old_c,
    char new_c
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `s` | `char *` | Pointer to the input string to be modified. |
| `old_c` | `char` | Character in the string to be replaced. |
| `new_c` | `char` | Character to replace occurrences of `old_c`. |

**Returns**

Returns the number of characters replaced in the string.

**Notes**

The function modifies the input string in place. Ensure that `s` is a valid, mutable string.

---

(count_char)=
## `count_char()`

The function `count_char()` counts the occurrences of a specified character in a given string.

```C
int count_char(const char *s, char c);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `s` | `const char *` | The input string in which occurrences of `c` will be counted. |
| `c` | `char` | The character to count within the string `s`. |

**Returns**

Returns the number of times the character `c` appears in the string `s`.

**Notes**

If `s` is NULL, the behavior is undefined. The function does not modify the input string.

---

(delete_left_blanks)=
## `delete_left_blanks()`

Removes leading whitespace characters (spaces, tabs, newlines, and carriage returns) from the given string `s` by shifting the non-whitespace characters to the left.

```C
void delete_left_blanks(char *s);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `s` | `char *` | The null-terminated string from which leading whitespace characters will be removed. The string is modified in place. |

**Returns**

None.

**Notes**

If the input string is empty or contains only whitespace, it will be reduced to an empty string.

---

(delete_left_char)=
## `delete_left_char()`

Removes all leading occurrences of the specified character `x` from the string `s`.

```C
char *delete_left_char(
    char *s,
    char x
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `s` | `char *` | The input string to modify in place. |
| `x` | `char` | The character to remove from the beginning of `s`. |

**Returns**

A pointer to the modified string `s`.

**Notes**

The function modifies the input string in place by shifting characters to the left.

---

(delete_right_blanks)=
## `delete_right_blanks()`

Removes trailing whitespace characters (spaces, tabs, carriage returns, and line feeds) from the end of the given string `s`.

```C
void delete_right_blanks(char *s);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `s` | `char *` | The null-terminated string to be modified in place. |

**Returns**

None.

**Notes**

The function modifies the input string directly by replacing trailing whitespace characters with null terminators.

---

(delete_right_char)=
## `delete_right_char()`

`delete_right_char()` removes all trailing occurrences of the specified character `x` from the string `s`.

```C
char *delete_right_char(char *s, char x);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `s` | `char *` | The input string from which trailing occurrences of `x` will be removed. |
| `x` | `char` | The character to be removed from the end of the string. |

**Returns**

Returns a pointer to the modified string `s`.

**Notes**

The function modifies the input string in place by replacing trailing occurrences of `x` with the null terminator.

---

(get_key_value_parameter)=
## `get_key_value_parameter()`

Extracts a key-value pair from a given string, where the key and value are separated by an '=' character. The function modifies the input string by inserting null terminators and returns a pointer to the extracted value.

```C
char *get_key_value_parameter(
    char *s,
    char **key,
    char **save_ptr
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `s` | `char *` | The input string containing the key-value pair. This string is modified in-place. |
| `key` | `char **` | Pointer to store the extracted key. The key is null-terminated. |
| `save_ptr` | `char **` | Pointer to store the remaining part of the string after the key-value pair. |

**Returns**

A pointer to the extracted value, or NULL if no valid key-value pair is found.

**Notes**

The function expects the input string to be formatted as 'key=value' or 'key="value"'. The input string is modified by inserting null terminators to separate the key and value.

---

(get_last_segment)=
## `get_last_segment()`

Extracts the last segment from a given file path by locating the last occurrence of the '/' character and returning the substring that follows it.

```C
char *get_last_segment(char *path);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `path` | `char *` | The file path from which the last segment will be extracted. |

**Returns**

A pointer to the last segment of the path. If no '/' is found, returns the original path.

**Notes**

The function does not modify the input string.

---

(get_parameter)=
## `get_parameter()`

`get_parameter()` extracts a parameter from a string, delimited by blanks (`' '` or `'	'`) or quotes (`'` or `"`). The input string is modified by inserting null terminators.

```C
char *get_parameter(
    char *s,
    char **save_ptr
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `s` | `char *` | The input string to parse. It is modified in-place. |
| `save_ptr` | `char **` | Pointer to store the next position in the string for subsequent calls. |

**Returns**

Returns a pointer to the extracted parameter, or `NULL` if no parameter is found.

**Notes**

If the parameter is enclosed in quotes (`'` or `"`), the function ensures that the returned string does not include them.

---

(helper_doublequote2quote)=
## `helper_doublequote2quote()`

The function `helper_doublequote2quote()` replaces all double quotes (`"`) in the given string with single quotes (`'`).

```C
char *helper_doublequote2quote(char *str);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `str` | `char *` | The input string in which double quotes will be replaced with single quotes. |

**Returns**

Returns the modified string with all double quotes replaced by single quotes.

**Notes**

This function modifies the input string in place. Ensure that the input string is mutable and properly allocated.

---

(helper_quote2doublequote)=
## `helper_quote2doublequote()`

The function `helper_quote2doublequote()` replaces all single quotes (`'`) in the input string with double quotes (`"`).

```C
char *helper_quote2doublequote(char *str);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `str` | `char *` | The input string to be modified in place. |

**Returns**

Returns the modified string with all single quotes replaced by double quotes.

**Notes**

This function modifies the input string in place and does not allocate new memory.

---

(hex2bin)=
## `hex2bin()`

`hex2bin` converts a hexadecimal string into its binary representation, storing the result in a provided buffer.

```C
char *hex2bin(
    char *bf,
    int bfsize,
    const char *hex,
    size_t hex_len,
    size_t *out_len
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Pointer to the output buffer where the binary data will be stored. |
| `bfsize` | `int` | Size of the output buffer in bytes. |
| `hex` | `const char *` | Pointer to the input hexadecimal string to be converted. |
| `hex_len` | `size_t` | Length of the input hexadecimal string. |
| `out_len` | `size_t *` | Pointer to a variable where the function will store the length of the resulting binary data. |

**Returns**

Returns a pointer to the output buffer `bf` containing the binary data.

**Notes**

['The function processes the input hexadecimal string two characters at a time, converting them into a single byte.', 'If a non-hexadecimal character is encountered, the conversion stops.', 'The function does not allocate memory; the caller must ensure `bf` has enough space.', 'The `out_len` parameter is optional; if provided, it will contain the number of bytes written to `bf`.']

---

(idx_in_list)=
## `idx_in_list()`

The function `idx_in_list()` searches for a string in a list of strings and returns its index if found, or -1 if not found.

```C
int idx_in_list(
    const char **list,
    const char *str,
    BOOL ignore_case
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `list` | `const char **` | A null-terminated array of string pointers to search within. |
| `str` | `const char *` | The string to search for in the list. |
| `ignore_case` | `BOOL` | If `TRUE`, the comparison is case-insensitive; otherwise, it is case-sensitive. |

**Returns**

Returns the index of `str` in `list` if found, or -1 if not found.

**Notes**

The function iterates through the list and compares each element with `str` using either `strcmp()` or `strcasecmp()` based on the `ignore_case` flag.

---

(left_justify)=
## `left_justify()`

The `left_justify()` function removes leading and trailing whitespace characters from the given string `s`, ensuring that the string is left-aligned with no extra spaces at the beginning or end.

```C
void left_justify(char *s);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `s` | `char *` | A pointer to a null-terminated string that will be modified in place to remove leading and trailing whitespace. |

**Returns**

This function does not return a value.

**Notes**

If `s` is NULL, the function does nothing. The function modifies the input string directly.

---

(nice_size)=
## `nice_size()`

`nice_size()` formats a byte count into a human-readable string with appropriate units (B, KB, MB, etc.), using either base-1000 or base-1024 scaling.

```C
void nice_size(
    char *bf,      size_t bfsize,
    uint64_t bytes, BOOL   b1024
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Buffer to store the formatted size string. |
| `bfsize` | `size_t` | Size of the buffer `bf`. |
| `bytes` | `uint64_t` | The number of bytes to format. |
| `b1024` | `BOOL` | If `TRUE`, uses base-1024 (binary units); otherwise, uses base-1000 (decimal units). |

**Returns**

None.

**Notes**

The function ensures that the formatted string fits within `bfsize` and selects the most appropriate unit for readability.

---

(pop_last_segment)=
## `pop_last_segment()`

`pop_last_segment()` removes and returns the last segment of a given file path, modifying the original string.

```C
char *pop_last_segment(char *path);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `path` | `char *` | A mutable string representing a file path. The function modifies this string in place. |

**Returns**

A pointer to the last segment of the path. The original `path` string is modified, with the last segment removed.

**Notes**

If no '/' is found in `path`, the entire string is returned, and `path` remains unchanged.

---

(split2)=
## `split2()`

`split2()` splits a string into a list of substrings using the specified delimiters, excluding empty substrings.

```C
const char **split2(
    const char *str,
    const char *delim,
    int *list_size
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `str` | `const char *` | The input string to be split. |
| `delim` | `const char *` | A string containing delimiter characters used to split `str`. |
| `list_size` | `int *` | Pointer to an integer that will store the number of substrings found. |

**Returns**

A dynamically allocated array of strings containing the split substrings. The caller must free the array using [`split_free2()`](#split_free2). Returns `NULL` on failure.

**Notes**

['Empty substrings are not included in the result.', 'The returned array is null-terminated.', 'Use [`split_free2()`](#split_free2) to free the allocated memory.']

---

(split3)=
## `split3()`

Splits the input string `str` into a list of substrings using the specified delimiters `delim`. Unlike [`split2()`](#split2), this function includes empty substrings in the result.

```C
const char **split3(
    const char *str,
    const char *delim,
    int *plist_size
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `str` | `const char *` | The input string to be split. |
| `delim` | `const char *` | A string containing delimiter characters used to split `str`. |
| `plist_size` | `int *` | Pointer to an integer that will be set to the number of substrings found. |

**Returns**

A dynamically allocated array of strings containing the split substrings. The caller must free the returned array using [`split_free3()`](#split_free3). Returns `NULL` on failure.

**Notes**

This function differs from [`split2()`](#split2) in that it includes empty substrings in the result when consecutive delimiters are found.

---

(split_free2)=
## `split_free2()`

Frees the memory allocated for a list of strings created by [`split2()`](#split2).

```C
void split_free2(const char **list);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `list` | `const char **` | Pointer to the list of strings to be freed. |

**Returns**

None.

**Notes**

This function should be used to deallocate memory allocated by [`split2()`](#split2) to prevent memory leaks.

---

(split_free3)=
## `split_free3()`

Frees the memory allocated for a list of strings created by `split3()`, ensuring proper deallocation of each string and the list itself.

```C
void split_free3(const char **list);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `list` | `const char **` | Pointer to the list of strings to be freed. Each string in the list and the list itself are deallocated. |

**Returns**

None.

**Notes**

This function should be used to free memory allocated by [`split3()`](#split3). It iterates through the list, freeing each string before deallocating the list itself.

---

(str_concat)=
## `str_concat()`

`str_concat()` concatenates two strings into a newly allocated buffer and returns the result.

```C
char *str_concat(
    const char *str1,
    const char *str2
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `str1` | `const char *` | The first string to concatenate. |
| `str2` | `const char *` | The second string to concatenate. |

**Returns**

A newly allocated string containing the concatenation of `str1` and `str2`. The caller must free the returned string using `str_concat_free()`.

**Notes**

If either `str1` or `str2` is `NULL`, it is treated as an empty string.

---

(str_concat3)=
## `str_concat3()`

Concatenates three strings into a newly allocated buffer and returns the result. The caller must free the returned string using `str_concat_free()`.

```C
char *str_concat3(
    const char *str1,
    const char *str2,
    const char *str3
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `str1` | `const char *` | First string to concatenate. Can be NULL. |
| `str2` | `const char *` | Second string to concatenate. Can be NULL. |
| `str3` | `const char *` | Third string to concatenate. Can be NULL. |

**Returns**

A newly allocated string containing the concatenation of `str1`, `str2`, and `str3`. The caller must free the returned string using [`str_concat_free()`](#str_concat_free). Returns NULL if memory allocation fails.

**Notes**

If any of the input strings are NULL, they are treated as empty strings.

---

(str_concat_free)=
## `str_concat_free()`

Frees memory allocated for a concatenated string created by [`str_concat()`](#str_concat) or [`str_concat3()`](#str_concat3).

```C
void str_concat_free(char *s);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `s` | `char *` | Pointer to the dynamically allocated string to be freed. |

**Returns**

None.

**Notes**

This function should only be used to free memory allocated by [`str_concat()`](#str_concat) or [`str_concat3()`](#str_concat3).

---

(str_in_list)=
## `str_in_list()`

The function `str_in_list()` checks if a given string exists within a list of strings, with an option to perform a case-insensitive comparison.

```C
BOOL str_in_list(
    const char **list,
    const char *str,
    BOOL ignore_case
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `list` | `const char **` | A null-terminated array of string pointers representing the list to search. |
| `str` | `const char *` | The string to search for within the list. |
| `ignore_case` | `BOOL` | If `TRUE`, the comparison is case-insensitive; otherwise, it is case-sensitive. |

**Returns**

Returns `TRUE` if the string is found in the list, otherwise returns `FALSE`.

**Notes**

The function iterates through the list and compares each entry with `str` using either `strcmp()` or `strcasecmp()` based on the `ignore_case` flag.

---

(strntolower)=
## `strntolower()`

`strntolower()` converts the first `n` characters of the input string to lowercase.

```C
char *strntolower(
    char *s,
    size_t n
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `s` | `char *` | Pointer to the null-terminated string to be converted. |
| `n` | `size_t` | Maximum number of characters to convert. |

**Returns**

Returns a pointer to the modified string `s`.

**Notes**

If `s` is NULL or `n` is zero, the function returns NULL without modifying the string.

---

(strntoupper)=
## `strntoupper()`

Converts the first `n` characters of the string `s` to uppercase in place.

```C
char *strntoupper(
    char *s,
    size_t n
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `s` | `char *` | Pointer to the null-terminated string to be converted. |
| `n` | `size_t` | Maximum number of characters to convert. |

**Returns**

Returns a pointer to the modified string `s`.

**Notes**

If `s` is NULL or `n` is zero, the function returns NULL without modifying the string.

---

(translate_string)=
## `translate_string()`

`translate_string` replaces characters in the `from` string with corresponding characters from the `mk_to` string, based on the mapping defined in `mk_from`.

```C
char *translate_string(
    char *to,
    int   tolen,
    const char *from,
    const char *mk_to,
    const char *mk_from
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `to` | `char *` | Buffer to store the translated string. |
| `tolen` | `int` | Size of the `to` buffer. |
| `from` | `const char *` | Input string containing characters to be replaced. |
| `mk_to` | `const char *` | String containing replacement characters. |
| `mk_from` | `const char *` | String containing characters to be replaced. |

**Returns**

Returns a pointer to the `to` buffer containing the translated string.

**Notes**

['The `mk_from` and `mk_to` strings define a mapping where each character in `mk_from` is replaced by the corresponding character in `mk_to`.', 'If `tolen` is too small, the function may not fully translate the input.', 'The function does not allocate memory; the caller must ensure `to` has sufficient space.']

---

(capitalize)=
## `capitalize()`

*Description pending — signature extracted from header.*

```C
char *capitalize(
    char *s
);
```

---

(lower)=
## `lower()`

*Description pending — signature extracted from header.*

```C
char *lower(
    char *s
);
```

---

(path_basename)=
## `path_basename()`

*Description pending — signature extracted from header.*

```C
const char *path_basename(
    const char *path
);
```

---

(replace_cli_vars)=
## `replace_cli_vars()`

*Description pending — signature extracted from header.*

```C
gbuffer_t *replace_cli_vars(
    const char *command,
    char *comment,
    int commentlen
);
```

---

(upper)=
## `upper()`

*Description pending — signature extracted from header.*

```C
char *upper(
    char *s
);
```

---

