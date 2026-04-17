/***********************************************************************
 *          MBEDTLS.C
 *
 *          Mbed-TLS specific code for the TLS/SSL layer
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 *
Flow of encrypted & unencrypted bytes
-------------------------------------

In mbedTLS, the data flow can be implemented similarly using a pair of buffers
to simulate the behavior of OpenSSL's BIO layers.
Instead of BIO, mbedTLS allows you to use the mbedtls_ssl_set_bio function
to set custom read and write callbacks.
Here's how the schema translates to mbedTLS:

  +------+                                      +-----+
  |......|--> read(fd) --> custom_recv_cb()  -->|.....|--> mbedtls_ssl_read()  --> IN
  |......|                                      |.....|
  |.sock.|                                      |.SSL.|
  |......|                                      |.....|
  |......|<-- write(fd) <-- custom_send_cb() <--|.....|<-- mbedtls_ssl_write() <-- OUT
  +------+                                      +-----+

          |                                    |       |                     |
          |<---------------------------------->|       |<------------------->|
          |         encrypted bytes            |       |  unencrypted bytes  |


    Custom Callbacks (custom_recv_cb, custom_send_cb):
        These simulate the role of BIO in OpenSSL. The custom_recv_cb reads encrypted bytes (from a socket or another source) and feeds them into mbedTLS. The custom_send_cb takes encrypted data produced by mbedTLS and writes it to a socket or other transport layer.

    Encrypted Data Flow:
        The custom_recv_cb reads data from the socket and feeds it to mbedtls_ssl_read, which decrypts it.
        mbedtls_ssl_write encrypts data and passes it to custom_send_cb for transmission.

    Unencrypted Data Flow:
        Unencrypted application data is passed to mbedtls_ssl_write for encryption and transmission.
        Decrypted data is received from mbedtls_ssl_read and passed to the application.


***********************************************************************/
#include <yuneta_config.h>

#ifdef CONFIG_HAVE_MBEDTLS

#include <mbedtls/ssl.h>
#include <mbedtls/error.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/pk.h>
#include <mbedtls/debug.h>  /* mbedtls_debug_set_threshold() */
#include <psa/crypto.h>     /* psa_crypto_init() required by mbedtls v4.0 */

#include <kwid.h>
#include "../ytls.h"
#include "mbedtls.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Structures
 ***************************************************************/
/*
 * Refcounted TLS material shared between the live ytls_t "current" pointer
 * and any sskt_t that was created while this state was current. This lets us
 * hot-reload certificates without tearing down live connections: we build a
 * fresh state, swap it into ytls->state, and decref the old one — live
 * sessions keep the old state alive via their own ref until they close.
 */
typedef struct mbedtls_state_s {
    int refcount;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cert;
    mbedtls_pk_context pkey;
    mbedtls_x509_crt ca_cert;
    BOOL has_own_cert;
    BOOL has_ca_cert;
} mbedtls_state_t;

typedef struct ytls_s {
    api_tls_t *api_tls;     // Must be the first item
    BOOL server;
    mbedtls_state_t *state; // current TLS state (ytls owns one ref)

    // mbedtls_ctr_drbg_context ctr_drbg;
    // mbedtls_entropy_context entropy;

    BOOL trace_tls;
    size_t rx_buffer_size;
    hgobj gobj;
    char ssl_server_name[256]; // Server name for SNI (client-side TLS only)
} ytls_t;

typedef struct sskt_s {
    ytls_t *ytls;
    mbedtls_state_t *state_ref; // pinned state for this session (owns one ref)
    mbedtls_ssl_context ssl; // mbedTLS SSL context
    BOOL handshake_informed;
    int (*on_handshake_done_cb)(void *user_data, int error);
    int (*on_clear_data_cb)(void *user_data, gbuffer_t *gbuf);
    int (*on_encrypted_data_cb)(void *user_data, gbuffer_t *gbuf);
    void *user_data;
    char last_error[256];
    int error;
    char rx_bf[16*1024];
    gbuffer_t *encrypted_buffer; // Receives encrypted bytes from the network (recv path)
    gbuffer_t *output_buffer;    // Accumulates encrypted bytes from send_callback (send path)
    BOOL *alive; // Points to stack var in flush_clear_data; set to FALSE when freed mid-callback
} sskt_t;

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE int flush_clear_data(sskt_t *sskt);
PRIVATE void mbedtls_debug_callback(
    void *ctx,
    int level,
    const char *file,
    int line,
    const char *str
);

/***************************************************************
 *              Api
 ***************************************************************/
