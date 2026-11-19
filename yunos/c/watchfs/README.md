# watchfs

A yuno that watches a directory tree. When a file that matches one or more regex patterns changes, it runs a shell command. Use it for automatic rebuild loops during development (docs, JS bundles, native builds, and other builds).

## Configuration

Set these keys in the `kw` section of the yuno:

| Key | Meaning |
|---|---|
| `path` | The root directory to watch |
| `recursive` | `true` to watch the subdirectories |
| `patterns` | A list of regexes, separated by semicolons (for example `.*\.js;.*\.css`) |
| `command` | The shell command to run on a match |
| `use_parameter` | Give the path of the last changed file of a burst as an argument (shell-escaped). If the command and the quoted path do not fit in 16 KB, the yuno does not run the command and logs an error |
| `ignore_changed_event` / `ignore_renamed_event` | Ignore the related `C_FS` events |

## Example

For a development loop that rebuilds `gobj-ui` with Vite when a source file changes, see `kernel/js/gobj-ui/watch.json` and `watch.sh`.
