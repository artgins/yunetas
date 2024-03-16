/****************************************************************************
 *          glog.c
 *
 *          Glog for Objects G for Yuneta Service
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <wchar.h>

#ifdef __linux__
    #include <execinfo.h>
#endif

#ifdef ESP_PLATFORM
    #include <esp_system.h>
#endif

#include "helpers.h"
#include "gobj.h"

extern void jsonp_free(void *ptr);

/***************************************************************
 *              Constants
 ***************************************************************/
#define MAX_LOG_HANDLER_TYPES 8

/***************************************************************
 *              Log Structures
 ***************************************************************/
/*
 *  Syslog priority definitions
 */
#define LOG_EMERG       0       /* system is unusable */
#define LOG_ALERT       1       /* action must be taken immediately */
#define LOG_CRIT        2       /* critical conditions */
#define LOG_ERR         3       /* error conditions */
#define LOG_WARNING     4       /* warning conditions */
#define LOG_NOTICE      5       /* normal but significant condition */
#define LOG_INFO        6       /* informational */
#define LOG_DEBUG       7       /* debug-level messages */

PRIVATE const char *priority_names[]={
    "EMERG",
    "ALERT",
    "CRITICAL",
    "ERROR",
    "WARNING",
    "NOTICE",
    "INFO",
    "DEBUG"
};

PRIVATE const char *log_handler_opt_names[]={
    "LOG_HND_OPT_ALERT",
    "LOG_HND_OPT_CRITICAL",
    "LOG_HND_OPT_ERROR",
    "LOG_HND_OPT_WARNING",
    "LOG_HND_OPT_INFO",
    "LOG_HND_OPT_DEBUG",
    0
};

typedef struct {
    char handler_type[16+1];
    loghandler_close_fn_t close_fn;
    loghandler_write_fn_t write_fn;
    loghandler_fwrite_fn_t fwrite_fn;
} log_reg_t;

typedef struct {
    DL_ITEM_FIELDS

    char *handler_name;
    log_handler_opt_t handler_options;
    log_reg_t *hr;
    void *h;
} log_handler_t;

typedef struct {
    size_t alloc;
    size_t len;
    char *msg;
    char indented;
    int items;
} ul_buffer_t;

typedef int hgen_t;

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void show_backtrace(loghandler_fwrite_fn_t fwrite_fn, void *h);
PRIVATE int stdout_write(void *v, int priority, const char *bf, size_t len);
PRIVATE int stdout_fwrite(void* v, int priority, const char* format, ...);
PRIVATE void _log_bf(int priority, log_opt_t opt, const char *bf, size_t len);
PRIVATE void _log(hgobj gobj, int priority, log_opt_t opt, va_list ap);
PRIVATE void discover(hgobj gobj, hgen_t hgen);

/*****************************************************************
 *          Json Data
 *  Auto-growing string buffers
 *  Copyright (c) 2012 BalaBit IT Security Ltd.
 *  All rights reserved.
 *****************************************************************/
PRIVATE ul_buffer_t *ul_buffer_reset(hgen_t hgen, int indented);
PRIVATE ul_buffer_t *ul_buffer_append(
    hgen_t hgen,
    const char *key,
    const char *value,
    int with_comillas
);
PRIVATE char *ul_buffer_finalize(hgen_t hgen);
PRIVATE void ul_buffer_finish(void);
PRIVATE void json_add_string(hgen_t hgen, const char *key, const char *str);
PRIVATE void json_add_null(hgen_t hgen, const char *key);
PRIVATE void json_add_double(hgen_t hgen, const char *key, double number);
PRIVATE void json_add_integer(hgen_t hgen, const char *key, long long int number);
PRIVATE char *json_get_buf(hgen_t hgen);
PRIVATE void json_vappend(hgen_t hgen, va_list ap);

static const unsigned char json_exceptions[] = {
    0x7f, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86,
    0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e,
    0x8f, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96,
    0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e,
    0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6,
    0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae,
    0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
    0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe,
    0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6,
    0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce,
    0xcf, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6,
    0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde,
    0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6,
    0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee,
    0xef, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6,
    0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe,
    0xff, '\0'
};

/*
 *  Máximum # of json buffer.
 */
PRIVATE ul_buffer_t escape_buffer[2];
PRIVATE ul_buffer_t ul_buffer[2];
PRIVATE char __initialized__ = 0;
PRIVATE volatile char __inside_log__ = 0;

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE char trace_with_short_name = FALSE; // TODO functions to set this variables
PRIVATE char trace_with_full_name = TRUE;  // TODO functions to set this variables

PRIVATE show_backtrace_fn_t show_backtrace_fn = show_backtrace;
PRIVATE dl_list_t dl_log_handlers;
PRIVATE int max_log_register = 0;
PRIVATE log_reg_t log_register[MAX_LOG_HANDLER_TYPES+1];

PRIVATE char last_message[256];
PRIVATE uint32_t __alert_count__ = 0;
PRIVATE uint32_t __critical_count__ = 0;
PRIVATE uint32_t __error_count__ = 0;
PRIVATE uint32_t __warning_count__ = 0;
PRIVATE uint32_t __info_count__ = 0;
PRIVATE uint32_t __debug_count__ = 0;