PRIVATE hytls init(
    hgobj gobj,
    json_t *jn_config,  // not owned
    BOOL server
);
PRIVATE void cleanup(hytls ytls);
PRIVATE int reload_certificates(hytls ytls, json_t *jn_config);
PRIVATE const char *version(hytls ytls);
PRIVATE hsskt new_secure_filter(
    hytls ytls,
    int (*on_handshake_done_cb)(void *user_data, int error),
    int (*on_clear_data_cb)(void *user_data, gbuffer_t *gbuf),
    int (*on_encrypted_data_cb)(void *user_data, gbuffer_t *gbuf),
    void *user_data
);
PRIVATE void shutdown_sskt(hsskt sskt);
PRIVATE void free_secure_filter(hsskt sskt);
PRIVATE int do_handshake(hsskt sskt);
PRIVATE int flush_encrypted_data(sskt_t *sskt);
PRIVATE int encrypt_data(hsskt sskt, gbuffer_t *gbuf);
PRIVATE int decrypt_data(hsskt sskt, gbuffer_t *gbuf);
PRIVATE const char *last_error(hsskt sskt);
PRIVATE void set_trace(hsskt sskt, BOOL set);
PRIVATE int flush(hsskt sskt);

PRIVATE api_tls_t api_tls = {
    "MBEDTLS",
    init,
    cleanup,
    reload_certificates,
    version,
    new_secure_filter,
    free_secure_filter,
    do_handshake,
    encrypt_data,
    decrypt_data,
    last_error,
    set_trace,
    flush,
    shutdown_sskt
};

/***************************************************************
 *              Data
 ***************************************************************/

/***************************************************************************
 *  Refcount helpers for mbedtls_state_t
 ***************************************************************************/
PRIVATE mbedtls_state_t *state_incref(mbedtls_state_t *s)
{
    if(s) {
        s->refcount++;
    }
    return s;
}

PRIVATE void state_decref(mbedtls_state_t *s)
{
    if(!s) {
        return;
    }
    s->refcount--;
    if(s->refcount <= 0) {
        mbedtls_ssl_config_free(&s->conf);
        mbedtls_x509_crt_free(&s->cert);
        mbedtls_pk_free(&s->pkey);
        mbedtls_x509_crt_free(&s->ca_cert);
        GBMEM_FREE(s);
    }
}

/***************************************************************************
 *  Build a fully-configured mbedtls_state_t from jn_config.
 *  Returns a new state with refcount=1 on success, or NULL on failure.
 *
 *  trace_arg is the ytls_t* used by the mbedtls debug callback; pass NULL
 *  when called before ytls_t exists (init path) — the caller rewires the
 *  debug callback afterwards if needed.
 ***************************************************************************/
