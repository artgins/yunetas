(sdata)=

# **SData**


**`Structured Data`** (`SData`) is a mechanism to define and manage structured fields,
`attributes`, and `commands` in a hierarchical and schema-driven manner.
It is used to define the attributes of objects, command parameters,
and database-like records in a highly structured and reusable way.

---

## Core Concepts

### SData Fields
SData fields are descriptors that define individual fields or attributes. These fields include information about their type, name, default value, description, flags, and other properties.

### SData Tables
SData tables are arrays of field descriptors (`sdata_desc_t`) that define structured data. These tables allow hierarchical definitions, enabling the creation of complex schemas.

---

## Data Types (`data_type_t`)

The `data_type_t` enumeration defines the types of data that SData fields can represent:

| **Type**      | **Description**                         |
|---------------|-----------------------------------------|
| `DTP_STRING`  | A text string.                         |
| `DTP_BOOLEAN` | A boolean value (`TRUE` or `FALSE`).    |
| `DTP_INTEGER` | An integer value.                      |
| `DTP_REAL`    | A floating-point number.               |
| `DTP_LIST`    | A list (array) of values.              |
| `DTP_DICT`    | A dictionary (key-value pairs).        |
| `DTP_JSON`    | A JSON object.                         |
| `DTP_POINTER` | A generic pointer.                     |

### Data Type Utilities
- **String types:** `DTP_IS_STRING(type)`
- **Boolean types:** `DTP_IS_BOOLEAN(type)`
- **Integer types:** `DTP_IS_INTEGER(type)`
- **Number types:** `DTP_IS_NUMBER(type)` (includes integer, real, and boolean)
- **List types:** `DTP_IS_LIST(type)`
- **Dictionary types:** `DTP_IS_DICT(type)`
- **JSON types:** `DTP_IS_JSON(type)`
- **Pointer types:** `DTP_IS_POINTER(type)`

---

## Field Flags (`sdata_flag_t`)

The `sdata_flag_t` enumeration defines the properties and characteristics of each field. Flags are bitwise-combinable to give fields multiple properties.

| **Flag**           | **Description**                                                                 |
|---------------------|---------------------------------------------------------------------------------|
| `SDF_NOTACCESS`     | Field is not accessible.                                                       |
| `SDF_RD`            | Field is read-only.                                                           |
| `SDF_WR`            | Field is writable (and readable).                                              |
| `SDF_REQUIRED`      | Field is required; it must not be null.                                        |
| `SDF_PERSIST`       | Field is persistent and must be saved/loaded.                                  |
| `SDF_VOLATIL`       | Field is volatile and must not be saved/loaded.                                |
| `SDF_RESOURCE`      | Field is a resource, referencing another schema.                               |
| `SDF_PKEY`          | Field is a primary key.                                                        |
| `SDF_STATS`         | Field holds statistical data (metadata).                                       |
| `SDF_RSTATS`        | Field holds resettable statistics, implicitly `SDF_STATS`.                     |
| `SDF_PSTATS`        | Field holds persistent statistics, implicitly `SDF_STATS`.                    |
| `SDF_AUTHZ_R`       | Read access requires authorization (`__read_attribute__`).                     |
| `SDF_AUTHZ_W`       | Write access requires authorization (`__write_attribute__`).                   |
| `SDF_AUTHZ_X`       | Execution requires authorization (`__execute_command__`).                      |
| `SDF_AUTHZ_S`       | Stats read requires authorization (`__read_stats__`).                          |
| `SDF_AUTHZ_RS`      | Stats reset requires authorization (`__reset_stats__`).                        |

### Common Flag Combinations
- **Public Attributes:** Combine `SDF_RD|SDF_WR|SDF_STATS|SDF_PERSIST|SDF_VOLATIL|SDF_RSTATS|SDF_PSTATS`.
- **Writable Attributes:** Combine `SDF_WR|SDF_PERSIST`.

---

(sdata_desc_t())=
## Descriptor Fields (`sdata_desc_t`)

The `sdata_desc_t` structure defines a field or schema. Each descriptor specifies the following:

| **Field**        | **Type**             | **Description**                                                                 |
|------------------|----------------------|---------------------------------------------------------------------------------|
| `type`           | `data_type_t`        | The type of the field (e.g., string, boolean).                                  |
| `name`           | `const char *`       | The name of the field.                                                          |
| `alias`          | `const char **`      | Alternative names (aliases) for the field.                                      |
| `flag`           | `sdata_flag_t`       | Flags defining the field's properties.                                          |
| `default_value`  | `const char *`       | The default value of the field.                                                 |
| `header`         | `const char *`       | Header text for table columns.                                                  |
| `fillspace`      | `int`                | Column width for table formatting.                                              |
| `description`    | `const char *`       | A description of the field's purpose.                                           |
| `json_fn`        | `json_function_fn`   | Custom function for processing JSON data.                                       |
| `schema`         | `const sdata_desc_t *` | Pointer to a sub-schema for compound fields.                                    |
| `authpth`        | `const char *`       | Authorization path for accessing or modifying the field.                        |

---

## Application

### Attributes
SData tables define attributes by listing fields with their types, default values, and flags. These fields form the basis of object definitions, enabling schema-based validation and management.

### Commands
SData tables can define commands with associated parameters, schemas, and descriptions. Commands extend the functionality of objects, providing structured inputs and outputs.

### Nested Schemas
Fields in SData can reference other schemas, enabling hierarchical definitions. This allows for the creation of complex, nested structures while maintaining clarity and reusability.
