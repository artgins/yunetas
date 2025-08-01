/***********************************************************************
 *          C_CURL.C
 *          GClass of CURL.

Example Usage

int main(void) {
    const char *html =
        "<html><body>"
        "<h1>Hello!</h1>"
        "<p>Fast path skips related MIME if no <img> or disabled:</p>"
        "<img src=\"logo.png\">"
        "</body></html>";

    // Inline detection enabled, will build multipart/related
    send_email(
        "smtp://smtp.gmail.com:587",
        "your_email@gmail.com",
        "recipient@example.com",
        "Test inline images present",
        "Plain text fallback",
        html,
        "file1.pdf; file2.txt",
        NULL, NULL,
        "ExampleOAuth2Token",
        1,  // strict TLS
        1   // auto_inline_images enabled
    );

    // Inline detection disabled, fast path (no multipart/related)
    send_email(
        "smtp://smtp.gmail.com:587",
        "your_email@gmail.com",
        "recipient@example.com",
        "Test inline detection disabled",
        "Plain text fallback",
        html,
        "file1.pdf; file2.txt",
        NULL, NULL,
        "ExampleOAuth2Token",
        1,  // strict TLS
        0   // auto_inline_images disabled
    );

    return 0;
}

 *          Copyright (c) 2014 by Niyamaka.
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include "c_curl.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define MAX_RETRIES      5
#define INITIAL_BACKOFF  2   // seconds

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Data
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name----------------flag--------default-description----------*/

SDATA (DTP_POINTER,     "user_data",        0,          0,      "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,          0,      "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,          0,      "subscriber of output-events. Default if null is parent."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_CURL    = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"curl",         "Trace curl"},
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj timer;
} PRIVATE_DATA;


/***************************************************************************
 *              Prototypes
 ***************************************************************************/




            /***************************
             *      Framework Methods
             ***************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */

    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
}

/***************************************************************************
 *      Framework Method start - return nonstart flag
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);


    return 0;
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE gbuffer_t *build_email(
    hgobj gobj,
    gbuffer_t *gbuf, // not owned
    const char *from,
    const char *from_beautiful,
    const char *to,
    const char *cc,
    const char *reply_to,
    const char *subject,
    BOOL is_html,
    const char *attachment,
    const char *inline_file_id
)
{
    // return new_gbuf;
}

/***************************************************************************
 *      Progress callback
 ***************************************************************************/
PRIVATE int progress_callback(
    void *clientp,
    curl_off_t dltotal, curl_off_t dlnow,
    curl_off_t ultotal, curl_off_t ulnow
) {
    if(ultotal > 0) {
        double percent = ((double)ulnow / (double)ultotal) * 100.0;
        fprintf(stderr, "\rUpload progress: %.2f%%", percent);
        fflush(stderr);
    }
    return 0;
}

/***************************************************************************
 *      Tokenize generic string
 ***************************************************************************/
PRIVATE char **parse_list_string(const char *input, size_t *count)
{
    *count = 0;
    if(!input) return NULL;

    char **list = NULL;
    size_t cap = 0;

    char *copy = strdup(input);
    char *token = strtok(copy, " ,;");
    while(token) {
        if(*count >= cap) {
            cap = cap ? cap * 2 : 4;
            list = realloc(list, cap * sizeof(char*));
        }
        list[*count] = strdup(token);
        (*count)++;
        token = strtok(NULL, " ,;");
    }
    free(copy);
    return list;
}

/***************************************************************************
 *      Tokenize recipients (curl_slist)
 ***************************************************************************/
PRIVATE struct curl_slist *parse_recipients(const char *recipients_str)
{
    struct curl_slist *list = NULL;
    if(!recipients_str) return NULL;

    char *copy = strdup(recipients_str);
    char *token = strtok(copy, " ,;");
    while(token) {
        list = curl_slist_append(list, token);
        token = strtok(NULL, " ,;");
    }
    free(copy);
    return list;
}

/***************************************************************************
 *  Auto-detect <img src="..."> in HTML
 ***************************************************************************/
