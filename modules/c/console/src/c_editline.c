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

    int in_completion;  /* The user pressed TAB and we are now in completion
                         * mode, so input is handled by completeLine(). */
    size_t completion_idx; /* Index of next completion to propose. */
    editline_completion_cb_t completion_callback;
    void *completion_user_data;

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
 *  Called from refreshLine() whenever l->in_search is set.
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

    printf(Erase_Whole_Line);
    printf(Move_Horizontal, 1);
    printf("%s%.*s", prefix, (int)match_len, match);
    printf(Move_Horizontal, prefix_len + 1);
    fflush(stdout);
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

/* This is an helper function for linenoiseEdit() and is called when the
 * user types the <tab> key in order to complete the string currently in the
 * input.
 *
 * The state of the editing is encapsulated into the pointed linenoiseState
 * structure as described in the structure definition. */
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
    } else {
        /* Multiple candidates: print them with descriptions, then let the
         * user cycle through them with further TABs. */
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
            /* Left-pad and truncate to name_width to keep descriptions aligned. */
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
            // trace_msg0("kb x%X %c", c, c);
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
                    /* Re-show original buffer */
                    if (i < lc.len) refreshLine(ls);
                    stop = 1;
                    break;
                default:
                    /* Apply the selected candidate (if on one). */
                    if (i < lc.len) {
                        nwritten = snprintf(ls->buf,ls->buflen,"%s",lc.cvec[i]);
                        ls->len = ls->pos = nwritten;
                        refreshLine(ls);
                    }
                    stop = 1;
                    /*
                     *  Re-dispatch the terminating key so Enter / Backspace /
                     *  printable chars take effect in the same keystroke —
                     *  otherwise the user has to press the key a second time.
                     */
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

        /* Move cursor to original position. */
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

    /* Don't add duplicated lines. */
    if(l->history_len && strcmp(l->history[l->history_len-1], line)==0)
        return 0;

    /*
     *  No repitas si está en las últimas x líneas.
     */
//     for (int i = 0; i < l->history_len && i<5; i++) {
//         int j = l->history_len-1-i;
//         if(j<0) {
//             break;
//         }
//         if(strcmp(l->history[j], line)==0) {
//             return 0;
//         }
//     }

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
PRIVATE int ac_keychar(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    PRIVATE_DATA *l = priv;

    if(kw_has_key(kw, "char")) {
        int c = (int)kw_get_int(gobj, kw, "char", 0, KW_REQUIRED);
        if(l->in_search) {
            /* Reverse-i-search mode: editing the pattern. */
            if(c == 27 || c == 7) {         /* ESC / Ctrl+G -> cancel */
                editline_exit_search(l, 0);
                refreshLine(l);
            } else if(c >= 0x20 && c < 0x7F) {
                if(l->search_pat_len + 1 < sizeof(l->search_pat)) {
                    l->search_pat[l->search_pat_len++] = (char)c;
                    l->search_pat[l->search_pat_len] = 0;
                    /* Re-run search from the current match position so the
                     * user can narrow down without jumping around. */
                    int start = (l->search_idx >= 0)
                        ? l->search_idx : (l->history_len - 1);
                    searchUpdate(l, start);
                }
            }
            /* Other chars are ignored in search mode. */
        } else {
            linenoiseEditInsert(l, c);
        }
    }

    if(kw_has_key(kw, "gbuffer")) {
        gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, KW_REQUIRED);
        char *p = gbuffer_cur_rd_pointer(gbuf);
        int len = (int)gbuffer_leftbytes(gbuf);

        for(int i=0; i<len; i++) {
            int c = p[i];
            linenoiseEditInsert(l, c);  // ascii editor, no utf-8
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
