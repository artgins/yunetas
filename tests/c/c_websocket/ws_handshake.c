/****************************************************************************
 *          WS_HANDSHAKE.C
 *
 *          The Sec-WebSocket-Accept a raw server of these tests answers:
 *          the SHA-1 of the key and the GUID of RFC 6455, in base64.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>

#include "sha1.h"
#include "ws_handshake.h"

/***************************************************************************
 *  The accept of the key of the request
 ***************************************************************************/
PUBLIC int ws_accept_of_request(const char *request, size_t len, char *accept, size_t size)
{
    static const char field[] = "Sec-WebSocket-Key: ";
    static const char magic[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    *accept = 0;
    const char *key = memmem(request, len, field, strlen(field));
    const char *end = key? memmem(key, len - (size_t)(key - request), "\r\n", 2) : NULL;
    if(!end) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The upgrade request has no Sec-WebSocket-Key",
            NULL
        );
        return -1;
    }
    key += strlen(field);

    char concat[128];
    snprintf(concat, sizeof(concat), "%.*s%s", (int)(end - key), key, magic);
    uint8_t digest[SHA1_DIGEST_SIZE];
    sha1_digest(concat, strlen(concat), digest);

    gbuffer_t *gbuf = gbuffer_binary_to_base64((const char *)digest, sizeof(digest));
    if(!gbuf) {
        return -1;  // Error already logged
    }
    snprintf(accept, size, "%.*s", (int)gbuffer_leftbytes(gbuf), (char *)gbuffer_cur_rd_pointer(gbuf));
    gbuffer_decref(gbuf);
    return 0;
}
