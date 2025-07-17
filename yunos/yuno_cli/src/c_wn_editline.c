/***********************************************************************
 *          C_WN_EDITLINE.C
 *          Wn_editline GClass.
 *
 *  Part of this code is copied of linenoise.c
 *  You can find the latest source code at:
 *
 * linenoise.c -- VERSION 1.0
 *
 * Guerrilla line editing library against the idea that a line editing lib
 * needs to be 20,000 lines of C code.
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
 * Copyright (c) 2010-2014, Salvatore Sanfilippo <antirez at gmail dot com>
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
 *          All Rights Reserved.
***********************************************************************/
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <ncurses/ncurses.h>
#include <ncurses/panel.h>
#include "c_wn_editline.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/


/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct linenoiseCompletions {
  size_t len;
  char **cvec;
} linenoiseCompletions;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
SDATA (ASN_OCTET_STR,   "prompt",               0,  "> ", "Prompt"),
SDATA (ASN_OCTET_STR,   "history_file",         0,  0, "History file"),
SDATA (ASN_INTEGER,     "history_max_len",      0,  200000, "history max len (max lines)"),
SDATA (ASN_INTEGER,     "buffer_size",          0,  4*1024, "edition buffer size"),
SDATA (ASN_INTEGER,     "x",                    0,  0, "x window coord"),
SDATA (ASN_INTEGER,     "y",                    0,  0, "y window coord"),
SDATA (ASN_INTEGER,     "cx",                   0,  80, "physical witdh window size"),
SDATA (ASN_INTEGER,     "cy",                   0,  1, "physical height window size"),
SDATA (ASN_OCTET_STR,   "bg_color",             0,  "cyan", "Background color"),
SDATA (ASN_OCTET_STR,   "fg_color",             0,  "white", "Foreground color"),
SDATA (ASN_POINTER,     "user_data",            0,  0, "user data"),
SDATA (ASN_POINTER,     "user_data2",           0,  0, "more user data"),
SDATA (ASN_POINTER,     "subscriber",           0,  0, "subscriber of output-events. If it's null then subscriber is the parent."),
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
    const char *fg_color;
    const char *bg_color;
    WINDOW *wn;     // ncurses window handler
    PANEL *panel;   // panel handler

    const char *history_file;
    const char *prompt; /* Prompt to display. */
    int history_max_len;
    int history_len;
    char **history;
    int mlmode;         /* Multi line mode. Default is single line. */
    char *buf;          /* Edited line buffer. */
    size_t buflen;      /* Edited line buffer size. */
    size_t plen;        /* Prompt length. */
    size_t pos;         /* Current cursor position. */
    size_t oldpos;      /* Previous refresh cursor position. */
    size_t len;         /* Current edited line length. */
    size_t cols;        /* Number of columns in terminal. */
    int history_index;  /* The history index we are currently editing. */
} PRIVATE_DATA;

PRIVATE void freeHistory(PRIVATE_DATA *l);
PRIVATE int linenoiseHistorySetMaxLen(PRIVATE_DATA *l, int len);
PRIVATE int linenoiseHistoryLoad(PRIVATE_DATA *l, const char *filename);
PRIVATE int linenoiseHistoryAdd(PRIVATE_DATA *l, const char *line);
PRIVATE void refreshLine(PRIVATE_DATA *l);



            /******************************
             *      Framework Methods
             ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  CHILD subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber)
        subscriber = gobj_parent(gobj);
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
    SET_PRIV(cx,                        gobj_read_int32_attr)
        priv->cols = priv->cx;
    SET_PRIV(cy,                        gobj_read_int32_attr)
    SET_PRIV(history_max_len,           gobj_read_int32_attr)

    int x = gobj_read_int32_attr(gobj, "x");
    int y = gobj_read_int32_attr(gobj, "y");

    int buffer_size = gobj_read_int32_attr(gobj, "buffer_size");
    priv->buf = gbmem_malloc(buffer_size);
    priv->buflen = buffer_size - 1;

    /*
     *  Don't log errors, in batch mode there is no curses windows.
     */
    priv->wn = newwin(priv->cy, priv->cx, y, x);
    if(priv->wn) {
        priv->panel = new_panel(priv->wn);
    }

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
        gobj_send_event(gobj, "EV_PAINT", 0, gobj);
    ELIF_EQ_SET_PRIV(cx,                    gobj_read_int32_attr)
        priv->cols = priv->cx;
        //TODO igual hay que refrescar
    ELIF_EQ_SET_PRIV(cy,                    gobj_read_int32_attr)
    ELIF_EQ_SET_PRIV(cy,                    gobj_read_int32_attr)
    ELIF_EQ_SET_PRIV(history_max_len,       gobj_read_int32_attr)
        linenoiseHistorySetMaxLen(priv, priv->history_max_len);
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    gobj_send_event(gobj, "EV_PAINT", 0, gobj);
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
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
    GBMEM_FREE(priv->buf);
    freeHistory(priv);
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    gobj_change_state(gobj, "ST_IDLE");
    // TODO re PAINT in enabled colors
    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    gobj_change_state(gobj, "ST_DISABLED");
    // TODO re PAINT in disabled colors
    return 0;
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *  Beep, used for completion when there is nothing to complete or when all
 *  the choices were already shown.
 ***************************************************************************/
