# ymake-skeleton

`ymake-skeleton` makes a new yuno skeleton from a copy of an existing project. It renames the identifiers and the paths to match the new role. To make a yuno from a library of templates, use `yuno-skeleton`.

## Usage

```bash
ymake-skeleton <source-project> <destination> --yunorole <role> --rootname <name>
```

Run `ymake-skeleton --help` to see all the options.

The tool replaces the keyword only as a whole word. The `_` character is a
separator: `time` changes in `c_time.c`, but not in `C_TIMER`. At the end, the
tool lists the words that held the keyword and that it did not change.

The engine is `utils/c/yclone-project/clone_tree_dir.c`. yclone-gclass,
yclone-project and ymake-skeleton use the same engine. The doc page tells
which files the tool copies.
