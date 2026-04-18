/***********************************************************************
 *          C_EDITLINE.C
 *          Editline GClass.
 *
 *  Part of this code is copied of linenoise.c
 *  You can find the latest source code at:
 *
 * linenoise.c -- guerrilla line editing library against the idea that a
 * line editing lib needs to be 20,000 lines of C code.
 *
 * You can find the latest source code at:
 *
 *   http://github.com/antirez/linenoise
 *
 * Does a number of crazy assumptions that happen to be true in 99.9999% of
 * the 2010 UNIX computers around.
 *
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2010-2023, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2010-2013, Pieter Noordhuis <pcnoordhuis at gmail dot com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  *  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  *  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ------------------------------------------------------------------------
 *
 * References:
 * - http://invisible-island.net/xterm/ctlseqs/ctlseqs.html
 * - http://www.3waylabs.com/nw/WWW/products/wizcon/vt220.html
 *
 * Todo list:
 * - Filter bogus Ctrl+<char> combinations.
 * - Win32 support
 *
 * Bloat:
 * - History search like Ctrl+r in readline?
 *
 * List of escape sequences used by this program, we do everything just
 * with three sequences. In order to be so cheap we may have some
 * flickering effect with some slow terminal, but the lesser sequences
 * the more compatible.
 *
 * EL (Erase Line)
 *    Sequence: ESC [ n K
 *    Effect: if n is 0 or missing, clear from cursor to end of line
 *    Effect: if n is 1, clear from beginning of line to cursor
 *    Effect: if n is 2, clear entire line
 *
 * CUF (CUrsor Forward)
 *    Sequence: ESC [ n C
 *    Effect: moves cursor forward n chars
 *
 * CUB (CUrsor Backward)
 *    Sequence: ESC [ n D
 *    Effect: moves cursor backward n chars
 *
 * The following is used to get the terminal width if getting
 * the width with the TIOCGWINSZ ioctl fails
 *
 * DSR (Device Status Report)
 *    Sequence: ESC [ 6 n
 *    Effect: reports the current cusor position as ESC [ n ; m R
 *            where n is the row and m is the column
 *
 * When multi line mode is enabled, we also use an additional escape
 * sequence. However multi line editing is disabled by default.
 *
 * CUU (Cursor Up)
 *    Sequence: ESC [ n A
 *    Effect: moves cursor up of n chars.
 *
 * CUD (Cursor Down)
 *    Sequence: ESC [ n B
 *    Effect: moves cursor down of n chars.
 *
 * When linenoiseClearScreen() is called, two additional escape sequences
 * are used in order to clear the screen and position the cursor at home
 * position.
 *
 * CUP (Cursor position)
 *    Sequence: ESC [ H
 *    Effect: moves the cursor to upper left corner
 *
 * ED (Erase display)
 *    Sequence: ESC [ 2 J
 *    Effect: clear the whole screen
 *
 *
 *          Copyright (c) 2016 Niyamaka.
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
***********************************************************************/
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pty.h>

#include "help_ncurses.h"
#include "g_ev_console.h"
#include "c_editline.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef editline_completions_t linenoiseCompletions;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void linenoiseAtExit(void);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE struct termios orig_termios;
PRIVATE int atexit_registered = 0; /* Register atexit just 1 time. */
PRIVATE int rawmode = 0; /* For atexit() function to check if restore is needed*/

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
SDATA (DTP_BOOLEAN,     "use_ncurses",          0,  0, "True to use ncurses"),
SDATA (DTP_STRING,      "prompt",               0,  "> ", "Prompt"),
SDATA (DTP_STRING,      "history_file",         0,  0, "History file"),
SDATA (DTP_INTEGER,     "history_max_len",      0,  "200000", "history max len (max lines)"),
SDATA (DTP_INTEGER,     "buffer_size",          0,  "4096", "edition buffer size"),
SDATA (DTP_INTEGER,     "x",                    0,  0, "x window coord"),
SDATA (DTP_INTEGER,     "y",                    0,  0, "y window coord"),
SDATA (DTP_INTEGER,     "cx",                   0,  "80", "physical width window size"),
SDATA (DTP_INTEGER,     "cy",                   0,  "1", "physical height window size"),
SDATA (DTP_STRING,      "bg_color",             0,  "cyan", "Background color, available: black,red,green,yellow,blue,magenta,cyan,white"),
SDATA (DTP_STRING,      "fg_color",             0,  "white", "Foreground color, available: black,red,green,yellow,blue,magenta,cyan,white"),
SDATA (DTP_POINTER,     "user_data",            0,  0, "user data"),
SDATA (DTP_POINTER,     "user_data2",           0,  0, "more user data"),
SDATA (DTP_POINTER,     "subscriber",           0,  0, "subscriber of output-events. If it's null then subscriber is the parent."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_USER = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"trace_user",        "Trace user description"},
{0, 0},
};


/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    int cx;
    int cy;

    BOOL use_ncurses;
    const char *fg_color;
    const char *bg_color;
    WINDOW *wn;     // ncurses window handler
    PANEL *panel;   // panel handler

    const char *history_file;
    int history_max_len;
    int history_len;
    char **history;
    int mlmode;         /* Multi line mode. Default is single line. */
    char *buf;          /* Edited line buffer. */

    int in_completion;  /* 1 while a multi-candidate popup is open (ncurses
                         * only). Further TAB/Enter/Esc/Backspace are rerouted
                         * by the action handlers to drive the popup instead of
                         * editing the line in the usual way. */
    size_t completion_idx;      /* currently highlighted candidate */
    editline_completion_cb_t completion_callback;
    void *completion_user_data;

    /* Ncurses popup state (valid only while in_completion==1). */
    WINDOW *completion_wn;
    PANEL  *completion_panel;
    char  **completion_cvec;    /* gbmem_strdup'd candidate strings */
    char  **completion_descs;   /* parallel; entries may be NULL */
    size_t  completion_len;
    size_t  completion_name_w;  /* widest candidate, capped for layout */
    /* Snapshot of the edit buffer taken when the popup opened, so Esc /
     * Backspace can restore the line the user had typed. */
    char   *completion_orig_buf;
    size_t  completion_orig_len;
    size_t  completion_orig_pos;

    editline_hints_cb_t hints_callback;
    editline_free_hint_cb_t free_hints_callback;
    void *hints_user_data;
    hgobj gobj_self;            /* own hgobj, needed from refreshLine (no gobj ptr there) */

    /*
     *  Reverse-i-search (Ctrl+R) state. While in_search != 0 the rendered
     *  line is "(reverse-i-search)'pat': <match>" instead of the normal
     *  prompt+buf. l->buf is left untouched until the user accepts.
     */
    int in_search;
    char search_pat[128];
    size_t search_pat_len;
    int search_idx;             /* history idx of current match, -1 none */

    size_t buflen;      /* Edited line buffer size. */
    const char *prompt; /* Prompt to display. */
    size_t plen;        /* Prompt length. */
    size_t pos;         /* Current cursor position. */
    size_t oldpos;      /* Previous refresh cursor position. */
    size_t len;         /* Current edited line length. */
    size_t cols;        /* Number of columns in terminal. */
    size_t oldrows;     /* Rows used by last refreshed line (multiline mode) */
    int history_index;  /* The history index we are currently editing. */
} PRIVATE_DATA;

PRIVATE void freeHistory(PRIVATE_DATA *l);
PRIVATE int linenoiseHistorySetMaxLen(PRIVATE_DATA *l, int len);
PRIVATE int linenoiseHistoryLoad(PRIVATE_DATA *l, const char *filename);
PRIVATE int linenoiseHistoryAdd(PRIVATE_DATA *l, const char *line);
PRIVATE void refreshLine(PRIVATE_DATA *l);
PRIVATE void refreshSearchLine(PRIVATE_DATA *l);
PRIVATE void editline_exit_search(PRIVATE_DATA *l, int commit);
PRIVATE void searchUpdate(PRIVATE_DATA *l, int restart_from);
PRIVATE void searchUpdateForward(PRIVATE_DATA *l, int restart_from);
PRIVATE int linenoiseEditInsert(PRIVATE_DATA *l, int c);

