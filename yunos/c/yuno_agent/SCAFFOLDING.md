# Scaffolding new yunos and gclasses

This document covers `yuno-skeleton`, the templated scaffolder shipped with
Yuneta, and how to use it correctly so a freshly-created yuno or gclass
matches the conventions enforced elsewhere (notably CLAUDE.md's
"GClass templates and skeletons" hard rule).

Sibling to [`YUNO_LIFECYCLE.md`](YUNO_LIFECYCLE.md) (deploying yunos),
[`REALMS.md`](REALMS.md) (the realm a yuno belongs to),
[`DEBUGGING.md`](DEBUGGING.md), [`IPC.md`](IPC.md).

---

## 1. Which template to pick

![Template decision tree: a gclass splits into SERVICE (gclass_service) or CHILD (gclass_child). A yuno splits into citizen (yuno_citizen) or standalone (yuno_standalone). A generic file or a pure C project splits into c_h_file or c_project. A JS gclass uses js_gclass.](../../../docs/doc.yuneta.io/_static/scaffolding_tree.svg)

The same decision in text:

```
                  what are you starting?
                            │
       ┌────────────────────┼────────────────────┐
       │                    │                    │
       ▼                    ▼                    ▼
   a gclass            a yuno              a generic file
       │                    │              / pure C project
       │                    │                    │
   ┌───┴────┐         ┌─────┴─────┐              ▼
   │        │         │           │         c_h_file
   │        │         │           │         c_project
SERVICE    CHILD   citizen     standalone
   │        │         │           │
   ▼        ▼         ▼           ▼
gclass_  gclass_   yuno_       yuno_
service  child     citizen     standalone

   JS gclass → js_gclass (CHILD) · js_gclass_service (SERVICE)
   application repository → project (MIT) · project_commercial (closed)
```

| Template          | Use when …                                                                |
|-------------------|---------------------------------------------------------------------------|
| `yuno_citizen`    | New yuno that will run **under the local yuno_agent** — registers itself, ships logs via the standard handlers, participates in realm coordination. The default for almost everything. |
| `yuno_standalone` | New yuno that runs **outside the agent** — own `argp` CLI, accepts `-f <config_file>`, no agent dependency. Useful for one-shot CLIs, test harnesses, edge cases. |
| `gclass_service`  | New service-level gclass — addressable by name, optional `subscriber` attr, public-event publication. Goes inside an existing yuno. |
| `gclass_child`    | New child-level gclass — created with a parent (`gobj_create_pure_child` or similar), parent is the natural audience. |
| `js_gclass`       | JS CHILD gclass, for the browser/SPA gobj framework (`@yuneta/gobj-js`).  |
| `js_gclass_service` | JS SERVICE gclass: the same, with the SERVICE subscription block.       |
| `project`         | A new application repository, open source: `yunos/` (its CMake), `yunos/batches/`, an MIT `LICENSE`. The yunos go in with `yuno_citizen`. |
| `project_commercial` | The same for a closed application: a proprietary `LICENSE`, and a README that says what a delivery must carry (Yuneta's notices, LGPL relinking, the SPA's `licenses.txt`). |
| `c_h_file`        | Standalone `.c` + `.h` pair, no gobj framework. Only a helper module.     |
| `c_project`       | Pure C project, only `argp-standalone.h` as a dep. No Yuneta runtime.     |

The CLAUDE.md rule is non-negotiable: **every gclass and every yuno
must match the structure of the matching template, even when sections
are empty.** New gclasses → copy the template. Legacy gclasses → do not
reorder them. Merge the new code into the existing layout.

---

## 2. CLI usage