PRIVATE mbedtls_state_t *build_state(
    hgobj gobj,
    json_t *jn_config,
    BOOL server,
    BOOL trace_tls,
    void *trace_arg
)
{
    mbedtls_state_t *s = GBMEM_MALLOC(sizeof(mbedtls_state_t));
    if(!s) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_MEMORY_ERROR,
            "msg",              "%s", "no memory for sizeof(mbedtls_state_t)",
            NULL
        );
        return NULL;
    }
    s->refcount = 1;
    mbedtls_ssl_config_init(&s->conf);
    mbedtls_x509_crt_init(&s->cert);
    mbedtls_pk_init(&s->pkey);
    mbedtls_x509_crt_init(&s->ca_cert);

    if(mbedtls_ssl_config_defaults(
        &s->conf,
        server ? MBEDTLS_SSL_IS_SERVER : MBEDTLS_SSL_IS_CLIENT,
        MBEDTLS_SSL_TRANSPORT_STREAM,
        MBEDTLS_SSL_PRESET_DEFAULT
    ) != 0) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_MBEDTLS_ERROR,
            "msg",              "%s", "mbedtls_ssl_config_defaults() FAILED",
            NULL
        );
        state_decref(s);
        return NULL;
    }

    const char *ssl_certificate = kw_get_str(
        gobj, jn_config, "ssl_certificate", "", server ? KW_REQUIRED : 0
    );
    const char *ssl_certificate_key = kw_get_str(
        gobj, jn_config, "ssl_certificate_key", "", server ? KW_REQUIRED : 0
    );

    if(ssl_certificate && ssl_certificate[0] != '\0') {
        if(mbedtls_x509_crt_parse_file(&s->cert, ssl_certificate) != 0) {
            gobj_log_error(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_MBEDTLS_ERROR,
                "msg",              "%s", "mbedtls_x509_crt_parse_file() FAILED",
                "cert",             "%s", ssl_certificate,
                NULL
            );
            state_decref(s);
            return NULL;
        }

        if(mbedtls_pk_parse_keyfile(
            &s->pkey,
            ssl_certificate_key,
            NULL
        ) != 0) {
            gobj_log_error(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_MBEDTLS_ERROR,
                "msg",              "%s", "mbedtls_pk_parse_keyfile() FAILED",
                "cert_key",         "%s", ssl_certificate_key,
                NULL
            );
            state_decref(s);
            return NULL;
        }

        if(mbedtls_ssl_conf_own_cert(&s->conf, &s->cert, &s->pkey) != 0) {
            gobj_log_error(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_MBEDTLS_ERROR,
                "msg",              "%s", "mbedtls_ssl_conf_own_cert() FAILED (key does not match cert?)",
                "cert",             "%s", ssl_certificate,
                "cert_key",         "%s", ssl_certificate_key,
                NULL
            );
            state_decref(s);
            return NULL;
        }
        s->has_own_cert = TRUE;
    }

    const char *ssl_trusted_certificate = kw_get_str(
        gobj, jn_config, "ssl_trusted_certificate", "", 0
    );
    if(ssl_trusted_certificate && ssl_trusted_certificate[0] != '\0') {
        if(mbedtls_x509_crt_parse_file(&s->ca_cert, ssl_trusted_certificate) != 0) {
            gobj_log_error(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_MBEDTLS_ERROR,
                "msg",              "%s", "mbedtls_x509_crt_parse_file() FAILED for trusted cert",
                "ssl_trusted_certificate", "%s", ssl_trusted_certificate,
                NULL
            );
            state_decref(s);
            return NULL;
        }
        mbedtls_ssl_conf_ca_chain(&s->conf, &s->ca_cert, NULL);
        if(server) {
            mbedtls_ssl_conf_authmode(&s->conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
        } else {
            mbedtls_ssl_conf_authmode(&s->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
        }
        s->has_ca_cert = TRUE;
    } else {
        mbedtls_ssl_conf_authmode(&s->conf, MBEDTLS_SSL_VERIFY_NONE);
    }

    if(trace_tls && trace_arg) {
        mbedtls_debug_set_threshold(1);
        mbedtls_ssl_conf_dbg(&s->conf, mbedtls_debug_callback, trace_arg);
    }

    return s;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE hytls init(
    hgobj gobj,
    json_t *jn_config,  // not owned
    BOOL server
)
{
    /* PSA must be initialised before any crypto operation in mbedtls v4.0 */
    psa_status_t psa_ret = psa_crypto_init();
    if(psa_ret != PSA_SUCCESS) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MBEDTLS_ERROR,
            "msg",          "%s", "psa_crypto_init() FAILED",
            "error_code",   "%d", (int)psa_ret,
            NULL
        );
        return NULL;
    }

    ytls_t *ytls = GBMEM_MALLOC(sizeof(ytls_t));
    if(!ytls) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_MEMORY_ERROR,
            "msg",              "%s", "no memory for sizeof(ytls_t)",
            "sizeof(ytls_t)",   "%d", sizeof(ytls_t),
            NULL
        );
        return NULL;
    }

    ytls->api_tls = &api_tls;
    ytls->server = server;
    ytls->gobj = gobj;
    ytls->rx_buffer_size = kw_get_int(gobj, jn_config, "rx_buffer_size", 32*1024, 0);
    ytls->trace_tls = kw_get_bool(gobj, jn_config, "trace_tls", 0, KW_WILD_NUMBER);
    snprintf(ytls->ssl_server_name, sizeof(ytls->ssl_server_name), "%s",
        kw_get_str(gobj, jn_config, "ssl_server_name", "", 0)
    );

    if(ytls->trace_tls) {
        gobj_log_debug(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INFO,
            "msg",              "%s", "MBEDTLS: set trace TRUE",
            NULL
        );
    }

    ytls->state = build_state(gobj, jn_config, server, ytls->trace_tls, ytls);
    if(!ytls->state) {
        GBMEM_FREE(ytls);
        return NULL;
    }

    return (hytls)ytls;
}

/***************************************************************************
 *  Hot-reload certificates: build a fresh state, swap it in, and release
 *  our reference to the old one. Existing sessions keep the old state alive
 *  via their own ref (state_ref in sskt_t) until they close.
 *
 *  On failure, the previous state is kept intact.
 ***************************************************************************/
PRIVATE int reload_certificates(hytls ytls_, json_t *jn_config)
{
    ytls_t *ytls = (ytls_t *)ytls_;
    if(!ytls) {
        return -1;
    }
    hgobj gobj = ytls->gobj;

    BOOL trace_tls = kw_get_bool(gobj, jn_config, "trace_tls", ytls->trace_tls, KW_WILD_NUMBER);

    mbedtls_state_t *new_state = build_state(gobj, jn_config, ytls->server, trace_tls, ytls);
    if(!new_state) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MBEDTLS_ERROR,
            "msg",          "%s", "reload_certificates FAILED: keeping previous state",
            NULL
        );
        return -1;
    }

    mbedtls_state_t *old_state = ytls->state;
    ytls->state = new_state;
    ytls->trace_tls = trace_tls;

    state_decref(old_state);  // live sskts keep their own ref

    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "TLS certificates reloaded",
        NULL
    );
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void cleanup(hytls ytls_)
{
    ytls_t *ytls = (ytls_t *)ytls_;
    if(!ytls) {
        return;
    }

    // Release our ref on the current state. If any sskt still holds a ref,
    // the state lives until the last one is freed.
    state_decref(ytls->state);
    ytls->state = NULL;

    // Free the ytls structure itself
    GBMEM_FREE(ytls);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE const char *version(hytls ytls)
{
    return MBEDTLS_VERSION_STRING;
}

PRIVATE int mbedtls_ssl_send_callback(
    void *ctx,
    const unsigned char *buf,
    size_t len
) {
    sskt_t *sskt = (sskt_t *)ctx;
    hgobj gobj = sskt->ytls->gobj;

    /*
     * Accumulate encrypted data in output_buffer.
     * flush_encrypted_data() will send it all at once via on_encrypted_data_cb.
     *
     * This is critical: mbedtls_ssl_write() calls this callback once per TLS
     * record (~16 KB). For a large payload (e.g. 138 MB) that means ~8000+
     * individual calls. If each call triggered a separate io_uring write,
     * the writes could be reordered on the wire, corrupting TLS record order
     * and causing bad_record_mac on the peer.
     *
     * By accumulating here and flushing once, we produce a single write
     * to the transport — the same approach OpenSSL uses with its memory BIO.
     */
    size_t appended = gbuffer_append(sskt->output_buffer, (void *)buf, len);
    if(appended < len) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_MEMORY_ERROR,
            "msg",              "%s", "output_buffer append failed",
            "requested",        "%d", (int)len,
            "appended",         "%d", (int)appended,
            NULL
        );
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }

    return (int)len;
}

