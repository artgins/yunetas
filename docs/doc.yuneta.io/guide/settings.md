(settings)=

# **Settings**

The settings of a yuno, defining the tree of gobj's to be created 
and their configuration, can be done with
the function:

```C
    PUBLIC char *json_config(
        BOOL print_verbose_config,
        BOOL print_final_config,
        const char *fixed_config,
        const char *variable_config,
        const char *config_json_file,
        const char *parameter_config,
        pe_flag_t quit
    );
```

Source code in:

- [json_config.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/json_config.c)


The `json_config()` function is a utility in Yuneta designed to generate a final JSON configuration by merging multiple JSON inputs. It supports advanced features like comment handling, variable substitution, and range-based expansion. The function can output the resulting configuration, validate it, and handle errors robustly.


The `json_config()` function merges multiple JSON configurations into a single final configuration string, following a specific order of precedence. It also includes features like comment handling, variable substitution, and range-based expansion for flexibility and advanced use cases.

The return of `json_config()` is a string that must be converted to JSON.

(global_settings)=

## Global Settings
If the final JSON has a key `global`, his value will be used as 
the argument `jn_global_settings` in [`gobj_start_up`](gobj_start_up),
and will be merge to `kw` when creating a `gobj`.


## Features

### 1. **Global Variable Substitution**
The function replaces strings enclosed in `(^^ ^^)` with corresponding values from the `__json_config_variables__` dictionary. This dictionary includes global variables returned by `gobj_global_variables()`.

#### Predefined Variables in `__json_config_variables__`

| **Variable**              | **Description**                          |
|---------------------------|------------------------------------------|
| `__node_owner__`          | Node owner of the Yuno.                  |
| `__realm_id__`            | Realm ID of the Yuno.                    |
| `__realm_owner__`         | Realm owner of the Yuno.                 |
| `__realm_role__`          | Realm role of the Yuno.                  |
| `__realm_name__`          | Name of the realm.                       |
| `__realm_env__`           | Environment of the realm.                |
| `__yuno_id__`             | Unique ID of the Yuno.                   |
| `__yuno_role__`           | Role of the Yuno.                        |
| `__yuno_name__`           | Name of the Yuno.                        |
| `__yuno_tag__`            | Tag of the Yuno.                         |
| `__yuno_role_plus_name__` | Role and name of the Yuno.               |
| `__hostname__`            | Hostname of the system.                  |
| `__sys_system_name__`     | System name (Linux only).                |
| `__sys_node_name__`       | Node name (Linux only).                  |
| `__sys_version__`         | System version (Linux only).             |
| `__sys_release__`         | System release (Linux only).             |
| `__sys_machine__`         | Machine type (Linux only).               |

### 2. **Comment Handling**
Supports single-line comments using the syntax `##^` in JSON strings.

### 3. **Range-Based Expansion**
Expands JSON objects based on ranges defined within the input JSON. For example:
:::json
{
"__range__": [12000, 12002],
"__content__": {
"item(^^__range__^^)": { "id": "(^^__range__^^)" }
}
}
:::
Will expand into:
:::json
{
"item12000": { "id": "12000" },
"item12001": { "id": "12001" },
"item12002": { "id": "12002" }
}
:::

### 4. **Order of Precedence**
The function processes inputs in the following order:
1. **`fixed_config`**: Non-writable configuration.
2. **`variable_config`**: Writable configuration.
3. **`config_json_file`**: Overwrites `variable_config` if specified.
4. **`parameter_config`**: Final overwrites.

---

## Behavior

- **Error Handling:** The function exits or continues based on the `quit` parameter if a JSON parsing error occurs.
- **Verbose Outputs:** If `print_verbose_config` or `print_final_config` is `TRUE`, the function prints the configurations and exits.

---

## Return Value

- Returns a dynamically allocated string containing the final JSON configuration.
- **Important:** The returned string must be freed using `jsonp_free()` to avoid memory leaks.
