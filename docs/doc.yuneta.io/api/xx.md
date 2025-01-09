

## Prototypes

**C Prototype**

```c
int gobj_start_up(
    int argc,
    char *argv[],
    json_t *jn_global_settings,             /* NOT owned */
    int (*startup_persistent_attrs)(void),
    void (*end_persistent_attrs)(void),
    int (*load_persistent_attrs)(
        hgobj gobj,
        json_t *keys  // owned
    ),
    int (*save_persistent_attrs)(
        hgobj gobj,
        json_t *keys  // owned
    ),
    int (*remove_persistent_attrs)(
        hgobj gobj,
        json_t *keys  // owned
    ),
    json_t * (*list_persistent_attrs)(
        hgobj gobj,
        json_t *keys  // owned
    ),
    json_function_t global_command_parser,
    json_function_t global_stats_parser,
    authz_checker_fn global_authz_checker,
    authenticate_parser_fn global_authenticate_parser,
    size_t max_block,                       /* largest memory block */
    size_t max_system_memory                /* maximum system memory */
);
```

**Python Prototype**

```python
def gobj_start_up(
    argc: int,
    argv: list[str],
    jn_global_settings: dict,
    startup_persistent_attrs: Callable[[], int],
    end_persistent_attrs: Callable[[], None],
    load_persistent_attrs: Callable[[hgobj, dict], int],
    save_persistent_attrs: Callable[[hgobj, dict], int],
    remove_persistent_attrs: Callable[[hgobj, dict], int],
    list_persistent_attrs: Callable[[hgobj, dict], dict],
    global_command_parser: Callable[..., dict] = None,
    global_stats_parser: Callable[..., dict] = None,
    global_authz_checker: Callable[..., bool] = None,
    global_authenticate_parser: Callable[..., dict] = None,
    max_block: int = 1024,
    max_system_memory: int = 1048576
) -> int:
    """
    Initialize a Yuno instance.
    """
```

**JavaScript Prototype**

```javascript
function gobj_start_up(
    argc,                        // Number of arguments (int)
    argv,                        // Array of argument strings (Array<string>)
    jn_global_settings,          // Global settings object (Object)
    startup_persistent_attrs,    // Function to initialize persistent attributes (Function: () => int)
    end_persistent_attrs,        // Function to deinitialize persistent attributes (Function: () => void)
    load_persistent_attrs,       // Function to load persistent attributes (Function: (hgobj, Object) => int)
    save_persistent_attrs,       // Function to save persistent attributes (Function: (hgobj, Object) => int)
    remove_persistent_attrs,     // Function to remove persistent attributes (Function: (hgobj, Object) => int)
    list_persistent_attrs,       // Function to list persistent attributes (Function: (hgobj, Object) => Object)
    global_command_parser = null, // Function for global command parsing (Optional Function)
    global_stats_parser = null,   // Function for global stats parsing (Optional Function)
    global_authz_checker = null,  // Function for global authorization checking (Optional Function)
    global_authenticate_parser = null, // Function for global authentication parsing (Optional Function)
    max_block = 1024,             // Maximum block size (int, default: 1024)
    max_system_memory = 1048576   // Maximum system memory (int, default: 1048576)
) {
    // Initialize a Yuno instance
    return 0; // 0 indicates success, negative values indicate errors
}
```

## Parameters

