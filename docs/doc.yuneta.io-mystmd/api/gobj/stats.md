# Stats

Runtime statistics exposed by gobjs. The global stats parser dispatches `stats` commands to the matching gobj and returns a JSON response.

Source code:

- [`gobj.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.h)
- [`gobj.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.c)

(gobj_decr_stat)=
## `gobj_decr_stat()`

Decrements the specified statistic attribute of the given `hgobj` by a given value and returns the new value.

```C
json_int_t gobj_decr_stat(
    hgobj gobj,
    const char *path,
    json_int_t value
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance whose statistic attribute is to be decremented. |
| `path` | `const char *` | The name of the statistic attribute to decrement. |
| `value` | `json_int_t` | The amount by which to decrement the statistic. |

**Returns**

The new value of the statistic after decrementing.

**Notes**

If the statistic attribute does not exist, it is initialized to zero before decrementing.

---

(gobj_get_stat)=
## `gobj_get_stat()`

Retrieves the value of a statistical attribute from the given `hgobj`.

```C
json_int_t gobj_get_stat(
    hgobj gobj,
    const char *path
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance from which the statistic is retrieved. |
| `path` | `const char *` | The name of the statistical attribute to retrieve. |

**Returns**

Returns the value of the specified statistical attribute as a `json_int_t`. If the attribute does not exist, returns `0`.

**Notes**

This function does not check if the attribute exists before retrieving its value. Ensure that the attribute is valid before calling [`gobj_get_stat()`](#gobj_get_stat).

---

(gobj_incr_stat)=
## `gobj_incr_stat()`

Increments the specified statistic attribute of the given `hgobj` by a given value and returns the new value.

```C
json_int_t gobj_incr_stat(
    hgobj gobj,
    const char *path,
    json_int_t value
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance whose statistic attribute is to be incremented. |
| `path` | `const char *` | The path to the statistic attribute to be incremented. |
| `value` | `json_int_t` | The amount by which to increment the statistic attribute. |

**Returns**

The new value of the statistic attribute after incrementing.

**Notes**

If the statistic attribute does not exist, it is initialized to zero before incrementing.

---

(gobj_jn_stats)=
## `gobj_jn_stats()`

Returns a JSON object containing the statistics of the given `hgobj`.

```C
json_t *gobj_jn_stats(
    hgobj gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance whose statistics are to be retrieved. |

**Returns**

A JSON object containing the statistics of the given `hgobj`. The returned JSON object is not owned by the caller and should not be modified or freed.

**Notes**

The returned JSON object provides access to the internal statistics of the `hgobj`. Modifying it directly may lead to undefined behavior.

---

(gobj_set_stat)=
## `gobj_set_stat()`

Sets the value of a statistical attribute for the given `hgobj` and returns the previous value.

```C
json_int_t gobj_set_stat(
    hgobj gobj, 
    const char *path, 
    json_int_t value
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance whose statistical attribute is being modified. |
| `path` | `const char *` | The name of the statistical attribute to modify. |
| `value` | `json_int_t` | The new value to set for the statistical attribute. |

**Returns**

The previous value of the statistical attribute before modification.

**Notes**

If the attribute does not exist, it is created with the specified value.

---
