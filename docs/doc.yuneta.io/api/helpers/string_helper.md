# String

String utilities used throughout Yuneta: trimming, case conversion, tokenisation, safe printing, numeric parsing and formatting.

Source code:

- [`helpers.h`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.h)
- [`helpers.c`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c)

(all_numbers)=
## [`all_numbers()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L1051)

`all_numbers()` checks if a given string consists entirely of numeric characters.

```C
BOOL all_numbers(const char *s);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `s` | `const char *` | The input string to be checked. |

**Returns**

Returns `TRUE` if the string contains only numeric characters and is not empty. Otherwise, returns `FALSE`.

**Notes**

An empty string is considered non-numeric and will return `FALSE`.

---

(bin2hex)=
## [`bin2hex()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L5547)

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
## [`build_path()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L903)

`build_path()` constructs a file path by concatenating multiple path segments. This makes sure of proper directory separators and removing redundant slashes.

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
## [`change_char()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L1260)

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

The function modifies the input string in place. Make sure that `s` is a valid, mutable string.

---

(count_char)=
## [`count_char()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L6283)

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
## [`delete_left_blanks()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L1124)

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
## [`delete_left_char()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L814)

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
## [`delete_right_blanks()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L1101)

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
## [`delete_right_char()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L793)

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
## [`get_key_value_parameter()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L1343)

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
## [`get_last_segment()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L980)

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
## [`get_parameter()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L1278)

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

If the parameter is enclosed in quotes (`'` or `"`), the function makes sure that the returned string does not include them.

---

(helper_doublequote2quote)=
## [`helper_doublequote2quote()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L1036)

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

This function modifies the input string in place. Make sure that the input string is mutable and properly allocated.

---

(helper_quote2doublequote)=
## [`helper_quote2doublequote()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L1021)

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
## [`hex2bin()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L5515)

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

['The function processes the input hexadecimal string two characters at a time, converting them into a single byte.', 'If a non-hexadecimal character is encountered, the conversion stops.', 'The function does not allocate memory. The caller must make sure that `bf` has enough space.', 'The `out_len` parameter is optional. If provided, it will contain the number of bytes written to `bf`.']

---

(idx_in_list)=
## [`idx_in_list()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L1695)

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
| `ignore_case` | `BOOL` | If `TRUE`, the comparison is case-insensitive. Otherwise, it is case-sensitive. |

**Returns**

Returns the index of `str` in `list` if found, or -1 if not found.

**Notes**

The function iterates through the list and compares each element with `str` using either `strcmp()` or `strcasecmp()` based on the `ignore_case` flag.

---

(is_secret_name)=
## [`is_secret_name()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L1438)

