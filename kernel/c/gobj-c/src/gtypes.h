/****************************************************************************
 *              gtypes.h
 *
 *              General and common definitions
 *
 *              Copyright (c) 1996-2015 Niyamaka.
 *              Copyright (c) 2024, ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <jansson.h>
#include <yuneta_config.h>      // don't remove, to create dependency
#include <yuneta_version.h>     // don't remove, to create dependency

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Constants
 ***************************************************************/
#ifndef BOOL
# define BOOL   int
#endif

#ifndef FALSE
# define FALSE  0
#endif

#ifndef TRUE
# define TRUE   1
#endif

#define PRIVATE static
#define PUBLIC

#ifndef ESP_PLATFORM
#define IRAM_ATTR
#endif

#ifndef MIN
# define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
# define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

#define EXEC_AND_RESET(function, variable) if(variable) {function((void *)(variable)); (variable)=0;}

/*
 * ARRAY_SIZE - get the number of elements in a visible array
 *  <at> x: the array whose size you want.
 *
 * This does not work on pointers, or arrays declared as [], or
 * function parameters.  With correct compiler support, such usage
 * will cause a build error (see the build_assert_or_zero macro).
 */
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#endif

/***************************************************************
 *              GClass/GObj types
 ***************************************************************/
typedef const char *gclass_name_t;      /**< unique pointer that exposes gclass names */
typedef const char *gobj_state_t;       /**< unique pointer that exposes states */
typedef const char *gobj_event_t;       /**< unique pointer that exposes events */
typedef const char *gobj_lmethod_t;     /**< unique pointer that exposes local methods */

typedef void *hgclass;      /* handler of a gclass */
typedef void *hgobj;        /* handler of a gobj */

typedef enum {
    PEF_CONTINUE    = 0,
    PEF_EXIT        = -1,
    PEF_ABORT       = -2,
    PEF_SYSLOG      = -3,
} pe_flag_t;

#ifdef __cplusplus
}
#endif
