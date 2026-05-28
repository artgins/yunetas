/****************************************************************************
 *          C_WN_STDSCR.H
 *          Wn_stdscr GClass.
 *
 *          Windows Framework with ncurses
 *
 *          The goal of wn_stdscr is :
 *              - detect changes in stdscr size and inform to all his children (they must be c_box)
 *              - centralize the background/foreground colors
 *              - centralize what window has the focus
 *              - generic write function
 *
 *          - Catch the signal SIGWINCH for changes of screen size: set the __new_stdsrc_size__ to TRUE.
 *          - The timeout check the variable and if true:
 *              - set __new_stdsrc_size__ to FALSE
 *              - sends the message EV_SIZE to the children
 *              - publishes the message EV_SCREEN_SIZE_CHANGE
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
   Available colors for bg_color and fg_color:
      "black"
      "red"
      "green"
      "yellow"
      "blue"
      "magenta"
      "cyan"
      "white"

**rst**/

PUBLIC int get_paint_color(const char *fg_color, const char *bg_color);

PUBLIC int SetDefaultFocus(hgobj gobj);
PUBLIC int SetFocus(hgobj gobj);
PUBLIC hgobj GetFocus(void);
PUBLIC int SetTextColor(hgobj gobj, const char *color);
PUBLIC int SetBkColor(hgobj gobj, const char *color);
PUBLIC int DrawText(hgobj gobj, int x, int y, const char *s); // Shortcut for EV_SETTEXT

#ifdef __cplusplus
}
#endif
