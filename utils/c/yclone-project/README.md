# yclone-project

`yclone-project` copies a full Yuneta project (a yuno, a util, or another project) with a new name. It replaces the source keyword in file names and in file contents, in lowercase, Capitalized and UPPERCASE forms. The result is a standalone copy that you can build.

## Usage

```bash
yclone-project <SOURCE-PROJECT> <DESTINATION-PROJECT> --src-keyword <kw> --dst-keyword <kw>
```

The two positional arguments and the two keyword options are required. `SOURCE-PROJECT` must
exist. `DESTINATION-PROJECT` must not exist.

To copy one GClass, use `yclone-gclass`.

The tool replaces the keyword only as a whole word. `_` is a separator: `time`
changes in `c_time.c`, but not in `C_TIMER`. At the end, the tool lists the words that contain the
keyword and that it did not change. The engine is
`utils/c/yclone-project/clone_tree_dir.c`. yclone-gclass,
yclone-project and ymake-skeleton use it. The doc page tells what the tool copies.
