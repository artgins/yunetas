#!/usr/bin/env python3
"""
    Assemble the Spanish artifact of /navigation into one self-contained
    HTML file, ready to publish on claude.ai.

    WHY A BUILD STEP: an artifact serves no sibling files and fetches
    nothing external, so the runtime and the demos have to travel inside
    the page. The site's own /navigation does not — it loads them from
    next door — and duplicating them by hand is how the two drift.

        python3 build_artifact.py            # -> /tmp/navigation.es.html
        python3 build_artifact.py out.html

    What it takes:
      - content.es.html   the Spanish page body (the only file kept by
                          hand; the English one lives in ../index.html)
      - ../index.html     for the demos' CSS, which is shared verbatim
      - ../demos.js       the demos, with their strings translated below
      - gobj-js's IIFE build, straight from the submodule

    The IIFE build and not the ES one: the ES bundle minifies its names
    and only exposes them through its `export`, so pasting it into a page
    gives the demos access to nothing. The IIFE publishes one global,
    `gobjJs`, which survives. The demos then run inside an IIFE of their
    own, because the bundle declares names of its own (`h`, for one) that
    would otherwise collide.
"""
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
PAGE = HERE.parent                      # docs/doc.yuneta.io/navigation
REPO = PAGE.parents[2]                  # the yunetas checkout
RUNTIME = REPO / "kernel/js/gobj-js/dist/gobj-js.iife.min.js"

#
#   The demos' user-visible strings. Everything else in demos.js is code
#   or comments, which stay in English like the rest of the repo.
#
STRINGS = [
    ('{id: "sales",   name: "Sales",   body: "Revenue by region, this quarter."}',
     '{id: "ventas",  name: "Ventas",  body: "Ingresos por región, este trimestre."}'),
    ('{id: "costs",   name: "Costs",   body: "Where the money went."}',
     '{id: "costes",  name: "Costes",  body: "A dónde se fue el dinero."}'),
    ('{id: "traffic", name: "Traffic", body: "Requests per second, by node."}',
     '{id: "trafico", name: "Tráfico", body: "Peticiones por segundo, por nodo."}'),
    ('"← Back"', '"← Atrás"'),
    ('"Forward →"', '"Adelante →"'),
    ('"browser history: "', '"historial del navegador: "'),
    ('"The link node ends the structure. From here the "\n                + "subpath belongs to the view."',
     '"El nodo link acaba la estructura. De aquí abajo el "\n                + "subpath es de la vista."'),
    ('"depth " + priv.path.length + " · one declared route: /admin"',
     '"profundidad " + priv.path.length + " · una ruta declarada: /admin"'),
    ('"the view at rest"', '"la vista en reposo"'),
    ('const PAGES = ["Pick a device", "Edit settings", "Confirm"];',
     'const PAGES = ["Elige dispositivo", "Edita ajustes", "Confirma"];'),
    ('"depth " + priv.stack.length + " · the url never moved"',
     '"profundidad " + priv.stack.length + " · la URL no se ha movido"'),
    ('"popped past the root → EV_PAGER_EXIT: the host closes."',
     '"pop más allá de la raíz → EV_PAGER_EXIT: el anfitrión cierra."'),
]


def die(msg):
    print(f"build_artifact: {msg}", file=sys.stderr)
    sys.exit(1)


def main():
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("/tmp/navigation.es.html")

    for f in (HERE / "content.es.html", PAGE / "index.html", PAGE / "demos.js", RUNTIME):
        if not f.exists():
            die(f"missing {f}")

    art = (HERE / "content.es.html").read_text(encoding="utf8")

    #
    #   The demos' CSS is taken from the English page rather than copied,
    #   so restyling a demo lands in both without anyone remembering to.
    #
    site = (PAGE / "index.html").read_text(encoding="utf8")
    try:
        i = site.index("  /*\n   *  The demos.")
        j = site.index("  a { color: var(--url); }", i)
    except ValueError:
        die("cannot find the demo CSS block in ../index.html — did its "
            "leading comment change?")
    if "  a { color: var(--url); }" not in art:
        die("cannot find the CSS anchor in content.es.html")
    art = art.replace("  a { color: var(--url); }",
                      site[i:j] + "  a { color: var(--url); }", 1)

    for slot in ("demo-url", "demo-tree", "demo-pager"):
        if f'id="{slot}"' not in art:
            die(f"content.es.html has no <div id=\"{slot}\"> for the demo")

    demos = (PAGE / "demos.js").read_text(encoding="utf8")

    #   The import becomes a destructuring of the IIFE's global.
    m = re.search(r'import \{([^}]*)\} from "[^"]*";[^\n]*', demos)
    if not m:
        die("../demos.js has no import to rewrite")
    names = [n.strip() for n in m.group(1).split(",") if n.strip()]
    demos = (demos[:m.start()]
             + "const {\n    " + ",\n    ".join(names) + "\n} = gobjJs;"
             + demos[m.end():])

    missing = [en for en, _ in STRINGS if en not in demos]
    if missing:
        die("these strings are no longer in demos.js — translate the new "
            "wording and update STRINGS:\n  " + "\n  ".join(missing))
    for en, es in STRINGS:
        demos = demos.replace(en, es)

    art += (
        "\n<!--\n"
        "    Runtime + demos INLINE: an artifact serves no sibling files and\n"
        "    fetches nothing external. Built by navigation/artifact/build_artifact.py\n"
        "    from the same demos as https://doc.yuneta.io/navigation.\n"
        "-->\n"
        "<script>\n" + RUNTIME.read_text(encoding="utf8") + "\n</script>\n"
        "<script>\n;(function () {\n" + demos + "\n})();\n</script>\n"
    )

    out.write_text(art, encoding="utf8")
    print(f"build_artifact: {out} ({len(art) // 1024} KB)")


if __name__ == "__main__":
    main()
