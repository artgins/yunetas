# yclone-gclass

Clone a GClass template under a new name. Rewrites file names and in-file identifiers (lowercase / Capitalized / UPPERCASE variants) so you get a ready-to-edit skeleton.

## Usage

```bash
yclone-gclass <OLD_GCLASS> <NEW_GCLASS>
```

Names must be given without `c_` prefix or `.c`/`.h` extension.

The keyword is replaced only as a **whole word** (`_` is a separator: `time`
changes in `c_time.c`, not in `C_TIMER`), and the words that held it and were
left as they were are listed at the end. The engine is
`utils/c/yclone-project/clone_tree_dir.c`, shared by yclone-gclass,
yclone-project and ymake-skeleton. See the doc page for what is copied.
