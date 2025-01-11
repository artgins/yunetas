# C Structures

## json_t
The `json_t` type is part of the [Jansson](https://jansson.readthedocs.io/) library, used for representing JSON data in C. It provides a flexible way to work with JSON objects, arrays, and primitives such as strings and numbers.

---

## persistent_attrs_t
The `persistent_attrs_t` structure contains function pointers for managing persistent attributes of a Yuno instance. This allows the user to handle attributes in storage or memory during the lifecycle of a Yuno.

```c
typedef struct {
    startup_persistent_attrs_t startup;  /**< Function to initialize persistent attributes */
    end_persistent_attrs_t end;          /**< Function to finalize persistent attributes */
    load_persistent_attrs_t load;        /**< Function to load persistent attributes */
    save_persistent_attrs_t save;        /**< Function to save persistent attributes */
    remove_persistent_attrs_t remove;    /**< Function to remove persistent attributes */
    list_persistent_attrs_t list;        /**< Function to list persistent attributes */
} persistent_attrs_t;
```

**Fields**
- **`startup`**: Initializes persistent attributes.
- **`end`**: Cleans up persistent attributes.
- **`load`**: Loads persistent attributes from storage.
- **`save`**: Saves persistent attributes to storage.
- **`remove`**: Deletes persistent attributes.
- **`list`**: Lists all persistent attributes.

---

## json_function_t
The `json_function_t` is a function pointer type used for processing JSON objects in global command or stats parsers. It takes in JSON data and optional parameters to perform custom parsing or processing tasks.

```c
typedef json_t *(*json_function_t)(
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
