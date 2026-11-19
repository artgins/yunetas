# yclone-gclass

yclone-gclass copies a GClass template under a new name. It changes the file names and the identifiers in the files (lowercase, Capitalized and UPPERCASE variants). The result is a skeleton that you can edit.

## Usage

```bash
yclone-gclass <OLD_GCLASS> <NEW_GCLASS>
```

Give the names without the `c_` prefix and without the `.c`/`.h` extension.

The tool replaces the keyword only as a whole word. `_` is a separator: `time` changes in `c_time.c`, but not in `C_TIMER`. At the end, the tool lists the words that contained the keyword and that it did not change.

The engine is `utils/c/yclone-project/clone_tree_dir.c`. yclone-gclass, yclone-project and ymake-skeleton share it. The doc page tells what the tool copies.
