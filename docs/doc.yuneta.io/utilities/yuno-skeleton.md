(util-yuno-skeleton)=
# `yuno-skeleton`

Instantiate a new yuno (or gclass) from a named template in the skeleton
library.

## Usage

```bash
yuno-skeleton --list                 # list available skeletons
yuno-skeleton <SKELETON>             # instantiate one
```

| Option | Purpose |
|--------|---------|
| `<SKELETON>` | Template to instantiate (positional) |
| `--list` / `-l` | List available skeletons |
| `--skeletons-path` / `-p` | Skeleton directory (default `/yuneta/bin/skeletons`) |

Run with no skeleton to print the available list. The templates are the ones
under `utils/c/yuno-skeleton/skeletons/`; the build installs them in
`/yuneta/bin/skeletons` and replaces the installed copy whole, except
`skeletons.d/`, where your own templates go.

## Example

The name is not an argument: the tool asks for it, then for the template's
variables. The answers are read from stdin, one per line, so a script can
pipe them in.

```text
$ yuno-skeleton js_gclass
Enter gclass name: my_widget
One-line description of the gclass ['']: Shows the readings
Author name ['']: ArtGins
Copying '/yuneta/bin/skeletons/js_gclass' in '.'
Creating filename: ./c_my_widget.js

$ printf 'my_widget\nShows the readings\nArtGins\n' | yuno-skeleton js_gclass
```

`c_my_widget.js` defines the gclass `C_MY_WIDGET`, imports from
`@yuneta/gobj-js` and exports `register_c_my_widget()`. A Yuno template asks
for the yuno role (at most 15 characters) and the root gobj name instead.

A new application repository, with one citizen yuno in it:

```bash
printf 'my_app\n\nMy application\nArtGins\nsupport@artgins.com\n\n' | yuno-skeleton project
cd my_app/yunos
printf 'my_yuno\n\n1.0.0\nMy yuno\nArtGins\nsupport@artgins.com\nMIT\n' | yuno-skeleton yuno_citizen
sed -i 's|^# add_subdirectory(my_yuno)$|add_subdirectory(my_yuno)|' CMakeLists.txt
```

`project` writes an MIT `LICENSE`; `project_commercial` a proprietary one,
and a README section on what a delivery of a closed application must carry.
Both make `gui/`, the web frontend (Vite, `@yuneta/gobj-js`, the declarative
shell of `@yuneta/gobj-ui`):

```bash
cd my_app/gui
npm install
npm run dev
```

The answers are one line per prompt (an empty line takes the default); an
input that ends before the last prompt is an error.

| Template | Type | Makes |
|----------|------|-------|
| `gclass_service`, `gclass_child` | GClass | `c_<name>.{c,h}` |
| `js_gclass`, `js_gclass_service` | GClass | `c_<name>.js` |
| `c_h_file` | GClass | `<name>.{c,h}`, no gobj |
| `yuno_citizen`, `yuno_standalone` | Yuno | `<role>/` |
| `c_project` | Utility | `<name>/`, a pure C program |
| `project`, `project_commercial` | Project | `<name>/`, an application repository with its `gui/` |

## See also

- [Scaffolding new yunos](../../../yunos/c/yuno_agent/SCAFFOLDING.md).
- [`utils/c/yuno-skeleton/README.md`](https://github.com/artgins/yunetas/blob/7.28.8/utils/c/yuno-skeleton/README.md).
