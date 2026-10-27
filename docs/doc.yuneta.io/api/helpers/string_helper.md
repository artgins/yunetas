# String

String utilities used throughout Yuneta: trimming, case conversion, tokenisation, safe printing, numeric parsing and formatting.

Source code:

- [`helpers.h`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.h)
- [`helpers.c`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c)

(all_numbers)=
## [`all_numbers()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L1059)

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
## [`bin2hex()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L8396)

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
## [`build_path()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L911)

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
## [`change_char()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L1331)

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
## [`count_char()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L9132)

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
## [`delete_left_blanks()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L1132)

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
## [`delete_left_char()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L822)

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
## [`delete_right_blanks()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L1109)

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
## [`delete_right_char()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L801)

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
## [`get_key_value_parameter()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L3890)

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
## [`get_last_segment()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L988)

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
## [`get_parameter()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L1349)

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
## [`helper_doublequote2quote()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L1044)

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
## [`helper_quote2doublequote()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L1029)

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
## [`hex2bin()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L8364)

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
## [`idx_in_list()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L4242)

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
## [`is_secret_name()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L1637)

Tells, by its NAME, whether a key holds a secret -- for what cannot be known
by a declaration ([`SDF_SECRET`](#SDF_SECRET)): the free keys of a command
that is forwarded (`SDF_WILD_CMD`, the agent's `command-yuno`), a config
variable, a field of an audit record. It is the one list of the SDK: the
command traces ([`command_mask_secret_kw()`](#command_mask_secret_kw),
[`command_mask_secret_line()`](#command_mask_secret_line)),
[`gobj_mask_secret_config()`](#gobj_mask_secret_config) and the agent's
audit record all ask it (the audit record, its stricter twin).

A name is a secret's, in any case, when it holds one of `passw`, `pwd`,
`passphrase`, `secret`, `token`, `jwt`, `bearer`, `authorization`, `cookie`,
`credential`, `salt`; or one of `apikey`, `sessionid`, `sessionkey`,
`authdata` once `_`, `-`, `.` and blanks are taken out (`api_key`,
`X-Api-Key`, `__session_id__`); or one of `pass`, `pin`, `otp` as a WHOLE
segment -- a run of letters and digits: `pass`, `db_pass`, `user-pin`,
`sim_pin`, `user[pin]`, `otp` are secrets, and `passive`, `bypass`, `compass`,
`ping`, `mapping`, `spinner`, `otpx`, `pin2` are not (7.27.0+); or both `priv`
and `key` (`private_key`). Not secrets: the PATH of a key or a certificate
(`ssl_certificate_key`), a public `cert_pem`, `max_sessions`, `authz`,
`auth_method`; and a name with a SEGMENT (split by `_`, `-`, `.` and blanks)
that names something ABOUT a credential: `endpoint`, `url`, `uri`, `domain`,
`path`, `file`, `public`, `pub`, `count`, `counts`, `type`, `name`, `len`,
`length`, `size`, `max`, `min`, `ttl`, `timeout`, `expiry`, `expires`, `mode`
(`token_endpoint`, `cookie_domain`, `jwt_public_keys`, `refresh_token_count`,
`pin_count`). The agent's audit record does not take that last exception: it
asks [`is_secret_name_any()`](#is_secret_name_any).

`key` and `cert` alone are NOT secrets' names, on purpose: a `key` is the key
of a record of timeranger as often as a credential (`delete-key key=K`, the
`keys` of a topic), and a certificate is public -- a path to one, or its PEM.
A secret by its VALUE is masked whatever its name: a JWT, the credential
after `Bearer` or `Basic` ([`mask_secrets_inline()`](#mask_secrets_inline)).

**A known effect, accepted: an ordinary name with one of those segments is a
secret's too.** `pass`, `pin` and `otp` are whole segments, so `gpio_pin` (a
pin of a board), a `pass` that counts passes, `first_pass` and wattyzer's
`sum_sla_pass` (`c_db_history_wz.c`) are taken for secrets: a trace or a dump
shows their values as `********`. It errs on the safe side -- telling them from
`db_pass` or `sim_pin` would need to know what a name means. The names are
compared in ASCII, whatever the locale (7.27.0+: a Turkish `LC_CTYPE` did not
take `PIN` for `pin`).

An attribute flagged [`SDF_SECRET`](#SDF_SECRET) must have a name this
function takes: [`gclass_create()`](#gclass_create) refuses the gclass
otherwise (7.27.0+, not on the ESP32 port).

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
is_secret_name("sim_pin", 7);           // TRUE (pin, a whole segment)
is_secret_name("db_pass", 7);           // TRUE (pass, a whole segment)
is_secret_name("mapping", 7);           // FALSE (pin inside a word)
is_secret_name("passive", 7);           // FALSE (pass inside a word)
is_secret_name("ssl_certificate_key", 19); // FALSE (a path)
is_secret_name("key", 3);               // FALSE (a record key as often)
is_secret_name("token_endpoint", 14);   // FALSE (about a token)
is_secret_name("username", 8);          // FALSE
is_secret_name("gpio_pin", 8);          // TRUE: the known effect above
```

---

(is_secret_name_any)=
## `is_secret_name_any()`

Like [`is_secret_name()`](#is_secret_name), without its exception: a name
that holds a part of a secret's name is TRUE whatever other segment it has
(`max`, `mode`, `type`, `url`, ...). For a record that is kept and must never
hold a secret -- the agent's audit log asks it -- where a name ABOUT a
credential redacted costs nothing and one that holds a secret in clear does.
Up to 7.25.21 the audit record asked `is_secret_name()`, and wrote
`api_key_max=...` or `token_mode=...` in clear.

```C
BOOL is_secret_name_any(
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

`TRUE` if it holds a part of a secret's name; `FALSE` if not, or when `name`
is NULL.

**Example**

```C
is_secret_name_any("token_endpoint", 14);   // TRUE  (is_secret_name(): FALSE)
is_secret_name_any("api_key_max", 11);      // TRUE  (is_secret_name(): FALSE)
is_secret_name_any("pin_count", 9);         // TRUE  (is_secret_name(): FALSE)
is_secret_name_any("smtp_password", 13);    // TRUE
is_secret_name_any("public_url", 10);       // FALSE
```

---

(left_justify)=
## [`left_justify()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L1158)

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
## [`json_mask_secrets()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L3880)

A json as a log or a trace may show it. At any depth, the value of a key whose
name is a secret's ([`is_secret_name()`](#is_secret_name)) is `"********"`,
whatever its json type (not an absent one, a null or an empty string, so "not
set" still shows); so is the `value` of a dict whose `attribute` names a
secret (a write-attr); and every string, under ANY key, is masked as
[`mask_secrets_inline()`](#mask_secrets_inline): a JWT, the credential after
`Bearer` or `Basic`, a secret `name=value` or `name: value` in it (7.27.0+:
up to 7.26.9 a JWT under a plain key showed). A string with a NUL inside (a
frame of a peer is decoded with `JSON_ALLOW_NUL`) is looked at past the NUL
too. A KEY is judged, and copied, by all its bytes: a key with a NUL (a parser
refuses one, C code can build it) was judged up to the NUL, its value shown,
and the copy held the key cut (7.27.0+). What
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
card): a name is judged by its last 128 bytes, and the walk has a budget of
4 MB units -- every node costs one, a key or a string its bytes too. Once it
is spent the walk STOPS, and one `"<not shown: too large to mask>"` stands
for the rest of each container being walked (a `"<more>"` key in a dict, a
last item in a list) -- never in clear.

**Limits**

A key is judged as a NAME, never as a value: a key that is itself a credential
-- a dict keyed by tokens, by session ids -- is shown as it is (its value is
masked only when the key has a secret's name). gobj-js's `json_mask_secrets()`
has the same limit.

```C
json_t *jn = json_pack("{s:s}", "eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiJib2IifQ.c2ln", "bob");
json_t *jn_shown = json_mask_secrets(jn);
// {"eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiJib2IifQ.c2ln": "bob"}: the key as it is
JSON_DECREF(jn_shown)
```

**Example**

```C
json_t *kw = json_pack("{s:I, s:{s:s}, s:s, s:s, s:s, s:s}",
    "password", (json_int_t)1234,
    "auth", "access_token", "eyJ...",
    "__command__", "set-user-pwd username=bob password=hunter2",
    "note", "eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiJib2IifQ.c2lnbmF0dXJl",
    "headers", "Authorization: Bearer abc123",
    "url", "https://cdn.public.example/x?access_token=abc123"
);
json_t *kw_shown = json_mask_secrets(kw);
// {"password": "********", "auth": {"access_token": "********"},
//  "__command__": "set-user-pwd username=bob password=********",
//  "note": "********", "headers": "Authorization: ********",
//  "url": "https://cdn.public.example/x?access_token=********"}
JSON_DECREF(kw_shown)
```

---

(mask_secrets_inline)=
## [`mask_secrets_inline()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L3066)

A text (a command line) with the value of every `name=value` whose name is a
secret's ([`is_secret_name()`](#is_secret_name)) written as `********`,
quoted or not; the same for a `name: value`, its name in quotes or not -- a
json, a header, a label: `"password": hunter2`, `Authorization: Basic x`,
`bearer: tok` (7.27.0+) --; and the `value=` of a write-attr whose
`attribute=` names a secret. A value that is a scheme and its credential is
masked WHOLE: `token=Bearer abc` is `token=********`, and so is
`Authorization: Bearer abc` -- `Authorization: ********` (up to 7.26.9 it
showed as it was: no `name: value` was masked). And a credential by its SHAPE,
wherever it is and whatever names it (below). The rest of the text is kept as
it is.

**Each rule judges the text as it came** (7.27.0+): the bytes to mask are
marked, and a run of them is ONE `********`. A rule that masked (or jumped
over) its value before the next one looked hid from it what it judges by: a
label's value hid the `=` of the `name=value` inside it (`x-auth-token:
password=a,b` showed `,b`). The rule of the `name=value` reads the text with
the names of now, jumping over nothing, so a `name=value` inside the word
after `Bearer` is judged even when that word is no credential (`Bearer pwd=x`
is `Bearer pwd=********`); and, wherever they can read it otherwise, as two
released versions read it, each reading whole: as 7.26.9 read it, with its
names -- `pass`, `pin` and `otp` were none, so the value of a name of a
segment jumped over no `name=value` after it: `db_pass='password'=hunter2` is
`db_pass=********=********` --, and as gobj-js 7.26.20 read it, jumping over
its shapes. Measured position by position against the library of 7.26.9, on
438 008 generated texts: nothing that 7.26.9 masked shows, inline or in a
dump. Against gobj-js 7.26.20, what shows is the word after a bare `Bearer`
that cannot be a credential (below), and the value of a parameter whose word
holds `pass`, `pin` or `otp` only before its last `&`, `?` or `;` (next
paragraph), and nothing else.

The name of a `name=value` is the word before the `=` (since the last blank,
or the last `=` of the word: `'password'=x`, `user[password]=x` are names
too). In that word `pass`, `pin` and `otp` count only after its last `&`, `?`
or `;`: before it they are words of the value of another parameter, so
`GET /search?q=boarding+pass&lang=es` is left as it is (in the first build of
7.27.0 it was `lang=********`), while `?q=1&db_pass=x` is
`?q=1&db_pass=********`. The other parts count in the whole word, as in
7.26.9: `/static/token.png?v=2` is `/static/token.png?v=********`. The name is
also the run of name characters just before the `=` -- letters,
digits, `_`, `-`, `.` --, as in a query string (7.27.0+):
`GET https://my-file-server.com/get?token=abc&x=1` is
`GET https://my-file-server.com/get?token=********&x=1`. Its word holds
`file`, a segment about a credential, and up to 7.26.9 the token showed.
Under that name the value runs to a blank or one of `&;,"'<>`, as in a dump.
And the name as a form or a query writes it (7.27.0+): the run of name
characters, brackets, quotes and `%` before the `=`, its `%XX` decoded, is a
secret's when one of its parts between the brackets and the quotes is --
`?user[password]=x`, `?user%5Bpassword%5D=x`, `?t%6Fken=x`, `'password'=x`
(the trailing run of name characters after the last bracket or quote is the
rule above's). Its value runs to the same stops. For this reading a part about
a credential is no secret's name (`token_type` of `user[token_type]`), but the
word before the `=` still masks the value, as in 7.26.9: `user[token_type]`
holds `token`, and `type]` is no segment about a credential (the segments are
split by `_`, `-`, `.` and blanks), so `user[token_type]=bearer` is
`user[token_type]=********`. A dump does not read that word
([`mask_secrets_in_text()`](#mask_secrets_in_text)), and shows it as it is.

A URL or a query `%`-encoded inside the value of a parameter is read decoded
too (7.27.0+), as the `redirect_uri=`, `next=` or `state=` of an OAuth log
carry one: the rules are run again on a view of the text with each `%XX` of
an ASCII character decoded (not `%00`, and not `%C3%A9`: a byte over `0x7F`
stays encoded), and what they mask in the view is masked on the characters it
came from, a `%XX` whole. The view is decoded twice at most, so a value
encoded inside an encoded value (`%253D`, a name encoded twice:
`%2570assword`) is read too; a third level is not.

| Text | Shown |
|---|---|
| `GET /cb?next=%2Flogin%3Fpassword%3Dhunter2` | `GET /cb?next=%2Flogin%3Fpassword%3D********` |
| `url=https%3A%2F%2Fbob%3Ahunter2%40host` | `url=https%3A%2F%2Fbob%3A********%40host` |
| `redirect_uri=https%3A%2F%2Fapp%2Fcb%3Ftoken%3Dabc123&state=1` | `redirect_uri=https%3A%2F%2Fapp%2Fcb%3Ftoken%3D********` |
| `state=%7B%22token%22%3A%22abc123%22%7D` | `state=%7B%22token%22%3A********%7D` |
| `%2570assword=hunter2` | `%2570assword=********` |

In the third row the value of `token=` runs, decoded, to the next blank: a
`name=value` of a command line runs to the next `word=` (below), so
`&state=1` goes with it. 7.26.9 masked `state` there already (the word before
its `=` holds `token`), and showed the token.

The password of the userinfo of a URL, `scheme://user:password@host`, is
masked too (7.27.0+): from the first `:` after the `://` to the last `@`
before a `/`, `?`, `#`, a blank, a quote, `<`, `>` or an end of line --
`https://bob:hunter2@git.example/x` is `https://bob:********@git.example/x`.
A user alone (`ssh://git@host`) and a port (`http://[::1]:8080/`) are kept.
The name of a `name: value` is the word before the `:` (since the last
blank, `=` or `:`: `{"password": x`, `password12:30` are names too; `pass`,
`pin` and `otp` count after its last `&`, `?` or `;`, as above). Each is
judged by its last 128 bytes; a text over 4 MB, or one whose masked copy
cannot be allocated, is `"<not shown: too large to mask>"`: it is linear, and
fails safe. An unquoted value of a `name=value` runs to the next `word=` (or
the end): a password written with blanks (`password=correct horse battery`,
`password= hunter2`) is masked whole, as the parser leaves those words unread.
A quote inside it is part of it (`value=ab'cd` is masked whole), unless it is
the quote that closes an outer quoted value (`command='set-user-pwd
password=x'` keeps its closing quote). The value of a `name: value` is a
quoted text to its closing quote, a json object or list to its closing
bracket, the text of a json inside a json, its quote escaped, to the quote
escaped the same way (`x='{\"password\":\"hunter2\"}'` is
`x='{\"password\":********}'`; it showed `hunter2`), a scheme and the word
after it, or one word, to a blank or one of `,;&)]}<>"'`.

**By its shape** (7.27.0+), the shapes the agent's audit record redacts:

- A JWT or a JWE: `eyJ` (the base64url of `{"`) at the start of a word,
  base64url, and a `.` after it -- three parts joined by `.` (a JWS), five (a
  JWE), or fewer when a cap cut it: the first 64 bytes of a JWT keep its header
  and part of its payload. The dots at the end of the run are not its own: a
  JWT ends a sentence (`... eyJ.eyJ.sig.`). A run with no `.`
  (`eyJhbGciOiJIUzI1NiJ9`, a base64url json) is no JWT, and neither is one
  whose only dots end it (`eyJhIjoxfQ.`: a header, 7.27.0+); where the text
  was cut (a dump, the window of a capped log line), a run that reaches the
  end is masked whatever its dots.
- The credential after the scheme word `Bearer` or `Basic` (any case, a word
  of its own, a blank after it). It is masked when it CAN be a credential:
  - it is in quotes (`Bearer "tok"`), or follows the scheme written twice
    (`Bearer Bearer tok`, a header prefixed twice): whatever its shape;
  - else it is the word after the blanks, as far as the token after
    `Bearer` went in gobj-js 7.26.20 and in the agent's audit record of
    7.26.9 -- to a blank, an end of line, a quote or one of `,;}]` --, masked
    WHOLE, its dots at the end too, when it is **not a word of prose**, judged
    without those dots (the end of a sentence). A word of prose is ASCII
    letters only, in runs joined by one `-`, `.` or `/` (`self-service`,
    `e.g`, `and/or`), each run in lower case, upper case or capitalized
    (`bond`, `Bond`, `BOND`), and shorter than 24 characters. A credential
    holds a digit, a symbol of its own (`_~+=%`...) or a character that is no
    ASCII, mixes its case past its first letter, or is long. For `Bearer` it
    has 8 characters at least; for `Basic` its run of base64 decodes to bytes
    that hold a `:` (a `user:password`), whatever follows the run in the word
    (`(Basic dXNlcjpwYXNz)`) and whatever its length. In the first commit of
    V24 the token was cut at the first byte of no token68 (`Bearer
    abc%2Bdefghijk` showed it all: `abc` was too short).

  | Text | Shown |
  |---|---|
  | `the bearer of this letter` | as it is: `of` is prose |
  | `Pay the Bearer Bond.` | as it is |
  | `the bearer self-service`, `bearer BOND` | as it is |
  | `a basic plan`, `Basic Operations` | as it is |
  | `Basic YWRtaW4=` | as it is: the base64 of `admin` holds no `:` |
  | `Bearer short1` | as it is: shorter than a token |
  | `Bearer mF_9.B5f-4.1JqM` (RFC 6750) | `Bearer ********` |
  | `bearer abcdEFGHijkl`, `Bearer 12345678` | `bearer ********`, `Bearer ********` |
  | `Bearer abcdefghijklmnopqrstuvwxyz` | `Bearer ********`: long |
  | `x Basic dXNlcjpwYXNz y` (`user:pass`) | `x Basic ******** y` |
  | `basic YTpi` (`a:b`) | `basic ********` |
  | `Bearer Bearer TOKEN` | `Bearer Bearer ********` |
  | `Bearer "tok"` | `Bearer ********` |
  | `x Bearer abc%2Bdefghijk y` | `x Bearer ******** y`: the word whole |
  | `sent Bearer abcdefgh12.` | `sent Bearer ********`: its dot too |
  | `Bearer pwd=x` | `Bearer pwd=********`: a name=value inside |

  Up to 7.26.9 this function masked no credential by its shape: a JWT, or the
  word after `Bearer`, showed unless a secret's name came before it (the
  agent's audit record redacted the word after `Bearer ` whatever it was, and
  still does). gobj-js 7.26.20 masked the word after `bearer` whatever it was
  (`the bearer ******** this letter`), and `Basic`, a quoted token, `Bearer
  Bearer x`, a JWE and a JWT cut by a cap were not masked. So what gobj-js
  7.26.20 masked and this rule shows is the word after a bare `Bearer` (not
  quoted, not after the scheme again, not under a secret's name): a word
  shorter than 8 characters, a word of prose of 8 to 23, or one of dots
  only. Of 232 444 generated texts, 8 876 show such a word, and nothing else
  that gobj-js 7.26.20 masked: tokens of 1 to 7 characters of any alphabet,
  and of 8 to 23 lower-case, upper-case or capitalized letters.

A secret's name followed by a `:` is read as a label, so its next word is
masked even in a sentence: `Wrong token: try again` is
`Wrong token: ******** again`. It errs on the safe side, as the names do
([`is_secret_name()`](#is_secret_name)), and so does the word before a `=`:
when a path carries a secret's name, the query after it is masked
(`/static/token.png?v=2` is `/static/token.png?v=********`, `?token&x=1` is
`?token&x=********`). 7.26.9 masked them so, and that word is how
`/reset_password?k=hunter2` is masked.

**What it does not mask**, by decision: a name that is no secret's even when
its value is a credential of a protocol the SDK does not speak -- OAuth's
`code=` (one use, minutes of life, and `code` names status, error and
country codes everywhere), `X-Amz-Signature=`, Azure's `sig=` (`signature`
names attack signatures in webstats); a `user:password@host` with no `://`
before it (a DSN, an scp target); and a token written as the user of a URL
(`https://TOKEN@host`: the user is kept).

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
s = mask_secrets_inline("curl -H 'Authorization: Bearer abc123' url");
// "curl -H 'Authorization: ********' url"
GBMEM_FREE(s)
s = mask_secrets_inline("{\"password\": \"hunter2\", \"user\": \"bob\"}");
// "{\"password\": ********, \"user\": \"bob\"}"
GBMEM_FREE(s)
s = mask_secrets_inline("note=eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiJib2IifQ.c2ln x=1");
// "note=******** x=1"
GBMEM_FREE(s)
s = mask_secrets_inline("cut eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiJ");  // "cut ********"
GBMEM_FREE(s)
s = mask_secrets_inline("sent Bearer mF_9.B5f-4.1JqM, twice");
// "sent Bearer ********, twice"
GBMEM_FREE(s)
s = mask_secrets_inline("the bearer of this letter");  // NULL: prose
GBMEM_FREE(s)
s = mask_secrets_inline("pin=1234 ping=1");  // "pin=******** ping=1"
GBMEM_FREE(s)
s = mask_secrets_inline("GET https://my-file-server.com/get?token=abc&x=1");
// "GET https://my-file-server.com/get?token=********&x=1"
GBMEM_FREE(s)
s = mask_secrets_inline("db_pass='password'=hunter2");  // "db_pass=********=********"
GBMEM_FREE(s)
s = mask_secrets_inline("GET https://my-file-server.com/get?user%5Bpassword%5D=abc&x=1");
// "GET https://my-file-server.com/get?user%5Bpassword%5D=********&x=1"
GBMEM_FREE(s)
s = mask_secrets_inline("clone https://bob:hunter2@git.example/x.git");
// "clone https://bob:********@git.example/x.git"
GBMEM_FREE(s)
s = mask_secrets_inline("the header eyJhIjoxfQ.");  // NULL: its only dot ends it
GBMEM_FREE(s)
s = mask_secrets_inline("x-auth-token: password=a,b c");  // "x-auth-token: ********"
GBMEM_FREE(s)
s = mask_secrets_inline("update-node x='{\\\"password\\\":\\\"hunter2\\\"}'");
// "update-node x='{\\\"password\\\":********}'"
GBMEM_FREE(s)
```

---

(mark_secrets_inline)=
## `mark_secrets_inline()`

The bytes of a text that [`mask_secrets_inline()`](#mask_secrets_inline) masks,
MARKED in a map of bits, for a writer of its own that must hide at least what
`mask_secrets_inline()` hides, with the same rules: the agent's audit record
writes each run of marked bytes it copies as `<redacted>`, so what the SDK
masks, the record never writes. The text is `len` bytes, not NUL-terminated (a
json string with a NUL inside is judged past it). Marks are only ever set,
never cleared: a caller can add its own.

```C
size_t mark_secrets_inline(
    const char *text,
    size_t      len,
    uint8_t    *marks   // (len + 7) / 8 bytes, zeroed
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `text` | `const char *` | The text. |
| `len` | `size_t` | Its bytes. |
| `marks` | `uint8_t *` | A map of `(len + 7) / 8` bytes, zeroed by the caller: bit `i % 8` of byte `i / 8` is set for byte `i` to mask. |

**Returns**

The bytes marked; `0` when there is nothing to mask.

**Example**

```C
const char *text = "login user=bob password=hunter2";
size_t len = strlen(text);
uint8_t marks[(sizeof("login user=bob password=hunter2") + 7) / 8] = {0};
size_t n = mark_secrets_inline(text, len, marks);   // 7: the bytes of hunter2
for(size_t i = 0; i < len; i++) {
    BOOL masked = (marks[i / 8] >> (i % 8)) & 1;    // TRUE for i 24 to 30
}
```

---

(mask_secrets_inline_capped)=
## `mask_secrets_inline_capped()`

A text as a log line shows it, CAPPED: masked as
[`mask_secrets_inline()`](#mask_secrets_inline) masks it, and THEN cut to
`bfsize - 1` bytes (a whole UTF-8 character) into `bf`. Capped first, a
credential that the cap cut lost its shape and showed: `Bearer abcdefg` was
all that a cap of 64 left of a long token, too short to be one, and the name
of an event that is not public, the stats and the first word of a command out
of session, and the log line of a peer's app (`EV_REMOTE_LOG`) wrote it. The
bytes judged are the cap and 256 more (the work stays bounded by the cap, not
by what a peer sent); a credential that reaches their end is masked whatever
its shape, and the `value=` of a write-attr is judged by the whole text. So is
the password of a URL whose authority reaches their end: from its first `:` to
their end, `@` or not, since the `@` that ends it may lie past them
(7.27.0+), as at the end of a dump
([`mask_secrets_in_text()`](#mask_secrets_in_text)): the userinfo ends at the
LAST `@`, and an IPv6 literal followed by a port (only digits to that end)
keeps its colons. When the bytes judged hold a `%XX`, they are read decoded
too, and there the literal is kept only when its `[` is written as it is (not
`%5B`) and its bytes as written, from that `[` to the end, are 118 at most
(the longest literal and its port): `password=` and 300 `x`,
` url=https://[fe80::1%25eth0]:1600 ` and 300 `z`, with a cap of 86, is
`password=******** url=https://[fe80::1%25eth0]:********`, and
`next=https%3A%2F%2F%5B2001%3Adb8%3A%3A1%5D%3A1600` cut in its port is
masked from `%3A` after `2001`. Only the `[` has to be written as it is: the
rest may be encoded, so `url=https://[2001%3Adb8%3A%3A1]%3A1600` cut in its
port keeps the literal and masks `%3A1600`. Encoded, a literal takes up to
three times its bytes. The 256 bytes
past the cap keep this away from what is shown unless the authority runs from
inside the cap to 256 bytes past it: `url=http://[2001:db8::1]:8080/p` with a
cap of 20 is `url=http://[2001:db`, as cut. A masked value is shorter than the bytes it hides, so the end of
the bytes judged can be in the copy shown: with a cap of 18,
`url=https://bob:` and 300 `P` and `@host/p` showed `url=https://bob:P`.

```C
size_t mask_secrets_inline_capped(
    char       *bf,
    size_t      bfsize,
    const char *text,
    size_t      len
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Where the text shown goes, NUL-terminated. |
| `bfsize` | `size_t` | Its size: `bfsize - 1` bytes of text at most. |
| `text` | `const char *` | The text. |
| `len` | `size_t` | Its first `len` bytes are shown (the first word of a line, `strcspn(line, " \t")`). |

**Returns**

`strlen(bf)`.

**Example**

```C
char name[65];
mask_secrets_inline_capped(name, sizeof(name), event_, strlen(event_));
// "EV_X Bearer abcdefghij12345" in 17 bytes: "EV_X Bearer ****", never "EV_X Bearer abcd"
gobj_log_warning(gobj, 0, "msg", "%s", "Frame of an event that is not public, refused",
    "event", "%s", name, NULL);

char shown[18];
mask_secrets_inline_capped(shown, sizeof(shown), url_text, strlen(url_text));
// "url=https://bob:" + 300 x "P" + "@host/p", its '@' past the 273 bytes judged:
// "url=https://bob:*", never "url=https://bob:P"
```

---

(json_mask_secrets_capped)=
## [`json_mask_secrets_capped()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L3871)

[`json_mask_secrets()`](#json_mask_secrets) on a budget of `max_bytes`
units: every node walked costs one, a key or a string its bytes too. Once it
is spent the walk stops, and one `"<not shown: too large to mask>"` stands
for the rest of each container being walked. A string is walked whole or not
shown at all, so a secret is never cut in half. For a dump of a few bytes of
a kw from a peer: the work is bounded by the cap, not by what was sent -- a
16 MB frame of `[{},{},...]` costs what 1024 units cost.
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
## [`mask_secrets_in_text()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L3658)

The bytes of a traffic dump with the credentials that can be told soundly
written as `*`, IN PLACE, the length kept (so the offsets and the rest of the
dump stay as they were):

- the value of an HTTP `Cookie:`, `Set-Cookie:`, `Authorization:` or
  `Proxy-Authorization:` header at the start of a line (for the last two
  after the scheme: `Authorization: Bearer ******`);
- the value of a `name=value` whose name is a secret's
  ([`is_secret_name()`](#is_secret_name)), as in a query string, a form body
  or a command line -- its name the run of name bytes before the `=`, or the
  name as a form writes it, a part of it in brackets or quotes, `%XX`
  decoded: `user[password]=x`, `user%5Bpassword%5D=x`, `'password'=x`,
  `t%6Fken=x` (7.27.0+; up to then a dump showed them);
- the password of the userinfo of a URL, `scheme://user:password@host`
  (7.27.0+), and, from its first `:` to the end, an authority that reaches
  the END of the bytes, `@` or not: the `@` may come in the next read
  (`GET https://u:hunter2` is `GET https://u:*******`), and the userinfo ends
  at the LAST `@`, so an `@` already in the bytes does not end it -- a
  password written with a raw `@` (`https://u:p@ss@host`), a user that is an
  email address (`GET imap://bob@example.com:hunter2` is
  `GET imap://bob@example.com:*******`). A port there is masked too (`to
  http://host:8080` is `to http://host:****`), and so is a host after a whole
  userinfo (`x https://u:hunter2@host:8080` is `x https://u:*****************`):
  the bytes do not say they are not a password's. The colons of an IP literal
  followed by a port are no password's: `see http://[::1]:8080` is
  `see http://[::1]:****`, `[fe80::1%25eth0]:80` and `[::ffff:192.0.2.1]:80`
  keep their literal too. Only an IPv6 literal counts -- eight hex groups of
  four digits at most, one `::` in place of one or more, a dotted IPv4 quad
  with no leading zero in place of the last two, a zone of 64 bytes at most
  with no `:` -- and only when one to five digits run from its `]:` to the
  end: a userinfo can begin with `[`. Anything else is masked from the first `:`: `x https://[dead:beef]:z@h` is
  `x https://[dead:*********`, `x https://[a%x:pass]:80` is
  `x https://[a%x:********`, `see http://[::1]:8o80` is
  `see http://[:********`, and so is a literal cut before its `]`. What follows
  the `@` of a URL `%`-encoded inside a value, read decoded, is masked too: a
  `&` is no end of an authority (`url=https%3A%2F%2Fbob%3Ax%40host&x=1` at the
  end of the bytes masks `x%40host&x=1`);
- the value of a json `"name": value` -- and of a `'name': value`, a Python
  dict or a log of one (7.27.0+) --, and of a `name: value` -- another
  header, a label: `X-Auth-Token: x`, `bearer: tok`, `user[password]: x`
  (7.27.0+) -- whose name is a secret's;
- a URL or a query `%`-encoded inside a value, read decoded by the rules above,
  twice at most, as [`mask_secrets_inline()`](#mask_secrets_inline) reads it
  (7.27.0+): `GET /cb?next=%2Flogin%3Fpassword%3Dhunter2 HTTP/1.1` is
  `GET /cb?next=%2Flogin%3Fpassword%3D******* HTTP/1.1`;
- under such a name a value that is a scheme and its credential is masked
  WHOLE: `token=Bearer abc`, `"password": Bearer abc`, `password=Basic xyz`.
  Up to 7.26.9 only the scheme was masked, and the token after it showed;
- a JWT and the credential after `Bearer` or `Basic`, wherever they are in the
  bytes, by the rule of [`mask_secrets_inline()`](#mask_secrets_inline) (a JWE,
  a JWT the window of a dump cut, a quoted credential; prose is left as it
  is) -- and at the END of the bytes whatever its shape: a dump is the first
  bytes of a frame, or one read of a stream, and its end may cut a
  credential (`Bearer abcdefg` of a long token, a JWT before its first `.`).
  So a dump that ends on `a basic plan` shows `a basic ****`.

Each rule judges the bytes as they came, and the rules of 7.26.9's dump are
kept whole as one pass, read with its names (7.27.0+): a value masked in place
by one rule hid the name or the `=` of another (the label `x-auth-token:
u?password="x"` showed the quoted value of `password=`), and the value of a
`pin=` -- a secret's name since 7.27.0 -- hid the `=` after it from the rule
that masked it in 7.26.9 (`pin=1234Authorizationcommand='x'` showed `x`).
Measured position by position against the library of 7.26.9 on 438 008
generated texts, nothing that its dump masked shows. A JWT header whose only
dots end it is no JWT (`eyJhIjoxfQ. x` shows as it is), but at the end of the
bytes, where a cap may have cut it (`cut eyJhIjoxfQ.` is `cut **********.`).

A byte already masked is not counted twice. The traffic dumps use it
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

char text2[] = "{\"msg\": \"sent with Bearer brr-hunter2, twice\", \"pin\": 7}";
mask_secrets_in_text(text2, strlen(text2));
// "{\"msg\": \"sent with Bearer ***********, twice\", \"pin\": *}"

char text3[] = "GET /x?token=Bearer tb-hunter2&x=1\r\nX-Auth-Token: xa-hunter2\r\n";
mask_secrets_in_text(text3, strlen(text3));
// "GET /x?token=*********************\r\nX-Auth-Token: **********\r\n":
// the word after "Bearer" goes on to a blank, an end of line, a quote or ,;}]

char text4[] = "the bearer of this letter, a basic plan.\r\n";
mask_secrets_in_text(text4, strlen(text4));     // 0: prose

char text5[] = "{'password': 'sq-hunter2'}, sent with Bearer abcdefg";
mask_secrets_in_text(text5, strlen(text5));
// "{'password': ************}, sent with Bearer *******": the end may cut a token

char text6[] = "POST /login\r\n\r\nuser%5Bemail%5D=a%40b.c&user%5Bpassword%5D=hunter2";
mask_secrets_in_text(text6, strlen(text6));
// "POST /login\r\n\r\nuser%5Bemail%5D=a%40b.c&user%5Bpassword%5D=*******"
```

**Limits**

It sees ONE buffer: what tells a value from the rest (the name before it, the
header it is in) must be in the same bytes. A dump is made per read or per
write, and TCP cuts a stream where it likes, so a credential whose name ends
one read and whose value begins the next is shown in the second dump:

```C
char read1[] = "user=bob&password=";
char read2[] = "hunter2&next=1";
mask_secrets_in_text(read1, strlen(read1));    // 0: no value in it
mask_secrets_in_text(read2, strlen(read2));    // 0: "hunter2" has no name here
```

The same for a value with no name next to it (a binary protocol, an AUTH line
in base64). Masking received bytes is a best effort, for the traffic of others
that no sender marks. What we SEND that holds a secret is marked at its
source with [`gbuffer_set_secret()`](#gbuffer_set_secret) (or
`"__secret__": true` in the kw of `EV_TX_DATA` to `C_TCP`), and is never
dumped (`"<N bytes hidden>"`), however it is cut.

---

(nice_size)=
## [`nice_size()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L1078)

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
## [`pop_last_segment()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L1010)

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

(replace_string)=
## [`replace_string()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/replace_string.c#L23)

Returns a new string: `str` with every occurrence of `old` replaced by `snew`,
from left to right, without overlaps.

```C
char *replace_string(
    const char *str,
    const char *old,
    const char *snew
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `str` | `const char *` | The string to copy. It is not modified. |
| `old` | `const char *` | The text to replace. Must not be empty. |
| `snew` | `const char *` | The text to put in its place. Can be empty. |

**Returns**

A new string, to free with `gbmem_free()`. `NULL`, logged, when an argument is
`NULL`, `old` is empty, or there is no memory.

**Notes**

The occurrences do not overlap: `"aaa"` with `"aa"` replaced by `"b"` is
`"ba"`. Up to 7.28.4 the result was allocated with libc's `malloc()` and freed
with `free()`; free it with `gbmem_free()` now.

**Example**

```C
char *s = replace_string("Hello $1, $1!", "$1", "Yuneta");   /* "Hello Yuneta, Yuneta!" */
if(s) {
    printf("%s\n", s);
    gbmem_free(s);
}
```

---

(split2)=
## [`split2()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L4032)

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
## [`split3()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L4101)

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
## [`split_free2()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L4081)

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
## [`split_free3()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L4153)

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
## [`str_concat()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L4170)

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
## [`str_concat3()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L4199)

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
## [`str_concat_free()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L4233)

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
## [`str_in_list()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L4265)

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
## [`str_match_regex()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L4287)

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
## [`SWITCHS` / `CASES` / `ICASES` / `CASES_RE` / `DEFAULTS` / `SWITCHS_END`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.h#L96)

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
## [`strntolower()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L1195)

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
## [`strntoupper()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L1176)

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

(utf8_prefix_len)=
## [`utf8_prefix_len()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L1216)

The bytes of `s` that fit in `max_bytes` WITHOUT cutting a UTF-8 character:
the precision to cap a string with, as `"%.*s"`.

```C
int utf8_prefix_len(
    const char *s,
    int max_bytes
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `s` | `const char *` | The string to cap. `NULL` is 0. |
| `max_bytes` | `int` | The most bytes the cap may hold. |

**Returns**

The length of the cap: the whole string when it fits, else at most
`max_bytes`, ending before the character that would be cut.

**Notes**

A cap by bytes (`"%.64s"`) can end in the middle of a character. The json
built with it is refused -- `json_sprintf()` checks UTF-8, and a command's
answer falls back to a generic comment -- or logged broken. The logger's own
`"%.Ns"` uses this function (since SDK 7.27.0), so a log needs nothing; a
comment built with `json_sprintf()` does:

```C
return msg_iev_build_response(gobj,
    -1,
    json_sprintf("%s: Topic not found: '%.*s'",
        gobj_yuno_role_plus_name(), utf8_prefix_len(topic_name, 64), topic_name),
    0,
    0,
    kw  // owned
);
```

`utf8_prefix_len("abc\xc3\xb1z", 4)` is 3: the 4th byte is the first of
`ñ`, which does not fit whole.

---

(utf8_columns)=
## [`utf8_columns()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L1235)

The columns a string takes on a terminal: one per UTF-8 character, not one
per byte.

```C
int utf8_columns(
    const char *s
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `s` | `const char *` | The string to measure. `NULL` is 0. |

**Returns**

The number of characters of `s`: its bytes that are not a continuation byte
(`10xxxxxx`).

**Notes**

`strlen()` is the width of an ASCII string only. A table that widens a
column to its header measures the header with this:

```C
if(fillspace < utf8_columns(header)) {
    fillspace = utf8_columns(header);
}
```

`utf8_columns("Espa\xc3\xb1a")` is 6, and its `strlen()` is 7. One column per
character is right for Latin text. Wide (CJK) and combining characters are
not measured. See [`utf8_prefix_columns()`](#utf8_prefix_columns) to cut and
pad a cell.

---

(utf8_prefix_columns)=
## [`utf8_prefix_columns()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L1253)

The bytes of the longest prefix of `s` that fits in `max_columns` columns
without cutting a character, and the columns that prefix takes.

```C
int utf8_prefix_columns(
    const char *s,
    int max_columns,
    int *columns
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `s` | `const char *` | The text of the cell. `NULL` is 0. |
| `max_columns` | `int` | The width of the cell, in columns. |
| `columns` | `int *` | Out: the columns of the prefix. May be `NULL`. |

**Returns**

The length in bytes of the prefix: the whole string when it fits, else the
characters that fit in `max_columns`.

**Notes**

`"%-*.*s"` pads and cuts by BYTES. Each non-ASCII character of a cell ("·",
"ñ") then pulls every column after it one place to the left, and the cut can
split a character. A cell is written with the two answers:

```C
int cols;
int bytes = utf8_prefix_columns(text, fillspace, &cols);
gbuffer_printf(gbuf, "%.*s%*s ", bytes, text, fillspace - cols, "");
```

The table tools (`ycommand`, `ycli`, `ybatch`, `mqtt_tui`, `msg2db_list`,
`treedb_list`) write their cells this way. For `"a\xc2\xb7b"` (`a·b`, 4
bytes) and a width of 2, it returns 3 bytes and 2 columns.

---

(translate_string)=
## [`translate_string()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L1286)

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
## [`capitalize()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L10339)

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
## [`lower()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L10326)

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
## [`path_basename()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L11011)

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
## [`replace_cli_vars()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L11097)

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
## [`upper()`](https://github.com/artgins/yunetas/blob/7.28.9/kernel/c/gobj-c/src/helpers.c#L10313)

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