PRIVATE char *process_html_for_inline_images(
    const char *html,
    char ***out_images,
    size_t *out_count
) {
    *out_images = NULL;
    *out_count = 0;
    if(!html) return NULL;

    char *modified_html = strdup(html);
    const char *p = html;
    size_t capacity = 0;

    while((p = strstr(p, "<img")) != NULL) {
        const char *src_attr = strstr(p, "src=");
        if(!src_attr) { p += 4; continue; }
        const char *quote = strchr(src_attr, '"');
        if(!quote) { p += 4; continue; }
        const char *end_quote = strchr(quote + 1, '"');
        if(!end_quote) { p += 4; continue; }

        size_t len = end_quote - (quote + 1);
        char *path = strndup(quote + 1, len);

        int exists = 0;
        for(size_t i = 0; i < *out_count; i++) {
            if(strcmp((*out_images)[i], path) == 0) { exists = 1; break; }
        }
        if(!exists) {
            if(*out_count >= capacity) {
                capacity = capacity ? capacity * 2 : 4;
                *out_images = realloc(*out_images, capacity * sizeof(char*));
            }
            (*out_images)[(*out_count)++] = path;
        } else {
            free(path);
        }
        p = end_quote + 1;
    }

    for(size_t i = 0; i < *out_count; i++) {
        char old_tag[512], new_tag[512];
        snprintf(old_tag, sizeof(old_tag), "src=\"%s\"", (*out_images)[i]);
        snprintf(new_tag, sizeof(new_tag), "src=\"cid:inline-img-%zu@local\"", i);

        char *pos;
        while((pos = strstr(modified_html, old_tag)) != NULL) {
            size_t prefix_len = pos - modified_html;
            size_t suffix_len = strlen(pos + strlen(old_tag));
            char *new_html = malloc(prefix_len + strlen(new_tag) + suffix_len + 1);
            memcpy(new_html, modified_html, prefix_len);
            strcpy(new_html + prefix_len, new_tag);
            strcpy(new_html + prefix_len + strlen(new_tag), pos + strlen(old_tag));
            free(modified_html);
            modified_html = new_html;
        }
    }
    return modified_html;
}

/***************************************************************************
 *      Cleanup helper
 ***************************************************************************/
PRIVATE void free_string_list(char ***list, size_t *count)
{
    if(list && *list) {
        for(size_t i = 0; i < *count; i++) {
            if((*list)[i]) {
                free((*list)[i]);
                (*list)[i] = NULL;
            }
        }
        free(*list);
        *list = NULL;
        *count = 0;
    }
}

/***************************************************************************
 * Send email with:
 *   - Plain text + HTML body
 *   - Optional auto inline image detection & Content-ID injection
 *   - Fast path (skip multipart/related) when no inline images
 *   - Attachments from one string ("file1; file2, file3")
 *   - Recipients parsed from one string
 *   - OAuth2 or username/password authentication
 *   - TLS strict/relaxed
 *   - Retry on transient errors
 *   - Progress callback
 *
 * @return 0 on success, -1 on error
 *      Cleanup helper
 ***************************************************************************/
PRIVATE int send_email(
    const char *smtp_url,
    const char *from,
    const char *recipients,
    const char *subject,
    const char *text_body,
    const char *html_body,
    const char *attachments_str,
    const char *username,
    const char *password,
    const char *oauth2_token,
    int strict_tls,
    int auto_inline_images
) {
    CURL *curl = NULL;
    CURLcode res = CURLE_OK;
    struct curl_slist *rcpt_list = NULL;
    struct curl_slist *headers = NULL;
    curl_mime *mime = NULL, *alt = NULL, *related = NULL;
    curl_mimepart *part;
    char subject_header[1024];
    int attempt = 0;
    int ret = -1;

    char **inline_images = NULL;
    size_t inline_count = 0;
    char *modified_html = NULL;

    char **attachments = NULL;
    size_t attach_count = 0;

    /*--- Parse inputs ---*/
    rcpt_list = parse_recipients(recipients);
    if(!rcpt_list) {
        fprintf(stderr, "No valid recipients found\n");
        goto cleanup;
    }

    if(auto_inline_images) {
        modified_html = process_html_for_inline_images(html_body, &inline_images, &inline_count);
    } else {
        modified_html = html_body ? strdup(html_body) : NULL;
    }

    attachments = parse_list_string(attachments_str, &attach_count);

retry_send:
    curl = curl_easy_init();
    if(!curl) {
        fprintf(stderr, "curl_easy_init() failed\n");
        goto cleanup;
    }

    curl_easy_setopt(curl, CURLOPT_URL, smtp_url);
    curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, strict_tls ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, strict_tls ? 2L : 0L);

    if(oauth2_token && *oauth2_token) {
        curl_easy_setopt(curl, CURLOPT_USERNAME, from);
        curl_easy_setopt(curl, CURLOPT_XOAUTH2_BEARER, oauth2_token);
    } else if(username && password) {
        curl_easy_setopt(curl, CURLOPT_USERNAME, username);
        curl_easy_setopt(curl, CURLOPT_PASSWORD, password);
    }

    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, from);
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, rcpt_list);

    mime = curl_mime_init(curl);

    /*--- multipart/alternative (always present) ---*/
    alt = curl_mime_init(curl);
    if(text_body) {
        part = curl_mime_addpart(alt);
        curl_mime_data(part, text_body, CURL_ZERO_TERMINATED);
        curl_mime_type(part, "text/plain; charset=utf-8");
    }
    if(modified_html) {
        part = curl_mime_addpart(alt);
        curl_mime_data(part, modified_html, CURL_ZERO_TERMINATED);
        curl_mime_type(part, "text/html; charset=utf-8");
    }

    /*--- Choose structure ---*/
    if(auto_inline_images && inline_count > 0) {
        /* multipart/related -> inline images */
        related = curl_mime_init(curl);
        part = curl_mime_addpart(related);
        curl_mime_subparts(part, alt);
        curl_mime_type(part, "multipart/alternative");

        for(size_t i = 0; i < inline_count; i++) {
            char cid[256];
            snprintf(cid, sizeof(cid), "inline-img-%zu@local", i);

            part = curl_mime_addpart(related);
            curl_mime_filedata(part, inline_images[i]);
            curl_mime_encoder(part, "base64");
            curl_mime_filename(part, inline_images[i]);

            char hdr[256];
            snprintf(hdr, sizeof(hdr), "Content-ID: <%s>", cid);
            struct curl_slist *cid_header = NULL;
            cid_header = curl_slist_append(cid_header, hdr);
            curl_mime_headers(part, cid_header, 1);
        }

        part = curl_mime_addpart(mime);
        curl_mime_subparts(part, related);
        curl_mime_type(part, "multipart/related");
    } else {
        /* simpler: just multipart/alternative */
        part = curl_mime_addpart(mime);
        curl_mime_subparts(part, alt);
        curl_mime_type(part, "multipart/alternative");
    }

    /*--- Regular attachments ---*/
    for(size_t i = 0; i < attach_count; i++) {
        part = curl_mime_addpart(mime);
        curl_mime_filedata(part, attachments[i]);
        curl_mime_encoder(part, "base64");
    }

    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

    snprintf(subject_header, sizeof(subject_header), "Subject: %s", subject);
    headers = curl_slist_append(headers, subject_header);
    headers = curl_slist_append(headers, "MIME-Version: 1.0");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);

    res = curl_easy_perform(curl);

    if(res != CURLE_OK) {
        long response_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        fprintf(stderr, "\nSend attempt %d failed: %s (SMTP code %ld)\n",
                attempt + 1, curl_easy_strerror(res), response_code);

        if(attempt < MAX_RETRIES && (response_code == 421 || response_code == 450)) {
            int backoff = INITIAL_BACKOFF << attempt;
            fprintf(stderr, "Retrying in %d seconds...\n", backoff);
            sleep(backoff);
            if(mime) { curl_mime_free(mime); mime = NULL; }
            if(headers) { curl_slist_free_all(headers); headers = NULL; }
            if(curl) { curl_easy_cleanup(curl); curl = NULL; }
            attempt++;
            goto retry_send;
        } else {
            goto cleanup;
        }
    }

    ret = 0;  // success