/* Ncurses completion popup (only used when l->use_ncurses && in_completion). */
PRIVATE void completion_preview_candidate(PRIVATE_DATA *l, size_t idx);
PRIVATE void completion_render_popup(PRIVATE_DATA *l);
PRIVATE void completion_restore_original(PRIVATE_DATA *l);
PRIVATE void completion_close(PRIVATE_DATA *l);




            /******************************
             *      Framework Methods
             ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    priv->gobj_self = gobj;

    if(!atexit_registered) {
        atexit(linenoiseAtExit);
        atexit_registered = 1;
    }

    /*
     *  CHILD subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(fg_color,                  gobj_read_str_attr)
    SET_PRIV(bg_color,                  gobj_read_str_attr)
    SET_PRIV(prompt,                    gobj_read_str_attr)
        priv->plen = strlen(priv->prompt);
    SET_PRIV(history_file,              gobj_read_str_attr)
    SET_PRIV(cx,                        gobj_read_integer_attr)
        priv->cols = priv->cx;
    SET_PRIV(cy,                        gobj_read_integer_attr)
    SET_PRIV(history_max_len,           gobj_read_integer_attr)
    SET_PRIV(use_ncurses,               gobj_read_bool_attr)

    trace_msg0("Editline use_ncurses: %d", priv->use_ncurses);

    int buffer_size = (int)gobj_read_integer_attr(gobj, "buffer_size");
    priv->buf = gbmem_malloc(buffer_size);
    priv->buflen = buffer_size - 1;

    /*
     *  Load history from file.
     *  The history file is just a plain text file
     *  where entries are separated by newlines.
     */
    if(!empty_string(priv->history_file)) {
        linenoiseHistoryLoad(priv, priv->history_file); /* Load the history at startup */
    }

    /* The latest history entry is always our current buffer, that
     * initially is just an empty string. */
    linenoiseHistoryAdd(priv, "");
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(bg_color,                gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(fg_color,              gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(prompt,                gobj_read_str_attr)
        priv->plen = strlen(priv->prompt);
    ELIF_EQ_SET_PRIV(cx,                    gobj_read_integer_attr)
        priv->cols = priv->cx;
        //TODO igual hay que refrescar
    ELIF_EQ_SET_PRIV(cy,                    gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(history_max_len,       gobj_read_integer_attr)
        linenoiseHistorySetMaxLen(priv, priv->history_max_len);
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Don't log errors, in batch mode there is no curses windows.
     */
    int x = (int)gobj_read_integer_attr(gobj, "x");
    int y = (int)gobj_read_integer_attr(gobj, "y");

    if(priv->use_ncurses) {
        priv->wn = newwin(priv->cy, priv->cx, y, x);
        if(!priv->wn) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "newwin() FAILED",
                NULL
            );
            return -1;
        }

        /* After newwin() in mt_start(), set default bkg once */
        int def_attr = get_paint_color(
            gobj_read_str_attr(gobj, "fg_color"),
            gobj_read_str_attr(gobj, "bg_color")
        );
        if(def_attr) {
            wbkgdset(priv->wn, ' ' | def_attr);
        }
        if(priv->wn) {
            priv->panel = new_panel(priv->wn);
        }
    }

    gobj_send_event(gobj, EV_PAINT, json_object(), gobj);
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->in_completion) {
        completion_close(priv);
    }
    if(priv->panel) {
        del_panel(priv->panel);
        priv->panel = 0;
        update_panels();
        doupdate();
    }
    if(priv->wn) {
        delwin(priv->wn);
        priv->wn = 0;
    }
    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    GBMEM_FREE(priv->buf);
    freeHistory(priv);

    linenoiseAtExit();
}




            /***************************
             *      Local Methods
             ***************************/




/*****************************************************************
 *  Raw mode: 1960 magic shit.
 *****************************************************************/
PRIVATE int enableRawMode(int fd)
{
    struct termios raw;

    if (!isatty(fd)) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "NOT a TTY",
            NULL
        );
        goto fatal;
    }
    if (!atexit_registered) {
        atexit(linenoiseAtExit);
        atexit_registered = 1;
    }
    if (tcgetattr(fd,&orig_termios) == -1) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "tcgetattr() FAILED",
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        goto fatal;
    }

    raw = orig_termios;  /* modify the original mode */
    /* input modes: no break, no CR to NL, no parity check, no strip char,
     * no start/stop output control. */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /* output modes - disable post processing */
    // raw.c_oflag &= ~(OPOST);
    raw.c_oflag |= (ONLCR);
    /* control modes - set 8 bit chars */
    raw.c_cflag |= (CS8);
    /* local modes - choing off, canonical off, no extended functions,
     * no signal chars (^Z,^C) */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    /* control chars - set return condition: min number of bytes and timer.
     * We want read to return every single byte, without timeout. */
    raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = 0; /* 1 byte, no timer */

    /* put terminal in raw mode after flushing */
    if (tcsetattr(fd,TCSAFLUSH,&raw) < 0) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "tcsetattr() FAILED",
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        goto fatal;
    }
    rawmode = 1;
    return 0;

fatal:
    errno = ENOTTY;
    return -1;
}

/*****************************************************************
 *  Raw mode: 1960 magic shit.
 *****************************************************************/
PRIVATE void disableRawMode(int fd)
{
    /* Don't even check the return value as it's too late. */
    if (rawmode && tcsetattr(fd, TCSAFLUSH, &orig_termios) != -1) {
        rawmode = 0;
    }
}

/***************************************************************************
 *  At exit we'll try to fix the terminal to the initial conditions.
 ***************************************************************************/
PRIVATE void linenoiseAtExit(void)
{
    disableRawMode(STDIN_FILENO);
    // freeHistory();
}

/***************************************************************************
 *  Create and return a 'stdin' fd, to read input keyboard, without echo,
 *  then you can feed the editline with EV_KEYCHAR event
 ***************************************************************************/
PUBLIC int tty_keyboard_init(void)
{
    int fd = STDIN_FILENO;

    int newfd = dup(fd);
    set_cloexec(newfd);

    if(enableRawMode(newfd) < 0) {
        close(newfd);
        return -1;
    }
    return newfd;
}

/***************************************************************************
 *  Beep, used for completion when there is nothing to complete or when all
 *  the choices were already shown.
 ***************************************************************************/
PRIVATE void linenoiseBeep(void)
{
    fprintf(stderr, "\x7");
    fflush(stderr);
}

/* ============================== History accessors ========================= */

/*
 *  Public helpers to inspect the in-memory history from outside the gclass.
 *  The last slot may be an "" placeholder for the line being edited — it's
 *  skipped so the 1-based indices match what the user sees on `history`.
 */
PRIVATE int effective_history_len(PRIVATE_DATA *l)
{
    int n = l->history_len;
    if(n > 0 && l->history[n-1] && l->history[n-1][0] == 0) {
        n--;  /* drop empty editing placeholder */
    }
    return n;
}

PUBLIC int editline_history_count(hgobj gobj)
{
    PRIVATE_DATA *l = gobj_priv_data(gobj);
    return effective_history_len(l);
}

PUBLIC const char *editline_history_get(hgobj gobj, int idx)
{
    PRIVATE_DATA *l = gobj_priv_data(gobj);
    int n = effective_history_len(l);
    if(idx < 1 || idx > n) {
        return NULL;
    }
    return l->history[idx - 1];
}

/* ========================== Reverse-i-search (Ctrl+R) ===================== */

/*
 *  Walk l->history[] backward from `from_idx` looking for an entry that
 *  contains `pat` as a substring. Returns the index, or -1 if none.
 */
PRIVATE int historySearchBackward(PRIVATE_DATA *l, int from_idx, const char *pat)
{
    if(!pat || !*pat) return -1;
    if(from_idx < 0) return -1;
    if(from_idx >= l->history_len) from_idx = l->history_len - 1;
    for(int i = from_idx; i >= 0; i--) {
        if(l->history[i] && strstr(l->history[i], pat)) {
            return i;
        }
    }
    return -1;
}

/*
 *  Walk l->history[] forward from `from_idx` looking for an entry that
 *  contains `pat` as a substring. Returns the index, or -1 if none.
 */
PRIVATE int historySearchForward(PRIVATE_DATA *l, int from_idx, const char *pat)
{
    if(!pat || !*pat) return -1;
    if(from_idx < 0) from_idx = 0;
    for(int i = from_idx; i < l->history_len; i++) {
        if(l->history[i] && strstr(l->history[i], pat)) {
            return i;
        }
    }
    return -1;
}

/*
 *  Render "(reverse-i-search)'pat': <match>" instead of the normal line.
 *  Called from refreshLine() whenever l->in_search is set. Draws on the
 *  editline's ncurses window when available; falls back to stdout for
 *  batch/stdout clients (ycommand).
 */
PRIVATE void refreshSearchLine(PRIVATE_DATA *l)
{
    const char *match = (l->search_idx >= 0 && l->search_idx < l->history_len
                         && l->history[l->search_idx])
                        ? l->history[l->search_idx] : "";
    char prefix[256];
    int prefix_len = snprintf(prefix, sizeof(prefix),
        "(reverse-i-search)'%s': ", l->search_pat);
    if(prefix_len < 0) prefix_len = 0;

    size_t match_len = strlen(match);
    if(l->cols > 0 && (size_t)prefix_len + match_len > l->cols) {
        match_len = ((size_t)prefix_len >= l->cols)
            ? 0 : (l->cols - (size_t)prefix_len);
    }

    /* Position the cursor on the first occurrence of the search pattern
     * within the match (bash readline behaviour). Falls back to the start
     * of the match when the pattern isn't found or got truncated away. */
    size_t cursor_offset = 0;
    if(l->search_pat_len > 0 && match_len > 0) {
        const char *hit = strstr(match, l->search_pat);
        if(hit) {
            size_t off = (size_t)(hit - match);
            if(off < match_len) {
                cursor_offset = off;
            }
        }
    }

    if(l->use_ncurses) {
        if(!l->wn) {
            return;
        }
        int attr = get_paint_color(l->fg_color, l->bg_color);
        if(attr) {
            wattron(l->wn, attr);
        }
        wmove(l->wn, 0, 0);
        wclrtoeol(l->wn);
        waddnstr(l->wn, prefix, prefix_len);
        waddnstr(l->wn, match, (int)match_len);
        if(attr) {
            wattroff(l->wn, attr);
        }
        wmove(l->wn, 0, prefix_len + (int)cursor_offset);
        wrefresh(l->wn);
    } else {
        printf(Erase_Whole_Line);
        printf(Move_Horizontal, 1);
        printf("%s%.*s", prefix, (int)match_len, match);
        printf(Move_Horizontal, prefix_len + (int)cursor_offset + 1);
        fflush(stdout);
    }
}

