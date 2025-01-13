# Authz

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
