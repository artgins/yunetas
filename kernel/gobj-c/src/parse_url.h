/****************************************************************************
 *          parse_url.h
 *
 *          Parse url
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include "00_http_parser.h"
#include "gobj.h"

#ifdef __cplusplus
extern "C"{
#endif


/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int parse_url(
    hgobj gobj,
    const char *uri,
    char *schema, size_t schema_size,
    char *host, size_t host_size,
    char *port, size_t port_size,
    char *path, size_t path_size,
    char *query, size_t query_size,
    BOOL no_schema // only host:port
);

/**rst**
   Get the schema of an url
**rst**/
PUBLIC int get_url_schema(
    hgobj gobj,
    const char *uri,
    char *schema, size_t schema_size
);

#ifdef __cplusplus
}
#endif
