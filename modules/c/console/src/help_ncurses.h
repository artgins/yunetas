/****************************************************************************
 *          help_ncurses.h
 *
 *          Ncurses helper
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <ncursesw/ncurses.h>
#include <ncursesw/panel.h>     // don't remove
#include "gobj.h"

#ifdef __cplusplus
extern "C"{
#endif

PUBLIC WINDOW *open_ncurses(hgobj gobj);
PUBLIC void close_ncurses(void);
PUBLIC int get_paint_color(const char *fg_color, const char *bg_color);

#ifdef __cplusplus
}
#endif
