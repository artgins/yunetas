/****************************************************************************
 *          help_ncurses.c
 *
 *          Ncurses helper
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <ncurses/ncurses.h>

#include "help_ncurses.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/
PRIVATE struct { // Code repeated
    int id;
    const char *name;
} table_id[] = {
    {COLOR_BLACK,   "black"},
    {COLOR_RED,     "red"},
    {COLOR_GREEN,   "green"},
    {COLOR_YELLOW,  "yellow"},
    {COLOR_BLUE,    "blue"},
    {COLOR_MAGENTA, "magenta"},
    {COLOR_CYAN,    "cyan"},
    {COLOR_WHITE,   "white"}
};

/***************************************************************************
 *              Prototypes
 ***************************************************************************/

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE BOOL __colors_ready__ = FALSE;

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC WINDOW *open_ncurses(hgobj gobj)
{
    WINDOW *wn = initscr();         /* Start curses mode            */
    cbreak();                       /* Line buffering disabled      */
    noecho();                       /* Don't echo() while we do getch */
    keypad(wn, TRUE);               /* We get F1, F2 etc..          */
    halfdelay(1);
    //wtimeout(wn, 10);             /* input non-blocking, wait 1 msec */
    if(has_colors()) {
        start_color();
        /* Allow -1 (default fg/bg) to pass through */
        assume_default_colors(-1, -1);
        __colors_ready__ = true;
    } else {
        __colors_ready__ = false;
    }
    return wn;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void close_ncurses(void)
{
    endwin();     // restore terminal
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int get_color_id(const char *color) // code repeated
{
    if(!color || !*color) {
        return -1; /* default */
    }
    if(!strcasecmp(color, "default") || !strcasecmp(color, "def")) {
        return -1;
    }
    for(size_t i = 0; i < ARRAY_SIZE(table_id); i++) {
        if(strcasecmp(table_id[i].name, color) == 0) {
            return table_id[i].id;
        }
    }
    return -1; /* fall back to default, not white */
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int get_color_pair(int fg, int bg) // code repeated
{
    static int next_pair = 1;
    static int cp[16][16]; /* allow up to 16 basic colors; still O(1) */
    int maxc = (COLORS > 16) ? 16 : COLORS;

    if(!__colors_ready__) {
        return 0; /* no color attribute */
    }

    /* Map -1 to default colors only if use_default_colors() succeeded */
    if(fg < -1 || bg < -1) {
        return 0;
    }

    /* Validate against actual COLORS; clamp to supported range */
    if((fg >= maxc && fg != -1) || (bg >= maxc && bg != -1)) {
        return 0;
    }

    int ifg = (fg < 0) ? 0 : fg;
    int ibg = (bg < 0) ? 0 : bg;

    if(cp[ifg][ibg]) {
        return COLOR_PAIR(cp[ifg][ibg]);
    }

    if(next_pair >= COLOR_PAIRS) {
        return 0; /* out of pairs */
    }

    /* init_pair() accepts -1 when use_default_colors() is active */
    if(init_pair((short)next_pair, (short)fg, (short)bg) != OK) {
        return 0;
    }

    cp[ifg][ibg] = next_pair++;
    return COLOR_PAIR(cp[ifg][ibg]);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int get_paint_color(const char *fg_color, const char *bg_color)
{
    int fg = get_color_id(fg_color);
    int bg = get_color_id(bg_color);
    return get_color_pair(fg, bg);
}
