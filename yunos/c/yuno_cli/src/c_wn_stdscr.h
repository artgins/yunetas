/****************************************************************************
 *          C_WN_STDSCR.H
 *          Wn_stdscr GClass.
 *
 *          Windows Framework with ncurses
 *
 *          The goal of wn_stdscr is :
 *              - detect changes in stdscr size and inform to all his children
 *              - centralize the background/foreground colors
 *              - centralize what window has the focus
 *              - centralize the writing in the window with focus
 *
 *          Copyright (c) 2015-2016 Niyamaka.
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
GOBJ_DECLARE_GCLASS(C_WN_STDSCR);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_wn_stdscr(void);

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PUBLIC int get_stdscr_size(int *w, int *h);


/**rst**
   Allocate a ncurses color pair.
    color_id        name
    =============   =========
    COLOR_BLACK     "black"
    COLOR_RED       "red"
    COLOR_GREEN     "green"
    COLOR_YELLOW    "yellow"
    COLOR_BLUE      "blue"
    COLOR_MAGENTA   "magenta"
    COLOR_CYAN      "cyan"
    COLOR_WHITE     "white"

**rst**/

PUBLIC int get_curses_color(const char *fg_color, const char *bg_color);

PUBLIC int SetDefaultFocus(hgobj gobj);
PUBLIC int SetFocus(hgobj gobj);
PUBLIC hgobj GetFocus(void);
PUBLIC int SetTextColor(hgobj gobj, const char *color);
PUBLIC int SetBkColor(hgobj gobj, const char *color);
PUBLIC int DrawText(hgobj gobj, int x, int y, const char *s);

#ifdef __cplusplus
}
#endif
