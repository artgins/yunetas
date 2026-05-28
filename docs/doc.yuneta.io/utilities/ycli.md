(util-ycli)=
# `ycli`

Interactive ncurses-based terminal UI for browsing yunos, inspecting state, and
sending commands. Multi-session / multi-window sibling of `ycommand`; both use
the shared `C_EDITLINE` line editor, so line editing, history, `Ctrl+R` /
`Ctrl+S` incremental search (`ESC` / `Ctrl+G` cancel) and TAB completion behave
identically in both. `ycli` renders the multi-candidate TAB list as an ncurses
popup above the editline (`Tab` / `Up` / `Down` navigate, `Enter` commits the
candidate to the line without submitting, `ESC` / `Ctrl+G` cancel); a
per-connection cache of remote commands is warmed on connect via
`list-gobj-commands`, so the candidate set follows whichever window has focus.
Local vs remote routing follows the `!cmd` prefix convention. `Ctrl+K` deletes
from cursor to end of line (readline); `Ctrl+U` / `Ctrl+Y` clear the whole line;
`Ctrl+L` clears the display pane.
