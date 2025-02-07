(command_parser_guide)=
# **Command Parser**

The command parser in Yuneta validates and processes commands defined in a GClass's `command_table`. It ensures commands sent to a GObj are properly formatted and conform to the schema for that GClass. The parser dynamically selects between an instance-specific method (`mt_command`), a global parser, or an internal parser to handle commands.

Commands are executed using the API  [`gobj_command()`](gobj_command()):

`json_t *gobj_command(hgobj gobj, const char *cmd, json_t *kw, hgobj src);`

Source code in:

- [command_parser.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/command_parser.c)
- [command_parser.h](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/command_parser.h)

---

## How the Command Parser Works

### 1. **Command Validation**
The parser checks the `cmd` and `kw` arguments to ensure:
- The command name exists in the `command_table`.
- The provided parameters match the schema for the command.

### 2. **Dynamic Handling**
When `gobj_command()` is called:
1. **Instance-Specific Handling:**
   - If the GClass defines an `mt_command` method, this method is invoked.
   - This allows GClasses to handle their own commands with custom logic.
2. **Global Parser Handling:**
   - If `mt_command` is not defined, the global command parser (provided in `gobj_start_up()`) is used.
   - The global parser applies standardized command validation and execution.
3. **Fallback to Internal Parser:**
   - If no global command parser is defined (null), the internal parser is used.

### 3. **Parameter Handling**
- **Single String:** The command and its parameters can be provided as a single string in `cmd`.
- **Separated Parameters:** The command name can be provided in `cmd`, with its parameters passed in the `kw` argument.

---

## Command Execution API

| **Parameter** | **Description**                                                                 |
|---------------|---------------------------------------------------------------------------------|
| `gobj`       | The GObj instance executing the command.                                        |
| `cmd`        | The name of the command, optionally including parameters.                      |
| `kw`         | A JSON object containing the command's parameters if not included in `cmd`.    |
| `src`        | The source GObj sending the command.                                           |

---

## Command Table

The `command_table` defines the commands supported by a GObj. Each entry includes:
- **Name:** The command's unique identifier.
- **Aliases:** Alternative names for the command.
- **Parameters:** A schema defining the command's expected parameters.
- **Handler Function:** A callback function that implements the command's behavior.
- **Description:** Documentation explaining the command's purpose.

---

## Workflow for `gobj_command()`

1. **Receiving a Command:**
   - A command is sent to a GObj using `gobj_command()`.
   - The `cmd` and `kw` arguments specify the command and its parameters.

2. **Command Resolution:**
   - If the GClass defines `mt_command`, it is invoked to handle the command.
   - If `mt_command` is not defined, the global command parser is used.
   - If no global parser is defined, the internal parser is used as a fallback.

3. **Parsing and Validation:**
   - The command name is checked against the `command_table`.
   - Parameters are validated based on the schema.

4. **Execution:**
   - The handler function associated with the command is invoked, processing the command and returning the result.

---

## Benefits of the Command Parser

- **Dynamic Handling:** Automatically selects the appropriate parser or method for each command.
- **Validation:** Ensures all commands conform to the schema before execution.
- **Customizability:** Allows GClasses to override the default behavior with `mt_command`.
- **Fallback Support:** The internal parser ensures commands are always processed, even without custom parsers.

---

## Use Cases

### 1. **Service-Oriented Gobjs**
- Validate and process commands received from external systems or other Yunos.

### 2. **Custom Command Logic**
- Implement specialized command handling in `mt_command` for unique behaviors.

### 3. **Dynamic Parameter Handling**
- Support commands with parameters passed inline or separately.
