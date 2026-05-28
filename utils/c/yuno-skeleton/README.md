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

See also `ymake-skeleton` for cloning from an existing project rather than a template library.