- **\`argc\`**:
  \- **C**: `int`
  \- **Python**: `int`
  \- **JavaScript**: `int`

  Number of arguments passed to the application.

- **\`argv\`**:
  \- **C**: `char *argv[]`
  \- **Python**: `list[str]`
  \- **JavaScript**: `Array<string>`

  Array of argument strings.

- **\`jn_global_settings\`**:
  \- **C**: `json_t *`
  \- **Python**: `dict`
  \- **JavaScript**: `Object`

  JSON object containing global settings for the Yuno.

- **\`startup_persistent_attrs\`**:
  \- **C**: `int (*startup_persistent_attrs)(void)`
  \- **Python**: `Callable[[], int]`
  \- **JavaScript**: `Function: () => int`

  Callback to initialize the persistent attributes system.

- **\`end_persistent_attrs\`**:
  \- **C**: `void (*end_persistent_attrs)(void)`
  \- **Python**: `Callable[[], None]`
  \- **JavaScript**: `Function: () => void`

  Callback to deinitialize the persistent attributes system.

- **\`load_persistent_attrs\`**:
  \- **C**: `int (*load_persistent_attrs)(hgobj, json_t *keys)`
  \- **Python**: `Callable[[hgobj, dict], int]`
  \- **JavaScript**: `Function: (hgobj, Object) => int`

  Callback to load persistent attributes.

- **\`save_persistent_attrs\`**:
  \- **C**: `int (*save_persistent_attrs)(hgobj, json_t *keys)`
  \- **Python**: `Callable[[hgobj, dict], int]`
  \- **JavaScript**: `Function: (hgobj, Object) => int`

  Callback to save persistent attributes.

- **\`remove_persistent_attrs\`**:
  \- **C**: `int (*remove_persistent_attrs)(hgobj, json_t *keys)`
  \- **Python**: `Callable[[hgobj, dict], int]`
  \- **JavaScript**: `Function: (hgobj, Object) => int`

  Callback to remove persistent attributes.

- **\`list_persistent_attrs\`**:
  \- **C**: `json_t * (*list_persistent_attrs)(hgobj, json_t *keys)`
  \- **Python**: `Callable[[hgobj, dict], dict]`
  \- **JavaScript**: `Function: (hgobj, Object) => Object`

  Callback to list persistent attributes.

- **Other Parameters**:
  \- Refer to individual prototypes for additional details.

## Return Value

- **\`0\`**: Success.
- **Negative value**: Indicates an error during initialization.

## Behavior

1. **Initialization**:
   Configures the Yuno with provided global settings and command-line arguments.
2. **Persistent Attributes**:
   Invokes callbacks to handle persistent attributes, including loading, saving, removing, and listing.
3. **Global Parsers**:
   Sets up parsers for commands, statistics, and authorization/authentication handlers.
4. **Memory Management**:
   Allocates memory based on specified maximum block size and total system memory.

## Example

Refer to the following example in C. Python and JavaScript implementations would follow their respective prototype conventions.

```c
int startup_persistent_attrs(void) {
    // Initialize persistent attributes system.
    return 0;
}

void end_persistent_attrs(void) {
    // Cleanup persistent attributes system.
}

int load_attrs(hgobj gobj, json_t *keys) {
    // Load attributes for the given gobj.
    return 0;
}

