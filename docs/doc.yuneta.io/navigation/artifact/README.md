# The Spanish artifact of `/navigation`

`content.es.html` is the Spanish version of [`../index.html`](../index.html)
(*Getting back* → *Volver atrás en gobj-ui*), published as an artifact on
claude.ai rather than on this site. It lives here because it is a **translation
of the page next door**: keeping it beside its original is what makes a diverged
translation visible instead of silent.

It is the one exception to the repo's English-only rule, and a deliberate one:
this is content the project *publishes to a reader*, not documentation *of* the
project. Everything that documents the repo — this README, the build script, the
comments in `demos.js` — stays in English.

## Building it

```bash
python3 build_artifact.py            # -> /tmp/navigation.es.html
python3 build_artifact.py out.html
```

The script assembles a self-contained page: `content.es.html`, plus the demos'
CSS lifted from `../index.html`, plus `../demos.js` with its user-visible
strings translated, plus gobj-js's **IIFE** build inlined. An artifact serves no
sibling files and fetches nothing external, so all of it has to travel in the
page — which the site's own `/navigation` does not need to do.

Only `content.es.html` is maintained by hand. The demos, their CSS and the
runtime come from the page, so a fix there reaches the artifact by rebuilding.
If a demo's wording changes, the script **fails loudly** naming the strings it
could no longer find, rather than shipping a half-translated page.

## Publishing it

Republish to the **same URL**, or it mints a new one and the old link rots:

<https://claude.ai/code/artifact/0b37d7eb-8b9b-40a5-beed-6881d0e1a33c>
