(kwid)=

# **Kwid**

## Overview of `kwid`

`kwid` is a library designed to provide a higher-level abstraction for managing JSON data across multiple programming languages. It extends basic JSON handling capabilities by introducing advanced features like path-based manipulation, cloning, filtering, serialization, and database-like utilities.

In **C**, the library is built on top of the [Jansson library](https://jansson.readthedocs.io/), while in other languages like **JavaScript** and **Python**, it leverages native types (`bool`, `array`, `object` in JS and `list`, `dict` in Python). This design ensures seamless integration with the native JSON structures of each language, enabling consistent behavior and cross-platform portability.

Source code in:
- [kwid.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/kwid.c)
- [kwid.h](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/kwid.h)

---

## Key Features and Goals

1. **Enhanced JSON Management**:
    - Provides functions for advanced JSON manipulations such as cloning, filtering, and path-based access.
    - Supports structured operations with JSON objects, arrays, and dictionaries.

2. **Cross-Language Implementation**:
    - Functions are implemented in **C** using the Jansson library.
    - In **JavaScript**, native types like `object`, `array`, and `bool` replace the need for external libraries.
    - In **Python**, the library will use native types like `list` and `dict`.
    - This cross-language compatibility ensures consistent functionality across environments.

3. **Path-Based Access and Manipulation**:
    - Functions like `kw_find_path`, `kw_set_dict_value`, and `kw_delete` allow for fine-grained control over nested JSON structures using path-based syntax.

4. **Database-Like Utilities**:
    - Provides record-based operations such as `kwid_find_record_in_list`, `kwid_compare_records`, and `kwjr_get`.
    - Enables filtering and matching of JSON data with `kw_clone_by_keys` and `kw_match_simple`.

5. **Customizability**:
    - Supports user-defined behavior through function pointers like `serialize_fn_t`, `deserialize_fn_t`, `incref_fn_t`, and `decref_fn_t`.

6. **Integration with Yuneta**:
    - Designed for seamless integration with the GObj framework, leveraging its logging, memory management, and contextual handling.

---

## Multi-Language Behavior

- **C**: Utilizes the Jansson library for robust JSON parsing, manipulation, and serialization.
- **JavaScript**: Leverages native JSON-like types (`object`, `array`, `bool`) for lightweight and efficient operations.
- **Python**: Planned implementation will use native types (`dict`, `list`) to align with Python's dynamic JSON-like data structures.

This multi-language approach ensures the library remains idiomatic in each environment while preserving a consistent API.

---

## Primary Use Cases

1. **JSON Manipulation**:
    - Simplify complex JSON operations like cloning, filtering, and updating.
    - Manage deeply nested JSON structures using path-based access.

2. **Data Storage and Persistence**:
    - Serialize and deserialize JSON data for integration with persistent storage systems.

3. **Application Configuration**:
    - Manage hierarchical application settings using JSON structures across platforms.

4. **Cross-Platform Portability**:
    - Provide consistent JSON manipulation utilities across C, JavaScript, Python, and other languages.

---

## How `kwid` Fits in Yuneta

- Acts as an intermediary between low-level JSON handling (via Jansson in C) and high-level application logic (e.g., in GObjs).
- Ensures compatibility across Yuneta's multi-language ecosystem by abstracting JSON operations into reusable, extensible utilities.
- Provides a standardized API that abstracts the complexities of JSON manipulation while remaining native to each language environment.












# JSON Reference Count Macros: `JSON_DECREF` and `JSON_INCREF`

## 📌 Overview

The macros `JSON_DECREF` and `JSON_INCREF` manage the reference count of `json_t *` objects, ensuring proper memory management in applications using the Jansson library.

---

(JSON_DECREF)=

## 🔻 `JSON_DECREF(json)`

### **Description**
Decreases the reference count of a JSON object and frees it if the count reaches zero.

### **Parameters**
- **json** ([`json_t *`](json_t)) → The JSON object whose reference count should be decreased.

### **Return Value**
- **None** → This macro does not return a value.

### **Notes**
Use this macro to safely free JSON objects when they are no longer needed.

---

(JSON_INCREF())=
## 🔺 `JSON_INCREF(json)`

### **Description**
Increases the reference count of a JSON object, preventing it from being freed prematurely.

### **Parameters**
- **json** ([`json_t *`](json_t)) → The JSON object whose reference count should be increased.

### **Return Value**
- **None** → This macro does not return a value.

### **Notes**
Use this macro when passing a JSON object to multiple owners to ensure it remains valid while in use.

---

## ✅ Conclusion

These macros help prevent memory leaks and segmentation faults when managing `json_t *` objects in Yuneta and other systems using Jansson.