int main(int argc, char *argv[]) {
    json_t *settings = json_pack("{s:i}", "example_setting", 42);

    int result = gobj_start_up(
        argc,
        argv,
        settings,
        startup_persistent_attrs,
        end_persistent_attrs,
        load_attrs,
        NULL,  // Save function not required in this example
        NULL,  // Remove function not required in this example
        NULL,  // List function not required in this example
        NULL,  // Command parser
        NULL,  // Stats parser
        NULL,  // Authz checker
        NULL,  // Authenticate parser
        1024,  // Max block size
        1048576  // Max system memory
    );

    if (result < 0) {
        fprintf(stderr, "Failed to initialize Yuno.\n");
        return -1;
    }

    // Proceed with application logic...
    return 0;
}
```













## `gobj_start_up`

**Initialize a Yuno instance.**

This function prepares a Yuno for operation by setting global configurations, handling persistent attributes, and configuring memory management. It serves as the entry point for starting a Yuno instance.

**Parameters**

- **`argc`** (`int`):  
  Number of arguments passed to the application.

- **`argv`** (`list`):  
  Array of argument strings.

- **`jn_global_settings`** (`json_t *`):  
  JSON object containing global settings for the Yuno (not owned by the function).

- **`startup_persistent_attrs`** (`function`):  
  Callback to initialize the persistent attributes system.

- **`end_persistent_attrs`** (`function`):  
  Callback to deinitialize the persistent attributes system.

- **`load_persistent_attrs`** (`function`):  
  Callback to load persistent attributes for a GObj.  
  Takes two parameters:
  - `hgobj gobj`: The GObj for which attributes are to be loaded.
  - `json_t *keys`: JSON object representing the keys to load.

- **`save_persistent_attrs`** (`function`):  
  Callback to save persistent attributes for a GObj.  
  Takes two parameters:
  - `hgobj gobj`: The GObj for which attributes are to be saved.
  - `json_t *keys`: JSON object representing the keys to save.

- **`remove_persistent_attrs`** (`function`):  
  Callback to remove persistent attributes for a GObj.  
  Takes two parameters:
  - `hgobj gobj`: The GObj for which attributes are to be removed.
  - `json_t *keys`: JSON object representing the keys to remove.

- **`list_persistent_attrs`** (`function`):  
  Callback to list persistent attributes for a GObj.  
  Takes two parameters:
  - `hgobj gobj`: The GObj for which attributes are to be listed.
  - `json_t *keys`: JSON object representing the keys to list.

- **`global_command_parser`** (`json_function_t`):  
  Global command parser function.

- **`global_stats_parser`** (`json_function_t`):  
  Global statistics parser function.

- **`global_authz_checker`** (`authz_checker_fn`):  
  Function to verify global authorization.

- **`global_authenticate_parser`** (`authenticate_parser_fn`):  
  Function to parse and handle global authentication requests.

- **`max_block`** (`size_t`):  
  Maximum size of memory blocks that can be allocated.

- **`max_system_memory`** (`size_t`):  
  Maximum memory that the system can allocate.

### Return Value

- `0`: Success.
- Negative value: Indicates an error during initialization.

### Behavior

1. **Initialization**:  
   Configures the Yuno with provided global settings and command-line arguments.

2. **Persistent Attributes**:  
   Invokes callbacks to handle persistent attributes, including loading, saving, removing, and listing.

3. **Global Parsers**:  
   Sets up parsers for commands, statistics, and authorization/authentication handlers.

4. **Memory Management**:  
   Allocates memory based on specified maximum block size and total system memory.

### Example

```c
int startup_persistent_attrs(void) {
    // Initialize persistent attributes system.
    return 0;
}

void end_persistent_attrs(void) {
    // Cleanup persistent attributes system.
}

int load_attrs(hgobj gobj, json_t *keys) {
    // Load attributes for the given gobj.
    return 0;
}

int main(int argc, char *argv[]) {
    json_t *settings = json_pack("{s:i}", "example_setting", 42);

    int result = gobj_start_up(
        argc,
        argv,
        settings,
        startup_persistent_attrs,
        end_persistent_attrs,
        load_attrs,
        NULL,  // Save function not required in this example
        NULL,  // Remove function not required in this example
        NULL,  // List function not required in this example
        NULL,  // Command parser
        NULL,  // Stats parser
        NULL,  // Authz checker
        NULL,  // Authenticate parser
        1024,  // Max block size
        1048576  // Max system memory
    );

    if (result < 0) {
        fprintf(stderr, "Failed to initialize Yuno.\n");
        return -1;
    }

    // Proceed with application logic...
    return 0;
}
```


## `add`

Add two numbers.

### Parameters
- `a` (_int or float_): The first number.
- `b` (_int or float_): The second number.

### Returns
- _int or float_: The sum of the numbers.

### Examples

```python
from my_module import add

result = add(5, 3)
```

## Function Documentation

### Description

This function performs a complex calculation involving matrix operations and returns the result.

### Parameters

- `matrix_a`: Input matrix A
- `matrix_b`: Input matrix B
- `result_matrix`: Output matrix storing the result

### Returns

The function returns the calculated result matrix.

### Implementation Details

The function uses optimized algorithms and SIMD instructions to perform fast matrix multiplication.

## Usage Example

**C Function:** `my_function(int a, int b)`

This function adds two integers together.

* **Args:**
    * `a`: The first integer.
    * `b`: The second integer.

* **Returns:**
    * The sum of `a` and `b`.

**Example:**

```c
#include <stdio.h>

