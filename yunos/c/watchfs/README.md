# watchfs

Yuno that watches a directory tree and executes a shell command when files matching one or more regex patterns are modified. Ideal for auto-rebuild loops during development (docs, JS bundles, native builds, …).

## Configuration

Set in the yuno's `kw` section:

| Key | Meaning |
|---|---|
| `path` | Root directory to watch |
| `recursive` | `true` to watch sub-directories |
| `patterns` | Semicolon-separated list of regexes (e.g. `.*\.js;.*\.css`) |
| `command` | Shell command to run on match |
| `use_parameter` | Pass the modified filename as an argument (shell-escaped) |
| `ignore_changed_event` / `ignore_renamed_event` | Filter the corresponding `C_FS` events |

## Example

See `kernel/js/lib-yui/watch.json` and `watch.sh` for a dev loop that rebuilds `lib-yui` with Vite whenever a source file changes.
