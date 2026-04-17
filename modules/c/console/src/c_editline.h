/****************************************************************************
 *          C_EDITLINE.H
 *          Editline GClass.
 *
 *          Copyright (c) 2016 Niyamaka.
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              FSM
 ***************************************************************/
/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DECLARE_GCLASS(C_EDITLINE);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Tab-completion API
 ***************************************************************/
typedef struct editline_completions_s {
    size_t len;
    char **cvec;        /* full-line replacement for each candidate */
    char **descs;       /* parallel array; each entry may be NULL */
} editline_completions_t;

typedef void (*editline_completion_cb_t)(
    hgobj gobj,
    const char *buf,
    editline_completions_t *lc,
    void *user_data
);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_editline(void);

PUBLIC int tty_keyboard_init(void); /* Create and return a 'stdin' fd, to read input keyboard, without echo, then you can feed the editline with EV_KEYCHAR event */

/*  Register a callback invoked on TAB; pass NULL to disable. */
PUBLIC void editline_set_completion_callback(
    hgobj gobj,
    editline_completion_cb_t cb,
    void *user_data
);

/*  Append a completion candidate from within a completion callback.
 *  `str`  is the full replacement line if the candidate is picked.
 *  `desc` is an optional human-readable description shown in the
 *         candidate list when more than one option is available; pass
 *         NULL if not available. */
PUBLIC void editline_add_completion(
    editline_completions_t *lc,
    const char *str,
    const char *desc
);

/***************************************************************
 *              Inline hints API (shown in-place, to the right)
 ***************************************************************/
/*
 *  Callback invoked on every line refresh. Return NULL for no hint, or a
 *  heap-allocated string. Optional outputs:
 *      out_color  ANSI SGR foreground color (default: 90 = bright black/gray)
 *      out_bold   1 to render bold, 0 otherwise
 *  The returned pointer is handed to free_cb (if set) after being drawn,
 *  so the callback is free to use any allocator.
 */
typedef char *(*editline_hints_cb_t)(
    hgobj gobj,
    const char *buf,
    int *out_color,
    int *out_bold,
    void *user_data
);
typedef void (*editline_free_hint_cb_t)(
    char *hint,
    void *user_data
);

PUBLIC void editline_set_hints_callback(
    hgobj gobj,
    editline_hints_cb_t cb,
    editline_free_hint_cb_t free_cb,
    void *user_data
);

#ifdef __cplusplus
}
#endif
