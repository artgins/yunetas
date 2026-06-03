(module-console)=
# Console

**Kconfig:** `CONFIG_MODULE_CONSOLE` — **GClasses:** `C_EDITLINE`

Interactive terminal line editing with history (based on linenoise). Used by
[`ycli`](#util-ycli), [`ycommand`](#util-ycommand) and [`mqtt_tui`](#util-mqtt_tui).

## C_EDITLINE

Line editor with keyboard input handling, cursor movement, text
manipulation, completion, and history navigation. Provides the input
layer for the `ycli`, `ycommand` and `mqtt_tui` clients.

**Events handled:** `EV_KEYCHAR`, `EV_EDITLINE_MOVE_START / _END / _LEFT / _RIGHT`,
`EV_EDITLINE_DEL_CHAR / _EOL / _LINE / _PREV_WORD`, `EV_EDITLINE_BACKSPACE`,
`EV_EDITLINE_COMPLETE_LINE`, `EV_EDITLINE_ENTER`,
`EV_EDITLINE_PREV_HIST / _NEXT_HIST`, `EV_EDITLINE_REVERSE_SEARCH / _FORWARD_SEARCH`
(incremental Ctrl+R / Ctrl+S history search), `EV_EDITLINE_SWAP_CHAR`,
`EV_SETTEXT` / `EV_GETTEXT`, `EV_CLEAR_HISTORY`.

**Public helpers (for clients):**

| Function | Purpose |
|----------|---------|
| `editline_set_completion_callback(gobj, cb, user_data)` | Register a TAB-completion callback; `cb` fills an `editline_completions_t` with candidates + optional descriptions via `editline_add_completion(lc, str, desc)`. |
| `editline_set_hints_callback(gobj, cb, free_cb, user_data)` | Register an inline-hint callback; `cb` returns a heap-allocated string (plus optional ANSI colour/bold) rendered to the right of the cursor in gray. `free_cb` releases the string after draw. |
| `editline_history_count(gobj)` / `editline_history_get(gobj, idx)` | Read-only access to the in-memory history (1-based) for bang expansion (`!N`), `!history`-style listings, or custom search UIs. |

History entries are de-duplicated on insert (bash `HISTCONTROL=erasedups` style)
and the file is overwritten from memory on `EV_EDITLINE_ENTER`.
