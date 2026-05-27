/***********************************************************************
 *          mime_encoder.c
 *          RFC 5322 + MIME builder for c_smtp_session.
 *
 *          See mime_encoder.h for the supported feature matrix.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <limits.h>

#include "mime_encoder.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define BOUNDARY_PREFIX     "=_yuneta_"

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE BOOL has_8bit_chars(const char *s, size_t len);
PRIVATE void build_boundary(char *out, size_t out_size);
PRIVATE void build_message_id(char *out, size_t out_size);
PRIVATE void build_date_header(char *out, size_t out_size);
PRIVATE int append_header(gbuffer_t *gbuf, const char *name, const char *value);
PRIVATE int append_header_maybe_encoded(gbuffer_t *gbuf, const char *name, const char *value);
PRIVATE int append_from_header(gbuffer_t *gbuf, const char *from, const char *from_beautiful);
PRIVATE int append_text_part(
    gbuffer_t *gbuf,
    const char *body,
    size_t body_len,
    BOOL is_html
);
PRIVATE int append_attachment_part(
    hgobj gobj,
    gbuffer_t *gbuf,
    const char *path,
    const char *inline_file_id
);
PRIVATE int append_base64_wrapped(gbuffer_t *gbuf, const char *src, size_t src_len);
PRIVATE const char *basename_of(const char *path);

/***************************************************************************
 *              Local helpers
 ***************************************************************************/

/***************************************************************************
 *  TRUE if any byte has the high bit set — used to decide whether a part
 *  needs base64 (we don't ship a QP encoder).
 ***************************************************************************/
PRIVATE BOOL has_8bit_chars(const char *s, size_t len)
{
    for(size_t i = 0; i < len; i++) {
        if((unsigned char)s[i] >= 0x80) {
            return TRUE;
        }
    }
    return FALSE;
}

/***************************************************************************
 *  Generate a MIME multipart boundary unlikely to collide with body bytes.
 *  RFC 2046 §5.1.1 allows up to 70 chars; we use the conventional shape
 *  "=_yuneta_<timestamp>_<rand>".
 ***************************************************************************/
PRIVATE void build_boundary(char *out, size_t out_size)
{
    snprintf(out, out_size, "%s%ld_%ld",
        BOUNDARY_PREFIX,
        (long)time(NULL),
        (long)random()
    );
}

/***************************************************************************
 *  RFC 5322 Message-ID: <timestamp.rand@host>.
 ***************************************************************************/
PRIVATE void build_message_id(char *out, size_t out_size)
{
    struct utsname uts;
    if(uname(&uts) != 0) {
        snprintf(uts.nodename, sizeof(uts.nodename), "localhost");
    }
    snprintf(out, out_size, "<%ld.%ld@%s>",
        (long)time(NULL),
        (long)random(),
        uts.nodename
    );
}

/***************************************************************************
 *  RFC 5322 Date header value, GMT.
 ***************************************************************************/
PRIVATE void build_date_header(char *out, size_t out_size)
{
    time_t now = time(NULL);
    struct tm tm;
    gmtime_r(&now, &tm);
    strftime(out, out_size, "%a, %d %b %Y %H:%M:%S +0000", &tm);
}

/***************************************************************************
 *  Append "<name>: <value>\r\n" to gbuf. Returns 0 on success, -1 on
 *  gbuffer overflow (already logged).
 ***************************************************************************/
PRIVATE int append_header(gbuffer_t *gbuf, const char *name, const char *value)
{
    gbuffer_append(gbuf, (void *)name, strlen(name));
    gbuffer_append(gbuf, ": ", 2);
    gbuffer_append(gbuf, (void *)value, strlen(value));
    if(gbuffer_append(gbuf, "\r\n", 2) != 2) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "gbuffer_append() FAILED (header line)",
            "name",         "%s", name,
            NULL
        );
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  Append a header whose value may be non-ASCII; if so, wrap it in an
 *  RFC 2047 encoded-word (base64 over UTF-8). Pure-ASCII values pass through.
 ***************************************************************************/
