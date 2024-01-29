/****************************************************************************
 *          parse_url.c
 *
 *          Parse url
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include "parse_url.h"

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int parse_url(
    hgobj gobj,
    const char *uri,
    char *schema, size_t schema_size,
    char *host, size_t host_size,
    char *port, size_t port_size,
    char *path, size_t path_size,
    char *query, size_t query_size,
    BOOL no_schema // only host:port
)
{
    struct http_parser_url u;
    http_parser_url_init(&u);

    if(host) host[0] = 0;
    if(schema) schema[0] = 0;
    if(port) port[0] = 0;
    if(path) path[0] = 0;
    if(query) query[0] = 0;

    int result = http_parser_parse_url(uri, strlen(uri), no_schema, &u);
    if (result != 0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_INTERNAL_ERROR,
            "msg",                  "%s", "http_parser_parse_url() FAILED",
            "url",                  "%s", uri,
            NULL
        );
        return -1;
    }

    size_t ln;

    /*
     *  Schema
     */
    if(schema && schema_size > 0) {
        if(!no_schema) {
            ln = u.field_data[UF_SCHEMA].len;
            if(ln > 0) {
                if(ln >= schema_size) {
                    ln = schema_size - 1;
                }
                memcpy(schema, uri + u.field_data[UF_SCHEMA].off, ln);
                schema[ln] = 0;
            }
        }
    }

    /*
     *  Host
     */
    if(host && host_size > 0) {
        ln = u.field_data[UF_HOST].len;
        if(ln > 0) {
            if(ln >= host_size) {
                ln = host_size - 1;
            }
            memcpy(host, uri + u.field_data[UF_HOST].off, ln);
            host[ln] = 0;
        }
    }

    /*
     *  Port
     */
    if(port && port_size > 0) {
        ln = u.field_data[UF_PORT].len;
        if(ln > 0) {
            if(ln >= port_size) {
                ln = port_size - 1;
            }
            memcpy(port, uri + u.field_data[UF_PORT].off, ln);
            port[ln] = 0;
        }
    }

    /*
     *  Path
     */
    if(path && path_size > 0) {
        ln = u.field_data[UF_PATH].len;
        if(ln > 0) {
            if(ln >= path_size) {
                ln = path_size - 1;
            }
            memcpy(path, uri + u.field_data[UF_PATH].off, ln);
            path[ln] = 0;
        }
    }

    /*
     *  Query
     */
    if(query && query_size > 0) {
        ln = u.field_data[UF_QUERY].len;
        if(ln > 0) {
            if(ln >= query_size) {
                ln = query_size - 1;
            }
            memcpy(query, uri + u.field_data[UF_QUERY].off, ln);
            query[ln] = 0;
        }
    }

    return 0;
}

/***************************************************************************
   Get the schema of an url
 ***************************************************************************/
PUBLIC int get_url_schema(
    hgobj gobj,
    const char *uri,
    char *schema, size_t schema_size
) {
    struct http_parser_url u;
    http_parser_url_init(&u);

    if(schema) schema[0] = 0;

    int result = http_parser_parse_url(uri, strlen(uri), FALSE, &u);
    if (result != 0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "http_parser_parse_url() FAILED",
            "url",          "%s", uri,
            NULL
        );
        return -1;
    }

    size_t ln;

    /*
     *  Schema
     */
    if(schema && schema_size > 0) {
        ln = u.field_data[UF_SCHEMA].len;
        if(ln > 0) {
            if(ln >= schema_size) {
                ln = schema_size - 1;
            }
            memcpy(schema, uri + u.field_data[UF_SCHEMA].off, ln);
            schema[ln] = 0;
            return 0;
        }
    }
    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL_ERROR,
        "msg",          "%s", "no schema found",
        "url",          "%s", uri,
        NULL
    );
    return -1;
}
