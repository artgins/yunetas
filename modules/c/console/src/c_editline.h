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
    char **cvec;
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

/*  Append a completion candidate from within a completion callback. */
PUBLIC void editline_add_completion(editline_completions_t *lc, const char *str);

#ifdef __cplusplus
}
#endif
