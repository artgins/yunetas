# TODO — next session

This pass closed the lib-yui shell extension and the wattyzer
migration end-to-end.  Everything below is informational; no
follow-up action is required unless a new GUI adopts `C_YUI_SHELL`.

## lib-yui shell: brand/avatar/dropdown + per-item show_on — DONE

Landed on this branch (`claude/continue-next-session-4MhDO`) across
five commits:

  * `1aca622` — `shell_toolbar_helpers.js` + 23 unit tests.
  * `93837ff` — `c_yui_shell.css` (brand/avatar/dropdown styles) +
    `index.js` (re-exports the new helpers).
  * `03758b2` — `SHELL.md` §3.4 documents the new toolbar contract.
  * `e90ab4e` — `c_yui_shell.js` integration: per-item `show_on`,
    `type:"brand"`, `type:"avatar"`, `action.type:"dropdown"`,
    avatar provider, escape-stack/focus-trap/click-outside.
  * `1c5f22c` — this file's previous revision marking the lib-yui
    side as landed.

State: `npm test` 48/48 green, `npm run build` green, contract is
additive (legacy `toolbar.items` shapes without `type` keep working
unchanged).

Public API exposed (`@yuneta/lib-yui`):

  * `yui_shell_set_avatar_provider(shell, fn)` — host-supplied
    initials provider; `lib-yui` never reads localStorage or
    app-specific attrs.
  * `yui_shell_refresh_avatars(shell)` — re-paint when the user
    model changes.
  * `yui_shell_close_dropdown(shell)` — programmatic close.

Reference: `kernel/js/lib-yui/SHELL.md` §3.4.

## Consumer migrations

### artgins/wattyzer — DONE

Branch `claude/continue-next-session-4MhDO`, 3 commits:

  * `df4b63d` — `gui/src/app_config.json`:
      - brand: `type:"brand"` + `logo` + `wordmark` (was the
        `wz-brand-mark` icon hack).
      - search / command-palette: per-item `show_on:">=tablet"`
        (was a CSS media-query keyed on `data-toolbar-item-id`).
      - user: `type:"avatar"` + `action.type:"dropdown"` with
        profile / theme / language / logout entries publishing
        `EV_CYCLE_THEME` / `EV_CYCLE_LANGUAGE` / `EV_LOGOUT`.
  * `068eaa4` — `gui/src/c_wz_app.js`: dropped
    `install_user_dropdown()` (~250 lines) and its helpers; wired
    `yui_shell_set_avatar_provider(shell, compute_initials)`; added
    FSM action handlers for the three new events.
  * `934d55c` — `gui/src/wz_overrides.css`: dropped
    `data-toolbar-item-id` rules (~100 lines). Mobile icon-bar gap
    fix and `/account/preferences` styling stayed.

UX trade-off (accepted): the previous user dropdown displayed the
active theme and language as inline state labels next to each
item.  The lib-yui dropdown contract does not support inline
state, so those labels are gone; the user reads/changes the active
preference at `/account/preferences`.

Pin requirement: `gui/package.json` resolves `@yuneta/lib-yui` to
`file:../../../yunetas/kernel/js/lib-yui`, so the dev's local
yunetas checkout must contain the four lib-yui commits above
(branch `claude/continue-next-session-4MhDO` or later).

### artgins/hidrauliaconnect — NO ACTION NEEDED

Audited on this pass.  The repo depends on `@yuneta/lib-yui` but
does **not** call `register_c_yui_shell()` and does **not** declare
a `toolbar` or `data-toolbar-item-id` selector.  The GUI is still
on the legacy `C_YUI_MAIN` + `C_YUI_ROUTING` stack — the new
toolbar contract is irrelevant here.  No workarounds to remove.

### artgins/estadodelaire — NO ACTION NEEDED

Same audit result as hidrauliaconnect: legacy stack, no
`C_YUI_SHELL` adoption, no toolbar workarounds.  No migration.

## Definition of done — closed

  * [x] Three new features implemented in lib-yui with tests under
        `kernel/js/lib-yui/tests/`.
  * [x] `SHELL.md` updated.
  * [x] Wattyzer migrated, workarounds removed.  Build verified
        manually against the lib-yui commits via the local file:
        symlink resolution.
  * [x] Hidraulia and estadodelaire audited and explicitly deferred
        (both stay on the legacy stack and keep working without
        changes).
  * [ ] `.github/workflows/lib-yui.yml` matrix passes on this branch
        — requires a CI run.