PRIVATE int append_header_maybe_encoded(
    gbuffer_t *gbuf,
    const char *name,
    const char *value
)
{
    size_t vlen = strlen(value);
    if(!has_8bit_chars(value, vlen)) {
        return append_header(gbuf, name, value);
    }

    gbuffer_t *b64 = gbuffer_binary_to_base64(value, vlen);
    if(!b64) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "base64 encode FAILED for header",
            "name",         "%s", name,
            NULL
        );
        return -1;
    }
    gbuffer_append(gbuf, (void *)name, strlen(name));
    gbuffer_append(gbuf, ": =?UTF-8?B?", sizeof(": =?UTF-8?B?") - 1);
    gbuffer_append(gbuf,
        gbuffer_cur_rd_pointer(b64),
        gbuffer_leftbytes(b64)
    );
    if(gbuffer_append(gbuf, "?=\r\n", 4) != 4) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "gbuffer_append() FAILED (encoded header tail)",
            "name",         "%s", name,
            NULL
        );
        GBUFFER_DECREF(b64)
        return -1;
    }
    GBUFFER_DECREF(b64)
    return 0;
}

/***************************************************************************
 *  From: header. If a display name is set, render as `"Name" <addr>` with
 *  the name encoded if non-ASCII.
 ***************************************************************************/
PRIVATE int append_from_header(
    gbuffer_t *gbuf,
    const char *from,
    const char *from_beautiful
)
{
    if(empty_string(from_beautiful)) {
        return append_header(gbuf, "From", from);
    }
    char value[LINE_MAX];
    /*
     *  Crude composition: name without quoting (RFC 5322 §3.4 allows
     *  unquoted display-name only for atoms; quoting is safer but the
     *  source value is operator-controlled). Wrap as encoded-word if 8-bit.
     */
    if(has_8bit_chars(from_beautiful, strlen(from_beautiful))) {
        gbuffer_t *b64 = gbuffer_binary_to_base64(
            from_beautiful, strlen(from_beautiful)
        );
        if(!b64) {
            gobj_log_error(0, 0,
                "function", "%s", __FUNCTION__,
                "msgset",   "%s", MSGSET_INTERNAL,
                "msg",      "%s", "base64 encode FAILED for From display-name",
                NULL
            );
            return -1;
        }
        snprintf(value, sizeof(value), "=?UTF-8?B?%.*s?= <%s>",
            (int)gbuffer_leftbytes(b64),
            (char *)gbuffer_cur_rd_pointer(b64),
            from
        );
        GBUFFER_DECREF(b64)
    } else {
        snprintf(value, sizeof(value), "%s <%s>", from_beautiful, from);
    }
    return append_header(gbuf, "From", value);
}

/***************************************************************************
 *  Append the body part headers + body data. If the body is 8-bit, encode
 *  the whole part with base64; otherwise emit it as 7bit (CRLF lines are
 *  expected to be already correct in the input — we do not normalise).
 ***************************************************************************/
PRIVATE int append_text_part(
    gbuffer_t *gbuf,
    const char *body,
    size_t body_len,
    BOOL is_html
)
{
    const char *ctype = is_html
        ? "Content-Type: text/html; charset=UTF-8\r\n"
        : "Content-Type: text/plain; charset=UTF-8\r\n";
    gbuffer_append(gbuf, (void *)ctype, strlen(ctype));

    BOOL needs_base64 = body && body_len > 0 && has_8bit_chars(body, body_len);
    if(needs_base64) {
        const char *cte = "Content-Transfer-Encoding: base64\r\n\r\n";
        gbuffer_append(gbuf, (void *)cte, strlen(cte));
        return append_base64_wrapped(gbuf, body, body_len);
    }

    const char *cte = "Content-Transfer-Encoding: 7bit\r\n\r\n";
    gbuffer_append(gbuf, (void *)cte, strlen(cte));
    if(body && body_len > 0) {
        gbuffer_append(gbuf, (void *)body, body_len);
        /* Ensure the part ends with a CRLF so the next boundary lands on
         * its own line. */
        if(body[body_len - 1] != '\n') {
            gbuffer_append(gbuf, "\r\n", 2);
        }
    }
    return 0;
}

/***************************************************************************
 *  Read a file in full and append a MIME part for it:
 *      Content-Type: application/octet-stream; name="..."
 *      Content-Disposition: attachment|inline; filename="..."
 *      Content-Transfer-Encoding: base64
 *      Content-ID: <...>            (only when inline_file_id is set)
 *
 *      <wrapped base64 body>
 ***************************************************************************/