/*
 *  Leave search mode. If `commit` is true and we had a match, copy it
 *  into the buffer so the line can be edited or submitted.
 */
PRIVATE void editline_exit_search(PRIVATE_DATA *l, int commit)
{
    if(!l->in_search) return;
    if(commit && l->search_idx >= 0 && l->search_idx < l->history_len
       && l->history[l->search_idx]) {
        const char *match = l->history[l->search_idx];
        size_t mlen = strlen(match);
        if(mlen >= l->buflen) mlen = l->buflen - 1;
        memcpy(l->buf, match, mlen);
        l->buf[mlen] = 0;
        l->len = mlen;
        l->pos = mlen;
    }
    l->in_search = 0;
    l->search_pat[0] = 0;
    l->search_pat_len = 0;
    l->search_idx = -1;
}

/*
 *  Re-run the backward search from the current position after the pattern
 *  or the anchor changed. `restart_from` is the idx to start from (so
 *  pressing Ctrl+R again moves past the current match).
 */
PRIVATE void searchUpdate(PRIVATE_DATA *l, int restart_from)
{
    int idx = historySearchBackward(l, restart_from, l->search_pat);
    if(idx >= 0) {
        l->search_idx = idx;
    } else {
        /* No older match: keep current search_idx so the previous match
         * stays visible. Still beep to signal "nothing further". */
        if(l->search_pat_len > 0) {
            linenoiseBeep();
        }
    }
    refreshSearchLine(l);
}

/*
 *  Forward counterpart of searchUpdate(). Used by Ctrl+S to step towards
 *  newer matches while staying in search mode.
 */
PRIVATE void searchUpdateForward(PRIVATE_DATA *l, int restart_from)
{
    int idx = historySearchForward(l, restart_from, l->search_pat);
    if(idx >= 0) {
        l->search_idx = idx;
    } else {
        if(l->search_pat_len > 0) {
            linenoiseBeep();
        }
    }
    refreshSearchLine(l);
}

/* ============================== Completion ================================ */

/* Free a list of completion option populated by editline_add_completion(). */
static void freeCompletions(linenoiseCompletions *lc) {
    size_t i;
    if(lc->cvec) {
        for (i = 0; i < lc->len; i++) {
            if(lc->cvec[i]) {
                gbmem_free(lc->cvec[i]);
            }
        }
        gbmem_free(lc->cvec);
    }
    if(lc->descs) {
        for (i = 0; i < lc->len; i++) {
            if(lc->descs[i]) {
                gbmem_free(lc->descs[i]);
            }
        }
        gbmem_free(lc->descs);
    }
}

/***************************************************************************
 *  Popup candidate preview: copy cvec[idx] into the edit buffer so the
 *  selected candidate is visible on the editline while the user decides.
 ***************************************************************************/
PRIVATE void completion_preview_candidate(PRIVATE_DATA *l, size_t idx)
{
    if(idx >= l->completion_len || !l->completion_cvec) {
        return;
    }
    const char *cand = l->completion_cvec[idx];
    size_t nwritten = (size_t)snprintf(l->buf, l->buflen, "%s", cand);
    l->len = l->pos = nwritten;
    refreshLine(l);
}

/***************************************************************************
 *  Draw the completion popup. Current candidate is rendered in A_REVERSE.
 *  If the list is taller than the popup, scroll the viewport to keep the
 *  selection in view. Cursor is parked back on the editline after drawing.
 ***************************************************************************/
PRIVATE void completion_render_popup(PRIVATE_DATA *l)
{
    if(!l->completion_wn) {
        return;
    }
    int h, w;
    getmaxyx(l->completion_wn, h, w);
    if(h <= 0 || w <= 0) {
        return;
    }

    /* When the list doesn't fit, reserve the top row for a non-inverse
     * status line ("3/42  ↑ N above  ↓ M below"). That keeps the scroll
     * indicator from colliding with the A_REVERSE of the selected row. */
    BOOL has_status = (l->completion_len > (size_t)h) && (h >= 2);
    int content_rows = has_status ? (h - 1) : h;
    int content_y0   = has_status ? 1 : 0;

    size_t start = 0;
    if(l->completion_len > (size_t)content_rows) {
        if(l->completion_idx >= (size_t)content_rows) {
            start = l->completion_idx - (size_t)content_rows + 1;
        }
        if(start + (size_t)content_rows > l->completion_len) {
            start = l->completion_len - (size_t)content_rows;
        }
    }

    /* Explicit per-cell write with a chtype that already contains the
     * desired attrs (color pair + optional A_REVERSE). This bypasses any
     * attr-only diff optimisation in ncurses / terminfo that was leaving
     * A_REVERSE on a previously-selected cell when the new cell happened
     * to hold the same character (a leading space). wbkgd retro-applies
     * the background to every existing cell; wclear forces the next
     * refresh to repaint the popup area from scratch. */
    int def_attr = get_paint_color(l->fg_color, l->bg_color);
    wattrset(l->completion_wn, A_NORMAL);
    wbkgd(l->completion_wn, (chtype)' ' | (chtype)def_attr);
    wclear(l->completion_wn);

    int name_w = (int)l->completion_name_w;
    char row_buf[512];

    for(int row = 0; row < content_rows; row++) {
        size_t k = start + (size_t)row;
        chtype row_attr = (chtype)def_attr;
        if(k < l->completion_len && k == l->completion_idx) {
            row_attr |= A_REVERSE;
        }
        size_t rlen = 0;
        if(k < l->completion_len) {
            const char *cand = l->completion_cvec[k];
            const char *desc = l->completion_descs ? l->completion_descs[k] : NULL;
            snprintf(row_buf, sizeof(row_buf), " %-*.*s  %s",
                name_w, name_w, cand ? cand : "",
                (desc && *desc) ? desc : "");
            rlen = strlen(row_buf);
            if((int)rlen > w) {
                rlen = (size_t)w;
            }
        } else {
            /* Past the end of the candidate list: fill with blanks using
             * the same bg so the row doesn't look different. */
            rlen = 0;
        }
        for(int col = 0; col < w; col++) {
            unsigned char ch = (col < (int)rlen)
                ? (unsigned char)row_buf[col] : (unsigned char)' ';
            mvwaddch(l->completion_wn, content_y0 + row, col,
                (chtype)ch | row_attr);
        }
    }

    if(has_status) {
        /* Status strip: "3/42  <up> N above  <down> M below"
         * Drawn with A_DIM on the popup background (no A_REVERSE) so it
         * never competes with the highlighted row. ACS_UARROW/ACS_DARROW
         * fall back to '^'/'v' in line-drawing-less terminals. */
        char lead[64];
        snprintf(lead, sizeof(lead), " %zu/%zu  ",
            l->completion_idx + 1, l->completion_len);
        int col = 0;

        wattron(l->completion_wn, A_DIM);
        mvwaddnstr(l->completion_wn, 0, col, lead, w);
        col += (int)strlen(lead);
        if(col >= w) {
            wattroff(l->completion_wn, A_DIM);
            goto status_done;
        }

        if(start > 0) {
            mvwaddch(l->completion_wn, 0, col++, ACS_UARROW);
            if(col < w) {
                char buf[32];
                int n = snprintf(buf, sizeof(buf), " %zu above  ", start);
                if(col + n > w) n = w - col;
                mvwaddnstr(l->completion_wn, 0, col, buf, n);
                col += n;
            }
        }
        size_t below = (start + (size_t)content_rows < l->completion_len)
            ? (l->completion_len - start - (size_t)content_rows) : 0;
        if(below > 0 && col < w) {
            mvwaddch(l->completion_wn, 0, col++, ACS_DARROW);
            if(col < w) {
                char buf[32];
                int n = snprintf(buf, sizeof(buf), " %zu below  ", below);
                if(col + n > w) n = w - col;
                mvwaddnstr(l->completion_wn, 0, col, buf, n);
                col += n;
            }
        }
        /* Pad the rest of the status row so A_DIM attribute fills it
         * uniformly (otherwise the trailing cells keep a different
         * background from werase'd ones). */
        while(col < w) {
            mvwaddch(l->completion_wn, 0, col++, ' ');
        }
        wattroff(l->completion_wn, A_DIM);
    }
status_done:

    /* touchwin forces ncurses to mark every line as changed so no cell
     * escapes the next physical refresh (belt-and-suspenders on top of
     * wclear + wbkgd + explicit chtype writes). */
    touchwin(l->completion_wn);
    wnoutrefresh(l->completion_wn);
    if(l->wn) {
        wmove(l->wn, 0, (int)(l->pos + l->plen));
        wnoutrefresh(l->wn);
    }
    update_panels();
    doupdate();
}