PRIVATE int mbedtls_ssl_recv_callback(void *ctx, unsigned char *buf, size_t len) {
    sskt_t *sskt = (sskt_t *)ctx;

    // Check if there is encrypted data available
    size_t available = gbuffer_chunk(sskt->encrypted_buffer);
    if(sskt->ytls->trace_tls) {
        hgobj gobj = sskt->ytls->gobj;
        gobj_trace_msg(gobj, "------- recv_callback: available=%zu, requested=%zu, userp %p",
            available, len, sskt->user_data);
    }
    if(available == 0) {
        return MBEDTLS_ERR_SSL_WANT_READ; // No data available, try again later
    }

    // Read the encrypted data from the buffer
    size_t to_read = (available < len) ? available : len;
    const char *src = gbuffer_cur_rd_pointer(sskt->encrypted_buffer);
    memcpy(buf, src, to_read);
    gbuffer_get(sskt->encrypted_buffer, to_read);

    return (int)to_read; // Return the number of bytes read
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE hsskt new_secure_filter(
    hytls ytls_,
    int (*on_handshake_done_cb)(void *user_data, int error),
    int (*on_clear_data_cb)(void *user_data, gbuffer_t *gbuf),
    int (*on_encrypted_data_cb)(void *user_data, gbuffer_t *gbuf),
    void *user_data
)
{
    ytls_t *ytls = (ytls_t *)ytls_;

    sskt_t *sskt = GBMEM_MALLOC(sizeof(sskt_t));
    if(!sskt) {
        gobj_log_error(ytls->gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_MEMORY_ERROR,
            "msg",              "%s", "no memory for sizeof(sskt_t)",
            "sizeof(sskt_t)",   "%d", sizeof(sskt_t),
            NULL
        );
        return NULL;
    }

    // Pin the current TLS state for the lifetime of this session. The state
    // may be swapped by reload_certificates() later, but this session keeps
    // using the state it was created with.
    sskt->state_ref = state_incref(ytls->state);
    if(!sskt->state_ref) {
        gobj_log_error(ytls->gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_MBEDTLS_ERROR,
            "msg",              "%s", "no TLS state (ytls not initialised?)",
            NULL
        );
        GBMEM_FREE(sskt);
        return NULL;
    }

    // Initialize the mbedTLS SSL context
    mbedtls_ssl_init(&sskt->ssl);

    if(mbedtls_ssl_setup(&sskt->ssl, &sskt->state_ref->conf) != 0) {
        gobj_log_error(ytls->gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_MBEDTLS_ERROR,
            "msg",              "%s", "mbedtls_ssl_setup() FAILED",
            NULL
        );
        state_decref(sskt->state_ref);
        GBMEM_FREE(sskt);
        return NULL;
    }

    // Set user-defined callbacks and user data
    sskt->ytls = ytls;
    sskt->on_handshake_done_cb = on_handshake_done_cb;
    sskt->on_clear_data_cb = on_clear_data_cb;
    sskt->on_encrypted_data_cb = on_encrypted_data_cb;
    sskt->user_data = user_data;
    sskt->alive = NULL;
    sskt->handshake_informed = FALSE;

    // Initialize encrypted data buffer (receives encrypted bytes from the network)
    sskt->encrypted_buffer = gbuffer_create(32 * 1024, 32 * 1024);
    if(!sskt->encrypted_buffer) {
        gobj_log_error(ytls->gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_MEMORY_ERROR,
            "msg",              "%s", "no memory for encrypted_buffer",
            NULL
        );
        free_secure_filter(sskt);
        return NULL;
    }

    // Initialize output buffer (accumulates encrypted bytes from send_callback)
    sskt->output_buffer = gbuffer_create(64 * 1024, gbmem_get_maximum_block());
    if(!sskt->output_buffer) {
        gobj_log_error(ytls->gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_MEMORY_ERROR,
            "msg",              "%s", "no memory for output_buffer",
            NULL
        );
        free_secure_filter(sskt);
        return NULL;
    }

    // Set SNI hostname for client connections (enables server_name extension in ClientHello)
    if(!ytls->server && ytls->ssl_server_name[0] != '\0') {
        int ret = mbedtls_ssl_set_hostname(&sskt->ssl, ytls->ssl_server_name);
        if(ret != 0) {
            char error_buf[256];
            mbedtls_strerror(ret, error_buf, sizeof(error_buf));
            gobj_log_error(ytls->gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_MBEDTLS_ERROR,
                "msg",              "%s", "mbedtls_ssl_set_hostname() FAILED",
                "ssl_server_name",  "%s", ytls->ssl_server_name,
                "error_code",       "%d", ret,
                "error_message",    "%s", error_buf,
                NULL
            );
            free_secure_filter(sskt);
            return NULL;
        }
    }

    // Set BIO callbacks for mbedTLS
    mbedtls_ssl_set_bio(
        &sskt->ssl,
        sskt,
        mbedtls_ssl_send_callback,  // mbedTLS calls this to output encrypted bytes
        mbedtls_ssl_recv_callback,  // mbedTLS calls this to read from encrypted_buffer
        NULL                        // Function for receive timeout (optional)
    );

    // For client: initiate the TLS handshake (sends Client Hello)
    if(!ytls->server) {
        int ret = mbedtls_ssl_handshake(&sskt->ssl);
        if(ret != 0 && ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            char error_buf[256];
            mbedtls_strerror(ret, error_buf, sizeof(error_buf));
            gobj_log_error(ytls->gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_MBEDTLS_ERROR,
                "msg",              "%s", "Initial TLS handshake (Client Hello) FAILED",
                "error_code",       "%d", ret,
                "error_message",    "%s", error_buf,
                NULL
            );
            free_secure_filter(sskt);
            return NULL;
        }
        // Flush the Client Hello that send_callback accumulated in output_buffer
        flush_encrypted_data(sskt);
    }

    return sskt;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void shutdown_sskt(hsskt sskt_)
{
    sskt_t *sskt = (sskt_t *)sskt_;

    // Perform a proper SSL/TLS shutdown
    int ret = mbedtls_ssl_close_notify(&sskt->ssl);
    if(ret < 0) {
        char error_buf[256];
        mbedtls_strerror(ret, error_buf, sizeof(error_buf));
        gobj_log_error(sskt->ytls->gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_MBEDTLS_ERROR,
            "msg",              "%s", "mbedtls_ssl_close_notify() FAILED",
            "error_code",       "%d", ret,
            "error_message",    "%s", error_buf,
            NULL
        );
    }

    // Flush any remaining encrypted data
    flush_encrypted_data(sskt);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void free_secure_filter(hsskt sskt_)
{
    sskt_t *sskt = (sskt_t *)sskt_;

    /*
     * If we are being freed from within the on_clear_data_cb callback
     * (re-entrant destruction), signal the alive marker on the caller's
     * stack so that flush_clear_data() can stop iterating after we return.
     */
    if(sskt->alive) {
        *sskt->alive = FALSE;
    }

    // Free the mbedTLS SSL context (must happen BEFORE releasing the state
    // reference, since ssl still points into state->conf).
    mbedtls_ssl_free(&sskt->ssl);

    // Free the encrypted buffer if it exists
    if(sskt->encrypted_buffer) {
        gbuffer_decref(sskt->encrypted_buffer);
    }

    // Free the output buffer if it exists
    if(sskt->output_buffer) {
        gbuffer_decref(sskt->output_buffer);
    }

    // Release this session's ref on the TLS state. If ytls has already been
    // cleaned up and/or a newer state is current, the old state is freed here.
    state_decref(sskt->state_ref);

    // Free the sskt structure itself
    GBMEM_FREE(sskt);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void mbedtls_debug_callback(
    void *ctx,
    int level,
    const char *file,
    int line,
    const char *str
) {
    ytls_t *ytls = (ytls_t *)ctx;

    // Log the debug message
    gobj_trace_msg(ytls->gobj,
        "mbedtls debug: level %d, file %s, line %d, msg '%s'",
        level, file, line, str
    );
}

PRIVATE void set_trace(hsskt sskt_, BOOL set)
{
    sskt_t *sskt = (sskt_t *)sskt_;
    ytls_t *ytls = sskt->ytls;
    ytls->trace_tls = set ? TRUE : FALSE;

    gobj_log_debug(ytls->gobj, 0,
        "function",         "%s", __FUNCTION__,
        "msgset",           "%s", MSGSET_INFO,
        "msg",              "%s", "MBEDTLS: set trace",
        "trace",            "%d", set,
        NULL
    );

    // Apply the trace setting to this session's pinned state config.
    if(sskt->state_ref) {
        if(ytls->trace_tls) {
            mbedtls_debug_set_threshold(1);
            mbedtls_ssl_conf_dbg(&sskt->state_ref->conf, mbedtls_debug_callback, ytls);
        } else {
            mbedtls_debug_set_threshold(0);
            mbedtls_ssl_conf_dbg(&sskt->state_ref->conf, NULL, NULL);
        }
    }
}

/***************************************************************************
    Do handshake
 ***************************************************************************/
PRIVATE int do_handshake(hsskt sskt_)
{
    sskt_t *sskt = (sskt_t *)sskt_;
    hgobj gobj = sskt->ytls->gobj;

    if(sskt->ytls->trace_tls) {
        gobj_trace_msg(gobj, "------- do_handshake, userp %p", sskt->user_data);
    }

    int ret = mbedtls_ssl_handshake(&sskt->ssl);

    if(ret != 0) {
        if(ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            if(sskt->ytls->trace_tls) {
                gobj_trace_msg(gobj, "------- handshake: %s, userp %p",
                    ret == MBEDTLS_ERR_SSL_WANT_READ ? "WANT_READ" : "WANT_WRITE",
                    sskt->user_data
                );
            }
            flush_encrypted_data(sskt); // Send accumulated handshake bytes
            return 0; // Handshake in progress
        } else {
            char error_buf[256];
            mbedtls_strerror(ret, error_buf, sizeof(error_buf));
            gobj_log_error(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_MBEDTLS_ERROR,
                "msg",              "%s", "TLS handshake failed",
                "error_code",       "%d", ret,
                "error_message",    "%s", error_buf,
                NULL
            );
            gobj_log_set_last_message("%s", error_buf);
            sskt->on_handshake_done_cb(sskt->user_data, -1);
            return -1111; // Mark as TLS error (must be < -1000 for c_tcp to close the socket)
        }
    }

    flush_encrypted_data(sskt); // Send accumulated handshake bytes

    if(!sskt->handshake_informed) {
        sskt->handshake_informed = TRUE;
        sskt->on_handshake_done_cb(sskt->user_data, 0); // Indicate success
    }

    return 1; // Handshake done
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int flush_encrypted_data(sskt_t *sskt)
{
    hgobj gobj = sskt->ytls->gobj;

    size_t pending = gbuffer_leftbytes(sskt->output_buffer);
    if(pending == 0) {
        return 0;
    }

    if(sskt->ytls->trace_tls) {
        gobj_trace_msg(gobj, "------- flush_encrypted_data() %zu, userp %p",
            pending, sskt->user_data);
    }

    /*
     * Hand over the accumulated output_buffer to the transport callback
     * and replace it with a fresh one for future writes.
     * This avoids copying potentially large amounts of data.
     */
    gbuffer_t *to_send = sskt->output_buffer;

    sskt->output_buffer = gbuffer_create(64 * 1024, gbmem_get_maximum_block());
    if(!sskt->output_buffer) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_MEMORY_ERROR,
            "msg",              "%s", "Cannot allocate new output_buffer",
            NULL
        );
        sskt->output_buffer = to_send; // Restore — don't lose data
        return -1;
    }

    if(sskt->on_encrypted_data_cb(sskt->user_data, to_send) < 0) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_MBEDTLS_ERROR,
            "msg",              "%s", "on_encrypted_data_cb() FAILED",
            NULL
        );
        gbuffer_decref(to_send);
        return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int encrypt_data(
    hsskt sskt_,
    gbuffer_t *gbuf // owned
) {
    sskt_t *sskt = (sskt_t *)sskt_;
    hgobj gobj = sskt->ytls->gobj;

    if(!mbedtls_ssl_is_handshake_over(&sskt->ssl)) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_MBEDTLS_ERROR,
            "msg",              "%s", "TLS handshake PENDING",
            NULL
        );
        GBUFFER_DECREF(gbuf);
        return -1;
    }

    size_t len;
    int want_retries = 0;
    while((len = gbuffer_chunk(gbuf)) > 0) {
        const char *p = gbuffer_cur_rd_pointer(gbuf); // Don't pop data, be sure it's written
        int written = mbedtls_ssl_write(&sskt->ssl, (const unsigned char *)p, len);

        if(written <= 0) {
            if(written == MBEDTLS_ERR_SSL_WANT_READ || written == MBEDTLS_ERR_SSL_WANT_WRITE) {
                if(sskt->ytls->trace_tls) {
                    gobj_trace_msg(gobj, "------- encrypt_data: WANT_READ/WANT_WRITE, userp %p", sskt->user_data);
                }
                if(++want_retries > 5) {
                    // Network stalled: no progress after repeated flush attempts.
                    // Caller will retry when new data arrives via the event loop.
                    gobj_log_warning(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_MBEDTLS_ERROR,
                        "msg",          "%s", "mbedtls_ssl_write() WANT stall, aborting",
                        NULL
                    );
                    flush_encrypted_data(sskt); // Send what we have so far
                    GBUFFER_DECREF(gbuf);
                    return -1;
                }
                flush_encrypted_data(sskt); // Send what we have to make progress
                flush_clear_data(sskt);
                continue;
            } else {
                char error_buf[256];
                mbedtls_strerror(written, error_buf, sizeof(error_buf));
                gobj_log_error(gobj, 0,
                    "function",         "%s", __FUNCTION__,
                    "msgset",           "%s", MSGSET_MBEDTLS_ERROR,
                    "msg",              "%s", "mbedtls_ssl_write() FAILED",
                    "error_code",       "%d", written,
                    "error_message",    "%s", error_buf,
                    NULL
                );
                flush_encrypted_data(sskt); // Send what we have so far
                GBUFFER_DECREF(gbuf);
                return -1;
            }
        }

        want_retries = 0;
        gbuffer_get(gbuf, written); // Pop data

        if(sskt->ytls->trace_tls) {
            gobj_trace_msg(gobj, "------- ==> encrypt_data DATA, userp %p, len %d", sskt->user_data, written);
        }

        /*
         * Do NOT flush here — let send_callback accumulate all TLS records
         * in output_buffer. Flushing once after the loop produces a single
         * write to the transport, avoiding io_uring write reordering.
         */
    }

    /*
     * All data encrypted — flush the accumulated output as a single write.
     */
    if(flush_encrypted_data(sskt) < 0) {
        GBUFFER_DECREF(gbuf);
        return -1;
    }

    GBUFFER_DECREF(gbuf);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int flush_clear_data(sskt_t *sskt)
{
    hgobj gobj = sskt->ytls->gobj;
    int ret = 0;

    /*
     * Alive marker: a stack variable whose address is stored in sskt->alive.
     * If on_clear_data_cb triggers re-entrant destruction of sskt
     * (i.e. free_secure_filter is called while we are still iterating),
     * free_secure_filter will set this to FALSE before freeing sskt,
     * and we will stop the loop without touching the freed memory.
     */
    BOOL sskt_alive = TRUE;
    sskt->alive = &sskt_alive;

    if(sskt->ytls->trace_tls) {
        gobj_trace_msg(gobj, "------- flush_clear_data(), userp %p", sskt->user_data);
    }

    while(TRUE) {
        gbuffer_t *gbuf = gbuffer_create(sskt->ytls->rx_buffer_size, sskt->ytls->rx_buffer_size);
        if(!gbuf) {
            gobj_log_error(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_MEMORY_ERROR,
                "msg",              "%s", "Failed to create gbuffer",
                NULL
            );
            sskt->alive = NULL;
            return -1;
        }

        char *p = gbuffer_cur_wr_pointer(gbuf);
        int nread = mbedtls_ssl_read(&sskt->ssl, (unsigned char *)p, sskt->ytls->rx_buffer_size);

        if(sskt->ytls->trace_tls) {
            gobj_trace_msg(gobj, "------- flush_clear_data() %d, userp %p", nread, sskt->user_data);
        }

        if(nread > 0) {
            gbuffer_set_wr(gbuf, nread);
            ret += sskt->on_clear_data_cb(sskt->user_data, gbuf);
            /*
             * Check alive marker: if the callback caused sskt to be freed
             * (re-entrant destruction), sskt_alive is now FALSE.
             * Do NOT touch sskt after this point.
             */
            if(!sskt_alive) {
                return ret;
            }
        } else {
            gbuffer_decref(gbuf);

            if(nread == MBEDTLS_ERR_SSL_WANT_READ || nread == MBEDTLS_ERR_SSL_WANT_WRITE) {
                if(sskt->ytls->trace_tls) {
                    gobj_trace_msg(gobj, "------- flush_clear_data WANT_READ/WANT_WRITE, userp %p", sskt->user_data);
                }
                /*
                 * mbedTLS may return WANT_READ after silently consuming a
                 * TLS 1.3 NewSessionTicket (without returning the NST code).
                 * If data still sits in encrypted_buffer, loop again so
                 * mbedTLS can parse the next record (e.g. the actual
                 * application data that follows the NSTs).
                 */
                if(gbuffer_chunk(sskt->encrypted_buffer) > 0) {
                    continue;
                }
                break; // Buffer empty — genuinely waiting for more network data
#ifdef MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET
            } else if(nread == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET) {
                /*
                 * TLS 1.3: server sent a NewSessionTicket post-handshake message.
                 * mbedTLS processed the ticket internally and returns this code
                 * to inform us — it is NOT a fatal error.  Continue reading;
                 * application data may follow immediately.
                 */
                if(sskt->ytls->trace_tls) {
                    gobj_trace_msg(gobj, "------- flush_clear_data: TLS1.3 NewSessionTicket received, continuing, userp %p", sskt->user_data);
                }
                continue;
#endif
            } else if(nread < 0) {
                char error_buf[256];
                mbedtls_strerror(nread, error_buf, sizeof(error_buf));
                if(nread == -30848) {
                    // SSL - The peer notified us that the connection is going to be closed
                    if(sskt->ytls->trace_tls) {
                        gobj_trace_msg(gobj, "------- flush_clear_data: The peer notified connection is going to be closed, userp %p", sskt->user_data);
                    }
                } else {
                    gobj_log_error(gobj, 0,
                        "function",         "%s", __FUNCTION__,
                        "msgset",           "%s", MSGSET_MBEDTLS_ERROR,
                        "msg",              "%s", "mbedtls_ssl_read() FAILED",
                        "error_code",       "%d", nread,
                        "error_message",    "%s", error_buf,
                        NULL
                    );
                }
                sskt->alive = NULL;
                return -1111; // Mark as TLS error
            } else {
                // No more data
                break;
            }
        }
    }

    sskt->alive = NULL;
    return ret;
}

/***************************************************************************
    Use this function decrypt encrypted data.
    The clear data will be returned in on_clear_data_cb callback.
 ***************************************************************************/
PRIVATE int decrypt_data(
    hsskt sskt_,
    gbuffer_t *gbuf // owned
) {
    sskt_t *sskt = (sskt_t *)sskt_;
    hgobj gobj = sskt->ytls->gobj;

    /*
     * Feed encrypted bytes into the recv buffer so that recv_callback
     * (mbedtls_ssl_recv_callback) can supply them to the TLS engine.
     */
    size_t len;
    while((len = gbuffer_chunk(gbuf)) > 0) {
        /* Reset the buffer when empty to reclaim linear space */
        if(gbuffer_leftbytes(sskt->encrypted_buffer) == 0) {
            gbuffer_clear(sskt->encrypted_buffer);
        }

        const char *src = gbuffer_cur_rd_pointer(gbuf);
        size_t appended = gbuffer_append(sskt->encrypted_buffer, (void *)src, len);
        if(appended == 0) {
            /* Buffer full — this should not happen with a 32 KB buffer for TLS */
            gobj_log_error(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_MBEDTLS_ERROR,
                "msg",              "%s", "encrypted_buffer full, dropping data",
                NULL
            );
            break;
        }
        gbuffer_get(gbuf, appended);

        if(sskt->ytls->trace_tls) {
            gobj_trace_msg(gobj, "------- <== decrypt_data, userp %p, len %zu", sskt->user_data, appended);
        }
    }
    GBUFFER_DECREF(gbuf);

    /*
     * Drive the TLS engine.
     * - During handshake: recv_callback pulls data from encrypted_buffer;
     *   send_callback forwards outgoing handshake bytes via on_encrypted_data_cb.
     * - After handshake: read decrypted application data via flush_clear_data.
     */
    if(!mbedtls_ssl_is_handshake_over(&sskt->ssl)) {
        int ret = mbedtls_ssl_handshake(&sskt->ssl);
        if(ret == 0) {
            /* Handshake just completed — flush handshake response bytes */
            flush_encrypted_data(sskt);
            if(!sskt->handshake_informed) {
                sskt->handshake_informed = TRUE;
                sskt->on_handshake_done_cb(sskt->user_data, 0);
            }
        } else if(ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            /* Normal: need more data from the network or waiting for write */
            flush_encrypted_data(sskt); // Send accumulated handshake bytes
            if(sskt->ytls->trace_tls) {
                gobj_trace_msg(gobj, "------- decrypt_data handshake: %s, userp %p",
                    ret == MBEDTLS_ERR_SSL_WANT_READ ? "WANT_READ" : "WANT_WRITE",
                    sskt->user_data);
            }
        } else {
            char error_buf[256];
            mbedtls_strerror(ret, error_buf, sizeof(error_buf));
            gobj_log_error(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_MBEDTLS_ERROR,
                "msg",              "%s", "TLS handshake failed",
                "error_code",       "%d", ret,
                "error_message",    "%s", error_buf,
                NULL
            );
            gobj_log_set_last_message("%s", error_buf);
            sskt->on_handshake_done_cb(sskt->user_data, -1);
            return -1111; // Mark as TLS error (must be < -1000 for c_tcp to close the socket)
        }
    } else {
        /* Handshake already done — read decrypted application data */
        int ret = flush_clear_data(sskt);
        if(ret < 0) {
            // Error already logged in flush_clear_data() (or silenced for close_notify)
            return ret;
        }
    }

    return 0;
}


/***************************************************************************
 *
 ***************************************************************************/
PRIVATE const char *last_error(hsskt sskt_)
{
    sskt_t *sskt = sskt_;
    if(!sskt) {
        return "???";
    }
    return sskt->last_error;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int flush(hsskt sskt)
{
    flush_encrypted_data(sskt);
    int ret = flush_clear_data(sskt);
    if(ret < 0) {
        // Error already logged in flush_clear_data
        return ret;
    }
    return 0;
}




                    /***************************
                     *      Public
                     ***************************/




/***************************************************************************
 *  Get api_tls_t
 ***************************************************************************/
PUBLIC api_tls_t *mbed_api_tls(void)
{
    return &api_tls;
}

#endif /* CONFIG_HAVE_MBEDTLS */