PRIVATE void linenoiseBeep(void)
{
    fprintf(stderr, "\x7");
    fflush(stderr);
}

/* ============================== Completion ================================ */

/* Free a list of completion option populated by linenoiseAddCompletion(). */
static void freeCompletions(linenoiseCompletions *lc) {
    size_t i;
    if(lc->cvec) {
        for (i = 0; i < lc->len; i++) {
            if(lc->cvec[i]) {
                free(lc->cvec[i]);
            }
        }
    }
    if (lc->cvec != NULL)
        free(lc->cvec);
}

/* This is an helper function for linenoiseEdit() and is called when the
 * user types the <tab> key in order to complete the string currently in the
 * input.
 *
 * The state of the editing is encapsulated into the pointed linenoiseState
 * structure as described in the structure definition. */
static int completeLine(PRIVATE_DATA *ls)
{
    linenoiseCompletions lc = { 0, NULL };
    int nread, nwritten;
    char c = 0;

    // TODO completionCallback(ls->buf,&lc);
    if (lc.len == 0) {
        linenoiseBeep();
    } else {
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
            log_debug_printf(0, "kb x%X %c", c, c);
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
                    /* Update buffer and return */
                    if (i < lc.len) {
                        nwritten = snprintf(ls->buf,ls->buflen,"%s",lc.cvec[i]);
                        ls->len = ls->pos = nwritten;
                    }
                    stop = 1;
                    break;
            }
        }
    }

    freeCompletions(&lc);
    return c; /* Return last read character */
}

/* Register a callback function to be called for tab-completion. */
// TODO void linenoiseSetCompletionCallback(linenoiseCompletionCallback *fn) {
//     completionCallback = fn;
// }

/* This function is used by the callback function registered by the user
 * in order to add completion options given the input string when the
 * user typed <tab>. See the example.c source code for a very easy to
 * understand example. */
void linenoiseAddCompletion(linenoiseCompletions *lc, const char *str) {
    size_t len = strlen(str);
    char *copy, **cvec;

    copy = malloc(len+1);
    if (copy == NULL) return;
    memcpy(copy,str,len+1);
    cvec = realloc(lc->cvec,sizeof(char*)*(lc->len+1));
    if (cvec == NULL) {
        free(copy);
        return;
    }
    lc->cvec = cvec;
    lc->cvec[lc->len++] = copy;
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
    size_t plen = strlen(l->prompt);
    char *buf = l->buf;
    size_t len = l->len;
    size_t pos = l->pos;

    /*
     *  Aquí debe estar ajustando la ventana visible de la línea,
     *  moviendo el cursor si se ha quedado fuera de la zona visible.
     */
    while((plen+pos) >= l->cols) {
        buf++;
        len--;
        pos--;
    }
    while (plen+len > l->cols) {
        len--;
    }

    /* Cursor to left edge */
    wmove(l->wn, 0, 0); // move to begining of line
    wclrtoeol(l->wn);   // erase to end of line

    /* Write the prompt and the current buffer content */
    waddnstr(l->wn, l->prompt, plen);
    waddnstr(l->wn, buf, len);

    /* Move cursor to original position. */
    wmove(l->wn, 0, (int)(pos+plen));

    wrefresh(l->wn);
}