/***************************************************************************
 *  Put back the text the user had typed before TAB opened the popup.
 ***************************************************************************/
PRIVATE void completion_restore_original(PRIVATE_DATA *l)
{
    if(!l->completion_orig_buf) {
        return;
    }
    size_t n = strlen(l->completion_orig_buf);
    if(n >= l->buflen) {
        n = l->buflen - 1;
    }
    memcpy(l->buf, l->completion_orig_buf, n);
    l->buf[n] = 0;
    l->len = l->completion_orig_len;
    l->pos = l->completion_orig_pos;
    if(l->len > l->buflen - 1) {
        l->len = l->buflen - 1;
    }
    if(l->pos > l->len) {
        l->pos = l->len;
    }
    refreshLine(l);
}

/***************************************************************************
 *  Tear down the popup and release the snapshot. Safe to call repeatedly.
 ***************************************************************************/
PRIVATE void completion_close(PRIVATE_DATA *l)
{
    if(l->completion_panel) {
        del_panel(l->completion_panel);
        l->completion_panel = 0;
    }
    if(l->completion_wn) {
        delwin(l->completion_wn);
        l->completion_wn = 0;
    }
    if(l->completion_cvec) {
        for(size_t i = 0; i < l->completion_len; i++) {
            if(l->completion_cvec[i]) {
                gbmem_free(l->completion_cvec[i]);
            }
        }
        gbmem_free(l->completion_cvec);
        l->completion_cvec = 0;
    }
    if(l->completion_descs) {
        for(size_t i = 0; i < l->completion_len; i++) {
            if(l->completion_descs[i]) {
                gbmem_free(l->completion_descs[i]);
            }
        }
        gbmem_free(l->completion_descs);
        l->completion_descs = 0;
    }
    if(l->completion_orig_buf) {
        gbmem_free(l->completion_orig_buf);
        l->completion_orig_buf = 0;
    }
    l->completion_len = 0;
    l->completion_idx = 0;
    l->completion_name_w = 0;
    l->completion_orig_len = 0;
    l->completion_orig_pos = 0;
    l->in_completion = 0;
    update_panels();
    doupdate();
    refreshLine(l);
}

/***************************************************************************
 *  Open the popup from a non-empty linenoiseCompletions. Ownership of the
 *  candidate strings is moved into the PRIVATE_DATA (duplicated here so the
 *  caller can freeCompletions its own lc right after).
 *  Returns 0 on success, -1 if the window could not be allocated.
 ***************************************************************************/
PRIVATE int completion_open_popup(PRIVATE_DATA *l, linenoiseCompletions *lc)
{
    if(!l->wn || lc->len < 2) {
        return -1;
    }

    size_t name_w = 0;
    for(size_t k = 0; k < lc->len; k++) {
        size_t n = lc->cvec[k] ? strlen(lc->cvec[k]) : 0;
        if(n > name_w) {
            name_w = n;
        }
    }
    if(name_w > 48) {
        name_w = 48;
    }
    size_t desc_w = 0;
    if(lc->descs) {
        for(size_t k = 0; k < lc->len; k++) {
            size_t n = lc->descs[k] ? strlen(lc->descs[k]) : 0;
            if(n > desc_w) {
                desc_w = n;
            }
        }
    }
    if(desc_w > 60) {
        desc_w = 60;
    }

    int ey, ex;
    getbegyx(l->wn, ey, ex);
    int term_h = 0, term_w = 0;
    getmaxyx(stdscr, term_h, term_w);
    (void)term_h;

    int max_rows_above = ey;            /* space between top and editline */
    int content_cap = 10;               /* up to 10 candidates visible */
    if(max_rows_above < content_cap) {
        content_cap = max_rows_above;
    }
    int desired_h;
    if((int)lc->len <= content_cap) {
        desired_h = (int)lc->len;       /* no scroll row needed */
    } else {
        desired_h = content_cap + 1;    /* +1 for the status/scroll row */
        if(desired_h > max_rows_above) {
            desired_h = max_rows_above;
        }
    }
    if(desired_h < 1) {
        desired_h = 1;
    }

    int desired_w = (int)(name_w + 2 + desc_w + 4);  /* " name  desc  " */
    if(desired_w < 16) {
        desired_w = 16;
    }
    if(desired_w > term_w) {
        desired_w = term_w;
    }

    int py = ey - desired_h;
    int px = ex;
    if(py < 0) {
        py = 0;
    }
    if(px + desired_w > term_w) {
        px = term_w - desired_w;
        if(px < 0) {
            px = 0;
        }
    }

    WINDOW *wn = newwin(desired_h, desired_w, py, px);
    if(!wn) {
        gobj_log_error(l->gobj_self, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "newwin() FAILED",
            NULL
        );
        return -1;
    }
    int def_attr = get_paint_color(l->fg_color, l->bg_color);
    if(def_attr) {
        wbkgdset(wn, ' ' | def_attr);
    }
    PANEL *panel = new_panel(wn);
    if(!panel) {
        delwin(wn);
        gobj_log_error(l->gobj_self, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "new_panel() FAILED",
            NULL
        );
        return -1;
    }
    top_panel(panel);

    /* Snapshot the edit buffer so Backspace can restore it verbatim. */
    char *orig = gbmem_strndup(l->buf, l->len);
    if(!orig) {
        del_panel(panel);
        delwin(wn);
        return -1;
    }

    char **cvec = gbmem_malloc(sizeof(char *) * lc->len);
    char **descs = gbmem_malloc(sizeof(char *) * lc->len);
    if(!cvec || !descs) {
        if(cvec) {
            gbmem_free(cvec);
        }
        if(descs) {
            gbmem_free(descs);
        }
        gbmem_free(orig);
        del_panel(panel);
        delwin(wn);
        return -1;
    }
    for(size_t k = 0; k < lc->len; k++) {
        cvec[k] = lc->cvec[k] ? gbmem_strdup(lc->cvec[k]) : NULL;
        descs[k] = (lc->descs && lc->descs[k]) ? gbmem_strdup(lc->descs[k]) : NULL;
    }

    l->completion_wn = wn;
    l->completion_panel = panel;
    l->completion_cvec = cvec;
    l->completion_descs = descs;
    l->completion_len = lc->len;
    l->completion_name_w = name_w;
    l->completion_orig_buf = orig;
    l->completion_orig_len = l->len;
    l->completion_orig_pos = l->pos;
    l->completion_idx = 0;
    l->in_completion = 1;

    completion_preview_candidate(l, 0);
    completion_render_popup(l);
    return 0;
}

/***************************************************************************
 *  TAB handler. Build candidate list via the user callback, then:
 *   - 0 candidates  → beep
 *   - 1 candidate   → apply directly
 *   - ≥2 candidates, ncurses → open the popup and return (further TAB /
 *                              Enter / printable / Backspace are handled by
 *                              the FSM intercepts)
 *   - ≥2 candidates, stdout → print the list and cycle via read(stdin),
 *                              preserving the previous ycommand behaviour.
 ***************************************************************************/
