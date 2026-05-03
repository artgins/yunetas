# TODO — next session

Coordinated work that needs more than this repo to land.  Each item lists
what to read, the change to apply, and the consumers to validate against.

## lib-yui shell: brand/avatar/dropdown + per-item show_on

**Status: lib-yui side LANDED on this branch (commits 1aca622 →
93837ff → 03758b2 → e90ab4e).** The three additive features are
implemented, documented and unit-tested under
`kernel/js/lib-yui/tests/shell_toolbar_helpers.test.mjs`
(23 new tests; 48 total green).  `npm run build` is green.

What lib-yui now exposes (all additive, legacy `toolbar.items` shapes
keep working unchanged):

  1. **Per-item `show_on`** on every toolbar item, same syntax/parser
     as `shell.zones[id].show_on`.
  2. **`type: "brand"`** — logo (img URL) + wordmark (text) item.
     Required: `logo`, `wordmark`.  Optional: `alt`, `action`
     (typically `navigate`).  Without an action it renders as a
     passive `<div>`; with an action as a `<button>`.
  3. **`type: "avatar"`** — circular initials badge.  Initials come
     from a host-registered callback:
     ```js
     yui_shell_set_avatar_provider(shell, () => "JD");
     yui_shell_refresh_avatars(shell);
     ```
     `lib-yui` never reads localStorage or app-specific attrs.
  4. **`action.type: "dropdown"`** — panel anchored to the trigger,
     mounted on the `popup` layer, integrated with the escape-stack
     and focus-trap.  Sub-items: navigate / drawer / event +
     `{type:"divider"}` separators; nested dropdowns rejected.
     Sub-items accept `show_on` for parity.

New public helpers (re-exported from `index.js`):

  * `yui_shell_set_avatar_provider(shell, fn)`
  * `yui_shell_refresh_avatars(shell)`
  * `yui_shell_close_dropdown(shell)`

Reference docs: `kernel/js/lib-yui/SHELL.md` §3.4.

### What still has to land — the consumer migrations

These are in *other* repositories; they cannot be touched from this
branch.  Each one removes hand-rolled workarounds that the new shell
contract makes obsolete.

#### artgins/wattyzer (current main consumer)

The mobile-navigation redesign in branch
`claude/redesign-mobile-navigation-NilY6` introduced three local
workarounds that should now be deleted in favour of the shell
contract:

  * `gui/src/app_config.json` — replace the `wz-brand-mark` icon hack
    with `type: "brand"` + `logo` + `wordmark`.  Replace the
    CSS-only `show_on` workarounds (`data-toolbar-item-id` selectors)
    with per-item `show_on`.  Switch the `user` item to
    `type: "avatar"` + `action.type: "dropdown"`.
  * `gui/src/c_wz_app.js` — delete `install_user_dropdown()` and
    helpers (~250 lines).  Wire
    `yui_shell_set_avatar_provider(shell, () => initials_from_user())`
    in the boot flow, and call `yui_shell_refresh_avatars(shell)`
    when the user model changes.
  * `gui/src/wz_overrides.css` — drop the toolbar-specific CSS
    targeting `data-toolbar-item-id` (~100 lines).
  * Re-run the bottom-bar / brand / dropdown flows on mobile and
    desktop.  Pin lib-yui to a build that contains the four commits
    above.

#### artgins/hidraulia (legacy lib-yui consumer)

Audit its toolbar config for any custom CSS hook on
`data-toolbar-item-id` or any imperative dropdown/avatar code.
Migrate to the new contract; drop workarounds.  Older lib-yui pin —
check the version-bump cost first.

#### artgins/estadodelaire (legacy lib-yui consumer)

Same audit + migration as hidraulia.  Likely on an even older pin;
the version-bump cost may dominate the migration value.  If the bump
is non-trivial, document the gap and defer; do not block on it.

### Definition of done (overall)

  * [x] Three new features implemented in lib-yui with tests under
        `kernel/js/lib-yui/tests/`.
  * [x] `SHELL.md` updated.  Skeleton (`skeleton/config.json`) is
        app-level config and does not need a shell-contract update.
  * [ ] Wattyzer migrated, workarounds removed, build green.
  * [ ] Hidraulia and estadodelaire either migrated or explicitly
        deferred with a note (both keep working without changes).
  * [ ] `.github/workflows/lib-yui.yml` matrix passes on this branch.
