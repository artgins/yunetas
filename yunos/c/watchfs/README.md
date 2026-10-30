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
| `use_parameter` | Pass the path of the last modified file of a burst as an argument (shell-escaped). A command and quoted path that do not fit in 16 KB are not run (logged) |
| `ignore_changed_event` / `ignore_renamed_event` | Filter the corresponding `C_FS` events |

## Example

See `kernel/js/gobj-ui/watch.json` and `watch.sh` for a dev loop that rebuilds `gobj-ui` with Vite whenever a source file changes.