The tool is `yuno-skeleton`, installed by the `utils/c/yuno-skeleton/`
build. Arguments declared at [`yuno_skeleton.c`](https://github.com/artgins/yunetas/blob/7.29.3/utils/c/yuno-skeleton/yuno_skeleton.c):

| Flag                          | Meaning                                                |
|-------------------------------|--------------------------------------------------------|
| `--list`, `-l`                | List available templates (reads `__skeletons__.json`)  |
| `--skeletons-path`, `-p PATH` | Override the default skeletons directory (default `/yuneta/bin/skeletons`) |
| positional `SKELETON`         | Template name to instantiate (the only argument)        |

The name is **not** an argument: the tool asks for it, then for the
catalog's per-template `vars` (description, author, license, …). A Yuno
template asks for the yuno role (at most 15 characters) and the root gobj
name (default: the role); a GClass template asks for the gclass name; a
Utility template asks for the utility name, a Project template for the
project name. The answer is lowercased and becomes `rootname` (`yunorole`,
`utility`, `project`) in its three case forms.

Typical invocations:

```bash
yuno-skeleton --list                # show what's available

yuno-skeleton yuno_citizen          # new citizen yuno → ./<role>/
yuno-skeleton yuno_standalone       # new standalone yuno → ./<role>/
yuno-skeleton gclass_service        # new service gclass → ./c_<name>.{c,h}
yuno-skeleton gclass_child          # new child gclass → ./c_<name>.{c,h}
yuno-skeleton js_gclass             # new JS gclass → ./c_<name>.js
yuno-skeleton project               # new application repository → ./<project>/
```

A session, as typed (the answers are read from stdin, so a script can pipe
them in, one per line):

```text
$ yuno-skeleton gclass_service
Enter gclass name: my_svc
One-line description of the gclass ['']: Collects the readings
Author name ['']: ArtGins
Copying '/yuneta/bin/skeletons/gclass_service' in '.'
Creating filename: ./c_my_svc.h
Creating filename: ./c_my_svc.c

$ printf 'my_svc\nCollects the readings\nArtGins\n' | yuno-skeleton gclass_service
```

Give one line per prompt, an empty one for a default. An input that ends
before the last prompt is an error (`ERROR: end of input before every
value was given`): up to 7.28.6 a required value was asked again for ever.

If you do not see what you expect, make sure that the catalog path
(`__skeletons__.json`) and the templates live where `--skeletons-path`
points.

---

## 3. The catalog (`__skeletons__.json`)

Lives at `utils/c/yuno-skeleton/skeletons/__skeletons__.json`. Each entry:

```json
{
  "name":    "<template-name>",
  "summary": "<one-line description>",
  "type":    "GClass" | "Yuno" | "Utility" | "Project",
  "vars": [
    { "var": "description", "message": "…", "default": "" },
    { "var": "author",      "message": "…", "default": "" }
  ]
}
```

Current entries (by line range in the JSON):

| Lines    | Name                | Type    | Summary                                          |
|----------|---------------------|---------|--------------------------------------------------|
| 3-18     | `c_h_file`          | GClass  | Generic `.c` + `.h` file                         |
| 20-50    | `c_project`         | Utility | Pure C project (only `argp-standalone.h`)        |
| 52-67    | `js_gclass`         | GClass  | JS child gclass (canonical JS CHILD)             |
| 69-84    | `js_gclass_service` | GClass  | JS service gclass (canonical JS SERVICE)         |
| 86-101   | `gclass_service`    | GClass  | Service gclass with timer (canonical SERVICE)    |
| 103-118  | `gclass_child`      | GClass  | Typical child gclass (canonical CHILD)           |
| 120-150  | `yuno_standalone`   | Yuno    | Standalone yuno project                          |
| 152-182  | `yuno_citizen`      | Yuno    | Realm's citizen yuno project                     |
| 184-214  | `project`           | Project | Application repository, open source (MIT)        |
| 216-246  | `project_commercial`| Project | Application repository, closed (proprietary)     |

Two optional keys:

- `"extends": "<dir>"` — a directory beside the templates, copied into the
  destination first; the template's own files go on top. A file in both is
  an error (`ERROR File ... ALREADY EXISTS`), never an overwrite.
- `"keep": ["<key>", ...]` — markers that belong to the generated code, not
  to the template: each `{{key}}` is rendered as itself, and says nothing
  on stderr. A `keep` that names a variable is refused.

If you add a new template, follow the same shape. The runner parses this
file at startup to fill the prompts.

---

## 4. The templating engine

`make_skeleton.c` + [`tmpl_dir.c`](https://github.com/artgins/yunetas/blob/7.29.3/utils/c/yuno-skeleton/tmpl_dir.c). Two substitution layers, both using
[PCRE2](https://www.pcre.org/) regex.

### 4.1 Content placeholders: `{{varname}}`

[`tmpl_dir.c`](https://github.com/artgins/yunetas/blob/7.29.3/utils/c/yuno-skeleton/tmpl_dir.c). Pattern `(\{\{.+?\}\})`. Every `{{var}}` block in a
file whose name ends in `_tmpl` is replaced by the matching value from
`jn_values`. The templates use it inside `main.c_tmpl`,
`c_+rootname+.c_tmpl` and the other `_tmpl` files.

A `{{var}}` that nothing fills (no prompt asks for it, no derived variable
is named so) is rendered empty and said on stderr, once per occurrence:

```text
WARNING: {{yunoname}} has no value, rendered empty
```

Up to 7.28.6 it was rendered empty in silence, and the two yuno READMEs
carried a `Name: {{yunoname}}` nobody asked for. A yuno's name is chosen
per instance when the agent creates it, so it has no place in a template;
use `{{yunorole}}`.

### 4.2 Filename placeholders: `+varname+`

[`tmpl_dir.c`](https://github.com/artgins/yunetas/blob/7.29.3/utils/c/yuno-skeleton/tmpl_dir.c). Pattern `(\+.+?\+)`. Every `+var+` block in a
filename or directory name is replaced. The `_tmpl` suffix is stripped
on output ([`tmpl_dir.c`](https://github.com/artgins/yunetas/blob/7.29.3/utils/c/yuno-skeleton/tmpl_dir.c)). Example:

```
template path:   skeletons/gclass_child/c_+rootname+.c_tmpl
rootname=foo →   ./c_foo.c
```

### 4.3 Derived variables

The catalog gives the user-prompted vars. The engine adds derived ones
automatically in [`make_skeleton.c`](https://github.com/artgins/yunetas/blob/7.29.3/utils/c/yuno-skeleton/make_skeleton.c):

| Variable      | Derivation                                                |
|---------------|-----------------------------------------------------------|
| `rootname`    | The gclass name (or root gobj name) asked for, lowercased |
| `Rootname`    | Capitalised (first letter upper)                          |
| `ROOTNAME`    | Uppercased                                                |
| `yunorole`    | Yuno role, lowercased (for yuno templates)                |
| `Yunorole`    | Capitalised                                               |
| `YUNOROLE`    | Uppercased                                                |
| `utility`, `Utility`, `UTILITY` | The utility name (Utility templates)    |
| `project`, `Project`, `PROJECT` | The project name (Project templates)    |
| `__year__`    | Current year from system time                             |

Use the right case form in your edits. Mixing them silently produces
ugly identifiers (`C_my_service` instead of `C_MY_SERVICE`).

### 4.4 Files that are not templated

Files whose name does not end in `_tmpl` are **copied verbatim**, even
if they contain `{{var}}` markers. The `_tmpl` suffix is the opt-in
trigger. Either kind refuses to overwrite a file that exists.

---

## 5. `yuno_citizen` vs `yuno_standalone`

Both produce a `src/main.c` plus the gclass scaffolding for the yuno's
main service. The structural difference is in `main.c_tmpl`.

### 5.1 `yuno_citizen` (default for almost all yunos)

`skeletons/yuno_citizen/src/main.c_tmpl`:

- Calls `register_c_{{rootname}}()`.
- The `environment` block wires the standard log handlers:
  UDP to logcenter, file to local rotatory, stdout when running in
  console mode. See [`DEBUGGING.md`](DEBUGGING.md) §5.4.
- `fixed_config` sets `yuno_role` and tags.
- `variable_config` declares the standard service stack:
  an `authz` service plus the app service.
- Memory tuning aimed at "lots of small services on a host" (~200M).

Talks to its local `yuno_agent` over an `ievent` channel (the identity
card handshake — see [`IPC.md`](IPC.md) §4.4). Logs ship via UDP to
logcenter by default. Use it for everything that you deploy with
`run-yuno`.

### 5.2 `yuno_standalone` (CLI / test / edge)

`skeletons/yuno_standalone/src/main.c_tmpl`:

- Has its own `argp` CLI parser — accepts `-f <config>`
  and `-V`/`--version`.
- No `authz` service, and no agent connection.
- `fixed_config` + `variable_config` simpler — only the app service.
- Memory tuning aimed at "one process owns the host" (~1G blocks).

Use it when the binary must run independently of any agent, for example a
CLI tool, a test harness, or an edge device that does not ship with a full
agent.

---

## 6. `gclass_service` vs `gclass_child`

Both produce a `c_+rootname+.{c,h}` pair conforming to CLAUDE.md's
banner structure. The crucial structural difference is the `mt_create`
subscription block. Both versions are in the templates verbatim — copy
them exactly, do not refactor.

### 6.1 `gclass_service` — SERVICE pattern

`skeletons/gclass_service/c_+rootname+.c_tmpl:115-124`:

```c
hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
if(subscriber) {
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);
} else if(gobj_is_pure_child(gobj)) {
    subscriber = gobj_parent(gobj);
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);
}
```

Reads:

- The standard case: a SERVICE has its `subscriber` set explicitly by
  whoever wires it.
- The fallback: if something created the gobj as a pure child, the gobj
  subscribes the parent instead.

This is the canonical C block: the root gclass of the `yuno_citizen` and
`yuno_standalone` templates (the yuno's default service) carries it too, and so does every service
gclass of the kernel. The JS SERVICE block has no fallback (see the JS
gclass template and CLAUDE.md, *GClass subscription model*).

Service templates also include `help` and `authzs` commands in their
`command_table`.

Use for **anything that is registered with `gobj_create_default_service`,
`gobj_create_service`, or expected to be reachable by name from
another yuno or the SPA**.

### 6.2 `gclass_child` — CHILD pattern

`skeletons/gclass_child/c_+rootname+.c_tmpl:83-89`:

```c
hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
if(!subscriber) {
    subscriber = gobj_parent(gobj);
}
gobj_subscribe_event(gobj, NULL, NULL, subscriber);
```

Reads:

- Use the explicit `subscriber` if provided, otherwise unconditionally
  the parent.
- There is **no else** — every child gets a subscriber.

The CHILD pattern assumes the gobj is always created with a parent
(`gobj_create_pure_child`, `gobj_create_volatil` and the other create
functions). The parent's
FSM **must** declare every event the child can publish (see
[`IPC.md`](IPC.md) §3.5 — this is the canonical *"Event NOT DEFINED in
state"* source).

Use for **per-connection objects, protocol handlers, transient helpers
that always have a parent**.

---

## 7. The banner convention

CLAUDE.md ("GClass templates and skeletons"): *every banner from the
skeleton must be present, even when its section is empty. Do not add
extra banners outside the skeleton set. Do not reorder sections in legacy
gclasses. Merge new code into the existing layout, to keep `git blame`
clean. Greenfield gclasses follow the skeleton order.*

The banner blocks the templates ship (example from
`yuno_citizen/src/c_+rootname+.c_tmpl`):

```c
                    /******************************
                     *      Framework Methods
                     ******************************/

                    /***************************
                     *      Commands
                     ***************************/

                    /***************************
                     *      Local Methods
                     ***************************/

                    /***************************
                     *      Actions
                     ***************************/
```

The templates also carry the canonical headers for `Attributes`,
`PRIVATE DATA`, `Event types` and `States`. See [`c_yuno.c`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/root-linux/src/c_yuno.c) (large
reference example) and [`c_timer.c`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/root-linux/src/c_timer.c) (minimal reference example).

Important consequences:

  - Do not delete a banner because its section has no entries.
- Do not insert a section between two banners.
- Do not rename a banner. The format is exact, so count the blank lines
  and the asterisks.

---

## 8. Post-scaffold checklist

After `yuno-skeleton <template> <name>` you have a tree with rendered
`_tmpl` files. Standard next steps:

```bash
cd <name>
mkdir build && cd build
cmake ..
make
```

For a citizen yuno you must also do these steps:

1. **Create a binary record** in the agent: `install-binary content64=$$(<role>)`
   (see [`YUNO_LIFECYCLE.md`](YUNO_LIFECYCLE.md) §6.1).
2. **Create a configuration**: `create-config id=<role>.<name>
content64=$$(<role>_<name>.json)`. The alias is `install-config`, and the
   `__version__` field inside the file gives the version.
3. **Create the yuno record** that links realm, binary and config.
4. **Enable the yuno, then run it**.

For a standalone yuno, run the binary with `-f <config.json>`.

If you scaffolded a new gclass, you also need to:

- `#include "c_<rootname>.h"` in the consumer.
- Call `register_c_<rootname>()` in the consuming yuno's `main.c`.
- Verify that you registered it before any `gobj_create*` that mentions
  the gclass — otherwise the runtime aborts.

---

## 9. Sharp edges

### 9.1 Do not refactor away the `mt_create` subscription block

The exact two patterns in §6.1 and §6.2 are CLAUDE.md hard rules. They look
strange, because the inverted condition `else if(gobj_is_pure_child(gobj))`
in the SERVICE case looks like a defect. It is not a defect. It is a
deliberate fallback that prevents *"Publish event WITHOUT subscribers"* in
mixed deployments. Copy it exactly.

### 9.2 The `_tmpl` suffix is the opt-in

A file without `_tmpl` is **not rendered**, even if it contains
`{{var}}` markers. If you write your own template and forget the suffix,
you get the literal `{{rootname}}` in the output.

### 9.3 `+varname+` only renames, `{{varname}}` only fills content

The two syntaxes are not interchangeable. If you want a file named after
a variable, use `+var+`. If you want the variable's value inside the
file, use `{{var}}`.

### 9.4 Case forms are not aliases

`{{rootname}}`, `{{Rootname}}`, `{{ROOTNAME}}` are three different
variables. The template author picks each occurrence consciously. If
you mix them, you produce names like `C_my_service` or `c_MY_SERVICE`
that compile but read like a bug report.

### 9.5 `--skeletons-path` defaults to `/yuneta/bin/skeletons`

A development checkout has them at
`utils/c/yuno-skeleton/skeletons/`. The build installs them there, and
replaces the installed copy whole (up to 7.28.6 a template
removed or renamed in the tree stayed installed, and the packages shipped
it). `skeletons.d/` is the user's own and is kept. To use the working tree
without installing, pass `-p`:

```bash
yuno-skeleton -p $YUNETAS_BASE/utils/c/yuno-skeleton/skeletons gclass_child
```

### 9.6 The README mentions a sibling `ymake-skeleton`

`utils/c/yuno-skeleton/README.md:12`: *"See also `ymake-skeleton` for
cloning from an existing project rather than a template library."*

`ymake-skeleton` is a separate utility (`utils/c/ymake-skeleton/`). It
turns an existing project into a template that `yuno-skeleton` can
instantiate; `yclone-project` clones a project directly into a new name.
The `yuno-skeleton` tool is the right pick for greenfield.

### 9.7 The JS GUI scaffold is the `gui/` of the project templates

The **JS GUI yuno** scaffold (Vite, gobj-js, the declarative shell of
gobj-ui v2) is the `gui/` that `project` and `project_commercial` make. It
lived in the `templates/js_gui/` of a private application, from 2026-05-21
to 2026-10-26, with a `generate.sh` of its own; it is kept once, in
`skeletons/project_base/gui/`, and rendered by `yuno-skeleton` like any
other template. Its locales carry i18next's own `{{name}}` and `{{n}}`,
which the catalog lists in `keep`, so they are left as they are.

gobj-ui's deploy round raises its ranges (`package.json_tmpl`) and runs
`utils/c/yuno-skeleton/check_project_gui.sh`, which generates a project and
installs and builds its gui; `yuno_skeleton/templates` checks the rest of it
without a network. Loading the generated app in a browser with a clean
console is still done by hand.

`js_gclass` and `js_gclass_service` make a single JS gclass, for an SPA
that exists already.

---

## 10. Recipes

### 10.1 New citizen yuno from scratch

```bash
# 1. scaffold (it asks for the role: my_service)
yuno-skeleton yuno_citizen

cd my_service
mkdir build && cd build
cmake .. && make

# 2. install the binary in the agent
ycommand -c "install-binary content64=\$\$(my_service)"

# 3. install a config (a JSON file you've written; version from its __version__)
ycommand -c "create-config id=my_service.instance_01 content64=\$\$(my_service_instance_01.json)"

# 4. create the yuno record (realm must already exist — see REALMS.md)
ycommand -c "create-yuno realm_id=<realm-id> yuno_role=my_service yuno_name=instance_01"

# 5. start
ycommand -c "enable-yuno id=<yuno_id>"
ycommand -c "run-yuno    id=<yuno_id>"
```

### 10.2 Add a new SERVICE gclass to an existing yuno

```bash
cd <existing-yuno>/src
yuno-skeleton gclass_service        # gclass name: my_new_service

# the new files: c_my_new_service.c, c_my_new_service.h

# in <existing-yuno>/src/CMakeLists.txt
#   add c_my_new_service.c to the sources list

# in <existing-yuno>/src/main.c
#   #include "c_my_new_service.h"
#   call register_c_my_new_service() BEFORE the first gobj_create

# rebuild + redeploy (YUNO_LIFECYCLE.md §6.2)
```

### 10.3 Add a new CHILD gclass

```bash
yuno-skeleton gclass_child          # gclass name: my_child

# - register in main.c BEFORE the first gobj_create that uses it
# - the parent gclass MUST declare every event my_child publishes,
#   in its event_action_list for the relevant states
#   (otherwise: "Event NOT DEFINED in state" — see IPC.md §8.2)
```

### 10.4 New JS gclass for the SPA

```bash
cd <spa>/src
yuno-skeleton js_gclass             # gclass name: my_widget

# the new file: c_my_widget.js, gclass C_MY_WIDGET, CHILD subscription
# model, a C_TIMER pure child armed in mt_start. It imports from
# @yuneta/gobj-js, which the SPA already depends on.
# js_gclass_service is the same with the SERVICE subscription block:
# for a gclass created with gobj_create_service(), whose audience
# subscribes through its `subscriber` attr.

# in the SPA's main.js
#   import {register_c_my_widget} from "./c_my_widget.js";
#   call register_c_my_widget() BEFORE the first gobj_create
#   that uses it (see IPC.md §7 for the SPA case)
```

### 10.5 Standalone CLI tool (no agent)

```bash
yuno-skeleton yuno_standalone       # role: my_tool
cd my_tool
mkdir build && cd build
cmake .. && make

# Run directly with a config file
./my_tool -f /path/to/my_tool.json
```

### 10.6 New application repository

```bash
yuno-skeleton project               # or project_commercial, for a closed one
                                    # project name: my_app
cd my_app/yunos
yuno-skeleton yuno_citizen          # role: my_yuno

# in my_app/yunos/CMakeLists.txt
#   replace  # add_subdirectory(my_yuno)  with  add_subdirectory(my_yuno)

cd ../..
yunetas register-project $PWD/my_app
yunetas build my_app
```

The repository gets `CMakeLists.txt` (builds `yunos/`), `yunos/CMakeLists.txt`
(finds the SDK through `YUNETAS_BASE`, one `add_subdirectory()` per yuno),
`yunos/batches/` for the configuration of each node, `README.md`,
`CHANGELOG.md`, `LICENSE`, `.gitignore` and `gui/`, the web frontend
(§9.7). `project` writes an MIT `LICENSE`; `project_commercial` a
proprietary one that leaves the third-party components under their own
licenses, and a README section with what a delivery must carry. Yuneta
itself stays MIT either way.

Both entries of the catalog `extends` the directory `project_base` (copied
first; their own `LICENSE_tmpl` and `README.md_tmpl` go on top, and a file
in both is an error), so what they share is written once:

```json
{
    "name": "project_commercial",
    "type": "Project",
    "extends": "project_base",
    "keep": ["name", "n"],
    "vars": [ ... ]
}
```

To start the GUI:

```bash
cd my_app/gui
npm install
npm run dev
```

---

## 11. Code pointers

| What                                              | Where                                                                  |
|---------------------------------------------------|------------------------------------------------------------------------|
| Tool entry point                                  | [`utils/c/yuno-skeleton/yuno_skeleton.c`](https://github.com/artgins/yunetas/blob/7.29.3/utils/c/yuno-skeleton/yuno_skeleton.c)                          |
| Templating engine (variables + dispatch)          | [`utils/c/yuno-skeleton/make_skeleton.c`](https://github.com/artgins/yunetas/blob/7.29.3/utils/c/yuno-skeleton/make_skeleton.c)                        |
| Directory walker + content rendering              | [`utils/c/yuno-skeleton/tmpl_dir.c`](https://github.com/artgins/yunetas/blob/7.29.3/utils/c/yuno-skeleton/tmpl_dir.c)                     |
| Template catalog                                  | `utils/c/yuno-skeleton/skeletons/__skeletons__.json`                   |
| `yuno_citizen` main template                      | `utils/c/yuno-skeleton/skeletons/yuno_citizen/src/main.c_tmpl`         |
| `yuno_standalone` main template                   | `utils/c/yuno-skeleton/skeletons/yuno_standalone/src/main.c_tmpl`      |
| SERVICE gclass subscription block                 | `skeletons/gclass_service/c_+rootname+.c_tmpl:115-124`                 |
| CHILD gclass subscription block                   | `skeletons/gclass_child/c_+rootname+.c_tmpl:83-89`                     |
| JS gclass templates                               | `skeletons/js_gclass/c_+rootname+.js_tmpl`, `skeletons/js_gclass_service/c_+rootname+.js_tmpl` |
| Application repository templates                  | `skeletons/project/`, `skeletons/project_commercial/`                  |
| Banner-rule discussion                            | [`CLAUDE.md`](../../../CLAUDE.md) — "GClass templates and skeletons"   |
| Canonical large gclass examples                   | [`kernel/c/root-linux/src/c_yuno.c`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/root-linux/src/c_yuno.c), [`yunos/c/yuno_agent/src/c_agent.c`](https://github.com/artgins/yunetas/blob/7.29.3/yunos/c/yuno_agent/src/c_agent.c) |
| Canonical minimal gclass example                  | [`kernel/c/root-linux/src/c_timer.c`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/root-linux/src/c_timer.c)                                    |
