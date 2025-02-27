# **Glossary**

```{glossary}

bool
    BOOL
    ```C
    #ifndef BOOL
    # define BOOL   bool
    #endif
    ```

false
    FALSE
    ```C
    #ifndef FALSE
    # define FALSE  false
    #endif
    ```

true
    TRUE
    ```C
    #ifndef TRUE
    # define TRUE   true
    #endif
    ```

private
    PRIVATE
    Defines a static function or variable with file scope.
    ```C
    #define PRIVATE static
    ```

public
    PUBLIC
    Defines a function or variable with external linkage.
    ```C
    #define PUBLIC
    ```

min
    MIN
    Returns the smaller of `a` and `b`.
    ```C
    #ifndef MIN
    # define MIN(a,b) (((a) < (b)) ? (a) : (b))
    #endif
    ```

max
    MAX
    Returns the larger of `a` and `b`.
    ```C
    #ifndef MAX
    # define MAX(a,b) (((a) > (b)) ? (a) : (b))
    #endif
    ```

array_size
    ARRAY_SIZE
    Returns the number of elements in array `a`.
    ```C
    #ifndef ARRAY_SIZE
    #define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
    #endif
    ```

gobj_define_gclass
    GOBJ_DEFINE_GCLASS
    Define a variable in the **`.c`** file.
    ```C
    #define GOBJ_DEFINE_GCLASS(id) gclass_name_t id = #id
    ```

gobj_declare_gclass
    GOBJ_DECLARE_GCLASS
    Declare a variable in the **`.h`** file.
    ```C
    #define GOBJ_DECLARE_GCLASS(id) extern gclass_name_t id
    ```

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


check_log_result
    Internal function

data_type_t
    Internal type

_rotatory
    Internal data

_duplicate_array
    Internal function

_log_jnbf
    Internal function

_build_stats
    Internal function

match_record
    Internal function

_duplicate_object
    Internal function


gobj_lmethod_t
    Unique pointer that exposes local methods names, defined as:

    In C:
    ```C
    typedef const char *gobj_lmethod_t;
    ```

hgclass
    Handler of a gclass, defined as:

    In C:
    ```C
    typedef void *hgclass;
    ```

hgobj
    Handler of a gobj, defines as:

    In C:
    ```C
    typedef void *hgobj;
    ```


ev_action_t

    In C:
    ```C
    typedef struct {
        gobj_event_t event;
        gobj_action_fn action;
        gobj_state_t next_state;
    } ev_action_t;
    ```

gobj_action_fn

    In C:
    ```C
    typedef int (*gobj_action_fn)(
        hgobj gobj,
        gobj_event_t event,
        json_t *kw,
        hgobj src
    );
    ```

authorization_checker_fn

    The `authorization_checker_fn` is a function pointer type used for checking authorization rules. It evaluates whether a given operation is authorized based on the provided parameters.

    ```c
    typedef BOOL (*authorization_checker_fn)(
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


authentication_parser_fn

    The `authentication_parser_fn` is a function pointer type used for parsing and handling authentication requests. It validates user credentials and provides a response in JSON format.

    ```c
    typedef json_t *(*authentication_parser_fn)(
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

re-launch

    In Yuneta, all yunos inherently support running as daemons. When operating in daemon mode, they are protected against unexpected termination through an internal mechanism:

    **1. Watcher-Worker Mechanism**
    - In the `ps` output, you will see two PIDs associated with a yuno:
      - **Watcher**: This process monitors the main yuno process.
      - **Worker**: The actual yuno performing the primary tasks.

    **2. Re-launch Behavior**
    - If the worker process dies unexpectedly, the watcher process automatically **re-launches** it to ensure high availability.
    - This mechanism guarantees resilience, except when the yuno exits explicitly with `exit(0)`.

    **3. No External Dependencies**
    - **You don't need systemd!**  
      Yuneta’s internal re-launch mechanism eliminates the need for external service managers, as the watcher process fully handles restarts.

    **4. Purpose**
    - The `re-launch` feature enhances reliability, ensuring that the yuno remains operational even in the event of failures or crashes.

    ---

    Yunos running as daemons are highly robust, self-healing, and independent of external service management systems.


istream_h

    ```c
    typedef void *istream_h;
    ```

```

[//]: # (:sorted:)
