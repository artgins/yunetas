# gui_agent — Yuneta Agent Console

A web SPA (single-page app) to operate yuneta **agents** from the browser,
built on the **v2 declarative shell** of `@yuneta/gobj-ui`
(`C_YUI_SHELL` + `C_YUI_NAV`).

It is the modern successor of the old webix "Yuneta CLI"
(`yuno_gui/v2/.../ui_yuneta_cli.js`): the control-plane **CLI to the agent**
is the first panel; treedb (table + graph) and live stats follow.

**Canonical URL:** the SPA is served at `https://agents.yunetacontrol.com`
(new apex — needs its own DNS zone + TLS cert at deploy time). This is the
app's own origin, **not** a backend endpoint: it does not go in
`csp_connect_src`. In Phase 2 it must be registered in the IdP as a Valid
Redirect URI (`https://agents.yunetacontrol.com/*`) and Web Origin, and the
agent must accept `wss` upgrades from this origin.

## Key design choice: config lives in the browser, not the repo

Unlike `gui_treedb` (which hardcodes endpoints in `src/conf/backend_config.js`),
this app ships **no private data**. The user enters the authentication URL and
the agent endpoints through **forms** in the *Settings* views, and those values
are persisted as **gobj persistent attrs** in the browser `localStorage`
(`db_save/load_persistent_attrs`, wired in `src/main.js`).

`src/conf/defaults.js` only carries empty templates and a non-secret example.

> **CSP note:** `config.json` → `csp_connect_src` is a **build-time** security
> boundary. The browser only allows WebSocket/HTTPS connections to the origins
> listed there. An agent URL the user adds in Settings **must** match one of
> those origins; adding a brand-new origin requires editing `config.json` and
> rebuilding.

## Library consumption (v2)

Both kernel JS packages are consumed as local `file:` deps on the submodules:

```
@yuneta/gobj-js -> ../../../kernel/js/gobj-js   (file:, v2 source via vite alias)
@yuneta/gobj-ui -> ../../../kernel/js/gobj-ui   (file:, v2 / main line)
```

This is the v2 (`main`) line — **not** the published npm v1 used by
estadodelaire/hidraulia.

## Transport to the agent

The CLI panel will create a `C_IEVENT_CLI` child (from `@yuneta/gobj-js`) and
send commands with `gobj_command(iev, command, kw, src)`, exactly like
`ycommand`. Defaults match `ycommand`: remote role `yuneta_agent`, service
`__default_service__`, transport `wss://…:1993` (OAuth2) or `ws://…:1991`
(plain, local only). Answers arrive as `EV_MT_COMMAND_ANSWER`
(`{result, comment, schema, data}`).

## Build & run

```bash
cd yunos/js/gui_agent
npm install
npm run dev        # vite dev server
npm run build      # production bundle into dist/
```

## Roadmap (phases)

| Phase | Content |
|-------|---------|
| **0** | Scaffold: shell + nav, placeholder views, green build *(this commit)* |
| **1** | `C_AGENT_CONSOLE` (CLI panel) + `C_SETTINGS` (agents form, persistent attrs); MVP target `app.wattyzer.com` over `wss`+OAuth2 |
| **2** | Authentication: configurable OIDC/Keycloak `auth_url` → JWT → `wss:1993` |
| **3** | TreeDB (table + graph, reusing `C_YUI_TREEDB_*`) and live stats (`gobj_stats` + `C_YUI_UPLOT`) |

## Status

**Phase 0 — skeleton.** Every menu leaf targets the placeholder
`C_GUI_AGENT_VIEW`; real views replace them per the table above.
