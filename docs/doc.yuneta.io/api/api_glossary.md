# Glossary

```{glossary}
:sorted:

PRIVATE
    Defines a static function or variable with file scope.

PUBLIC
    Defines a function or variable with external linkage.

BOOL
    Boolean type (`true` or `false`).

MIN
    Returns the smaller of `a` and `b`.

MAX
    Returns the larger of `a` and `b`.

ARRAY_SIZE
    Returns the number of elements in array `a`.

json_t
    The `json_t` type is part of the [Jansson](https://jansson.readthedocs.io/) library, used for representing JSON data in C. It provides a flexible way to work with JSON objects, arrays, and primitives such as strings and numbers.


```



## persistent_attrs_t
The `persistent_attrs_t` structure contains function pointers for managing persistent attributes of a Yuno instance. This allows the user to handle attributes in storage or memory during the lifecycle of a Yuno.

```c
typedef struct {
    startup_persistent_attrs_t  startup_persistent_attrs;
    end_persistent_attrs_t      end_persistent_attrs;
    load_persistent_attrs_t     load_persistent_attrs;
    save_persistent_attrs_t     save_persistent_attrs;
    remove_persistent_attrs_t   remove_persistent_attrs;
    list_persistent_attrs_t     list_persistent_attrs;
} persistent_attrs_t;
```

**Fields**
- **`startup_persistent_attrs`**: Initializes persistent attributes.
- **`end_persistent_attrs`**: Cleans up persistent attributes.
- **`load_persistent_attrs`**: Loads persistent attributes from storage.
- **`save_persistent_attrs`**: Saves persistent attributes to storage.
- **`remove_persistent_attrs`**: Deletes persistent attributes.
- **`list_persistent_attrs`**: Lists all persistent attributes.

---

## json_function_fn
The `json_function_fn` is a function pointer type used for processing JSON objects in global command or stats parsers. It takes in JSON data and optional parameters to perform custom parsing or processing tasks.

```c
typedef json_t *(*json_function_fn)(
    void *param1,
    const char *something,
    json_t *kw, // Owned
    void *param2
);
```

**Parameters**
- **`param1`**: A generic parameter for user-defined data.
- **`something`**: A string input for specific parsing.
- **`kw`**: A JSON object (owned by the function).
- **`param2`**: Another generic parameter for user-defined data.

**Return Value**

- Returns a `json_t` object as the result of the processing.

---

## authz_checker_fn
The `authz_checker_fn` is a function pointer type used for checking authorization rules. It evaluates whether a given operation is authorized based on the provided parameters.

```c
typedef BOOL (*authz_checker_fn)(
    hgobj gobj,
    const char *authz,
    json_t *kw,
    hgobj src
);
```

**Parameters**
- **`gobj`**: The GObj that is being authorized.
- **`authz`**: The authorization string to evaluate.
- **`kw`**: A JSON object containing additional parameters for the check.
- **`src`**: The source GObj requesting the authorization.

**Return Value**

- Returns `TRUE` if the operation is authorized, otherwise `FALSE`.

---

## authenticate_parser_fn
The `authenticate_parser_fn` is a function pointer type used for parsing and handling authentication requests. It validates user credentials and provides a response in JSON format.

```c
typedef json_t *(*authenticate_parser_fn)(
    hgobj gobj,
    json_t *kw,
    hgobj src
);
```

**Parameters**
- **`gobj`**: The GObj handling the authentication.
- **`kw`**: A JSON object containing the authentication data.
- **`src`**: The source GObj making the authentication request.

**Return Value**

- Returns a JSON object with the authentication result, typically including a success status and user details.
