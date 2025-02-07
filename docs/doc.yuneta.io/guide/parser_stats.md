(stats_parser_guide)=
# **Statistics Parser**

The statistics parser in Yuneta handles requests for retrieving and managing statistics from a GObj. It operates by collecting attributes marked with specific statistical flags (`SDF_STATS`, `SDF_RSTATS`, and `SDF_PSTATS`) and providing a structured way to query, reset, or persist these attributes.

The statistics parser integrates with the [`gobj_stats`](gobj_stats()) API, dynamically selecting between a global parser or a GClass-specific `mt_stats` method if defined.

```C
json_t *gobj_stats(hgobj gobj, const char *stats, json_t *kw, hgobj src);
```

Source code in:

- [stats_parser.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/stats_parser.c)
- [stats_parser.h](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/stats_parser.h)

---

## How the Internal Statistics Parser Works

### 1. **Attribute Collection**
When `gobj_stats()` is called:
- Attributes marked with `SDF_STATS`, `SDF_RSTATS`, or `SDF_PSTATS` are collected.
- These attributes are treated as statistics and included in the response.

### 2. **Querying Statistics**
- If the `stats` parameter contains the name of an attribute, the value of that specific attribute is returned.
- If the `stats` parameter is empty, all attributes marked with `SDF_STATS` are returned.

### 3. **Resetting Statistics**
The internal global parser includes a special name, `__reset__`:
- When `__reset__` is passed as the `stats` parameter, all attributes marked with `SDF_RSTATS` are reset to their default values.

### 4. **Persistence of Statistics**
Attributes marked with `SDF_PSTATS` serve dual purposes:
- They are treated as persistent attributes and saved when `gobj_save_persistent_attrs()` is called.
- They are also included as part of the statistics queried via `gobj_stats()`.

### 5. **Custom GClass Handling**
- If a GClass defines an `mt_stats` method, `gobj_stats()` calls this method instead of the global parser.
- This allows GClasses to implement custom logic for managing and returning statistics.

---

## Flags Relevant to Statistics

| **Flag**        | **Description**                                                                 |
|------------------|---------------------------------------------------------------------------------|
| `SDF_STATS`      | Marks the attribute as a statistic to be retrieved by the statistics parser.    |
| `SDF_RSTATS`     | Indicates that the attribute is a resettable statistic. Can be reset via `__reset__`. |
| `SDF_PSTATS`     | Combines `SDF_STATS` with persistence. The attribute is saved as a persistent statistic. |

---

## API for Statistics

| **API**              | **Description**                                                                 |
|-----------------------|---------------------------------------------------------------------------------|
| `gobj_stats()`        | Retrieve or reset statistics for a GObj.                                       |

| **Parameter** | **Description**                                                                 |
|---------------|---------------------------------------------------------------------------------|
| `gobj`       | The GObj instance for which statistics are being requested.                     |
| `stats`      | The name of the statistic to retrieve, or `__reset__` to reset all resettable statistics. |
| `kw`         | Additional parameters for the statistics request.                                |
| `src`        | The source GObj sending the statistics request.                                  |

---

## Workflow for `gobj_stats()`

1. **Receiving a Statistics Request:**
    - A request is sent to a GObj using `gobj_stats()`.
    - The `stats` and `kw` parameters define the requested statistics and additional options.

2. **Attribute Collection:**
    - The parser collects attributes marked with `SDF_STATS`, `SDF_RSTATS`, and `SDF_PSTATS`.

3. **Parsing and Validation:**
    - If a specific statistic name is provided, its value is returned.
    - If no name is provided, all `SDF_STATS` attributes are returned.

4. **Custom or Global Handling:**
    - If the GClass defines `mt_stats`, it is invoked for custom processing.
    - Otherwise, the global parser processes the request.

5. **Resetting Statistics:**
    - If `__reset__` is passed, all attributes marked with `SDF_RSTATS` are reset.

---

## Benefits of the Statistics Parser

- **Dynamic Querying:** Supports both specific and bulk statistics requests.
- **Customizability:** GClasses can define their own `mt_stats` method for specialized handling.
- **Persistence Integration:** Attributes marked with `SDF_PSTATS` are automatically persisted.
- **Reset Capability:** The `__reset__` keyword simplifies resetting of statistics.

---

## Use Cases

### 1. **Monitoring and Reporting**
Retrieve runtime statistics for performance monitoring or system diagnostics.

### 2. **Dynamic Configuration**
Integrate with external systems to query or modify statistical attributes dynamically.

### 3. **State Management**
Reset runtime statistics programmatically to maintain operational consistency.
