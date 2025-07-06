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

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#endif

/***********************************************************************
 *  Macros of switch for strings
 *  copied from https://gist.github.com/HoX/abfe15c40f2d9daebc35

Example:

int main(int argc, char **argv) {
     SWITCHS(argv[1]) {
        CASES("foo")
        CASES("bar")
            printf("foo or bar (case sensitive)\n");
            break;

        ICASES("pi")
            printf("pi or Pi or pI or PI (case insensitive)\n");
            break;

        CASES_RE("^D.*",0)
            printf("Something that start with D (case sensitive)\n");
            break;

        CASES_RE("^E.*",REG_ICASE)
            printf("Something that start with E (case insensitive)\n");
            break;

        CASES("1")
            printf("1\n");

        CASES("2")
            printf("2\n");
            break;

        DEFAULTS
            printf("No match\n");
            break;
    } SWITCHS_END

    return 0;
}
 ***********************************************************************/

/** Begin a switch for the string x */
#define SWITCHS(x) \
    { regmatch_t pmatch[1]; (void)pmatch; const char *__sw = (x); BOOL __done = FALSE; BOOL __cont = FALSE; \
        regex_t __regex; regcomp(&__regex, ".*", 0); do {

/** Check if the string matches the cases argument (case sensitive) */
#define CASES(x)    } if ( __cont || !strcmp ( __sw, x ) ) \
    { __done = TRUE; __cont = TRUE;

/** Check if the string matches the icases argument (case insensitive) */
#define ICASES(x)    } if ( __cont || !strcasecmp ( __sw, x ) ) { \
    __done = TRUE; __cont = TRUE;

/** Check if the string matches the specified regular expression using regcomp(3) */
#define CASES_RE(x,flags) } regfree ( &__regex ); if ( __cont || ( \
  0 == regcomp ( &__regex, x, flags ) && \
  0 == regexec ( &__regex, __sw, ARRAY_SIZE(pmatch), pmatch, 0 ) ) ) { \
    __done = TRUE; __cont = TRUE;

/** Default behaviour */
#define DEFAULTS } if ( !__done || __cont ) {

/** Close the switchs */
#define SWITCHS_END } while ( 0 ); regfree(&__regex); }

/***************************************************************
 *              GClass/GObj types
 ***************************************************************/
typedef const char *gclass_name_t;      /**< unique pointer that exposes gclass names */
typedef const char *gobj_state_t;       /**< unique pointer that exposes states */
typedef const char *gobj_event_t;       /**< unique pointer that exposes events */
typedef const char *gobj_lmethod_t;     /**< unique pointer that exposes local methods */

typedef void *hgclass;      /* handler of a gclass */
typedef void *hgobj;        /* handler of a gobj */

typedef void (*fnfree)(void *);

typedef enum {
    PEF_CONTINUE    = 0,
    PEF_EXIT        = -1,
    PEF_ABORT       = -2,
    PEF_SYSLOG      = -3,
} pe_flag_t;

/***************************************************************
 *  inline functions
 ***************************************************************/
static inline BOOL empty_string(const char *str)
{
    return (str && *str)?0:1;
}

static inline BOOL empty_json(const json_t *jn)
{
    if((json_is_array(jn) && json_array_size(jn)==0) ||
        (json_is_object(jn) && json_object_size(jn)==0) ||
        json_is_null(jn)) {
        return TRUE;
    } else {
        return FALSE;
    }
}

#ifdef __cplusplus
}
#endif
