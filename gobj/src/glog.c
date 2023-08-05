/****************************************************************************
 *          glog.c
 *
 *          Glog for Objects G for Yuneta Simplified
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stddef.h>
#include <wchar.h>

#ifdef __linux__
    #include <pwd.h>
    #include <strings.h>
    #include <sys/utsname.h>
    #include <unistd.h>
    #include <execinfo.h>
#endif

#include "ansi_escape_codes.h"
#include "gobj_environment.h"
#include "kwid.h"
#include "gobj.h"
#include "gbuffer.h"

/***************************************************************
 *              Constants
 ***************************************************************/
#define MAX_LOG_HANDLER_TYPES 8

extern void jsonp_free(void *ptr); // TODO remove

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

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void show_backtrace(loghandler_fwrite_fn_t fwrite_fn, void *h);
PRIVATE int stdout_write(void *v, int priority, const char *bf, size_t len);
PRIVATE int stdout_fwrite(void* v, int priority, const char* format, ...);
PRIVATE void _log_bf(int priority, log_opt_t opt, const char *bf, size_t len);

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
PRIVATE BOOL log_exist_handler(const char *handler_name)
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

    if(log_exist_handler(handler_name)) {
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
        json_object_set_new(jn_dict, "handler_options", json_integer(lh->handler_options));

        /*
         *  Next
         */
        lh = dl_next(lh);
    }
    return jn_array;
}

/*****************************************************************
 *      Discover extra data
 *****************************************************************/
PRIVATE void discover(hgobj gobj, json_t *jn)
{
    const char *yuno_attrs[] = {
        "node_uuid",
        "process",
        "hostname",
        "pid"
    };

    for(size_t i=0; i<ARRAY_SIZE(yuno_attrs); i++) {
        const char *attr = yuno_attrs[i];
        if(gobj_has_attr(gobj_yuno(), attr)) { // WARNING Check that attr exists:  avoid recursive loop
            json_t *value = gobj_read_attr(gobj_yuno(), attr, 0);
            if(value) {
                json_object_set(jn, attr, value);
            }
        }
    }

#ifdef ESP_PLATFORM
    #include <esp_system.h>
    size_t size = esp_get_free_heap_size();
    json_object_set_new(jn, "HEAP free", json_integer((json_int_t)size));
#endif

    if(!gobj) {
        return;
    }
    json_object_set_new(jn, "gclass", json_string(gobj_gclass_name(gobj)));
    json_object_set_new(jn, "gobj_name", json_string(gobj_name(gobj)));
    json_object_set_new(jn, "state", json_string(gobj_current_state(gobj)));
    if(trace_with_full_name) {
        json_object_set_new(jn,
            "gobj_full_name",
            json_string(gobj_full_name(gobj))
        );
    }
    if(trace_with_short_name) {
        json_object_set_new(jn,
            "gobj_short_name",
            json_string(gobj_short_name(gobj))
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
                json_object_set_new(jn, attr, json_string(attr));
            }
        }
    }
}

/*****************************************************************
 *  Add key/values from va_list argument
 *****************************************************************/
PRIVATE void json_vappend(json_t *hgen, va_list ap)
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
                                json_object_set_new(hgen, key, json_integer(v));

                            } else {
                                long int v;
                                v = va_arg(ap, long int);
                                json_object_set_new(hgen, key, json_integer(v));
                            }
                        } else {
                            int v;
                            v = va_arg(ap, int);
                            json_object_set_new(hgen, key, json_integer(v));
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
                            json_object_set_new(hgen, key, json_real((double)v));
                        } else {
                            double v;
                            v = va_arg (ap, double);
                            json_object_set_new(hgen, key, json_real(v));
                        }
                        eof = 1;
                        break;
                    case 'c':
                        if (fmt [i - 1] == 'l') {
                            wint_t v = va_arg (ap, wint_t);
                            json_object_set_new(hgen, key, json_integer(v));
                        } else {
                            int v = va_arg (ap, int);
                            json_object_set_new(hgen, key, json_integer(v));
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
                                json_object_set_new(hgen, key, json_string(value));
                            } else {
                                json_object_set_new(hgen, key, json_null());
                            }

                        } else {
                            char *p;
                            int len;

                            p = va_arg (ap, char *);
                            if(p && (len = snprintf(value, sizeof(value), "%s", p))>=0) {
                                if(strcmp(key, "msg")==0) {
                                    gobj_log_set_last_message("%s", value);
                                }
                                json_object_set_new(hgen, key, json_string(value));
                            } else {
                                json_object_set_new(hgen, key, json_null());
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
                            json_object_set_new(hgen, key, json_string(value));
                        } else {
                            json_object_set_new(hgen, key, json_null());
                        }
                    }
                        break;
                    case 'j':
                    {
                        json_t *jn;

                        jn = va_arg (ap, void *);
                        eof = 1;

                        if(jn) {
                            size_t flags = JSON_ENCODE_ANY |
                                           JSON_INDENT(0) |
                                           JSON_REAL_PRECISION(12);
                            char *bf = json_dumps(jn, flags);
                            if(bf) {
                                helper_doublequote2quote(bf);
                                json_object_set_new(hgen, key, json_string(bf));
                                jsonp_free(bf) ;
                            } else {
                                json_object_set_new(hgen, key, json_null());
                            }
                        } else {
                            json_object_set_new(hgen, key, json_null());
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
 *      Log data in transparent format
 *****************************************************************/
PRIVATE void _log_bf(int priority, log_opt_t opt, const char *bf, size_t len)
{
    static char __inside_log__ = 0;

    if(len <= 0) {
        return;
    }

    if(__inside_log__) {
        return;
    }
    __inside_log__ = 1;

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

    __inside_log__ = 0;

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

    static char __inside_log__ = 0;

    if(__inside_log__) {
        return;
    }
    __inside_log__ = 1;

    current_timestamp(timestamp, sizeof(timestamp));
    json_t *jn_log = json_object();
    json_object_set_new(jn_log, "timestamp", json_string(timestamp));
    discover(gobj, jn_log);

    json_vappend(jn_log, ap);

    char *s = json_dumps(jn_log, JSON_INDENT(2)|JSON_ENCODE_ANY);
    _log_bf(priority, opt, s, strlen(s));
    jsonp_free(s);
    json_decref(jn_log);

    __inside_log__ = 0;
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
    static char __inside_log__ = 0;

    if(__inside_log__) {
        return;
    }
    __inside_log__ = 1;

    current_timestamp(timestamp, sizeof(timestamp));
    json_t *jn_log = json_object();
    json_object_set_new(jn_log, "timestamp", json_string(timestamp));
    discover(gobj, jn_log);

    vsnprintf(msg, sizeof(msg), fmt, ap);
    json_object_set_new(jn_log, "msgset", json_string(msgset));
    json_object_set_new(jn_log, "msg", json_string(msg));
    if(jn_data) {
        json_object_set(jn_log, "data", jn_data);
        json_object_set_new(jn_log, "refcount", json_integer(jn_data->refcount));
        json_object_set_new(jn_log, "type", json_integer(jn_data->type));
    }

    char *s = json_dumps(jn_log, JSON_INDENT(2)|JSON_ENCODE_ANY);
    _log_bf(priority, opt, s, strlen(s));
    jsonp_free(s);
    json_decref(jn_log);

    __inside_log__ = 0;
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
