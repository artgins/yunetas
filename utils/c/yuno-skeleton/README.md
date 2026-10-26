# yuno-skeleton

Create a new yuno from a pre-defined template. Lists the available skeletons or instantiates one.

## Usage

```bash
yuno-skeleton --list                           # show available templates
yuno-skeleton <SKELETON>                        # instantiate a template
yuno-skeleton -p <path> <SKELETON>              # use a custom skeletons dir
```

Options: `-l`/`--list`, `-p`/`--skeletons-path PATH` (default `/yuneta/bin/skeletons`).
With no `SKELETON` it prints the available list.

The name is not an argument: the tool asks for it (the yuno role and root
name for a Yuno template, the gclass name for a GClass one), then for the
template's variables, reading the answers from stdin:

```bash
printf 'my_widget\nShows the readings\nArtGins\n' | yuno-skeleton js_gclass   # -> ./c_my_widget.js
```

`make install` replaces `/yuneta/bin/skeletons` whole, except `skeletons.d/`
(the user's own templates), so a template removed or renamed here does not
stay installed.

Templates: C gclasses (`gclass_service`, `gclass_child`, `c_h_file`), JS
gclasses (`js_gclass`, `js_gclass_service`), yunos (`yuno_citizen`,
`yuno_standalone`), a pure C program (`c_project`) and an application
repository (`project`, MIT; `project_commercial`, closed). `--list` prints
them with their summaries.

See also `ymake-skeleton` for cloning from an existing project rather than a template library.
