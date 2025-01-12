# Glossary

```{glossary}

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

json_function_fn
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

gobj
    A `gobj` (Generic Object) is an **instance** of a  [](#gclass) (Generic Class) within the Yuneta framework. It is a modular, reusable, and event-driven component that encapsulates data, behavior, and state.

```

[//]: # (:sorted:)
