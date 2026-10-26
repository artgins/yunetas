# yclone-project

Clone a whole Yuneta project (yuno, util, …) under a new name. Renames source keywords (lowercase / Capitalized / UPPERCASE) throughout file names and file contents, producing a standalone copy ready to build.

## Usage

```bash
yclone-project <SOURCE-PROJECT> <DESTINATION-PROJECT> --src-keyword <kw> --dst-keyword <kw>
```

Both positionals and both keyword options are required. `SOURCE-PROJECT` must
exist; `DESTINATION-PROJECT` must not.

See also `yclone-gclass` for cloning a single GClass.

The keyword is replaced only as a **whole word** (`_` is a separator: `time`
changes in `c_time.c`, not in `C_TIMER`), and the words that held it and were
left as they were are listed at the end. The engine is
`utils/c/yclone-project/clone_tree_dir.c`, shared by yclone-gclass,
yclone-project and ymake-skeleton. See the doc page for what is copied.
