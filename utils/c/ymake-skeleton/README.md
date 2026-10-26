# ymake-skeleton

Generate a new yuno skeleton from an existing project, renaming identifiers and paths to match the new role. Complements `yuno-skeleton` (which creates from a library of templates).

## Usage

```bash
ymake-skeleton <source-project> <destination> --yunorole <role> --rootname <name>
```

Run `ymake-skeleton --help` for all options.

The keyword is replaced only as a **whole word** (`_` is a separator: `time`
changes in `c_time.c`, not in `C_TIMER`), and the words that held it and were
left as they were are listed at the end. The engine is
`utils/c/yclone-project/clone_tree_dir.c`, shared by yclone-gclass,
yclone-project and ymake-skeleton. See the doc page for what is copied.