/****************************************************************************
 *
 ****************************************************************************/
PUBLIC void glog_init(void)
{
    dl_init(&dl_log_handlers);

    /*--------------------------------*
     *  Register included handlers
     *--------------------------------*/
    gobj_log_register_handler(
        "stdout",           // handler_name
        0,                  // close_fn
        stdout_write,       // write_fn
        stdout_fwrite       // fwrite_fn
    );

    __initialized__ = TRUE;
}

/****************************************************************************
 *
 ****************************************************************************/
PUBLIC void glog_end(void)
{
    /*
     *  WARNING Free log handler at the end! to let log the last errors
     */
    log_handler_t *lh;
    while((lh=dl_first(&dl_log_handlers))) {
        gobj_log_del_handler(lh->handler_name);
    }
    max_log_register = 0;

    ul_buffer_finish();

    __initialized__ = FALSE;
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC int gobj_log_register_handler(
    const char* handler_type,
    loghandler_close_fn_t close_fn,
    loghandler_write_fn_t write_fn,
    loghandler_fwrite_fn_t fwrite_fn
) {
    if(max_log_register >= MAX_LOG_HANDLER_TYPES) {
        return -1;
    }
    strncpy(
        log_register[max_log_register].handler_type,
        handler_type,
        sizeof(log_register[0].handler_type) - 1
    );
    log_register[max_log_register].close_fn = close_fn;
    log_register[max_log_register].write_fn = write_fn;
    log_register[max_log_register].fwrite_fn = fwrite_fn;

    max_log_register++;
    return 0;
}

/*****************************************************************
 *  Return if handler exists
 *****************************************************************/
PUBLIC BOOL gobj_log_exist_handler(const char *handler_name)
{
    if(empty_string(handler_name)) {
        return FALSE;
    }
    log_handler_t *lh = dl_first(&dl_log_handlers);
    while(lh) {
        log_handler_t *next = dl_next(lh);
        if(strcmp(lh->handler_name, handler_name)==0) {
            return TRUE;
        }
        /*
         *  Next
         */
        lh = next;
    }
    return FALSE;
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC int gobj_log_add_handler(
    const char *handler_name,
    const char *handler_type,
    log_handler_opt_t handler_options,
    void *h
) {
    if(empty_string(handler_name)) {
        gobj_log_set_last_message("gobj_log_add_handler(): no handler name");
        return -1;
    }

    if(gobj_log_exist_handler(handler_name)) {
        gobj_log_set_last_message("gobj_log_add_handler(): handler name already exists");
        return -1;
    }

    /*-------------------------------------*
     *          Find type
     *-------------------------------------*/
    int type;
    for(type = 0; type < max_log_register; type++) {
        if(strcmp(log_register[type].handler_type, handler_type)==0) {
            break;
        }
    }
    if(type == max_log_register) {
        gobj_log_set_last_message("gobj_log_add_handler(): handler type not found");
        return -1;
    }

    /*-------------------------------------*
     *      Alloc memory
     *  HACK use system memory,
     *  need log handler until the end
     *-------------------------------------*/
    log_handler_t *lh = malloc(sizeof(log_handler_t));
    if(!lh) {
        gobj_log_set_last_message("gobj_log_add_handler(): no memory");
        return -1;
    }
    memset(lh, 0, sizeof(log_handler_t));
    lh->handler_name = strdup(handler_name);
    lh->handler_options = handler_options?handler_options:LOG_OPT_ALL;
    lh->hr = &log_register[type];
    lh->h = h;

    /*----------------*
     *  Add to list
     *----------------*/
    return dl_add(&dl_log_handlers, lh);
}

/*****************************************************************
 *  Return handlers deleted.
 *  Delete all handlers is handle_name is empty
 *****************************************************************/
PUBLIC int gobj_log_del_handler(const char *handler_name)
{
    /*-------------------------------------*
     *      Free memory
     *  HACK use system memory,
     *  need log handler until the end
     *-------------------------------------*/
    BOOL found = FALSE;

    log_handler_t *lh = dl_first(&dl_log_handlers);
    while(lh) {
        log_handler_t *next = dl_next(lh);
        if(empty_string(handler_name) || strcmp(lh->handler_name, handler_name)==0) {
            found = TRUE;
            dl_delete(&dl_log_handlers, lh, 0);
            if(lh->h && lh->hr->close_fn) {
                lh->hr->close_fn(lh->h);
            }
            if(lh->handler_name) {
                free(lh->handler_name);
            }
            free(lh);
        }
        /*
         *  Next
         */
        lh = next;
    }

    if(!found) {
        gobj_log_set_last_message("Handler not found");
        return -1;
    }
    return 0;
}

/*****************************************************************
 *  Return list of handlers
 *****************************************************************/
PUBLIC json_t *gobj_log_list_handlers(void)
{
    json_t *jn_array = json_array();
    log_handler_t *lh = dl_first(&dl_log_handlers);
    while(lh) {
        json_t *jn_dict = json_object();
        json_array_append_new(jn_array, jn_dict);
        json_object_set_new(jn_dict, "handler_name", json_string(lh->handler_name));
        json_object_set_new(jn_dict, "handler_type", json_string(lh->hr->handler_type));
        json_object_set_new(
            jn_dict,
            "handler_options",
            bits2jn_strlist(log_handler_opt_names, lh->handler_options)
        );

        /*
         *  Next
         */
        lh = dl_next(lh);
    }
    return jn_array;
}

/*****************************************************************
 *
 *****************************************************************/
PRIVATE BOOL must_ignore(log_handler_t *lh, int priority)
{
    BOOL ignore = TRUE;
    log_handler_opt_t handler_options = lh->handler_options;

    switch(priority) {
    case LOG_ALERT:
        if(handler_options & LOG_HND_OPT_ALERT)
            ignore = FALSE;
        break;
    case LOG_CRIT:
        if(handler_options & LOG_HND_OPT_CRITICAL)
            ignore = FALSE;
        break;
    case LOG_ERR:
        if(handler_options & LOG_HND_OPT_ERROR)
            ignore = FALSE;
        break;
    case LOG_WARNING:
        if(handler_options & LOG_HND_OPT_WARNING)
            ignore = FALSE;
        break;
    case LOG_INFO:
        if(handler_options & LOG_HND_OPT_INFO)
            ignore = FALSE;
        break;
    case LOG_DEBUG:
        if(handler_options & LOG_HND_OPT_DEBUG)
            ignore = FALSE;
        break;

    default:
        break;
}
return ignore;
}

/*****************************************************************
 *      Log alert
 *****************************************************************/
PUBLIC void gobj_log_alert(hgobj gobj, log_opt_t opt, ...)
{
    int priority = LOG_ALERT;

    __alert_count__++;

    va_list ap;
    va_start(ap, opt);
    _log(gobj, priority, opt, ap);
    va_end(ap);
}

/*****************************************************************
 *      Log critical
 *****************************************************************/
PUBLIC void gobj_log_critical(hgobj gobj, log_opt_t opt, ...)
{
    int priority = LOG_CRIT;

    __critical_count__++;

    va_list ap;
    va_start(ap, opt);
    _log(gobj, priority, opt, ap);
    va_end(ap);
}

/*****************************************************************
 *      Log error
 *****************************************************************/
PUBLIC void gobj_log_error(hgobj gobj, log_opt_t opt, ...)
{
    int priority = LOG_ERR;

    __error_count__++;

    va_list ap;
    va_start(ap, opt);
    _log(gobj, priority, opt, ap);
    va_end(ap);
}

/*****************************************************************
 *      Log warning
 *****************************************************************/
PUBLIC void gobj_log_warning(hgobj gobj, log_opt_t opt, ...)
{
    int priority = LOG_WARNING;

    __warning_count__++;

    va_list ap;
    va_start(ap, opt);
    _log(gobj, priority, opt, ap);
    va_end(ap);
}

/*****************************************************************
 *      Log info
 *****************************************************************/
PUBLIC void gobj_log_info(hgobj gobj, log_opt_t opt, ...)
{
    int priority = LOG_INFO;

    __info_count__++;

    va_list ap;
    va_start(ap, opt);
    _log(gobj, priority, opt, ap);
    va_end(ap);
}

/*****************************************************************
 *      Log debug
 *****************************************************************/
PUBLIC void gobj_log_debug(hgobj gobj, log_opt_t opt, ...)
{
    int priority = LOG_DEBUG;

    __debug_count__++;

    va_list ap;
    va_start(ap, opt);
    _log(gobj, priority, opt, ap);
    va_end(ap);
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC const char *gobj_get_log_priority_name(int priority)
{
    if(priority >= (int)ARRAY_SIZE(priority_names)) {
        return "";
    }
    return priority_names[priority];
}

/*****************************************************************
 *      Clear counters
 *****************************************************************/
PUBLIC json_t *gobj_get_log_data(void)
{
    json_t *jn_logs = json_object();
    json_object_set_new(jn_logs, "debug", json_integer(__debug_count__));
    json_object_set_new(jn_logs, "info", json_integer(__info_count__));
    json_object_set_new(jn_logs, "warning", json_integer(__warning_count__));
    json_object_set_new(jn_logs, "error", json_integer(__error_count__));
    json_object_set_new(jn_logs, "critical", json_integer(__critical_count__));
    json_object_set_new(jn_logs, "alert", json_integer(__alert_count__));
    return jn_logs;
}

/*****************************************************************
 *      Clear counters
 *****************************************************************/
PUBLIC void gobj_log_clear_counters(void)
{
    __debug_count__ = 0;
    __info_count__ = 0;
    __warning_count__ = 0;
    __error_count__ = 0;
    __critical_count__ = 0;
    __alert_count__ = 0;
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC const char *gobj_log_last_message(void)
{
    return last_message;
}

/*****************************************************************
 *      Log alert
 *****************************************************************/
PUBLIC void gobj_log_set_last_message(const char *msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    vsnprintf(last_message, sizeof(last_message), msg, ap);
    va_end(ap);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int stdout_write(void* v, int priority, const char* bf, size_t len)
{
#define BWhite "\033[1;37m"       // White
#define On_Red "\033[41m"         // Red
#define Color_Off "\033[0m"       // Text Reset
#define On_Black "\033[40m"       // Black
#define BYellow "\033[1;33m"      // Yellow

    if(!bf) {
        // silence
        return -1;
    }
    if(len<=0) {
        // silence
        return -1;
    }
    if(priority >= (int)ARRAY_SIZE(priority_names)) {
        priority = LOG_DEBUG;
    }

    if(priority <= LOG_ERR) {
        fwrite(On_Red, strlen(On_Red), 1, stdout);
        fwrite(BWhite, strlen(BWhite), 1, stdout);
        fwrite(priority_names[priority], strlen(priority_names[priority]), 1, stdout);
        fwrite(Color_Off, strlen(Color_Off), 1, stdout);

    } else if(priority <= LOG_WARNING) {
        fwrite(On_Black, strlen(On_Black), 1, stdout);
        fwrite(BYellow, strlen(BYellow), 1, stdout);
        fwrite(priority_names[priority], strlen(priority_names[priority]), 1, stdout);
        fwrite(Color_Off, strlen(Color_Off), 1, stdout);
    } else {
        fwrite(priority_names[priority], strlen(priority_names[priority]), 1, stdout);
    }

    fwrite(": ", strlen(": "), 1, stdout);
    fwrite(bf, len, 1, stdout);
    fwrite("\n", strlen("\n"), 1, stdout);
    fflush(stdout);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int stdout_fwrite(void *v, int priority, const char *fmt, ...)
{
    va_list ap;
    va_list aq;
    int length;
    char *buf;

    va_start(ap, fmt);
    va_copy(aq, ap);

    length = vsnprintf(NULL, 0, fmt, ap);
    if(length>0) {
        buf = malloc((size_t)length + 1);
        if(buf) {
            vsnprintf(buf, (size_t)length + 1, fmt, aq);
            stdout_write(v, priority, buf, strlen(buf));
            free(buf);
        }
    }

    va_end(ap);
    va_end(aq);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void show_backtrace(loghandler_fwrite_fn_t fwrite_fn, void *h)
{
#ifdef __linux__
    void* callstack[128];
    int frames = backtrace(callstack, sizeof(callstack) / sizeof(void*));
    char** symbols = backtrace_symbols(callstack, frames);

    if (symbols == NULL) {
        return;
    }
    fwrite_fn(h, LOG_DEBUG, "===============> begin stack trace <==================");
    for (int i = 0; i < frames; i++) {
        fwrite_fn(h, LOG_DEBUG, "%s", symbols[i]);
    }
    fwrite_fn(h, LOG_DEBUG, "===============> end stack trace <==================\n");
    free(symbols);
#endif
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void set_show_backtrace_fn(show_backtrace_fn_t show_backtrace)
{
    show_backtrace_fn = show_backtrace;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void gobj_trace_msg(hgobj gobj, const char *fmt, ... )
{
    va_list ap;

    va_start(ap, fmt);
    trace_vjson(gobj, 0, "trace_msg", fmt, ap);
    va_end(ap);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void gobj_trace_json(
    hgobj gobj,
    json_t *jn, // now owned
    const char *fmt, ...
) {
    va_list ap;

    va_start(ap, fmt);
    trace_vjson(gobj, jn, "trace_json", fmt, ap);
    va_end(ap);

    if((gobj_trace_level(gobj) & TRACE_GBUFFERS)) {
        gbuffer_t *gbuf = (gbuffer_t *)(size_t)json_integer_value(json_object_get(jn, "gbuffer"));
        if(gbuf) {
            gobj_trace_dump_gbuf(gobj, gbuf, "gbuffer");
        }
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void gobj_trace_buffer(
    hgobj gobj,
    const char *bf,
    size_t len,
    const char *fmt, ...
) {
    va_list ap;

    json_t *jn_data = json_stringn(bf, len);

    va_start(ap, fmt);
    trace_vjson(gobj, jn_data, "trace_buffer", fmt, ap);
    va_end(ap);

    json_decref(jn_data);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void gobj_trace_dump(
    hgobj gobj,
    const char *bf,
    size_t len,
    const char *fmt,
    ...
) {
    va_list ap;

    json_t *jn_data = tdump2json((uint8_t *)bf, len);

    va_start(ap, fmt);
    trace_vjson(gobj, jn_data, "trace_dump", fmt, ap);
    va_end(ap);

    json_decref(jn_data);
}

/***************************************************************************
 *  Print ERROR message to stdout
 ***************************************************************************/
PUBLIC void print_error(
    const char *fmt,
    ...
)
{
    va_list ap;

    fwrite(On_Red, strlen(On_Red), 1, stdout);
    fwrite(BWhite, strlen(BWhite), 1, stdout);

    int length = vsnprintf(NULL, 0, fmt, ap);
    if(length>0) {
        char *buf = malloc((size_t)length + 1);
        if(buf) {
            vsnprintf(buf, (size_t)length + 1, fmt, ap);
            fwrite(buf, strlen(buf), 1, stdout);
            free(buf);
        }
    }
    fwrite(Color_Off, strlen(Color_Off), 1, stdout);
    fprintf(stdout, "\n");
}

/*****************************************************************
 *      Log data in transparent format
 *****************************************************************/
PRIVATE void _log_bf(int priority, log_opt_t opt, const char *bf, size_t len)
{
    if(len <= 0) {
        return;
    }

    BOOL backtrace_showed = FALSE;
    log_handler_t *lh = dl_first(&dl_log_handlers);
    while(lh) {
        if(must_ignore(lh, priority)) {
            /*
             *  Next
             */
            lh = dl_next(lh);
            continue;
        }

        if(lh->hr->write_fn) {
            int ret = (lh->hr->write_fn)(lh->h, priority, bf, len);
            if(ret < 0) { // Handler owns the message
                break;
            }
        }
        if(opt & (LOG_OPT_TRACE_STACK|LOG_OPT_EXIT_NEGATIVE|LOG_OPT_ABORT)) {
            if(!backtrace_showed) {
                if(show_backtrace_fn && lh->hr->fwrite_fn) {
                    show_backtrace_fn(lh->hr->fwrite_fn, lh->h);
                }
            }
            backtrace_showed = TRUE;
        }

        /*
         *  Next
         */
        lh = dl_next(lh);
    }

    if(opt & LOG_OPT_EXIT_NEGATIVE) {
        exit(-1);
    }
    if(opt & LOG_OPT_EXIT_ZERO) {
        exit(0);
    }
    if(opt & LOG_OPT_ABORT) {
        abort();
    }
}

/*****************************************************************
 *      Log
 *****************************************************************/
PRIVATE void _log(hgobj gobj, int priority, log_opt_t opt, va_list ap)
{
    char timestamp[90];

    if(!__initialized__) {
        return;
    }
    if(__inside_log__) {
        return;
    }
    __inside_log__ = 1;

    current_timestamp(timestamp, sizeof(timestamp));

    ul_buffer_reset(0, TRUE);
    json_add_string(0, "timestamp", timestamp);

    discover(gobj, 0);

    json_vappend(0, ap);

    if(opt & LOG_OPT_EXIT_NEGATIVE) {
        json_add_string(0, "exiting", "-1");
    }
    if(opt & LOG_OPT_EXIT_ZERO) {
        json_add_string(0, "exiting", "0");
    }
    if(opt & LOG_OPT_ABORT) {
        json_add_string(0, "exiting", "abort");
    }

    char *s = json_get_buf(0);

    _log_bf(priority, opt, s, strlen(s));

    __inside_log__ = 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void trace_vjson(
    hgobj gobj,
    json_t *jn_data,    // now owned
    const char *msgset,
    const char *fmt,
    va_list ap
) {
    int priority = LOG_DEBUG;
    log_opt_t opt = 0;
    char timestamp[90];
    char msg[256];

    if(!__initialized__) {
        return;
    }
    if(__inside_log__) {
        return;
    }
    __inside_log__ = 1;

    current_timestamp(timestamp, sizeof(timestamp));
    json_t *jn_log = json_object();
    json_object_set_new(jn_log, "timestamp", json_string(timestamp));

    if(gobj_has_attr(gobj, "id")) {
        const char *value = gobj_read_str_attr(gobj, "id");
        if(!empty_string(value)) {
            json_object_set_new(jn_log, "id", json_string(value));
        }
    }

    json_object_set(jn_log, "node_uuid", gobj_read_attr(gobj_yuno(), "node_uuid", gobj));
    json_object_set(jn_log, "process", gobj_read_attr(gobj_yuno(), "process", gobj));
    json_object_set(jn_log, "hostname", gobj_read_attr(gobj_yuno(), "hostname", gobj));

    if(gobj) {
        json_object_set_new(jn_log, "gclass", json_string(gobj_gclass_name(gobj)));
        json_object_set_new(jn_log, "gobj_name", json_string(gobj_name(gobj)));
        json_object_set_new(jn_log, "state", json_string(gobj_current_state(gobj)));
        if(trace_with_full_name) {
            json_object_set_new(jn_log,
                "gobj_full_name",
                json_string(gobj_full_name(gobj))
            );
        }
    }

#ifdef ESP_PLATFORM
    size_t size = esp_get_free_heap_size();
    json_object_set_new(jn_log, "HEAP free", json_integer(size));
#endif
    json_object_set_new(jn_log, "max_system_memory", json_integer(get_max_system_memory()));
    json_object_set_new(jn_log, "cur_system_memory", json_integer(get_cur_system_memory()));

    vsnprintf(msg, sizeof(msg), fmt, ap);
    json_object_set_new(jn_log, "msgset", json_string(msgset));
    json_object_set_new(jn_log, "msg", json_string(msg));
    if(jn_data) {
        json_object_set(jn_log, "data", jn_data);
        json_object_set_new(jn_log, "refcount", json_integer((json_int_t)jn_data->refcount));
        json_object_set_new(jn_log, "type", json_integer(jn_data->type));
    }
    char *s = json_dumps(jn_log, JSON_INDENT(2)|JSON_ENCODE_ANY);
    if(s) {
        _log_bf(priority, opt, s, strlen(s));
        jsonp_free(s);
    } else {
        print_json2(On_Red BWhite "ERROR json_dumps()" Color_Off, jn_log);
        if(show_backtrace_fn) {
            show_backtrace_fn(stdout_fwrite, stdout);
        }
    }
    json_decref(jn_log);

    __inside_log__ = 0;
}

/****************************************************************************
 *  Trace machine function
 ****************************************************************************/
PUBLIC void trace_machine(const char *fmt, ...)
{
    va_list ap;
    char temp1[40];
    char dtemp[90];
    char temp[512];

    if(!__initialized__) {
        return;
    }
    if(__inside_log__) {
        return;
    }
    __inside_log__ = 1;

    tab(temp1, sizeof(temp1));
    current_timestamp(dtemp, sizeof(dtemp));

    snprintf(temp, sizeof(temp),
        "%s -%s",
        dtemp,
        temp1
    );

    va_start(ap, fmt);
    size_t len = strlen(temp);
    vsnprintf(temp+len, sizeof(temp)-len, fmt, ap);
    va_end(ap);

    _log_bf(LOG_DEBUG, 0, temp, strlen(temp));

    __inside_log__ = 0;
}

/*****************************************************************
 *      Discover extra data
 *****************************************************************/
PRIVATE void discover(hgobj gobj, hgen_t hgen)
{
    const char *yuno_attrs[] = {
        "node_uuid",
        "process",
        "hostname",
        "pid"
    };

    for(size_t i=0; i<ARRAY_SIZE(yuno_attrs); i++) {
        const char *attr = yuno_attrs[i];
        if(gobj_has_attr(gobj_yuno(), attr)) { // WARNING Check that attr exists: avoid recursive loop
            json_t *value = gobj_read_attr(gobj_yuno(), attr, 0);
            if(value) {
                if(json_is_integer(value)) {
                    json_add_integer(hgen, attr, json_integer_value(value));
                } else if(json_is_string(value)) {
                    json_add_string(hgen, attr, json_string_value(value));

                } else if(json_is_null(value)) {
                    json_add_null(hgen, attr);
                }
            }
        }
    }

#ifdef ESP_PLATFORM
    size_t size = esp_get_free_heap_size();
    json_add_integer(hgen, "HEAP free", size);
#endif

    json_add_integer(hgen, "max_system_memory", get_max_system_memory());
    json_add_integer(hgen, "cur_system_memory", get_cur_system_memory());

    if(!gobj) {
        return;
    }
    json_add_string(hgen, "gclass", gobj_gclass_name(gobj));
    json_add_string(hgen, "gobj_name", gobj_name(gobj));
    json_add_string(hgen, "state", gobj_current_state(gobj));
    if(trace_with_full_name) {
        json_add_string(hgen,
            "gobj_full_name",
            gobj_full_name(gobj)
        );
    }
    if(trace_with_short_name) {
        json_add_string(hgen,
            "gobj_short_name",
            gobj_short_name(gobj)
        );
    }

    const char *gobj_attrs[] = {
        "id"
    };
    for(size_t i=0; i<ARRAY_SIZE(gobj_attrs); i++) {
        const char *attr = gobj_attrs[i];
        if(gobj_has_attr(gobj, attr)) { // WARNING Check that attr exists:  avoid recursive loop
            const char *value = gobj_read_str_attr(gobj, attr);
            if(!empty_string(value)) {
                json_add_string(hgen, attr, value);
            }
        }
    }
}

/*****************************************************************
 *  Add key/values from va_list argument
 *****************************************************************/
PRIVATE void json_vappend(hgen_t hgen, va_list ap)
{
    char *key;
    char *fmt;
    size_t i;
    char value[256];

    while ((key = (char *)va_arg (ap, char *)) != NULL) {
        fmt = (char *)va_arg (ap, char *);
        for (i = 0; i < strlen (fmt); i++) {
            int eof = 0;

            if (fmt[i] != '%')
                continue;
            i++;
            while (eof != 1) {
                switch (fmt[i]) {
                    case 'd':
                    case 'i':
                    case 'o':
                    case 'u':
                    case 'x':
                    case 'X':
                        if (fmt[i - 1] == 'l') {
                            if (i - 2 > 0 && fmt[i - 2] == 'l') {
                                long long int v;
                                v = va_arg(ap, long long int);
                                json_add_integer(hgen, key, v);

                            } else {
                                long int v;
                                v = va_arg(ap, long int);
                                json_add_integer(hgen, key, v);
                            }
                        } else {
                            int v;
                            v = va_arg(ap, int);
                            json_add_integer(hgen, key, v);
                        }
                        eof = 1;
                        break;
                    case 'e':
                    case 'E':
                    case 'f':
                    case 'F':
                    case 'g':
                    case 'G':
                    case 'a':
                    case 'A':
                        if (fmt[i - 1] == 'L') {
                            long double v;
                            v = va_arg (ap, long double);
                            json_add_double(hgen, key, v);
                        } else {
                            double v;
                            v = va_arg (ap, double);
                            json_add_double(hgen, key, v);
                        }
                        eof = 1;
                        break;
                    case 'c':
                        if (fmt [i - 1] == 'l') {
                            wint_t v = va_arg (ap, wint_t);
                            json_add_integer(hgen, key, v);
                        } else {
                            int v = va_arg (ap, int);
                            json_add_integer(hgen, key, v);
                        }
                        eof = 1;
                        break;
                    case 's':
                        if (fmt [i - 1] == 'l') {
                            wchar_t *p;
                            int len;

                            p = va_arg (ap, wchar_t *);
                            if(p && (len = snprintf(value, sizeof(value), "%ls", p))>=0) {
                                if(strcmp(key, "msg")==0) {
                                    gobj_log_set_last_message("%s", value);
                                }
                                json_add_string(hgen, key, value);
                            } else {
                                json_add_null(hgen, key);
                            }

                        } else {
                            char *p;
                            int len;

                            p = va_arg (ap, char *);
                            if(p && (len = snprintf(value, sizeof(value), "%s", p))>=0) {
                                if(strcmp(key, "msg")==0) {
                                    gobj_log_set_last_message("%s", value);
                                }
                                json_add_string(hgen, key, value);
                            } else {
                                json_add_null(hgen, key);
                            }
                        }
                        eof = 1;
                        break;
                    case 'p':
                        {
                            void *p;
                            int len;

                            p = va_arg (ap, void *);
                            eof = 1;
                            if(p && (len = snprintf(value, sizeof(value), "%p", (char *)p))>0) {
                                json_add_string(hgen, key, value);
                            } else {
                                json_add_null(hgen, key);
                            }
                        }
                        break;
                    case 'j':
                        {
                            json_t *jn;

                            jn = va_arg (ap, void *);
                            eof = 1;

                            if(jn) {
                                size_t flags = JSON_ENCODE_ANY | JSON_COMPACT |
                                               JSON_INDENT(0) |
                                               JSON_REAL_PRECISION(12);
                                char *bf = json_dumps(jn, flags);
                                if(bf) {
                                    helper_doublequote2quote(bf);
                                    json_add_string(0, key, bf);
                                    jsonp_free(bf) ;
                                } else {
                                    json_add_null(hgen, key);
                                }
                            } else {
                                json_add_null(hgen, key);
                            }
                        }
                        break;
                    case '%':
                        eof = 1;
                        break;
                    default:
                        i++;
                }
            }
        }
    }
}




                    /*---------------------------------*
                     *      json buffer functions
                     *---------------------------------*/




/*****************************************************************
 *
 *****************************************************************/
PRIVATE void ul_buffer_finish(void)
{
    for(size_t i=0; i<ARRAY_SIZE(escape_buffer); i++) {
        if(escape_buffer[i].msg) {
            free(escape_buffer[i].msg);
            escape_buffer[i].msg = 0;
        }
        if(ul_buffer[i].msg) {
            free(ul_buffer[i].msg);
            ul_buffer[i].msg = 0;
        }
    }
}

/*****************************************************************
 *
 *****************************************************************/
PRIVATE char *_ul_str_escape(
    const char *str,
    char *dest,
    size_t * length)
{
    const unsigned char *p;
    char *q;
    static unsigned char exmap[256];
    static int exmap_inited = 0;

    if (!str)
        return NULL;

    p = (unsigned char *) str;
    q = dest;

    if (!exmap_inited) {
        const unsigned char *e = json_exceptions;

        memset(exmap, 0, 256);
        while (*e) {
            exmap[*e] = 1;
            e++;
        }
        exmap_inited = 1;
    }

    while (*p) {
        if (exmap[*p])
            *q++ = *p;
        else {
            switch (*p) {
            case '\b':
                *q++ = '\\';
                *q++ = 'b';
                break;
            case '\f':
                *q++ = '\\';
                *q++ = 'f';
                break;
            case '\n':
                *q++ = '\\';
                *q++ = 'n';
                break;
            case '\r':
                *q++ = '\\';
                *q++ = 'r';
                break;
            case '\t':
                *q++ = '\\';
                *q++ = 't';
                break;
            case '\\':
                *q++ = '\\';
                *q++ = '\\';
                break;
            case '"':
                *q++ = '\\';
                *q++ = '"';
                break;
            default:
                if ((*p < ' ') || (*p >= 0177)) {
                    const char *json_hex_chars = "0123456789abcdef";

                    *q++ = '\\';
                    *q++ = 'u';
                    *q++ = '0';
                    *q++ = '0';
                    *q++ = json_hex_chars[(*p) >> 4];
                    *q++ = json_hex_chars[(*p) & 0xf];
                } else
                    *q++ = *p;
                break;
            }
        }
        p++;
    }

    *q = 0;
    if (length)
        *length = q - dest;
    return dest;
}

/*****************************************************************
 *
 *****************************************************************/
PRIVATE inline ul_buffer_t *_ul_buffer_ensure_size(
    ul_buffer_t * buffer, size_t size)
{
    if (buffer->alloc < size) {
        buffer->alloc += size * 2;
        char *msg = realloc(buffer->msg, buffer->alloc);
        if (!msg) {
            return NULL;
        }
        buffer->msg = msg;
    }
    return buffer;
}

/*****************************************************************
 *
 *****************************************************************/
PRIVATE ul_buffer_t *ul_buffer_reset(hgen_t hgen, int indented)
{
    ul_buffer_t *buffer = &ul_buffer[hgen];
    _ul_buffer_ensure_size(buffer, 512);
    _ul_buffer_ensure_size(&escape_buffer[hgen], 256);
    if(indented) {
        memcpy(buffer->msg, "{\n", 2);
        buffer->len = 2;
    } else {
        memcpy(buffer->msg, "{", 1);
        buffer->len = 1;
    }
    buffer->indented = (char)indented;
    buffer->items = 0;
    return buffer;
}

/*****************************************************************
 *
 *****************************************************************/
PRIVATE ul_buffer_t *ul_buffer_append(hgen_t hgen,
    const char *key,
    const char *value,
    int with_comillas)
{
    char *k, *v;
    size_t lk, lv;
    ul_buffer_t * buffer = &ul_buffer[hgen];
    size_t orig_len = buffer->len;

    /* Append the key to the buffer */
    escape_buffer[hgen].len = 0;
    _ul_buffer_ensure_size(&escape_buffer[hgen], strlen(key) * 6 + 1);
    k = _ul_str_escape(key, escape_buffer[hgen].msg, &lk);
    if (!k) {
        return NULL;
    }

    buffer = _ul_buffer_ensure_size(buffer, buffer->len + lk + 10);
    if (!buffer)
        return NULL;

    if(buffer->items > 0) {
        memcpy(buffer->msg + buffer->len, ",", 1);
        buffer->len += 1;
        if(buffer->indented) {
            memcpy(buffer->msg + buffer->len, "\n", 1);
            buffer->len += 1;
        } else {
            memcpy(buffer->msg + buffer->len, " ", 1);
            buffer->len += 1;
        }
    }
    if(buffer->indented) {
        memcpy(buffer->msg + buffer->len, "    \"", 5);
        buffer->len += 5;
    } else {
        memcpy(buffer->msg + buffer->len, "\"", 1);
        buffer->len += 1;
    }

    memcpy(buffer->msg + buffer->len, k, lk);
    buffer->len += lk;
    if(with_comillas) {
        memcpy(buffer->msg + buffer->len, "\": \"", 4);
        buffer->len += 4;
    } else {
        memcpy(buffer->msg + buffer->len, "\": ", 3);
        buffer->len += 3;
    }

    /* Append the value to the buffer */
    escape_buffer[hgen].len = 0;
    _ul_buffer_ensure_size(&escape_buffer[hgen], strlen(value) * 6 + 1);
    v = _ul_str_escape(value, escape_buffer[hgen].msg, &lv);
    if (!v) {
        buffer->len = orig_len;
        return NULL;
    }

    buffer = _ul_buffer_ensure_size(buffer, buffer->len + lv + 6);
    if (!buffer) {
        // PVS-Studio
        // buffer->len = orig_len;
        return NULL;
    }

    memcpy(buffer->msg + buffer->len, v, lv);
    buffer->len += lv;
    if(with_comillas) {
        memcpy(buffer->msg + buffer->len, "\"", 1);
        buffer->len += 1;
    }
    buffer->items++;
    return buffer;
}

/*****************************************************************
 *
 *****************************************************************/
PRIVATE char *ul_buffer_finalize(hgen_t hgen)
{
    ul_buffer_t *buffer = &ul_buffer[hgen];
    if (!_ul_buffer_ensure_size(buffer, buffer->len + 3)) {
        return NULL;
    }
    if(buffer->indented)
        buffer->msg[buffer->len++] = '\n';
    else
        buffer->msg[buffer->len++] = ' ';
    buffer->msg[buffer->len++] = '}';
    buffer->msg[buffer->len] = '\0';
    return buffer->msg;
}

/*****************************************************************
 *
 *****************************************************************/
PRIVATE char * json_get_buf(hgen_t hgen)
{
    return ul_buffer_finalize (hgen);
}

/*****************************************************************
 *
 *****************************************************************/
PRIVATE void json_add_string(hgen_t hgen, const char *key, const char *str)
{
    if(empty_string(key) || !str) {
        return;
    }
    ul_buffer_append(hgen, key, str, 1);
}

/*****************************************************************
 *
 *****************************************************************/
PRIVATE void json_add_null(hgen_t hgen, const char *key)
{
    if(empty_string(key)) {
        return;
    }
    ul_buffer_append(hgen, key, "null", 0);
}

/*****************************************************************
 *
 *****************************************************************/
PRIVATE void json_add_double(hgen_t hgen, const char *key, double number)
{
    char temp[64];

    if(empty_string(key)) {
        return;
    }
    snprintf(temp, sizeof(temp), "%.20g", number);
    if (strspn(temp, "0123456789-") == strlen(temp)) {
        strcat(temp, ".0");
    }
    ul_buffer_append(hgen, key, temp, 0);
}

/*****************************************************************
 *
 *****************************************************************/
PRIVATE void json_add_integer(hgen_t hgen, const char *key, json_int_t number)
{
    char temp[64];

    if(empty_string(key)) {
        return;
    }
    snprintf(temp, sizeof(temp), "%"JSON_INTEGER_FORMAT, number);
    ul_buffer_append(hgen, key, temp, 0);
}
