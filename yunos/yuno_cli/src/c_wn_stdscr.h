/****************************************************************************
 *          C_WN_STDSCR.H
 *          Wn_stdscr GClass.
 *
 *          Windows Framework with ncurses
 *
 *          - El framework mantiene el arbol de ventanas.
 *          - El framework recibe los input de los interfaces externos (teclado, raton, ...):
 *              - se queda con los eventos propios del framework:
 *                  - posicionar delante, cambiar de size o posicion, etc
 *                  - como tiene la estrutura de ventanas, sabe en qué ventana han "pinchado"
 *                  - reenviar los eventos que no se quede a la ventana con cursor.
 *                  - mantiene la ventana con cursor.
 *
 *          - Las ventanas que tienen que decir algo lo publican con un evento.
 *
 *          - El creador del framework es el subscriber de los eventos publicados por sus ventanas.
 *          - Podrían haber varios arboles de ventanas en el framework, y cada uno con un subscritor diferente.
 *          - En una ventana modal se "pausan" todos los arboles menos el modal.
 *              Los eventos a ventanas en "pausa" se ignoran, y sólo se procesan los de la ventana modal.
 *              Esto se puede conseguir con el "play" "pause", o quizá mejor con estados,
 *              así el ignorar eventos en el estado "DISABLED" es automático.
 *              Con estados tampoco se procesarían los propios del framework (EV_SIZE, EV_MOVE,...)
 *              que es el funcionamiento que se persigue.
 *              Al final un hibrido: uso play/pause y estos metodos cambian al estado IDLE/DISABLED
 *              Este api lo tienen que implemetar todas los gobj win.
 *
 *          La subcripción de eventos es el equivalente al WinProc de Windows.
 *
 *          The goal of wn_stdscr is detect changes in stdscr size
 *          and inform of them to all his childs
 *
 *          Copyright (c) 2015-2016 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yuneta.h>

/**rst**

.. _wn_stdscr-gclass:

**"Gwin"** :ref:`GClass`
===========================

Description
===========

Windows with ncurses

Events
======

Input Events
------------

Order
^^^^^

Request
^^^^^^^

Output Events
-------------

Response
^^^^^^^^

Unsolicited
^^^^^^^^^^^

Macros
======

``GCLASS_WN_STDSCR_NAME``
   Macro of the gclass string name, i.e **"Gwin"**.

``GCLASS_WN_STDSCR``
   Macro of the :func:`gclass_wn_stdscr()` function.


**rst**/

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************************
 *              Structures
 ***************************************************************************/
// typedef struct _GRECT {
//     int x;
//     int y;
//     int cx;
//     int cy;
// } GRECT;
//
// typedef struct _GPOINT {
//     int x;
//     int y;
// } GPOINT;
//
// typedef struct _GLINE {
//     GPOINT p1;  /* punto inicial de la l¡nea */
//     GPOINT p2;  /* punto final (incluido) de la l¡nea */
// } GLINE;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/

/**rst**
   Return a pointer to the :ref:`GCLASS` struct defining the :ref:`wn_stdscr-gclass`.
**rst**/
PUBLIC GCLASS *gclass_wn_stdscr(void);

#define GCLASS_WN_STDSCR_NAME "Wn_stdscr"
#define GCLASS_WN_STDSCR gclass_wn_stdscr()

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

PUBLIC int _get_curses_color(const char *fg_color, const char *bg_color);

PUBLIC int SetDefaultFocus(hgobj gobj);
PUBLIC int SetFocus(hgobj gobj);
PUBLIC hgobj GetFocus(void);
PUBLIC int SetTextColor(hgobj gobj, const char *color);
PUBLIC int SetBkColor(hgobj gobj, const char *color);
PUBLIC int DrawText(hgobj gobj, int x, int y, const char *s);

/*
 *  Se pueden habilitar/deshabilitar ventanas hijo (button, edit, etc)
 *  Y también ventanas padre (las hijas directas de wn_stdscr).
 *  El manejo de las ventas modales se consigue creando una nueva ventana padre,
 *  y deshabilitando el resto de ventanas padre de wn_stdscr.
 *  La habilitación se expande por todos los hijos.
 */
PUBLIC int EnableWindow(hgobj gobj, BOOL enable);

#ifdef __cplusplus
}
#endif