Tells, by its NAME, whether a key holds a secret -- for what cannot be known
by a declaration ([`SDF_SECRET`](#SDF_SECRET)): the free keys of a command
that is forwarded (`SDF_WILD_CMD`, the agent's `command-yuno`), a config
variable, a field of an audit record. It is the one list of the SDK: the
command traces ([`command_mask_secret_kw()`](#command_mask_secret_kw),
[`command_mask_secret_line()`](#command_mask_secret_line)),
[`gobj_mask_secret_config()`](#gobj_mask_secret_config) and the agent's
audit record all ask it.

A name is a secret's, in any case, when it holds one of `passw`, `pwd`,
`passphrase`, `secret`, `token`, `jwt`, `bearer`, `authorization`, `cookie`,
`credential`, `salt`; or one of `apikey`, `sessionid`, `sessionkey`,
`authdata` once `_`, `-`, `.` and blanks are taken out (`api_key`,
`X-Api-Key`, `__session_id__`); or both `priv` and `key` (`private_key`). Not
secrets: the PATH of a key or a certificate (`ssl_certificate_key`), a public
`cert_pem`, `max_sessions`, `authz`, `auth_method`; and a name with a SEGMENT
(split by `_`, `-`, `.` and blanks) that names something ABOUT a credential:
`endpoint`, `url`, `uri`, `domain`, `path`, `file`, `public`, `pub`, `count`,
`counts`, `type`, `name`, `len`, `length`, `size`, `max`, `min`, `ttl`,
`timeout`, `expiry`, `expires`, `mode` (`token_endpoint`,
`cookie_domain`, `jwt_public_keys`, `refresh_token_count`).

```C
BOOL is_secret_name(
    const char *name,
    size_t      len
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `name` | `const char *` | The name. It need not be NUL-terminated. |
| `len` | `size_t` | The bytes of `name` to look at. |

**Returns**

`TRUE` if it is the name of a secret; `FALSE` if not, or when `name` is NULL.

**Example**

```C
is_secret_name("smtp_password", 13);    // TRUE
is_secret_name("X-Api-Key", 9);         // TRUE (apikey, joined)
is_secret_name("private_key", 11);      // TRUE (priv + key)
is_secret_name("ssl_certificate_key", 19); // FALSE (a path)
is_secret_name("token_endpoint", 14);   // FALSE (about a token)
is_secret_name("username", 8);          // FALSE
```

---

(left_justify)=
## [`left_justify()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L1150)

The `left_justify()` function removes leading and trailing whitespace characters from the given string `s`. This makes sure that the string is left-aligned with no extra spaces at the beginning or end.

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

(json_mask_secrets)=
## [`json_mask_secrets()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L1591)

A json as a log or a trace may show it. At any depth, the value of a key whose
name is a secret's ([`is_secret_name()`](#is_secret_name)) is `"********"`,
whatever its json type (not an absent one, a null or an empty string, so "not
set" still shows); so is the `value` of a dict whose `attribute` names a
secret (a write-attr); and every string is masked as
[`mask_secrets_inline()`](#mask_secrets_inline). What
[`gobj_trace_json_masked()`](#gobj_trace_json_masked) prints, and what
`C_IEVENT_SRV` writes of a kw before its session.

```C
json_t *json_mask_secrets(
    json_t *jn      // not owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `jn` | `json_t *` | Any json. Not owned, not modified. |

**Returns**

A NEW reference, to decref: a masked copy (the dicts and lists on the way to
a masked value are copied, the rest is shared), or `jn` itself when there was
nothing to mask. `NULL` for `NULL`. A dict or list met twice is masked once,
the same everywhere; a cycle (jansson lets `json_object_set()` build one) is
`"<cycle>"`; a dict or list reached at 64 levels (as in gobj-js) is
`"<deeper not shown>"`. With the memo the limit is about the FIRST time a dict
is met: one met first near the limit is `"<deeper not shown>"` everywhere it
appears, and one masked first higher up shows whole wherever else it appears,
deeper ones included.

It is linear and bounded, because a peer reaches it before its session
(`C_IEVENT_SRV` dumps the kw of an event that comes before the identity
card): no more than 128 bytes of a name are asked about, and once 4 MB of
keys and strings are walked, the rest is `"<not shown: too large to mask>"`
-- never in clear.

**Example**

```C
json_t *kw = json_pack("{s:I, s:{s:s}, s:s}",
    "password", (json_int_t)1234,
    "auth", "access_token", "eyJ...",
    "__command__", "set-user-pwd username=bob password=hunter2"
);
json_t *kw_shown = json_mask_secrets(kw);
// {"password": "********", "auth": {"access_token": "********"},
//  "__command__": "set-user-pwd username=bob password=********"}
JSON_DECREF(kw_shown)
```

---

(mask_secrets_inline)=
## [`mask_secrets_inline()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L1505)

A text (a command line) with the value of every `name=value` whose name is a
secret's ([`is_secret_name()`](#is_secret_name)) written as `********`,
quoted or not; and the `value=` of a write-attr whose `attribute=` names a
secret. The rest of the text is kept as it is. The name is the run of name
characters just before the `=`, no longer than 128 bytes, and a text over
4 MB is `"<not shown: too large to mask>"`: it is linear. An unquoted value
runs to the next `word=` (or the end): a password written with blanks
(`password=correct horse battery`, `password= hunter2`) is masked whole, as
the parser leaves those words unread. A quote inside it is part of it
(`value=ab'cd` is masked whole), unless it is the quote that closes an outer
quoted value (`command='set-user-pwd password=x'` keeps its closing quote).

```C
char *mask_secrets_inline(
    const char *str
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `str` | `const char *` | The text. |

**Returns**

A `gbmem` string, to `GBMEM_FREE`; or `NULL` when there was nothing to mask,
and the text is to be shown as it is.

**Example**

```C
char *s = mask_secrets_inline("write-attr attribute=password value=hunter2");
// "write-attr attribute=password value=********"
GBMEM_FREE(s)
s = mask_secrets_inline("login user=bob token='a b c'");  // "login user=bob token=********"
GBMEM_FREE(s)
```

---

(json_mask_secrets_capped)=
## [`json_mask_secrets_capped()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L1975)

[`json_mask_secrets()`](#json_mask_secrets) that walks no more than
`max_bytes` of keys and strings: what is left is
`"<not shown: too large to mask>"`. A string is walked whole or not shown at
all, so a secret is never cut in half. For a dump of a few bytes of a kw from
a peer: the cost is bounded by what is shown, not by what was sent.
`C_IEVENT_SRV` dumps the kw of an event before the identity card through it,
capped at four times the 256 bytes it shows.

```C
json_t *json_mask_secrets_capped(
    json_t *jn,         // not owned
    size_t  max_bytes
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `jn` | `json_t *` | Any json. Not owned, not modified. |
| `max_bytes` | `size_t` | The bytes of keys and strings walked at most. |

**Returns**

A NEW reference, to decref, as [`json_mask_secrets()`](#json_mask_secrets).

**Example**

```C
json_t *kw_shown = json_mask_secrets_capped(kw_from_peer, 1024);
char dump[256];
json_dumpb(kw_shown, dump, sizeof(dump)-1, JSON_COMPACT);
JSON_DECREF(kw_shown)
```

---

(mask_secrets_in_text)=
## [`mask_secrets_in_text()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L1599)

The bytes of a traffic dump with the credentials that can be told soundly
written as `*`, IN PLACE, the length kept (so the offsets and the rest of the
dump stay as they were): the value of an HTTP `Cookie:`, `Set-Cookie:`,
`Authorization:` or `Proxy-Authorization:` header (for the last two after the
scheme: `Authorization: Bearer ******`); the value of a `name=value` whose
name is a secret's ([`is_secret_name()`](#is_secret_name)), as in a query
string, a form body or a command line; and the value of a json
`"name": value` whose name is a secret's. The traffic dumps use it
([`gobj_trace_dump()`](#gobj_trace_dump),
[`gobj_trace_dump_gbuf()`](#gobj_trace_dump_gbuf),
[`gobj_trace_dump_full_gbuf()`](#gobj_trace_dump_full_gbuf)) on a copy of
what they dump.

```C
size_t mask_secrets_in_text(
    char   *bf,     // modified in place
    size_t  len
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | The bytes, not NUL-terminated. Work on a COPY. |
| `len` | `size_t` | The number of bytes. |

**Returns**

The number of bytes masked; `0` when there was nothing to mask.

**Example**

```C
char text[] = "Cookie: sid=abc\r\n\r\nuser=bob&password=hunter2";
mask_secrets_in_text(text, strlen(text));
// "Cookie: *******\r\n\r\nuser=bob&password=*******"
```

---

(nice_size)=
## [`nice_size()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L1070)

`nice_size()` formats a byte count into a human-readable string with appropriate units (B, KB, MB and more.), using either base-1000 or base-1024 scaling.

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
| `b1024` | `BOOL` | If `TRUE`, uses base-1024 (binary units). Otherwise, uses base-1000 (decimal units). |

**Returns**

None.

**Notes**

The function makes sure that the formatted string fits within `bfsize` and selects the most appropriate unit for readability.

---

(pop_last_segment)=
## [`pop_last_segment()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L1002)

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

(version_cmp)=
## [`version_cmp()`](%s/helpers.c#L1200)

Compares two dotted versions **segment by segment**, like `strcmp`.

```C
int version_cmp(
    const char *version1,
    const char *version2
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `version1` | `const char *` | First version. |
| `version2` | `const char *` | Second version. |

**Returns**

Less than 0, 0, or greater than 0, as `version1` sorts before, equal to, or
after `version2`.

**Notes**

Both `.` and `-` separate, so a release and its revision order together
("1.9.0.0-2" is five segments), and a missing segment counts as 0, so "7.11" and
"7.11.0" are the same version.

⚠️ **Do not "simplify" this into a single number.** The agent used to weigh each
segment by 1000 and accumulate: "1.9.0.0-2" needs 10^12, which overflowed an int
and came out NEGATIVE, so the older release won every comparison and the agent
promoted it back on every restart for eleven days on a client node. A wider
accumulator only moves the ceiling, and it still assumes every segment stays
under 1000. Comparing segment by segment assumes neither, which is what the JS
and Python sides of this project already do.

---

(split2)=
## [`split2()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L1485)

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
## [`split3()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L1554)

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
## [`split_free2()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L1534)

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

This function must be used to deallocate memory allocated by [`split2()`](#split2) to prevent memory leaks.

---

(split_free3)=
## [`split_free3()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L1606)

Frees the memory allocated for a list of strings created by `split3()`. This makes sure of proper deallocation of each string and the list itself.

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

This function must be used to free memory allocated by [`split3()`](#split3). It iterates through the list, freeing each string before deallocating the list itself.

---

(str_concat)=
## [`str_concat()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L1623)

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
## [`str_concat3()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L1652)

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
## [`str_concat_free()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L1686)

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

This function must only be used to free memory allocated by [`str_concat()`](#str_concat) or [`str_concat3()`](#str_concat3).

---

(str_in_list)=
## [`str_in_list()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L1718)

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
| `ignore_case` | `BOOL` | If `TRUE`, the comparison is case-insensitive. Otherwise, it is case-sensitive. |

**Returns**

Returns `TRUE` if the string is found in the list, otherwise returns `FALSE`.

**Notes**

The function iterates through the list and compares each entry with `str` using either `strcmp()` or `strcasecmp()` based on the `ignore_case` flag.

---

(str_match_regex)=
## [`str_match_regex()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L1740)

`str_match_regex()` says whether a string matches a regular expression. It compiles, matches and frees on each call: nothing is kept, so it can be called from anywhere, as often as needed.

```C
BOOL str_match_regex(
    const char *str,
    const char *pattern,
    int         cflags
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `str` | `const char *` | The string to test. |
| `pattern` | `const char *` | The regular expression, in the syntax `cflags` selects. |
| `cflags` | `int` | `regcomp(3)` flags: `0` for a basic expression, `REG_EXTENDED`, `REG_ICASE`, ... (`REG_NOSUB` is always added). |

**Returns**

`TRUE` if `str` matches `pattern`. `FALSE` if it does not, if either argument is `NULL`, or if the pattern does not compile -- those two last cases are logged with `gobj_log_error()`.

**Example**

```C
if(str_match_regex(filename, "^tracks-[0-9]{4}-[0-9]{2}-[0-9]{2}\\.json$", REG_EXTENDED)) {
    /* a daily tracks file */
}
```

**Notes**

It is what `CASES_RE` of the string switch below uses.

---

(string-switch)=
## [`SWITCHS` / `CASES` / `ICASES` / `CASES_RE` / `DEFAULTS` / `SWITCHS_END`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.h#L96)

A `switch` on strings. `CASES` compares exactly, `ICASES` ignoring case, `CASES_RE` with a regular expression (through `str_match_regex()`), and `DEFAULTS` takes what nothing matched. As in a C `switch`, a case without `break` falls through to the next one, and `break` leaves the switch.

**Example**

```C
PRIVATE int kind_of(const char *s)
{
    SWITCHS(s) {
        CASES("measure")
        CASES("alarm")
            return 1;           // two cases, one body

        ICASES("status")
            return 2;           // "STATUS", "Status", ...

        CASES_RE("^diag-[0-9]+$", REG_EXTENDED)
            return 3;

        DEFAULTS
            return 0;
    } SWITCHS_END

    return -1;
}
```

**Notes**

**Leaving the switch from inside a case is safe**: `return`, `break` or `goto`. `SWITCHS` allocates nothing, and `CASES_RE` frees its regex before the body of the case runs.

Up to 7.25.6 it was not: `SWITCHS` compiled a regex on entry and only `SWITCHS_END` freed it, so every `return` from a case lost glibc's compiled automaton, a few KB, outside gbmem -- invisible to its audit and to `cur_system_memory`. `C_MQIOGATE`'s send path returned from a case, so every gate with a queue lost ~1 KB per message: in yunovatios' stress test a `gate_central` grew to 1 GB in 25 minutes. Any yuno built before the fix keeps the leak until it is rebuilt: the macros are expanded where they are used.

---

(strntolower)=
## [`strntolower()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L1187)

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
## [`strntoupper()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L1168)

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
## [`translate_string()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L1215)

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

['The `mk_from` and `mk_to` strings define a mapping where each character in `mk_from` is replaced by the corresponding character in `mk_to`.', 'If `tolen` is too small, the function cannot fully translate the input.', 'The function does not allocate memory. The caller must make sure that `to` has sufficient space.']

---

(capitalize)=
## [`capitalize()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L7418)

Converts the first character to uppercase and the remaining characters to lowercase.

```C
char *capitalize(
    char *s
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `s` | `char *` | The string to capitalize. Modified in place. |

**Returns**

Returns a pointer to the modified string `s`.

**Notes**

The function modifies the input string in place. If `s` is NULL or empty, it is returned unchanged.

---

(lower)=
## [`lower()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L7405)

Converts all characters in the string to lowercase.

```C
char *lower(
    char *s
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `s` | `char *` | The string to convert. Modified in place. |

**Returns**

Returns a pointer to the modified string `s`.

**Notes**

The function modifies the input string in place. If `s` is NULL, it is returned unchanged.

---

(path_basename)=
## [`path_basename()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L8090)

Extracts the filename component from a file path.

```C
const char *path_basename(
    const char *path
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `path` | `const char *` | The file path from which to extract the filename. |

**Returns**

A pointer to the last component (filename) within the `path` string. If no directory separator is found, returns the original `path` pointer.

**Notes**

The returned pointer points into the original `path` string. No new memory is allocated. The input string is not modified.

---

(replace_cli_vars)=
## [`replace_cli_vars()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L8176)

Replaces CLI variable placeholders in a command string with base64-encoded values.

```C
gbuffer_t *replace_cli_vars(
    const char *command,
    char *comment,
    int commentlen
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `command` | `const char *` | The command string containing variable placeholders to be replaced. |
| `comment` | `char *` | Buffer to receive a descriptive comment about the replacements performed. |
| `commentlen` | `int` | Size of the `comment` buffer. |

**Returns**

A `gbuffer_t *` containing the command string with all CLI variable placeholders replaced, or NULL on failure.

**Notes**

Placeholders use the `$$(...)` syntax. The referenced values are read and base64-encoded before substitution into the command string.

---

(upper)=
## [`upper()`](https://github.com/artgins/yunetas/blob/7.25.20/kernel/c/gobj-c/src/helpers.c#L7392)

Converts all characters in the string to uppercase.

```C
char *upper(
    char *s
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `s` | `char *` | The string to convert. Modified in place. |

**Returns**

Returns a pointer to the modified string `s`.

**Notes**

The function modifies the input string in place. If `s` is NULL, it is returned unchanged.

---