static int completeLine(hgobj gobj)
{
    PRIVATE_DATA *ls = gobj_priv_data(gobj);
    linenoiseCompletions lc = { 0 };
    int nread, nwritten;
    char c = 0;

    if(ls->completion_callback) {
        ls->completion_callback(gobj, ls->buf, &lc, ls->completion_user_data);
    }
    if (lc.len == 0) {
        linenoiseBeep();
    } else if (lc.len == 1) {
        /* Exactly one candidate: apply directly, no cycling. */
        nwritten = snprintf(ls->buf, ls->buflen, "%s", lc.cvec[0]);
        ls->len = ls->pos = (size_t)nwritten;
        refreshLine(ls);
        freeCompletions(&lc);
        return c;
    } else if(ls->use_ncurses) {
        /* Popup-driven: no blocking read here, the FSM resumes on next key. */
        if(completion_open_popup(ls, &lc) < 0) {
            linenoiseBeep();
        }
        freeCompletions(&lc);
        return c;
    } else {
        /* Stdout path (ycommand / batch): print the list with descriptions,
         * then cycle synchronously via read(stdin). */
        size_t name_width = 0;
        for (size_t k = 0; k < lc.len; k++) {
            size_t l = strlen(lc.cvec[k]);
            if (l > name_width) name_width = l;
        }
        if (name_width > 48) name_width = 48;

        printf("\n");
        for (size_t k = 0; k < lc.len; k++) {
            const char *cand = lc.cvec[k];
            const char *desc = lc.descs ? lc.descs[k] : NULL;
            printf("  %-*.*s", (int)name_width, (int)name_width, cand);
            if (desc && *desc) {
                printf("  %s", desc);
            }
            printf("\n");
        }

        size_t stop = 0, i = 0;

        while(!stop) {
            /* Show completion or original buffer */
            if (i < lc.len) {
                PRIVATE_DATA saved = *ls;

                ls->len = ls->pos = strlen(lc.cvec[i]);
                ls->buf = lc.cvec[i];
                refreshLine(ls);
                ls->len = saved.len;
                ls->pos = saved.pos;
                ls->buf = saved.buf;
            } else {
                refreshLine(ls);
            }

            nread = read(STDIN_FILENO, &c, 1);
            if (nread <= 0) {
                freeCompletions(&lc);
                return -1;
            }

            switch(c) {
                case 9: /* tab */
                    i = (i+1) % (lc.len+1);
                    if (i == lc.len) linenoiseBeep();
                    break;
                case 27: /* escape */
                    if (i < lc.len) refreshLine(ls);
                    stop = 1;
                    break;
                default:
                    if (i < lc.len) {
                        nwritten = snprintf(ls->buf,ls->buflen,"%s",lc.cvec[i]);
                        ls->len = ls->pos = nwritten;
                        refreshLine(ls);
                    }
                    stop = 1;
                    if (c == 13 || c == 10) {           /* Enter */
                        gobj_send_event(gobj, EV_EDITLINE_ENTER, 0, gobj);
                    } else if (c == 0x7F || c == 0x08) { /* Backspace / Ctrl-H */
                        gobj_send_event(gobj, EV_EDITLINE_BACKSPACE, 0, gobj);
                    } else if ((unsigned char)c >= 0x20 && (unsigned char)c < 0x7F) {
                        linenoiseEditInsert(ls, c);
                        refreshLine(ls);
                    }
                    break;
            }
        }
    }

    freeCompletions(&lc);
    return c; /* Return last read character */
}

/* Register a callback function to be called for tab-completion. */
PUBLIC void editline_set_completion_callback(
    hgobj gobj,
    editline_completion_cb_t cb,
    void *user_data
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    priv->completion_callback = cb;
    priv->completion_user_data = user_data;
}

/* Register a hints callback. Pass NULL to disable. */
PUBLIC void editline_set_hints_callback(
    hgobj gobj,
    editline_hints_cb_t cb,
    editline_free_hint_cb_t free_cb,
    void *user_data
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    priv->hints_callback = cb;
    priv->free_hints_callback = free_cb;
    priv->hints_user_data = user_data;
}

/* This function is used by the callback function registered by the user
 * in order to add completion options given the input string when the
 * user typed <tab>. */
PUBLIC void editline_add_completion(
    editline_completions_t *lc,
    const char *str,
    const char *desc
) {
    size_t len = strlen(str);
    char *copy, **cvec, **dvec;

    copy = gbmem_malloc(len+1);
    if (copy == NULL) return;
    memcpy(copy,str,len+1);
    cvec = gbmem_realloc(lc->cvec,sizeof(char*)*(lc->len+1));
    if (cvec == NULL) {
        gbmem_free(copy);
        return;
    }
    dvec = gbmem_realloc(lc->descs,sizeof(char*)*(lc->len+1));
    if (dvec == NULL) {
        /* cvec survived; put it back so freeCompletions can clean up. */
        lc->cvec = cvec;
        gbmem_free(copy);
        return;
    }
    lc->cvec = cvec;
    lc->descs = dvec;
    lc->cvec[lc->len] = copy;
    lc->descs[lc->len] = (desc && *desc) ? gbmem_strdup(desc) : NULL;
    lc->len++;
}

/* =========================== Line editing ================================= */

/***************************************************************************
 * Single line low level line refresh.
 *
 * Rewrite the currently edited line accordingly to the buffer content,
 * cursor position, and number of columns of the terminal.
 ***************************************************************************/
PRIVATE void refreshLine(PRIVATE_DATA *l)
{
    if(l->in_search) {
        refreshSearchLine(l);
        return;
    }
    size_t plen = strlen(l->prompt);
    char *buf = l->buf;
    size_t len = l->len;
    size_t pos = l->pos;

    /*
     *  Aquí debe estar ajustando la ventana visible de la línea,
     *  moviendo el cursor si se ha quedado fuera de la zona visible.
     */
    if(l->cols > 0) {
        while((plen+pos) >= l->cols) {
            buf++;
            len--;
            pos--;
        }
        while (plen+len > l->cols) {
            len--;
        }
    }

    if(l->use_ncurses) {
        // Set color on
        int attr = get_paint_color(l->fg_color, l->bg_color);
        if(attr) {
            wattron(l->wn, attr);
        }

        /* Cursor to left edge */
        wmove(l->wn, 0, 0); // move to beginning of line
        wclrtoeol(l->wn);   // erase to end of line

        /* Write the prompt and the current buffer content */
        waddnstr(l->wn, l->prompt, (int)plen);
        waddnstr(l->wn, buf, (int)len);

        // Set color off
        if(attr) {
            wattroff(l->wn, attr);
        }

        /* Inline hint in a softer colour. A_DIM alone doesn't help when
         * fg is already black — can't dim further. A_BOLD on COLOR_BLACK
         * is rendered as "bright black" / grey by most terminals (same
         * idea as SGR 90 used on the stdout path). */
        if(l->hints_callback) {
            int color = -1, bold = 0;
            char *hint = l->hints_callback(
                l->gobj_self, l->buf, &color, &bold, l->hints_user_data
            );
            if(hint) {
                size_t hintlen = strlen(hint);
                if(l->cols > 0) {
                    size_t used = plen + len;
                    if(used < l->cols) {
                        size_t room = l->cols - used;
                        if(hintlen > room) hintlen = room;
                    } else {
                        hintlen = 0;
                    }
                }
                if(hintlen > 0) {
                    int hint_attr = A_BOLD | A_DIM;
                    int pair = get_paint_color(l->fg_color, l->bg_color);
                    if(pair) {
                        hint_attr |= pair;
                    }
                    wattron(l->wn, hint_attr);
                    waddnstr(l->wn, hint, (int)hintlen);
                    wattroff(l->wn, hint_attr);
                }
                if(l->free_hints_callback) {
                    l->free_hints_callback(hint, l->hints_user_data);
                }
            }
        }

        /* Move cursor to original position (after the buf, before any hint). */
        wmove(l->wn, 0, (int)(pos+plen));

        wrefresh(l->wn);

    } else {
        printf(Erase_Whole_Line);           // Erase line
        printf(Move_Horizontal, 1);         // Move to beginning of line
        /* Write the prompt and the current buffer content */
        printf("%s%*.*s", l->prompt, (int)len, (int)len, buf);

        /* Inline hint (in-place, shown to the right of the buffer). */
        if(l->hints_callback) {
            int color = -1, bold = 0;
            char *hint = l->hints_callback(
                l->gobj_self, l->buf, &color, &bold, l->hints_user_data
            );
            if(hint) {
                size_t hintlen = strlen(hint);
                /* Truncate so prompt+buf+hint fits in cols. */
                if(l->cols > 0) {
                    size_t used = plen + len;
                    if(used < l->cols) {
                        size_t room = l->cols - used;
                        if(hintlen > room) hintlen = room;
                    } else {
                        hintlen = 0;
                    }
                }
                if(hintlen > 0) {
                    if(color < 0) color = 90;   /* bright black (gray) */
                    printf("\033[%d;%dm%.*s\033[0m",
                        bold, color, (int)hintlen, hint);
                }
                if(l->free_hints_callback) {
                    l->free_hints_callback(hint, l->hints_user_data);
                }
            }
        }

        printf(Move_Horizontal, (int)(pos+plen+1));   // Move cursor to original position
        fflush(stdout);
    }
}

/***************************************************************************
 *  Insert the character 'c' at cursor current position.
 ***************************************************************************/
PRIVATE int linenoiseEditInsert(PRIVATE_DATA *l, int c)
{
    /*
     *  WARNING By the moment ONLY ascii
     */
    if(c<0x20 || c>=0x7F) {
        return -1;
    }

    if (l->len < l->buflen) {
        if (l->len == l->pos) {
            l->buf[l->pos] = c;
            l->pos++;
            l->len++;
            l->buf[l->len] = '\0';
        } else {
            memmove(l->buf+l->pos+1,l->buf+l->pos,l->len-l->pos);
            l->buf[l->pos] = c;
            l->len++;
            l->pos++;
            l->buf[l->len] = '\0';
        }
        refreshLine(l);
    }
    return 0;
}

/***************************************************************************
 *  Move cursor on the left.
 ***************************************************************************/
PRIVATE void linenoiseEditMoveLeft(PRIVATE_DATA *l)
{
    if (l->pos > 0) {
        l->pos--;
        refreshLine(l);
    }
}

/***************************************************************************
 *  Move cursor on the right.
 ***************************************************************************/
PRIVATE void linenoiseEditMoveRight(PRIVATE_DATA *l)
{
    if (l->pos != l->len) {
        l->pos++;
        refreshLine(l);
    }
}

