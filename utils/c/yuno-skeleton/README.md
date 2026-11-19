# yuno-skeleton

`yuno-skeleton` creates the source files of a new yuno, gclass, or application from a template. To copy an existing project and not a template, use `ymake-skeleton`.

## Usage

```bash
yuno-skeleton --list                           # show available templates
yuno-skeleton <SKELETON>                        # instantiate a template
yuno-skeleton -p <path> <SKELETON>              # use a custom skeletons dir
```

1. Run `yuno-skeleton --list` to see the templates and their summaries.
2. Run `yuno-skeleton <SKELETON>` with the name of one template.
3. Type the answer to each prompt.

To give the answers from a script, send them to stdin, one answer on each line:

```bash
printf 'my_widget\nShows the readings\nArtGins\n' | yuno-skeleton js_gclass   # -> ./c_my_widget.js
```

| Option | Default | Description |
|---|---|---|
| `-l`, `--list` | | Print the templates and their summaries. |
| `-p`, `--skeletons-path PATH` | `/yuneta/bin/skeletons` | The directory of the templates. |

If you give no `SKELETON`, the tool prints the list of templates.

## How it works

The name is not an argument. The first prompt asks for it. For a Yuno template, the name is the yuno role and root name. For a GClass template, the name is the gclass name. The next prompts ask for the variables of the template. The tool reads each answer from stdin. If the input ends before the last prompt, the tool stops with an error.

The templates are:

- C gclasses: `gclass_service`, `gclass_child`, `c_h_file`.
- JS gclasses: `js_gclass`, `js_gclass_service`.
- Yunos: `yuno_citizen`, `yuno_standalone`.
- A pure C program: `c_project`.
- An application repository with its web frontend in `gui/`: `project` (MIT) and `project_commercial` (closed).

The catalog of templates is `__skeletons__.json`. An entry of the catalog can have two more keys:

- `extends` names a directory beside the template. The tool copies that directory first. Then it copies the files of the template on top. Both project templates extend `project_base`.
- `keep` lists markers that belong to the generated code, for example the `{{name}}` of i18next. The tool writes each of these markers unchanged.

`make install` replaces all of `/yuneta/bin/skeletons`, but it keeps `skeletons.d/`. That directory holds the templates of the user. As a result, a template that you remove or rename in `skeletons/` does not stay installed.

`check_project_gui.sh` generates a project. Then it runs `npm install` and `npm run build` in the `gui/` of that project. The deploy round of gobj-ui runs this script when it raises the version ranges in `skeletons/project_base/gui/package.json_tmpl`.
