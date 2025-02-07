(authz)=
# **Authz**

Source code in:
- [gobj.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.c)
- [gobj.h](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.h)

# Authentication and Authorization in Yuneta

Authentication and authorization in Yuneta ensure secure identification of users and
validation of their permissions to access resources or perform actions. 
These processes are managed via APIs such as [`gobj_authenticate`](gobj_authenticate)
and [`gobj_user_has_authz`](gobj_user_has_authz), with the ability to use custom or
built-in parsers and checkers.

---

## Core Concepts

(authentication_parser)=
### 1. **Authentication**
Authentication verifies the identity of a user by validating credentials such as tokens or other identifiers.

- **API:** 
```C
json_t *gobj_authenticate(hgobj gobj, json_t *kw, hgobj src)
```

- **Parser:** Defined in the `global_authentication_parser` argument of 
  [`gobj_start_up()`](gobj_start_up). If null, a default parser is used.

The authentication parser:
- Processes the credentials provided in `kw`.
- Returns a JSON response indicating success or failure.

(authorization_checker)=
### 2. **Authorization**
Authorization ensures that an authenticated user has the necessary permissions to perform an action or access a resource.

- **API:**

```c
BOOL gobj_user_has_authz(hgobj gobj_to_check, const char *authz, json_t *kw, hgobj src)
```

- **Checker:** Defined in the `global_authorization_checker` argument of
  [`gobj_start_up()`](gobj_start_up). If null, a default checker is used.

The authorization checker:
- Uses the [`mt_authz_checker`](mt_authz_checker) method if defined in the GClass.
- Verifies if the user has the required permission (`authz`) based on roles and parameters.
- Returns `TRUE` if the user is authorized; otherwise, `FALSE`.

---

## GClass `C_AUTHZ`

Yuneta provides a GClass `C_AUTHZ` with default implementations for authentication and authorization:
- **Authentication:** `PUBLIC json_t *authenticate_parser(hgobj gobj_service, json_t *kw, hgobj src)`
- **Authorization:** `PUBLIC BOOL authz_checker(hgobj gobj_to_check, const char *authz, json_t *kw, hgobj src)`

These functions can be used directly by passing them to `gobj_start_up()`.

---

## Workflow

### Authentication Workflow

1. **Request Authentication:**
    - Call `gobj_authenticate()` with user credentials in `kw`.

2. **Parser Selection:**
    - If the GClass defines `mt_authenticate`, it is called.
    - Otherwise, the `global_authentication_parser` is used.
    - If no parser is provided, the default mechanism is used.

3. **Validation:**
    - Credentials are validated, possibly using external systems (e.g., OAuth2, JWT).

4. **Response:**
    - A JSON response indicates authentication success or failure.

### Authorization Workflow

1. **Request Authorization:**
    - Call `gobj_user_has_authz()` with the required permission (`authz`).

2. **Checker Selection:**
    - If the GClass defines `mt_authz_checker`, it is called.
    - Otherwise, the `global_authorization_checker` is used.
    - If no checker is provided, the default mechanism is used.

3. **Validation:**
    - The checker evaluates the user's roles and permissions against the required `authz`.

4. **Response:**
    - The method returns `TRUE` if authorized or `FALSE` otherwise.

---

## Features

### Authentication
- Support for JWT tokens, OAuth2, and system users.
- Integration with external identity providers for secure token validation.

### Authorization
- Role-based access control (RBAC) for granular permission management.
- Hierarchical roles and services enable complex access control scenarios.

### Integration with GObjs
- Authentication and authorization can be defined per GObj or applied globally.
- Built-in methods in `C_AUTHZ` simplify implementation.

---

## Benefits

- **Flexibility:** Custom parsers and checkers enable tailored authentication and authorization logic.
- **Security:** Centralized access controls ensure consistency and reliability.
- **Simplicity:** Built-in functionality in `C_AUTHZ` reduces development overhead.