/***************************************************************************
 *  Insert the character 'c' at cursor current position.
 ***************************************************************************/
PRIVATE int linenoiseEditInsert(PRIVATE_DATA *l, int c)
{
    /*
     *  WARNING By the moment ONLY ascii
     */
    if(c>127) {
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
        refreshLine(l);
    }
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
            #define PASSW "passw"
            char *p = strstr(l->history[i], PASSW);
            if(p) {
                p += strlen(PASSW);
                *p = 0;
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
PRIVATE int ac_keychar(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    PRIVATE_DATA *l = priv;
    GBUFFER *gbuf = (GBUFFER *)(size_t)kw_get_int(kw, "gbuffer", 0, KW_REQUIRED);
    char *p = gbuf_cur_rd_pointer(gbuf);
    int len = gbuf_leftbytes(gbuf);

    for(int i=0; i<len; i++) {
        int c = p[i];
        linenoiseEditInsert(l, c);  // ascii editor, no utf-8
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  function
 ***************************************************************************/
PRIVATE int ac_move_start(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *l = gobj_priv_data(gobj);

    /* go to the start of the line */
    linenoiseEditMoveHome(l);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  function
 ***************************************************************************/
PRIVATE int ac_move_end(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *l = gobj_priv_data(gobj);

    /* go to the end of the line */
    linenoiseEditMoveEnd(l);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  function
 ***************************************************************************/
PRIVATE int ac_move_left(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    PRIVATE_DATA *l = priv;

    /* Move cursor on the left */
    linenoiseEditMoveLeft(l);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  function
 ***************************************************************************/
PRIVATE int ac_del_char(hgobj gobj, const char *event, json_t *kw, hgobj src)
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
PRIVATE int ac_move_right(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *l = gobj_priv_data(gobj);

    /* Move cursor on the right */
    linenoiseEditMoveRight(l);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  function
 ***************************************************************************/
PRIVATE int ac_backspace(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    linenoiseEditBackspace(priv);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  function
 ***************************************************************************/
PRIVATE int ac_complete_line(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    completeLine(gobj);
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  function
 ***************************************************************************/
PRIVATE int ac_del_eol(hgobj gobj, const char *event, json_t *kw, hgobj src)
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
PRIVATE int ac_enter(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    PRIVATE_DATA *l = priv;

    if(!empty_string(priv->buf) && strlen(priv->buf)>1) {
        l->history_len--;
        gbmem_free(l->history[l->history_len]);
        linenoiseHistoryAdd(priv, priv->buf); /* Add to the history. */
        if(!empty_string(priv->history_file)) {
            linenoiseHistorySave(priv, priv->history_file); /* Save the history on disk. */
        }
    }
    gobj_publish_event(gobj, "EV_COMMAND", 0);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  function
 ***************************************************************************/
PRIVATE int ac_prev_hist(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    PRIVATE_DATA *l = priv;

    linenoiseEditHistoryNext(l, LINENOISE_HISTORY_PREV);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  function
 ***************************************************************************/
PRIVATE int ac_next_hist(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    PRIVATE_DATA *l = priv;

    linenoiseEditHistoryNext(l, LINENOISE_HISTORY_NEXT);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  function
 ***************************************************************************/
PRIVATE int ac_swap_char(hgobj gobj, const char *event, json_t *kw, hgobj src)
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
PRIVATE int ac_del_line(hgobj gobj, const char *event, json_t *kw, hgobj src)
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
PRIVATE int ac_del_prev_word(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    PRIVATE_DATA *l = priv;

    /* delete previous word */
    linenoiseEditDeletePrevWord(l);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_paint(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->wn) {
        // Debugging in kdevelop or batch mode has no wn
        printf("\n%s%s", priv->prompt, priv->buf);
        fflush(stdout);
        KW_DECREF(kw);
        return 0;
    }

    wclear(priv->wn);

    if(has_colors()) {
        if(!empty_string(priv->fg_color) && !empty_string(priv->bg_color)) {
            wbkgd(
                priv->wn,
                _get_curses_color(priv->fg_color, priv->bg_color)
            );
        }
    }
    refreshLine(priv);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_gettext(hgobj gobj, const char *event, json_t *kw, hgobj src)
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
PRIVATE int ac_settext(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *data = kw_get_str(kw, "text", "", KW_REQUIRED);

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

    gobj_send_event(gobj, "EV_PAINT", 0, gobj);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_setfocus(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->wn) {
        wmove(priv->wn, 0, priv->plen + priv->pos);
        wrefresh(priv->wn);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_move(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int x = kw_get_int(kw, "x", 0, KW_REQUIRED);
    int y = kw_get_int(kw, "y", 0, KW_REQUIRED);
    gobj_write_int32_attr(gobj, "x", x);
    gobj_write_int32_attr(gobj, "y", y);

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
PRIVATE int ac_size(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int cx = kw_get_int(kw, "cx", 0, KW_REQUIRED);
    int cy = kw_get_int(kw, "cy", 0, KW_REQUIRED);
    gobj_write_int32_attr(gobj, "cx", cx);
    gobj_write_int32_attr(gobj, "cy", cy);

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
    gobj_send_event(gobj, "EV_PAINT", 0, gobj);  // repaint, ncurses doesn't do it

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_clear_history(hgobj gobj, const char *event, json_t *kw, hgobj src)
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

/***************************************************************************
 *                          FSM
 ***************************************************************************/
PRIVATE const EVENT input_events[] = {
    {"EV_KEYCHAR",                  0,              0,  0},
    {"EV_EDITLINE_MOVE_START",      0,              0,  0},
    {"EV_EDITLINE_MOVE_LEFT",       0,              0,  0},
    {"EV_EDITLINE_DEL_CHAR",        0,              0,  0},
    {"EV_EDITLINE_MOVE_END",        0,              0,  0},
    {"EV_EDITLINE_MOVE_RIGHT",      0,              0,  0},
    {"EV_EDITLINE_BACKSPACE",       0,              0,  0},
    {"EV_EDITLINE_COMPLETE_LINE",   0,              0,  0},
    {"EV_EDITLINE_DEL_EOL",         0,              0,  0},
    {"EV_EDITLINE_ENTER",           0,              0,  0},
    {"EV_EDITLINE_PREV_HIST",       0,              0,  0},
    {"EV_EDITLINE_NEXT_HIST",       0,              0,  0},
    {"EV_EDITLINE_SWAP_CHAR",       0,              0,  0},
    {"EV_EDITLINE_DEL_LINE",        0,              0,  0},
    {"EV_EDITLINE_DEL_PREV_WORD",   0,              0,  0},
    {"EV_GETTEXT",                  EVF_KW_WRITING,  0,  0},
    {"EV_SETTEXT",                  0,              0,  0},
    {"EV_KILLFOCUS",                0,              0,  0},
    {"EV_SETFOCUS",                 0,              0,  0},
    {"EV_PAINT",                    0,              0,  0},
    {"EV_MOVE",                     0,              0,  0},
    {"EV_SIZE",                     0,              0,  0},
    {"EV_CLEAR_HISTORY",            0,              0,  0},
    {NULL, 0, 0, 0}
};
PRIVATE const EVENT output_events[] = {
    {"EV_COMMAND",                  0,              0,  0},
    {NULL, 0, 0, 0}
};
PRIVATE const char *state_names[] = {
    "ST_IDLE",
    "ST_DISABLED",
    NULL
};

PRIVATE EV_ACTION ST_IDLE[] = {
    {"EV_KEYCHAR",                  ac_keychar,         0},
    {"EV_EDITLINE_MOVE_START",      ac_move_start,      0},
    {"EV_EDITLINE_MOVE_LEFT",       ac_move_left,       0},
    {"EV_EDITLINE_DEL_CHAR",        ac_del_char,        0},
    {"EV_EDITLINE_MOVE_END",        ac_move_end,        0},
    {"EV_EDITLINE_MOVE_RIGHT",      ac_move_right,      0},
    {"EV_EDITLINE_BACKSPACE",       ac_backspace,       0},
    {"EV_EDITLINE_COMPLETE_LINE",   ac_complete_line,   0},
    {"EV_EDITLINE_DEL_EOL",         ac_del_eol,         0},
    {"EV_EDITLINE_ENTER",           ac_enter,           0},
    {"EV_EDITLINE_PREV_HIST",       ac_prev_hist,       0},
    {"EV_EDITLINE_NEXT_HIST",       ac_next_hist,       0},
    {"EV_EDITLINE_SWAP_CHAR",       ac_swap_char,       0},
    {"EV_EDITLINE_DEL_LINE",        ac_del_line,        0},
    {"EV_EDITLINE_DEL_PREV_WORD",   ac_del_prev_word,   0},

    {"EV_GETTEXT",                  ac_gettext,         0},
    {"EV_SETTEXT",                  ac_settext,         0},
    {"EV_SETFOCUS",                 ac_setfocus,        0},
    {"EV_KILLFOCUS",                0,                  0},
    {"EV_MOVE",                     ac_move,            0},
    {"EV_SIZE",                     ac_size,            0},
    {"EV_PAINT",                    ac_paint,           0},
    {"EV_CLEAR_HISTORY",            ac_clear_history,   0},
    {0,0,0}
};

PRIVATE EV_ACTION ST_DISABLED[] = {
    {0,0,0}
};

PRIVATE EV_ACTION *states[] = {
    ST_IDLE,
    ST_DISABLED,
    NULL
};

PRIVATE FSM fsm = {
    input_events,
    output_events,
    state_names,
    states,
};

/***************************************************************************
 *              GClass
 ***************************************************************************/
/*---------------------------------------------*
 *              Local methods table
 *---------------------------------------------*/
PRIVATE LMETHOD lmt[] = {
    {0, 0, 0}
};

/*---------------------------------------------*
 *              GClass
 *---------------------------------------------*/
PRIVATE GCLASS _gclass = {
    0,  // base
    GCLASS_WN_EDITLINE_NAME,
    &fsm,
    {
        mt_create,
        0, //mt_create2,
        mt_destroy,
        mt_start,
        mt_stop,
        mt_play,
        mt_pause,
        mt_writing,
        0, //mt_reading,
        0, //mt_subscription_added,
        0, //mt_subscription_deleted,
        0, //mt_child_added,
        0, //mt_child_removed,
        0, //mt_stats,
        0, //mt_command,
        0, //mt_inject_event,
        0, //mt_create_resource,
        0, //mt_list_resource,
        0, //mt_save_resource,
        0, //mt_delete_resource,
        0, //mt_future21
        0, //mt_future22
        0, //mt_get_resource
        0, //mt_state_changed,
        0, //mt_authenticate,
        0, //mt_list_childs,
        0, //mt_stats_updated,
        0, //mt_disable,
        0, //mt_enable,
        0, //mt_trace_on,
        0, //mt_trace_off,
        0, //mt_gobj_created,
        0, //mt_future33,
        0, //mt_future34,
        0, //mt_publish_event,
        0, //mt_publication_pre_filter,
        0, //mt_publication_filter,
        0, //mt_authz_checker,
        0, //mt_future39,
        0, //mt_create_node,
        0, //mt_update_node,
        0, //mt_delete_node,
        0, //mt_link_nodes,
        0, //mt_future44,
        0, //mt_unlink_nodes,
        0, //mt_topic_jtree,
        0, //mt_get_node,
        0, //mt_list_nodes,
        0, //mt_shoot_snap,
        0, //mt_activate_snap,
        0, //mt_list_snaps,
        0, //mt_treedbs,
        0, //mt_treedb_topics,
        0, //mt_topic_desc,
        0, //mt_topic_links,
        0, //mt_topic_hooks,
        0, //mt_node_parents,
        0, //mt_node_childs,
        0, //mt_list_instances,
        0, //mt_node_tree,
        0, //mt_topic_size,
        0, //mt_future62,
        0, //mt_future63,
        0, //mt_future64
    },
    lmt,
    tattr_desc,
    sizeof(PRIVATE_DATA),
    0,  // acl
    s_user_trace_level,
    0, // cmds
    0, // gcflag
};

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC GCLASS *gclass_wn_editline(void)
{
    return &_gclass;
}