PRIVATE int append_attachment_part(
    hgobj gobj,
    gbuffer_t *gbuf,
    const char *path,
    const char *inline_file_id
)
{
    FILE *f = fopen(path, "rb");
    if(!f) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "fopen() FAILED for attachment",
            "path",         "%s", path,
            NULL
        );
        return -1;
    }
    if(fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "fseek() FAILED",
            "path",         "%s", path,
            NULL
        );
        return -1;
    }
    long sz = ftell(f);
    if(sz < 0) {
        fclose(f);
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "ftell() FAILED",
            "path",         "%s", path,
            NULL
        );
        return -1;
    }
    rewind(f);
    char *buf = gbmem_malloc((size_t)sz);
    if(!buf) {
        fclose(f);
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "gbmem_malloc() FAILED for attachment",
            "size",         "%ld", sz,
            NULL
        );
        return -1;
    }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if(got != (size_t)sz) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "fread() short read for attachment",
            "got",          "%d", (int)got,
            "expected",     "%ld", sz,
            NULL
        );
        gbmem_free(buf);
        return -1;
    }

    const char *fname = basename_of(path);
    char header_line[LINE_MAX];

    snprintf(header_line, sizeof(header_line),
        "Content-Type: application/octet-stream; name=\"%s\"\r\n", fname);
    gbuffer_append(gbuf, header_line, strlen(header_line));

    if(empty_string(inline_file_id)) {
        snprintf(header_line, sizeof(header_line),
            "Content-Disposition: attachment; filename=\"%s\"\r\n", fname);
    } else {
        snprintf(header_line, sizeof(header_line),
            "Content-Disposition: inline; filename=\"%s\"\r\n", fname);
    }
    gbuffer_append(gbuf, header_line, strlen(header_line));

    if(!empty_string(inline_file_id)) {
        snprintf(header_line, sizeof(header_line),
            "Content-ID: <%s>\r\n", inline_file_id);
        gbuffer_append(gbuf, header_line, strlen(header_line));
    }

    const char *cte = "Content-Transfer-Encoding: base64\r\n\r\n";
    gbuffer_append(gbuf, (void *)cte, strlen(cte));

    int ret = append_base64_wrapped(gbuf, buf, (size_t)sz);
    gbmem_free(buf);
    return ret;
}

/***************************************************************************
 *  Wrap base64 at 76 chars per RFC 2045 §6.8.
 ***************************************************************************/
PRIVATE int append_base64_wrapped(gbuffer_t *gbuf, const char *src, size_t src_len)
{
    if(src_len == 0) {
        gbuffer_append(gbuf, "\r\n", 2);
        return 0;
    }
    gbuffer_t *b64 = gbuffer_binary_to_base64(src, src_len);
    if(!b64) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "gbuffer_binary_to_base64() FAILED",
            "src_len",      "%d", (int)src_len,
            NULL
        );
        return -1;
    }
    const char *p = gbuffer_cur_rd_pointer(b64);
    size_t remaining = gbuffer_leftbytes(b64);
    while(remaining > 0) {
        size_t chunk = remaining > 76 ? 76 : remaining;
        gbuffer_append(gbuf, (void *)p, chunk);
        gbuffer_append(gbuf, "\r\n", 2);
        p += chunk;
        remaining -= chunk;
    }
    GBUFFER_DECREF(b64)
    return 0;
}

/***************************************************************************
 *  Last path segment (no allocation, returns pointer into input).
 ***************************************************************************/
PRIVATE const char *basename_of(const char *path)
{
    const char *slash = strrchr(path, '/');
    if(slash) {
        return slash + 1;
    }
    return path;
}




                    /***************************
                     *      Public API
                     ***************************/




/***************************************************************************
 *  Compose the full RFC 5322 message:
 *
 *      Top-level layout depends on the presence of an attachment:
 *
 *          (no attachment)
 *              <headers>
 *              Content-Type: text/(plain|html); charset=UTF-8
 *              <body>
 *
 *          (attachment, no inline_file_id)
 *              <headers>
 *              Content-Type: multipart/mixed; boundary="B"
 *              --B
 *              <text part>
 *              --B
 *              <attachment part>
 *              --B--
 *
 *          (attachment, with inline_file_id)
 *              <headers>
 *              Content-Type: multipart/related; boundary="B"
 *              --B
 *              <text part>
 *              --B
 *              <attachment part with Content-ID>
 *              --B--
 *
 *  The body buffer is sized for the worst case (text part as base64 ≈ 4/3
 *  of body, attachment as base64 ≈ 4/3 of file size + a small header
 *  overhead). The gbuffer max-size cap is conservative.
 ***************************************************************************/
