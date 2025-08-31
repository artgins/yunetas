/****************************************************************************
 *              glogger.h
 *
 *              Logger for Objects G for Yuneta Simplified
 *
 *              Copyright (c) 1996-2015 Niyamaka.
 *              Copyright (c) 2024, ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
#pragma once

#include "gtypes.h"
#include "msgsets.h"

#ifdef __cplusplus
extern "C"{
#endif

/**************************************************************
 *       Constants
 **************************************************************/
/*
 *  Options for handlers
 */
typedef enum {
    LOG_HND_OPT_ALERT           = 0x0001,
    LOG_HND_OPT_CRITICAL        = 0x0002,
    LOG_HND_OPT_ERROR           = 0x0004,
    LOG_HND_OPT_WARNING         = 0x0008,
    LOG_HND_OPT_INFO            = 0x0010,
    LOG_HND_OPT_DEBUG           = 0x0020,
    LOG_HND_OPT_AUDIT           = 0x0040, // Set by default in all handler_options:255, be careful
    LOG_HND_OPT_MONITOR         = 0x0080, // Set by default in all handler_options:255, be careful
    LOG_HND_OPT_NODISCOVER      = 0x0100,
    LOG_HND_OPT_NOTIME          = 0x0200,
    LOG_HND_OPT_TRACE_STACK     = 0x0400,
    LOG_HND_OPT_BEAUTIFUL_JSON  = 0x0800,

} log_handler_opt_t;

#define LOG_OPT_UP_ERROR \
(LOG_HND_OPT_ALERT|LOG_HND_OPT_CRITICAL|LOG_HND_OPT_ERROR)

#define LOG_OPT_UP_WARNING \
(LOG_HND_OPT_ALERT|LOG_HND_OPT_CRITICAL|LOG_HND_OPT_ERROR|LOG_HND_OPT_WARNING)

#define LOG_OPT_UP_INFO \
(LOG_HND_OPT_ALERT|LOG_HND_OPT_CRITICAL|LOG_HND_OPT_ERROR|LOG_HND_OPT_WARNING|LOG_HND_OPT_INFO)

#define LOG_OPT_LOGGER \
(LOG_HND_OPT_ALERT|LOG_HND_OPT_CRITICAL|LOG_HND_OPT_ERROR|LOG_HND_OPT_WARNING|LOG_HND_OPT_INFO|LOG_HND_OPT_DEBUG)

#define LOG_OPT_ALL (LOG_OPT_LOGGER|LOG_HND_OPT_AUDIT|LOG_HND_OPT_MONITOR)

/*
 *  Values for log_*() functions
 */
typedef enum {
    LOG_NONE                = 0x0001,   /* Backward compatibility, only log */
    LOG_OPT_EXIT_ZERO       = 0x0002,   /* Exit(0) after log the error or critical */
    LOG_OPT_EXIT_NEGATIVE   = 0x0004,   /* Exit(-1) after log the error or critical */
    LOG_OPT_ABORT           = 0x0008,   /* abort() after log the error or critical */
    LOG_OPT_TRACE_STACK     = 0x0010,   /* Trace stack */
} log_opt_t;

typedef void (*loghandler_close_fn_t)(void *h);
typedef int  (*loghandler_write_fn_t)(void *h, int priority, const char *bf, size_t len);
typedef int  (*loghandler_fwrite_fn_t)(void *h, int priority, const char *format, ...)  JANSSON_ATTRS((format(printf, 3, 4)));
typedef void (*show_backtrace_fn_t)(loghandler_fwrite_fn_t fwrite_fn, void *h);

/**************************************************************
 *       Prototypes
 **************************************************************/
PUBLIC void glog_init(void);
PUBLIC void glog_end(void); // Better you don't call. It's few memory and you will have log all time
/*
 *  log handler "stdout" is included
 */
PUBLIC int gobj_log_register_handler(
    const char *handler_type,
    loghandler_close_fn_t close_fn,
    loghandler_write_fn_t write_fn,     // for log
    loghandler_fwrite_fn_t fwrite_fn    // for backtrace
);
PUBLIC BOOL gobj_log_exist_handler(const char *handler_name);

PUBLIC int gobj_log_add_handler(
    const char *handler_name,
    const char *handler_type,
    log_handler_opt_t handler_options,
    void *h
);
PUBLIC int gobj_log_del_handler(const char *handler_name); // delete all handlers if handle_name is empty
PUBLIC json_t *gobj_log_list_handlers(void);

PUBLIC void _log_bf(int priority, log_opt_t opt, const char *bf, size_t len);

/*
 *  Only for LOG_HND_OPT_BEAUTIFUL_JSON
 */
PUBLIC int gobj_log_set_global_handler_option(
    log_handler_opt_t log_handler_opt,
    BOOL set
);

PUBLIC int stdout_write(void *v, int priority, const char *bf, size_t len);
PUBLIC int stdout_fwrite(void* v, int priority, const char* format, ...);

PUBLIC void gobj_log_alert(hgobj gobj, log_opt_t opt, ...); // WARNING don't put format printf here
PUBLIC void gobj_log_critical(hgobj gobj, log_opt_t opt, ...);
PUBLIC void gobj_log_error(hgobj gobj, log_opt_t opt, ...);
PUBLIC void gobj_log_warning(hgobj gobj, log_opt_t opt, ...);
PUBLIC void gobj_log_info(hgobj gobj, log_opt_t opt, ...);
PUBLIC void gobj_log_debug(hgobj gobj, log_opt_t opt, ...);
PUBLIC const char *gobj_get_log_priority_name(int priority);
PUBLIC json_t *gobj_get_log_data(void);
PUBLIC void gobj_log_clear_counters(void);
PUBLIC const char *gobj_log_last_message(void);
PUBLIC void gobj_log_set_last_message(const char *msg, ...) JANSSON_ATTRS((format(printf, 1, 2)));
PUBLIC void set_show_backtrace_fn(show_backtrace_fn_t show_backtrace_fn);
PUBLIC BOOL set_trace_with_short_name(BOOL trace_with_short_name); // return previous value, default FALSE
PUBLIC BOOL set_trace_with_full_name(BOOL trace_with_full_name); // return previous value, default TRUE
PUBLIC void print_backtrace(void);

PUBLIC void trace_vjson(
    hgobj gobj,
    int priority,
    json_t *jn_data,    // not owned
    const char *msgset,
    const char *fmt,
    va_list ap
);
PUBLIC void gobj_trace_msg(
    hgobj gobj,
    const char *fmt,
    ...
) JANSSON_ATTRS((format(printf, 2, 3)));

PUBLIC int trace_msg0(const char *fmt, ...) JANSSON_ATTRS((format(printf, 1, 2)));

PUBLIC void gobj_info_msg(
    hgobj gobj,
    const char *fmt,
    ...
) JANSSON_ATTRS((format(printf, 2, 3)));

PUBLIC void gobj_trace_json(
    hgobj gobj,
    json_t *jn, // not owned
    const char *fmt,
    ...
) JANSSON_ATTRS((format(printf, 3, 4)));

PUBLIC void gobj_trace_dump(
    hgobj gobj,
    const char *bf,
    size_t len,
    const char *fmt,
    ...
) JANSSON_ATTRS((format(printf, 4, 5)));

PUBLIC void print_error( //  Print ERROR message to stdout and syslog if quit != 0
    pe_flag_t quit,
    const char *fmt,
    ...
) JANSSON_ATTRS((format(printf, 2, 3)));


#ifdef __cplusplus
}
#endif
