(module-console-api)=
# Console C API

The functions a client of the [Console module](#module-console) calls: the
helpers of `C_EDITLINE` (keyboard, TAB completion, inline hints, history) and
the ncurses helpers the `ycli` windows draw with.

Source code:

- [`c_editline.h`](https://github.com/artgins/yunetas/blob/7.26.7/modules/c/console/src/c_editline.h)
- [`c_editline.c`](https://github.com/artgins/yunetas/blob/7.26.7/modules/c/console/src/c_editline.c)
- [`help_ncurses.h`](https://github.com/artgins/yunetas/blob/7.26.7/modules/c/console/src/help_ncurses.h)
- [`help_ncurses.c`](https://github.com/artgins/yunetas/blob/7.26.7/modules/c/console/src/help_ncurses.c)

Every `editline_*` function takes the `C_EDITLINE` gobj as its first argument
and reads its private data with no check of the gclass: pass it the gobj
`gobj_create_service(..., C_EDITLINE, ...)` returned, nothing else.

The canonical client is `ycli` (`utils/c/ycli/src/c_cli.c`): it creates the
editline, registers both callbacks, then opens the keyboard:

```C
priv->gobj_editline = gobj_create_service(
    "editline",
    C_EDITLINE,
    kw_editline,    // prompt, history_file, use_ncurses, colors, cx, cy, subscriber
    gobj
);
editline_set_completion_callback(priv->gobj_editline, cli_completion_cb, gobj);
editline_set_hints_callback(priv->gobj_editline, cli_hints_cb, cli_free_hint_cb, gobj);
priv->tty_fd = tty_keyboard_init();     // then a yev read event feeds EV_KEYCHAR
```

---

(tty_keyboard_init)=
## `tty_keyboard_init()`

`tty_keyboard_init()` gives the caller a file descriptor of the terminal to
read the keyboard from: a `dup()` of `STDIN_FILENO`, marked close-on-exec and
put in raw mode (no echo, no line buffering, Ctrl+C and Ctrl+Z arrive as keys
instead of signals). The first call registers an `atexit()` handler that puts
the terminal back as it was. The editline does not read the descriptor: the
caller reads it (`ycli`, `ycommand` and `mqtt_tui`, with a
`yev_create_read_event()`) and sends each key to the editline as
`EV_KEYCHAR`.

```C
int tty_keyboard_init(void);
```

**Parameters** — none.

**Returns**

The new file descriptor, or `-1` when the terminal cannot be put in raw mode,
already logged: *"NOT a TTY"* when stdin is not a terminal (a pipe, a cron
job, no login session), *"tcgetattr() FAILED"* / *"tcsetattr() FAILED"* with
the `errno`. `ycli` then says *"Cannot open a tty device. Are you in a valid
login session?"* and stops.

**Example**

```C
int tty_fd = tty_keyboard_init();
if(tty_fd < 0) {
    return -1;  // Error already logged
}
yev_event_h yev_reading = yev_create_read_event(
    yuno_event_loop(),
    yev_callback,
    gobj,
    tty_fd,
    gbuffer_create(1024, 1024)
);
```

---

(editline_set_completion_callback)=
## `editline_set_completion_callback()`

`editline_set_completion_callback()` registers the function the editline calls
when TAB is pressed. The callback receives the current line and fills a list
of candidates with [`editline_add_completion()`](#editline_add_completion):
none beeps, one replaces the line at once, several are shown to pick from
(with their descriptions). A second call replaces the first; `cb = NULL`
disables completion.

```C
typedef void (*editline_completion_cb_t)(
    hgobj gobj,                     // the C_EDITLINE gobj
    const char *buf,                // the line as typed so far
    editline_completions_t *lc,     // fill it with editline_add_completion()
    void *user_data
);

void editline_set_completion_callback(
    hgobj gobj,
    editline_completion_cb_t cb,
    void *user_data
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `C_EDITLINE` gobj. |
| `cb` | `editline_completion_cb_t` | The completion callback, or `NULL` to disable completion. |
| `user_data` | `void *` | Handed back to `cb` on every call (`ycli` passes its own gobj). |

**Returns** — nothing.

**Example** — complete the first word against a table of commands:

```C
PRIVATE void my_completion_cb(
    hgobj editline_gobj,
    const char *buf,
    editline_completions_t *lc,
    void *user_data
) {
    static const char *commands[] = {"help", "history", "list-yunos", 0};
    size_t len = strlen(buf);
    for(int i = 0; commands[i]; i++) {
        if(strncmp(commands[i], buf, len) == 0) {
            editline_add_completion(lc, commands[i], NULL);
        }
    }
}

editline_set_completion_callback(gobj_editline, my_completion_cb, gobj);
```

---

(editline_add_completion)=
## `editline_add_completion()`

`editline_add_completion()` appends one candidate to the list a completion
callback is filling. Both strings are copied (with `gbmem_*`), and the editline
frees the list after the TAB is handled: the caller keeps ownership of what it
passed.

```C
void editline_add_completion(
    editline_completions_t *lc,
    const char *str,
    const char *desc
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `lc` | `editline_completions_t *` | The list the callback received. |
| `str` | `const char *` | The WHOLE line that replaces the current one if this candidate is picked, not only the word being completed. |
| `desc` | `const char *` | A description shown next to the candidate when there are several; `NULL` or `""` for none. |

**Returns** — nothing. When memory cannot be allocated the candidate is not
added and nothing is logged (the allocator logs it).

**Example** — complete the last word and keep what was typed before it, as
`ycli` does for the parameters of a command:

```C
const char *last = strrchr(buf, ' ');
last = last ? last + 1 : buf;
char candidate[1024];
snprintf(candidate, sizeof(candidate), "%.*s%s",
    (int)(last - buf), buf,     // the head of the line, untouched
    "service="                  // the word that completes the last token
);
editline_add_completion(lc, candidate, "name of the service to command");
```

---

(editline_set_hints_callback)=
## `editline_set_hints_callback()`

`editline_set_hints_callback()` registers the function that draws an inline
hint: a text shown to the right of the cursor, in gray by default, on every
refresh of the line (`ycli` shows there the parameters a command still takes).
The callback returns `NULL` for no hint, or a string the editline draws and
then hands to `free_cb`. A second call replaces the first; `cb = NULL`
disables hints.

```C
typedef char *(*editline_hints_cb_t)(
    hgobj gobj,             // the C_EDITLINE gobj
    const char *buf,        // the line as typed so far
    int *out_color,         // ANSI SGR foreground color, default 90 (gray)
    int *out_bold,          // 1 bold, 0 normal
    void *user_data
);
typedef void (*editline_free_hint_cb_t)(
    char *hint,
    void *user_data
);

void editline_set_hints_callback(
    hgobj gobj,
    editline_hints_cb_t cb,
    editline_free_hint_cb_t free_cb,
    void *user_data
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `C_EDITLINE` gobj. |
| `cb` | `editline_hints_cb_t` | The hints callback, or `NULL` to disable hints. |
| `free_cb` | `editline_free_hint_cb_t` | Called with each hint after it is drawn, so the callback may use any allocator. With `NULL` nothing is freed: pass `NULL` only when `cb` returns static strings. |
| `user_data` | `void *` | Handed back to `cb` and `free_cb`. |

**Returns** — nothing.

**Example**

```C
PRIVATE char *my_hints_cb(
    hgobj editline_gobj,
    const char *buf,
    int *out_color,
    int *out_bold,
    void *user_data
) {
    if(strcmp(buf, "kill-yuno") == 0) {
        *out_color = 90;
        *out_bold = 0;
        return gbmem_strdup(" id=<yuno id>");
    }
    return NULL;
}

PRIVATE void my_free_hint_cb(char *hint, void *user_data)
{
    gbmem_free(hint);
}

editline_set_hints_callback(gobj_editline, my_hints_cb, my_free_hint_cb, gobj);
```

---

(editline_history_count)=
## `editline_history_count()`

`editline_history_count()` returns the number of entries of the in-memory
history. The empty slot the editline keeps for the line being edited is not
counted, so the numbers match what a `history` listing shows and what a
`!N` expansion refers to.

```C
int editline_history_count(hgobj gobj);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `C_EDITLINE` gobj. |

**Returns**

The number of history entries, `0` when there is none.

**Example** — see [`editline_history_get()`](#editline_history_get).

---

(editline_history_get)=
## `editline_history_get()`

`editline_history_get()` returns one entry of the in-memory history, numbered
from `1` (the oldest) to [`editline_history_count()`](#editline_history_count)
(the newest). History is de-duplicated on insert: a line typed again moves to
the newest place.

```C
const char *editline_history_get(hgobj gobj, int idx);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `C_EDITLINE` gobj. |
| `idx` | `int` | The 1-based index of the entry. |

**Returns**

The entry, owned by the editline (do not free it, and do not keep it past the
next line entered: the history may be reordered or trimmed). `NULL` when `idx`
is out of range.

**Example** — `ycli`'s `!history [filter]`:

```C
int n = editline_history_count(gobj_editline);
for(int i = 1; i <= n; i++) {
    const char *h = editline_history_get(gobj_editline, i);
    if(!h || !*h) {
        continue;
    }
    if(!empty_string(filter) && !strstr(h, filter)) {
        continue;
    }
    printf("%4d  %s\n", i, h);
}
```

---

(open_ncurses)=
## `open_ncurses()`

`open_ncurses()` starts ncurses on the terminal: it honours the user's locale
first (`setlocale(LC_ALL, "")`, so wide ncurses decodes UTF-8 and counts
multi-cell glyphs), then `initscr()`, `cbreak()`, `noecho()`, the keypad on,
and `halfdelay(1)` -- a read waits a tenth of a second at most. When the
terminal has colors it starts them with the terminal's own default
foreground and background (`-1`), which [`get_paint_color()`](#get_paint_color)
needs.

```C
WINDOW *open_ncurses(hgobj gobj);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | Not used. |

**Returns**

The `stdscr` window. `NULL` when ncurses is ALREADY open: the call does
nothing, and the caller keeps the window it got the first time.

**Notes**

Pair it with [`close_ncurses()`](#close_ncurses) on every exit path, or the
terminal is left in raw mode. `ycli` registers the close with `atexit()`
before opening.

**Example**

```C
PRIVATE void my_close_ncurses(void)
{
    close_ncurses();
}

atexit(my_close_ncurses);
WINDOW *wn = open_ncurses(gobj);
```

---

(close_ncurses)=
## `close_ncurses()`

`close_ncurses()` ends ncurses (`endwin()`) and gives the terminal back as it
was. It does nothing when ncurses is not open, so it is safe to call twice
(from an `atexit()` handler and from a normal stop).

```C
void close_ncurses(void);
```

**Parameters** — none.

**Returns** — nothing.

**Example** — see [`open_ncurses()`](#open_ncurses).

---

(get_paint_color)=
## `get_paint_color()`

`get_paint_color()` turns a pair of color names into the ncurses attribute
that paints with them, creating the color pair the first time the
combination is asked for and reusing it afterwards.

```C
int get_paint_color(const char *fg_color, const char *bg_color);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `fg_color` | `const char *` | Foreground: `black`, `red`, `green`, `yellow`, `blue`, `magenta`, `cyan` or `white` (case does not matter). `NULL`, `""`, `default`, `def` or an unknown name is the terminal's default color. |
| `bg_color` | `const char *` | Background, same names. |

**Returns**

The attribute (`COLOR_PAIR(n)`) to give `wattron()` or `wbkgdset()`. `0`, no
color attribute, when the terminal has no colors, ncurses is not open, a color
is beyond what the terminal supports, or the color pairs are exhausted: a `0`
paints with the default colors, and is not an error.

**Example** — the default background of a `ycli` window, then its text color:

```C
int def_attr = get_paint_color(
    gobj_read_str_attr(gobj, "fg_color"),
    gobj_read_str_attr(gobj, "bg_color")
);
if(def_attr) {
    wbkgdset(wn, ' ' | def_attr);
}

int attr = get_paint_color("white", "blue");
if(attr) {
    wattron(wn, attr);
}
```