cleanup:
    if(mime) { curl_mime_free(mime); mime = NULL; }
    if(headers) { curl_slist_free_all(headers); headers = NULL; }
    if(curl) { curl_easy_cleanup(curl); curl = NULL; }
    if(rcpt_list) { curl_slist_free_all(rcpt_list); rcpt_list = NULL; }
    if(modified_html) { free(modified_html); modified_html = NULL; }
    free_string_list(&inline_images, &inline_count);
    free_string_list(&attachments, &attach_count);

    fprintf(stderr, "\n");
    return ret;
}



            /***************************
             *      Actions
             ***************************/



/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_command(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_trace_json(gobj, kw, "curl command");

    const char *url = kw_get_str(gobj, kw, "url", "", 0);
    const char *where = kw_get_str(gobj, kw, "where", "", 0);
    const char *username = kw_get_str(gobj, kw, "username", "", 0);
    const char *password = kw_get_str(gobj, kw, "password", "", 0);
    const char *command = kw_get_str(gobj, kw, "command", "", 0);
    const char *dst_event = kw_get_str(gobj, kw, "dst_event", "", 0);
    void * user_reference = (void *)(uintptr_t)kw_get_int(gobj, kw, "user_reference", 0, 0);

    JSON_DECREF(kw);
    return 0;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/

/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create      = mt_create,
    .mt_destroy     = mt_destroy,
    .mt_start       = mt_start,
    .mt_stop        = mt_stop,
    .mt_writing     = mt_writing
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_CURL);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_CURL_COMMAND);
GOBJ_DEFINE_EVENT(EV_CURL_RESPONSE);

/***************************************************************************
 *          Create the GClass
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL_ERROR,
            "msg",      "%s", "GClass ALREADY created",
            "gclass",   "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*------------------------*
     *      States
     *------------------------*/
    ev_action_t st_idle[] = {
        {EV_CURL_COMMAND,   ac_command,      0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_CURL_COMMAND,   0},
        {EV_CURL_RESPONSE,  EVF_OUTPUT_EVENT},
        {NULL, 0}
    };

    /*----------------------------------------*
     *          Register GClass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0, // lmt,
        tattr_desc,
        sizeof(PRIVATE_DATA),
        0,                  // acl
        0,                  // command table
        s_user_trace_level,
        gcflag_manual_start
    );
    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************************
 *              Local methods table
 ***************************************************************************/
PRIVATE LMETHOD lmt[] = {
    {0, 0, 0}
};

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC int register_c_curl(void)
{
    return create_gclass(C_CURL);
}