/***************************************************************************
 *  Move cursor to the start of the line.
 ***************************************************************************/
PRIVATE void linenoiseEditMoveHome(PRIVATE_DATA *l)
{
    if (l->pos != 0) {
        l->pos = 0;
        refreshLine(l);
    }
}

/***************************************************************************
 *  Move cursor to the end of the line.
 ***************************************************************************/
PRIVATE void linenoiseEditMoveEnd(PRIVATE_DATA *l)
{
    if (l->pos != l->len) {
        l->pos = l->len;
        refreshLine(l);
    }
}

/***************************************************************************
 *  Substitute the currently edited line with the next or previous history
 *  entry as specified by 'dir'.
 ***************************************************************************/
#define LINENOISE_HISTORY_NEXT 0
#define LINENOISE_HISTORY_PREV 1
PRIVATE void linenoiseEditHistoryNext(PRIVATE_DATA *l, int dir)
{
    if (l->history_len > 1) {
        /* Update the current history entry before to
         * overwrite it with the next one. */
        gbmem_free(l->history[l->history_len - 1 - l->history_index]);
        l->history[l->history_len - 1 - l->history_index] = gbmem_strdup(l->buf);
        /* Show the new entry */
        l->history_index += (dir == LINENOISE_HISTORY_PREV) ? 1 : -1;
        if (l->history_index < 0) {
            l->history_index = 0;
            return;
        } else if (l->history_index >= l->history_len) {
            l->history_index = l->history_len-1;
            return;
        }
        strncpy(l->buf, l->history[l->history_len - 1 - l->history_index],l->buflen);
        l->buf[l->buflen-1] = '\0';
        l->len = l->pos = strlen(l->buf);
    }
    refreshLine(l);
}

/***************************************************************************
 *  Delete the character at the right of the cursor without altering the cursor
 *  position. Basically this is what happens with the "Delete" keyboard key.
 ***************************************************************************/
PRIVATE void linenoiseEditDelete(PRIVATE_DATA *l)
{
    if (l->len > 0 && l->pos < l->len) {
        memmove(l->buf+l->pos,l->buf+l->pos+1,l->len-l->pos-1);
        l->len--;
        l->buf[l->len] = '\0';
        refreshLine(l);
    }
}

/***************************************************************************
 *  Backspace implementation.
 ***************************************************************************/
PRIVATE void linenoiseEditBackspace(PRIVATE_DATA *l)
{
    if (l->pos > 0 && l->len > 0) {
        memmove(l->buf+l->pos-1,l->buf+l->pos,l->len-l->pos);
        l->pos--;
        l->len--;
        l->buf[l->len] = '\0';
        refreshLine(l);
    }
}

/***************************************************************************
 *  Delete the previous word, maintaining the cursor at the start of the
 *  current word.
 ***************************************************************************/
PRIVATE void linenoiseEditDeletePrevWord(PRIVATE_DATA *l)
{
    size_t old_pos = l->pos;
    size_t diff;

    while (l->pos > 0 && l->buf[l->pos-1] == ' ')
        l->pos--;
    while (l->pos > 0 && l->buf[l->pos-1] != ' ')
        l->pos--;
    diff = old_pos - l->pos;
    memmove(l->buf+l->pos,l->buf+old_pos,l->len-old_pos+1);
    l->len -= diff;
    refreshLine(l);
}




/* ================================ History ================================= */





/***************************************************************************
 *  Free the history, but does not reset it. Only used when we have to
 *  exit() to avoid memory leaks are reported by valgrind & co.
 ***************************************************************************/
PRIVATE void freeHistory(PRIVATE_DATA *l)
{
    if(l->history) {
        int j;
        for (j = 0; j < l->history_len; j++)
            gbmem_free(l->history[j]);
        gbmem_free(l->history);
        l->history = NULL;
        l->history_index = 0;
        l->history_len = 0;
    }
}

/***************************************************************************
 *  This is the API call to add a newo entry in the linenoise history.
 *  It uses a fixed array of char pointers that are shifted (memmoved)
 *  when the history max length is reached in order to remove the older
 *  entry and make room for the newo one, so it is not exactly suitable for huge
 *  histories, but will work well for a few hundred of entries.
 *
 *  Using a circular buffer is smarter, but a bit more complex to handle.
 ***************************************************************************/
PRIVATE int linenoiseHistoryAdd(PRIVATE_DATA *l, const char *line)
{
    char *linecopy;

    if (l->history_max_len == 0)
        return 0;

    /* Initialization on first call. */
    if (l->history == NULL) {
        l->history = gbmem_malloc(sizeof(char*) * l->history_max_len);
        if(l->history == NULL)
            return 0;
    }

    /*
     *  Dedup: if `line` already appears anywhere in history, remove the
     *  previous occurrences before appending (bash HISTCONTROL=erasedups
     *  style). Keeps history compact and puts the freshest use at the end.
     */
    int write = 0;
    for(int i = 0; i < l->history_len; i++) {
        if(l->history[i] && strcmp(l->history[i], line) == 0) {
            gbmem_free(l->history[i]);
            continue;
        }
        l->history[write++] = l->history[i];
    }
    l->history_len = write;

    /* Add an heap allocated copy of the line in the history.
     * If we reached the max length, remove the older line. */
    linecopy = gbmem_strdup(line);
    if(!linecopy)
        return 0;
    if(l->history_len == l->history_max_len) {
        gbmem_free(l->history[0]);
        memmove(l->history, l->history+1, sizeof(char*) * (l->history_max_len-1));
        l->history_len--;
    }
    l->history[l->history_len] = linecopy;
    //log_debug_printf(0, "history %d %s", l->history_len, linecopy);
    l->history_len++;
    return 1;
}

/***************************************************************************
 *  Set the maximum length for the history. This function can be called even
 *  if there is already some history, the function will make sure to retain
 *  just the latest 'len' elements if the newo history length value is smaller
 *  than the amount of items already inside the history.
 ***************************************************************************/
PRIVATE int linenoiseHistorySetMaxLen(PRIVATE_DATA *l, int len)
{
    char **newo;

    if(len < 1)
        return 0;
    if(l->history) {
        int tocopy = l->history_len;

        newo = gbmem_malloc(sizeof(char*) * len);
        if (newo == NULL)
            return 0;

        /* If we can't copy everything, free the elements we'll not use. */
        if (len < tocopy) {
            int j;

            for (j = 0; j < tocopy-len; j++)
                gbmem_free(l->history[j]);
            tocopy = len;
        }
        memset(newo, 0, sizeof(char*) * len);
        memcpy(newo, l->history+(l->history_len-tocopy), sizeof(char*) * tocopy);
        gbmem_free(l->history);
        l->history = newo;
    }
    l->history_max_len = len;
    if (l->history_len > l->history_max_len)
        l->history_len = l->history_max_len;
    return 1;
}

/***************************************************************************
 *  Save the history in the specified file. On success 0 is returned
 *  otherwise -1 is returned.
 ***************************************************************************/
PRIVATE int linenoiseHistorySave(PRIVATE_DATA *l, const char *filename)
{
    FILE *fp = fopen(filename, "w");

    if (fp == NULL)
        return -1;
    for (int i = 0; i < l->history_len; i++) {
        if(!empty_string(l->history[i])) {
            #define PASSW "password"
            char *p = strstr(l->history[i], PASSW);
            if(p) {
                p += strlen(PASSW);
                *p = 0;
            } else {
                #define PASSW2 "passw"
                p = strstr(l->history[i], PASSW2);
                if(p) {
                    p += strlen(PASSW2);
                    *p = 0;
                }
            }

            fprintf(fp, "%s\n", l->history[i]);
        }
    }
    fclose(fp);
    return 0;
}

/***************************************************************************
 *  Load the history from the specified file. If the file does not exist
 *  zero is returned and no operation is performed.
 *
 *  If the file exists and the operation succeeded 0 is returned, otherwise
 *  on error -1 is returned.
 ***************************************************************************/