int my_function(int a, int b) {
    return a + b;
}

int main() {
    int result = my_function(5, 3);
    printf("The sum is: %d\n", result);
    return 0;
}
```


> - `widget_name` [deprecated](https://link-to-issue) in GitLab 14.7.

# Sample Document

This is some introductory text.  
You can jump directly to the **Detailed Section** by clicking this link:  
[Go to Detailed Section](#detailed-section)

---

## Collapsible "Popup" Section

Sometimes you want to provide extra information without cluttering the page.  
You can achieve this using a `<details>` block:

<details>
  <summary>Click to see more details</summary>

Here is the detailed text that’s hidden by default.  
You can add **formatted text**, or even images and code blocks if you like!
</details>

---

## Detailed Section

This is the section you jumped to using the link above.  
You can put more content here.

# Sample Document

This is some introductory text.  
You can jump directly to the **Detailed Section** by clicking this link:  
[Go to Detailed Section](#detailed-section)

## Tooltip Example

To show a tooltip in the Sphinx Book Theme, you can use inline HTML like this:

Hover over this word:  
<span title="This is a tooltip that appears on hover!">hover here</span>.

Alternatively, you can use the `abbr` HTML element for abbreviations with tooltips:  
<abbr title="HyperText Markup Language">HTML</abbr>

## Collapsible Section

To provide expandable details, use Sphinx's `dropdown` directive:

```{dropdown} Click to see more details
Here is the detailed text that’s hidden by default.  
You can include **formatted text**, images, or code blocks:
```



---

### Key Points

1. **Tooltip Implementation**:  
   - Use `<span>` with a `title` attribute for basic tooltips.
   - Use `<abbr>` for semantic abbreviations with tooltips.

2. **Collapsible Details**:  
   - Sphinx Book Theme supports the `dropdown` directive for collapsible sections. This integrates smoothly into your Sphinx documentation.

3. **Linking to Sections**:  
   - Use Markdown syntax `[Link Text](#section-id)` to link to sections. Sphinx automatically assigns IDs to headings based on their text, converted to lowercase and with spaces replaced by hyphens.

### Rendering Notes

- Ensure your Sphinx configuration includes the Book Theme. Add it to `conf.py` like so:

```python
html_theme = "sphinx_book_theme"
```

## Constants and Macros

```{eval-rst}
.. list-table::
   :header-rows: 1

   * - Name
     - Description
   * - ``PRIVATE``
     - Defines a static function or variable with file scope.
   * - ``PUBLIC``
     - Defines a function or variable with external linkage.
   * - ``BOOL``
     - Boolean type (`true` or `false`).
   * - ``MIN(a, b)``
     - Returns the smaller of `a` and `b`.
   * - ``MAX(a, b)``
     - Returns the larger of `a` and `b`.
   * - ``ARRAY_SIZE(a)``
     - Returns the number of elements in array `a`.
```


# Glossary

- **[`PRIVATE`](#private)**: Defines a static function or variable with file scope.
- **[`PUBLIC`](#public)**: Defines a function or variable with external linkage.
- **[`BOOL`](#bool)**: Boolean type (`true` or `false`).
- **[`MIN(a, b)`](#min)**: Returns the smaller of `a` and `b`.
- **[`MAX(a, b)`](#max)**: Returns the larger of `a` and `b`.
- **[`ARRAY_SIZE(a)`](#array-size)**: Returns the number of elements in array `a`.

## PRIVATE
Defines a static function or variable with file scope.

## PUBLIC
Defines a function or variable with external linkage.

## BOOL
Boolean type (`true` or `false`).

## MIN
Returns the smaller of `a` and `b`.

## MAX
Returns the larger of `a` and `b`.

## ARRAY_SIZE
Returns the number of elements in array `a`.
