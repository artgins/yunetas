/****************************************************************************
 *          mime_encoder.h
 *          Build a complete RFC 5322 + MIME message ready for SMTP DATA.
 *
 *          Pure helpers, no gclass. The single public entry point is
 *          mime_build_message(); everything else is file-local.
 *
 *          Coverage (matches what cmd_send_email exposes today):
 *              - text/plain or text/html body (UTF-8)
 *              - one optional attachment (application/octet-stream)
 *              - optional Content-ID on the attachment so an HTML body can
 *                reference it as <img src="cid:..."> (multipart/related)
 *              - RFC 2047 base64 word encoding for non-ASCII headers
 *                (Subject, display-name in From)
 *
 *          Out of scope (deliberately):
 *              - multipart/alternative (text + html together)
 *              - multiple attachments
 *              - auto-scan of HTML for <img> rewrites
 *              - QP body encoding (we use base64 when 8-bit, 7bit otherwise)
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C"{
#endif

typedef struct mime_email_s {
    const char *from;               /* envelope + From: address (required) */
    const char *from_beautiful;     /* optional display name, e.g. "ArtGins" */
    const char *to;                 /* To: header value (comma-separated) */
    const char *cc;                 /* optional Cc: header */
    const char *reply_to;           /* optional Reply-To: */
    const char *subject;            /* may contain UTF-8 */
    BOOL is_html;                   /* TRUE -> text/html, FALSE -> text/plain */
    gbuffer_t *body;                /* body bytes (not owned, may be NULL/empty) */
    const char *attachment;         /* optional absolute path to a file */
    const char *inline_file_id;     /* optional Content-ID for the attachment */
} mime_email_t;

/**
 *  Build the full RFC 5322 message. Returns a new gbuffer owned by the
 *  caller (gbuffer_decref to free), or NULL on failure (already logged).
 *
 *  The result is CRLF-terminated and does NOT include the SMTP "\r\n.\r\n"
 *  terminator — c_smtp_session adds it after dot-stuffing.
 */
PUBLIC gbuffer_t *mime_build_message(hgobj gobj, const mime_email_t *m);

#ifdef __cplusplus
}
#endif
