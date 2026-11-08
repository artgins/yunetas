/****************************************************************************
 *          WS_HANDSHAKE.H
 *
 *          The raw servers of these tests answer the upgrade of a
 *          C_WEBSOCKET client by hand, and the client checks the
 *          Sec-WebSocket-Accept: this is the one due to its request.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Prototypes
 ***************************************************************/
/*
 *  The Sec-WebSocket-Accept due to the Sec-WebSocket-Key of `request`, in
 *  `accept`, with its NUL: 0, or -1 (logged) when the request has no key.
 */
PUBLIC int ws_accept_of_request(
    const char *request,
    size_t len,
    char *accept,
    size_t size
);


#ifdef __cplusplus
}
#endif
