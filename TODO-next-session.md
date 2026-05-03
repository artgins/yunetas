# TODO — next session

Coordinated work that needs more than this repo to land.  Each item lists
what to read, the change to apply, and the consumers to validate against.

## lib-yui shell: absorb the wattyzer toolbar workarounds

**Context.** The mobile-navigation redesign in
`artgins/wattyzer` (branch `claude/redesign-mobile-navigation-NilY6`)
needed three features the declarative shell does not yet expose:

  1. **Per-item `show_on`** — hide individual toolbar items per
     breakpoint, not just whole zones.
  2. **A brand item type** — render a logo image plus a wordmark
     with its own typography, not a font icon glyph.
  3. **A dropdown action type** — open a panel with menu entries
     instead of navigating / firing an event / toggling a drawer.

Wattyzer ships with workarounds (CSS attribute selectors on
`data-toolbar-item-id`, `icon: "wz-brand-mark"` repurposed as a CSS
class, an imperative `install_user_dropdown()` in `c_wz_app.js` that
mutates the rendered DOM).  See
`artgins/wattyzer:gui/src/wz_overrides.css`,
`artgins/wattyzer:gui/src/c_wz_app.js` and the tail of
`artgins/wattyzer:gui/src/app_config.json` for the current state.

**Goal.** Bring those three features into `kernel/js/lib-yui/` as
first-class config so wattyzer (and the legacy consumers below) can
delete the workarounds.

### Files to read first

  * `kernel/js/lib-yui/src/c_yui_shell.js`
      - `build_toolbar()` (renderer)
      - `build_zones()` and `apply_show_on()`
      - `handle_toolbar_action()` (current `action.type` switch)
  * `kernel/js/lib-yui/src/shell_show_on.js`
      - `breakpoints_from_expr()` already parses the same syntax we
        want at item level; reuse it.
  * `kernel/js/lib-yui/SHELL.md`
      - declarative contract; **must be updated** alongside the code.
  * `kernel/js/lib-yui/src/c_yui_shell.css`
      - `yui-hidden-*` helpers, toolbar / navbar classes.

### Proposed API (review with the user before coding)

```jsonc
{
    "toolbar": {
        "items": [
            { "id": "burger", "icon": "yi-bars", ... },

            // 1. Brand item — new type
            { "id": "brand", "type": "brand",
              "logo": "/wattyzer-mark.svg",
              "wordmark": "Wattyzer",
              "action": { "type": "navigate", "route": "/welcome" } },

            // 2. Per-item show_on
            { "id": "search", "icon": "yi-magnifying-glass",
              "show_on": ">=tablet", ... },

            // 3. Dropdown action type
            { "id": "user", "type": "avatar",
              "initials_from": "wz.user.name",
              "action": { "type": "dropdown",
                          "items": [
                              { "id": "profile", "name": "My profile",
                                "icon": "yi-user",
                                "action": { "type": "navigate",
                                            "route": "/account/profile" } },
                              { "type": "divider" },
                              { "id": "theme", "name": "Theme",
                                "icon": "yi-moon",
                                "action": { "type": "event",
                                            "event": "EV_CYCLE_THEME" } },
                              { "id": "lang", "name": "Language",
                                "icon": "yi-language",
                                "action": { "type": "event",
                                            "event": "EV_CYCLE_LANGUAGE" } },
                              { "type": "divider" },
                              { "id": "logout", "name": "logout",
                                "icon": "yi-right-from-bracket",
                                "action": { "type": "event",
                                            "event": "EV_LOGOUT" } }
                          ] } }
        ]
    }
}
```

Open questions for the design pass:
  * Should `brand` also accept a font-icon variant (no logo file) or
    is it always image + text?
  * Do dropdown items need their own `show_on`?  Probably yes for
    parity with toolbar items.
  * Avatar with initials — separate type (`avatar`) or a flag on the
    user item?  Initials source: localStorage key, an attr on the
    shell, or a callback registered by the host?

### Consumers to validate (access to be granted next session)

The existing toolbar config in each consumer is the regression
surface for the additive change.  Re-render each app and confirm the
workaround removal does not break it.

  * **artgins/wattyzer** (current main consumer)
      - `gui/src/app_config.json` — replace `icon: "wz-brand-mark"`
        with `type: "brand"`, drop the CSS-only `show_on` workaround,
        switch the `user` item to `action.type: "dropdown"`.
      - Delete `install_user_dropdown()` and friends from
        `gui/src/c_wz_app.js` (~250 lines).
      - Trim toolbar-specific rules from `gui/src/wz_overrides.css`
        (~100 lines).
      - Re-test the bottom-bar / brand / dropdown flows on mobile and
        desktop.
  * **hidraulia** (legacy lib-yui consumer)
      - Audit its toolbar config; flag any custom CSS hook on
        `data-toolbar-item-id` that the new contract subsumes.
      - Migrate to the new config, drop workarounds.
  * **estadodelaire** (legacy lib-yui consumer)
      - Same audit + migration as hidraulia.
      - This one is likely on an older lib-yui pin; check the
        version bump cost before promising the migration.

### Scope guard

`kernel/js/lib-yui/` is now flagged as "in active growth" in `CLAUDE.md`,
so the strict approval gate does not apply here — but the additive-only
rule does.  Existing `toolbar.items` shapes (no `type`, no `show_on`,
no `action.type: dropdown`) **must keep working unchanged** for any
consumer that hasn't migrated yet.

### Definition of done

  * Three new features implemented in lib-yui with tests under
    `kernel/js/lib-yui/tests/`.
  * `SHELL.md` updated, plus the `skeleton/` if applicable.
  * Wattyzer migrated, workarounds removed, build green.
  * Hidraulia and estadodelaire either migrated or explicitly
    deferred with a note (both keep working without changes).
  * `.github/workflows/lib-yui.yml` matrix passes.