PRIVATE int linenoiseHistoryLoad(PRIVATE_DATA *l, const char *filename)
{
#define LINENOISE_MAX_LINE 4096
    FILE *fp = fopen(filename,"r");
    char buf[LINENOISE_MAX_LINE];

    if (fp == NULL)
        return -1;

    while (fgets(buf, LINENOISE_MAX_LINE, fp) != NULL) {
        char *p;

        p = strchr(buf,'\r');
        if (!p) p = strchr(buf,'\n');
        if (p) *p = '\0';
        linenoiseHistoryAdd(l, buf);
    }
    fclose(fp);
    return 0;
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *  char
 ***************************************************************************/
/* One byte of input → either grow the search pattern (while in_search) or
 * insert into the edit buffer. Shared by both kw shapes ("char" / "gbuffer")
 * so the two paths stay in lock-step. */
PRIVATE void feed_one_char(PRIVATE_DATA *l, int c)
{
    if(l->in_search) {
        if(c == 27 || c == 7) {         /* ESC / Ctrl+G -> cancel */
            editline_exit_search(l, 0);
            refreshLine(l);
            return;
        }
        if(c >= 0x20 && c < 0x7F) {
            if(l->search_pat_len + 1 < sizeof(l->search_pat)) {
                l->search_pat[l->search_pat_len++] = (char)c;
                l->search_pat[l->search_pat_len] = 0;
                /* Re-run from the current match so narrowing doesn't jump. */
                int start = (l->search_idx >= 0)
                    ? l->search_idx : (l->history_len - 1);
                searchUpdate(l, start);
            }
        }
        /* Other chars are ignored in search mode. */
        return;
    }
    linenoiseEditInsert(l, c);
}

PRIVATE int ac_keychar(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    PRIVATE_DATA *l = priv;

    /* A printable char while the popup is open commits the previewed
     * candidate and types the new char after it (readline semantics). */
    if(priv->in_completion) {
        completion_close(priv);
    }

    if(kw_has_key(kw, "char")) {
        int c = (int)kw_get_int(gobj, kw, "char", 0, KW_REQUIRED);
        feed_one_char(l, c);
    }

    if(kw_has_key(kw, "gbuffer")) {
        gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, KW_REQUIRED);
        char *p = gbuffer_cur_rd_pointer(gbuf);
        int len = (int)gbuffer_leftbytes(gbuf);

        for(int i=0; i<len; i++) {
            feed_one_char(l, (unsigned char)p[i]);
        }
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  function
 ***************************************************************************/
PRIVATE int ac_move_start(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *l = gobj_priv_data(gobj);

    if(l->in_completion) { completion_close(l); }
    if(l->in_search) { editline_exit_search(l, 1); refreshLine(l); }

    /* go to the start of the line */
    linenoiseEditMoveHome(l);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  function
 ***************************************************************************/
PRIVATE int ac_move_end(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *l = gobj_priv_data(gobj);

    if(l->in_completion) { completion_close(l); }
    if(l->in_search) { editline_exit_search(l, 1); refreshLine(l); }

    /* go to the end of the line */
    linenoiseEditMoveEnd(l);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  function
 ***************************************************************************/
PRIVATE int ac_move_left(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    PRIVATE_DATA *l = priv;

    if(l->in_completion) { completion_close(l); }
    if(l->in_search) { editline_exit_search(l, 1); refreshLine(l); }

    /* Move cursor on the left */
    linenoiseEditMoveLeft(l);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  function
 ***************************************************************************/
PRIVATE int ac_del_char(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *l = gobj_priv_data(gobj);

    /*
     *  remove char at right of cursor,
     *  or if theline is empty, act as end-of-file.
     */
    if (l->len > 0) {
        linenoiseEditDelete(l);
    } else {
        l->history_len--;
        gbmem_free(l->history[l->history_len]);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  function
 ***************************************************************************/
PRIVATE int ac_move_right(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *l = gobj_priv_data(gobj);

    if(l->in_completion) { completion_close(l); }
    if(l->in_search) { editline_exit_search(l, 1); refreshLine(l); }

    /* Move cursor on the right */
    linenoiseEditMoveRight(l);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  function
 ***************************************************************************/
PRIVATE int ac_backspace(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->in_search) {
        /* Shrink the search pattern; restart search from newest on change. */
        if(priv->search_pat_len > 0) {
            priv->search_pat_len--;
            priv->search_pat[priv->search_pat_len] = 0;
        }
        if(priv->search_pat_len == 0) {
            priv->search_idx = -1;
            refreshSearchLine(priv);
        } else {
            searchUpdate(priv, priv->history_len - 1);
        }
    } else if(priv->in_completion) {
        /* Backspace cancels the completion and restores the text the user
         * had typed; it does NOT delete a char on top of that (less
         * surprising than commit+delete when the popup is visible). */
        completion_restore_original(priv);
        completion_close(priv);
    } else {
        linenoiseEditBackspace(priv);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  function
 ***************************************************************************/
PRIVATE int ac_complete_line(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->in_completion) {
        /* Cycle to the next candidate; preview it on the editline. */
        if(priv->completion_len > 0) {
            priv->completion_idx = (priv->completion_idx + 1) % priv->completion_len;
            completion_preview_candidate(priv, priv->completion_idx);
            completion_render_popup(priv);
        }
        KW_DECREF(kw);
        return 0;
    }

    completeLine(gobj);
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  function
 ***************************************************************************/
PRIVATE int ac_del_eol(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    PRIVATE_DATA *l = priv;

    /* delete from current to end of line. */
    l->buf[l->pos] = '\0';
    l->len = l->pos;
    refreshLine(l);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  function
 ***************************************************************************/
PRIVATE int ac_enter(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    PRIVATE_DATA *l = priv;

    /* Commit any in-flight reverse-i-search match into the buffer first. */
    if(priv->in_search) {
        editline_exit_search(priv, 1);
        refreshLine(priv);
    }

    /* Enter inside completion only commits the previewed candidate to
     * the edit line and closes the popup — it does NOT submit the
     * command. A second Enter (outside the popup) is what runs it, so
     * the user has a chance to edit the chosen candidate first. */
    if(priv->in_completion) {
        completion_close(priv);
        KW_DECREF(kw);
        return 0;
    }

    if(!empty_string(priv->buf) && strlen(priv->buf)>1) {
        l->history_len--;
        gbmem_free(l->history[l->history_len]);
        linenoiseHistoryAdd(priv, priv->buf); /* Add to the history. */
        if(!empty_string(priv->history_file)) {
            linenoiseHistorySave(priv, priv->history_file); /* Save the history on disk. */
        }
    }
    gobj_publish_event(gobj, EV_COMMAND, 0);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  function
 ***************************************************************************/
PRIVATE int ac_prev_hist(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    PRIVATE_DATA *l = priv;

    if(priv->in_completion) {
        /* Up inside the popup = previous candidate. */
        if(priv->completion_len > 0) {
            priv->completion_idx =
                (priv->completion_idx + priv->completion_len - 1)
                % priv->completion_len;
            completion_preview_candidate(priv, priv->completion_idx);
            completion_render_popup(priv);
        }
        KW_DECREF(kw);
        return 0;
    }

    if(l->in_search) { editline_exit_search(l, 0); refreshLine(l); }

    linenoiseEditHistoryNext(l, LINENOISE_HISTORY_PREV);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  function
 ***************************************************************************/
PRIVATE int ac_next_hist(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    PRIVATE_DATA *l = priv;

    if(priv->in_completion) {
        /* Down inside the popup = next candidate. */
        if(priv->completion_len > 0) {
            priv->completion_idx =
                (priv->completion_idx + 1) % priv->completion_len;
            completion_preview_candidate(priv, priv->completion_idx);
            completion_render_popup(priv);
        }
        KW_DECREF(kw);
        return 0;
    }

    if(l->in_search) { editline_exit_search(l, 0); refreshLine(l); }

    linenoiseEditHistoryNext(l, LINENOISE_HISTORY_NEXT);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  function
 ***************************************************************************/
PRIVATE int ac_swap_char(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    PRIVATE_DATA *l = priv;

    /*
     *  swaps current character with previous.
     */
    if (l->pos > 0 && l->pos < l->len) {
        int aux = l->buf[l->pos-1];
        l->buf[l->pos-1] = l->buf[l->pos];
        l->buf[l->pos] = aux;
        if (l->pos != l->len-1) l->pos++;
        refreshLine(l);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  function
 ***************************************************************************/
PRIVATE int ac_del_line(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    PRIVATE_DATA *l = priv;

    /* delete the whole line. */
    l->buf[0] = '\0';
    l->pos = l->len = 0;
    refreshLine(l);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  function
 ***************************************************************************/
PRIVATE int ac_del_prev_word(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    PRIVATE_DATA *l = priv;

    /* delete previous word */
    linenoiseEditDeletePrevWord(l);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Ctrl+R: enter reverse-i-search mode, or advance to the next older match.
 ***************************************************************************/
PRIVATE int ac_reverse_search(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *l = gobj_priv_data(gobj);

    if(!l->in_search) {
        l->in_search = 1;
        l->search_pat[0] = 0;
        l->search_pat_len = 0;
        l->search_idx = -1;
        refreshSearchLine(l);
    } else {
        /* Already searching: step to the previous (older) match. */
        int start = (l->search_idx > 0) ? (l->search_idx - 1)
                                        : (l->history_len - 1);
        searchUpdate(l, start);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Esc / Ctrl+G: cancel whichever sub-mode is active. Search restores the
 *  pre-search buffer (untouched during search anyway). Completion restores
 *  the snapshot taken when the popup opened.
 ***************************************************************************/
PRIVATE int ac_cancel(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *l = gobj_priv_data(gobj);

    if(l->in_completion) {
        completion_restore_original(l);
        completion_close(l);
    } else if(l->in_search) {
        editline_exit_search(l, 0);
        refreshLine(l);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Ctrl+S: forward counterpart of Ctrl+R. Once in search mode, step to the
 *  next (newer) match. If invoked outside search mode, enter search from
 *  the oldest entry — typing then narrows forward.
 ***************************************************************************/
PRIVATE int ac_forward_search(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *l = gobj_priv_data(gobj);

    if(!l->in_search) {
        l->in_search = 1;
        l->search_pat[0] = 0;
        l->search_pat_len = 0;
        l->search_idx = -1;
        refreshSearchLine(l);
    } else {
        int start = (l->search_idx >= 0) ? (l->search_idx + 1) : 0;
        searchUpdateForward(l, start);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  HACK kw is EVF_KW_WRITING
 ***************************************************************************/
PRIVATE int ac_gettext(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    left_justify(priv->buf);
    json_object_set_new(kw, "text", json_string(priv->buf));

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_settext(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *data = kw_get_str(gobj, kw, "text", "", KW_REQUIRED);

    PRIVATE_DATA *l = priv;
    l->oldpos = l->pos = 0;
    l->len = 0;
    l->history_index = 0;

    if(!empty_string(data)) {
        for(int i=0; i<strlen(data); i++) {
            linenoiseEditInsert(l, data[i]);
        }
    } else {
        /*
         *  The latest history entry is always our current buffer, that
         *  initially is just an empty string.
         */
        l->buf[0] = 0;
        linenoiseHistoryAdd(l, "");
    }

    gobj_send_event(gobj, EV_PAINT, kw_incref(kw), gobj);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_setfocus(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->wn) {
        wmove(priv->wn, 0, (int)(priv->plen + priv->pos));
        wrefresh(priv->wn);
    }
    refreshLine(priv);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_move(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int x = (int)kw_get_int(gobj, kw, "x", 0, KW_REQUIRED);
    int y = (int)kw_get_int(gobj, kw, "y", 0, KW_REQUIRED);
    gobj_write_integer_attr(gobj, "x", x);
    gobj_write_integer_attr(gobj, "y", y);

    if(priv->panel) {
        //log_debug_printf(0, "move panel x %d y %d %s", x, y, gobj_name(gobj));
        move_panel(priv->panel, y, x);
        update_panels();
        doupdate();
    } else if(priv->wn) {
        //log_debug_printf(0, "move window x %d y %d %s", x, y, gobj_name(gobj));
        mvwin(priv->wn, y, x);
        wrefresh(priv->wn);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_size(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int cx = (int)kw_get_int(gobj, kw, "cx", 0, KW_REQUIRED);
    int cy = (int)kw_get_int(gobj, kw, "cy", 0, KW_REQUIRED);
    gobj_write_integer_attr(gobj, "cx", cx);
    gobj_write_integer_attr(gobj, "cy", cy);

    if(priv->use_ncurses) {
        if(priv->panel) {
            //log_debug_printf(0, "size panel cx %d cy %d %s", cx, cy, gobj_name(gobj));
            wresize(priv->wn, cy, cx);
            update_panels();
            doupdate();
        } else if(priv->wn) {
            //log_debug_printf(0, "size window cx %d cy %d %s", cx, cy, gobj_name(gobj));
            wresize(priv->wn, cy, cx);
            wrefresh(priv->wn);
        }
    }

    gobj_send_event(gobj, EV_PAINT, kw_incref(kw), gobj);  // repaint, ncurses doesn't do it

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *    Colors available for fg_color, bg_color:
        "black"
        "red"
        "green"
        "yellow"
        "blue"
        "magenta"
        "cyan"
        "white"
 ***************************************************************************/
PRIVATE int ac_paint(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->use_ncurses) {
        wclear(priv->wn);
    }
    refreshLine(priv);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_clear_history(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    freeHistory(priv);
    priv->history = gbmem_malloc(sizeof(char*) * priv->history_max_len);

    if(!empty_string(priv->history_file)) {
        unlink(priv->history_file);
    }

    KW_DECREF(kw);
    return 0;
}

/*-----------------------------------------------------------------------*
 *  Mouse: click/move adjusts caret if inside our single-line box
 *-----------------------------------------------------------------------*/
PRIVATE int ac_mouse(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    // PRIVATE_DATA *priv = gobj_priv_data(gobj);
    //
    // int x = (int)kw_get_int(gobj, kw, "x", 0, 0);
    // int y = (int)kw_get_int(gobj, kw, "y", 0, 0);
    // int button = (int)kw_get_int(gobj, kw, "button", 0, 0);
    // const char *type = kw_get_str(gobj, kw, "type", "", 0);
    //
    // int left = (int)(priv->x + (int)priv->plen);
    //
    // if(y == (int)priv->y && x >= left) {
    //     size_t newpos = (size_t)(x - left);
    //     if(newpos > priv->len) {
    //         newpos = priv->len;
    //     }
    //     priv->pos = newpos;
    //     refreshLine(priv);
    //
    //     if(button == 0 && type && strcmp(type, "down")==0) {
    //         gobj_send_event(gobj, EV_SETFOCUS, 0, gobj);
    //     }
    // }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create      = mt_create,
    .mt_destroy     = mt_destroy,
    .mt_start       = mt_start,
    .mt_stop        = mt_stop,
    .mt_writing     = mt_writing,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_EDITLINE);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************************
 *          Create the GClass
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "GClass ALREADY created",
            "gclass",       "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*------------------------*
     *      States
     *------------------------*/
    ev_action_t st_idle[] = {
        {EV_KEYCHAR,                 ac_keychar,         0},
        {EV_EDITLINE_MOVE_START,     ac_move_start,      0},
        {EV_EDITLINE_MOVE_LEFT,      ac_move_left,       0},
        {EV_EDITLINE_DEL_CHAR,       ac_del_char,        0},
        {EV_EDITLINE_MOVE_END,       ac_move_end,        0},
        {EV_EDITLINE_MOVE_RIGHT,     ac_move_right,      0},
        {EV_EDITLINE_BACKSPACE,      ac_backspace,       0},
        {EV_EDITLINE_COMPLETE_LINE,  ac_complete_line,   0},
        {EV_EDITLINE_DEL_EOL,        ac_del_eol,         0},
        {EV_EDITLINE_ENTER,          ac_enter,           0},
        {EV_EDITLINE_PREV_HIST,      ac_prev_hist,       0},
        {EV_EDITLINE_NEXT_HIST,      ac_next_hist,       0},
        {EV_EDITLINE_SWAP_CHAR,      ac_swap_char,       0},
        {EV_EDITLINE_DEL_LINE,       ac_del_line,        0},
        {EV_EDITLINE_DEL_PREV_WORD,  ac_del_prev_word,   0},
        {EV_EDITLINE_REVERSE_SEARCH, ac_reverse_search,  0},
        {EV_EDITLINE_FORWARD_SEARCH, ac_forward_search,  0},
        {EV_EDITLINE_CANCEL,         ac_cancel,          0},
        {EV_MOUSE,                   ac_mouse,           0},
        {EV_GETTEXT,                 ac_gettext,         0},
        {EV_SETTEXT,                 ac_settext,         0},
        {EV_SETFOCUS,                ac_setfocus,        0},
        {EV_KILLFOCUS,               0,                  0},
        {EV_MOVE,                    ac_move,            0},
        {EV_SIZE,                    ac_size,            0},
        {EV_PAINT,                   ac_paint,           0},
        {EV_CLEAR_HISTORY,           ac_clear_history,   0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,       st_idle},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_COMMAND,                    EVF_OUTPUT_EVENT},
        {EV_KEYCHAR,                    0},
        {EV_EDITLINE_MOVE_START,        0},
        {EV_EDITLINE_MOVE_LEFT,         0},
        {EV_EDITLINE_DEL_CHAR,          0},
        {EV_EDITLINE_MOVE_END,          0},
        {EV_EDITLINE_MOVE_RIGHT,        0},
        {EV_EDITLINE_BACKSPACE,         0},
        {EV_EDITLINE_COMPLETE_LINE,     0},
        {EV_EDITLINE_DEL_EOL,           0},
        {EV_EDITLINE_ENTER,             0},
        {EV_EDITLINE_PREV_HIST,         0},
        {EV_EDITLINE_NEXT_HIST,         0},
        {EV_EDITLINE_SWAP_CHAR,         0},
        {EV_EDITLINE_DEL_LINE,          0},
        {EV_EDITLINE_DEL_PREV_WORD,     0},
        {EV_EDITLINE_REVERSE_SEARCH,    0},
        {EV_EDITLINE_FORWARD_SEARCH,    0},
        {EV_EDITLINE_CANCEL,            0},
        {EV_GETTEXT,                    0},
        {EV_SETTEXT,                    0},
        {EV_KILLFOCUS,                  0},
        {EV_SETFOCUS,                   0},
        {EV_PAINT,                      0},
        {EV_MOVE,                       0},
        {EV_SIZE,                       0},
        {EV_MOUSE,                      0},
        {EV_CLEAR_HISTORY,              0},
        {NULL, 0}
    };

    /*----------------------------------------*
     *          Register GClass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,  // lmt
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,  // acl
        0,  // cmds
        s_user_trace_level,
        0   // gcflags
    );
    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************************
 *          Public access
 ***************************************************************************/
PUBLIC int register_c_editline(void)
{
    return create_gclass(C_EDITLINE);
}
