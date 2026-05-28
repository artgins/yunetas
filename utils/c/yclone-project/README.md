# yclone-project

Clone a whole Yuneta project (yuno, util, …) under a new name. Renames source keywords (lowercase / Capitalized / UPPERCASE) throughout file names and file contents, producing a standalone copy ready to build.

## Usage

```bash
yclone-project <SOURCE-PROJECT> <DESTINATION-PROJECT> --src-keyword <kw> --dst-keyword <kw>
```

Both positionals and both keyword options are required. `SOURCE-PROJECT` must
exist; `DESTINATION-PROJECT` must not.

See also `yclone-gclass` for cloning a single GClass.