PUBLIC gbuffer_t *mime_build_message(hgobj gobj, const mime_email_t *m)
{
    if(!m || empty_string(m->from)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "mime_email_t missing required 'from'",
            NULL
        );
        return NULL;
    }

    size_t body_len = m->body ? gbuffer_leftbytes(m->body) : 0;
    const char *body_data = m->body ? gbuffer_cur_rd_pointer(m->body) : "";

    /*
     *  Worst-case sizing. We never go past max_size; gbuffer grows lazily
     *  from data_size up to max_size on append.
     */
    size_t attach_size_estimate = 0;
    if(!empty_string(m->attachment)) {
        struct stat st;
        if(stat(m->attachment, &st) == 0) {
            attach_size_estimate = (size_t)st.st_size;
        }
    }
    size_t cap = 8192
        + body_len * 2
        + attach_size_estimate * 2
        + 1024;
    size_t initial = body_len + 4096 < cap ? body_len + 4096 : cap;

    gbuffer_t *out = gbuffer_create(initial, cap);
    if(!out) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "gbuffer_create() FAILED for mime body",
            "cap",          "%d", (int)cap,
            NULL
        );
        return NULL;
    }

    /*--------------------------------*
     *      Headers (RFC 5322)
     *--------------------------------*/
    char date_value[128];
    build_date_header(date_value, sizeof(date_value));
    if(append_header(out, "Date", date_value) < 0) {
        /* Error already logged */
        GBUFFER_DECREF(out)
        return NULL;
    }

    char msgid_value[256];
    build_message_id(msgid_value, sizeof(msgid_value));
    append_header(out, "Message-ID", msgid_value);

    if(append_from_header(out, m->from, m->from_beautiful) < 0) {
        /* Error already logged */
        GBUFFER_DECREF(out)
        return NULL;
    }

    if(!empty_string(m->to)) {
        append_header(out, "To", m->to);
    }
    if(!empty_string(m->cc)) {
        append_header(out, "Cc", m->cc);
    }
    if(!empty_string(m->reply_to)) {
        append_header(out, "Reply-To", m->reply_to);
    }
    if(!empty_string(m->subject)) {
        append_header_maybe_encoded(out, "Subject", m->subject);
    }
    append_header(out, "MIME-Version", "1.0");

    /*--------------------------------*
     *      Body
     *--------------------------------*/
    if(empty_string(m->attachment)) {
        /* Single part: just text/plain or text/html in the top body. */
        if(append_text_part(out, body_data, body_len, m->is_html) < 0) {
            /* Error already logged */
            GBUFFER_DECREF(out)
            return NULL;
        }
        return out;
    }

    /* Attachment present → multipart. mixed for plain attachments, related
     * for inline images so an HTML body can reference them by cid. */
    const char *top_type = empty_string(m->inline_file_id)
        ? "multipart/mixed"
        : "multipart/related";

    char boundary[80];
    build_boundary(boundary, sizeof(boundary));

    char ct_line[160];
    snprintf(ct_line, sizeof(ct_line),
        "Content-Type: %s; boundary=\"%s\"\r\n\r\n", top_type, boundary);
    gbuffer_append(out, ct_line, strlen(ct_line));

    /* Part 1: the text/html body. */
    char delim[100];
    snprintf(delim, sizeof(delim), "--%s\r\n", boundary);
    gbuffer_append(out, delim, strlen(delim));
    if(append_text_part(out, body_data, body_len, m->is_html) < 0) {
        /* Error already logged */
        GBUFFER_DECREF(out)
        return NULL;
    }

    /* Part 2: the attachment. */
    gbuffer_append(out, delim, strlen(delim));
    if(append_attachment_part(gobj, out, m->attachment, m->inline_file_id) < 0) {
        /* Error already logged */
        GBUFFER_DECREF(out)
        return NULL;
    }

    /* Closing boundary. */
    snprintf(delim, sizeof(delim), "--%s--\r\n", boundary);
    gbuffer_append(out, delim, strlen(delim));

    return out;
}
