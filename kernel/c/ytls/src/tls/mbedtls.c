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
typedef struct ytls_s {
    api_tls_t *api_tls;     // Must be the first item
    BOOL server;
    mbedtls_ssl_config conf;  // mbedTLS SSL configuration
    mbedtls_x509_crt cert;    // Own certificate (server, or client mutual-auth)
    mbedtls_pk_context pkey;  // Private key
    mbedtls_x509_crt ca_cert; // Trusted CA certificate for peer verification

    // mbedtls_ctr_drbg_context ctr_drbg;
    // mbedtls_entropy_context entropy;

    BOOL trace_tls;
    size_t rx_buffer_size;
    hgobj gobj;
    char ssl_server_name[256]; // Server name for SNI (client-side TLS only)
} ytls_t;

typedef struct sskt_s {
    ytls_t *ytls;
    mbedtls_ssl_context ssl; // mbedTLS SSL context
    BOOL handshake_informed;
    int (*on_handshake_done_cb)(void *user_data, int error);
    int (*on_clear_data_cb)(void *user_data, gbuffer_t *gbuf);
    int (*on_encrypted_data_cb)(void *user_data, gbuffer_t *gbuf);
    void *user_data;
    char last_error[256];
    int error;
    char rx_bf[16*1024];
    gbuffer_t *encrypted_buffer; // TODO review
    BOOL *alive; // Points to stack var in flush_clear_data; set to FALSE when freed mid-callback
} sskt_t;

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE int flush_clear_data(sskt_t *sskt);

/***************************************************************
 *              Api
 ***************************************************************/
PRIVATE hytls init(
    hgobj gobj,
    json_t *jn_config,  // not owned
    BOOL server
);
PRIVATE void cleanup(hytls ytls);
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
 *
 ***************************************************************************/
PRIVATE hytls init(
    hgobj gobj,
    json_t *jn_config,  // not owned
    BOOL server
)
{
    /* PSA must be initialised before any crypto operation in mbedtls v4.0 */
    psa_crypto_init();

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

    // Initialize mbedTLS structures
    mbedtls_ssl_config_init(&ytls->conf);
    mbedtls_x509_crt_init(&ytls->cert);
    mbedtls_pk_init(&ytls->pkey);
    mbedtls_x509_crt_init(&ytls->ca_cert);
    // mbedtls_ctr_drbg_init(&ytls->ctr_drbg);
    // mbedtls_entropy_init(&ytls->entropy);

    // Seed the CTR-DRBG
    // const char *pers = "tls";
    // if(mbedtls_ctr_drbg_seed(
    //     &ytls->ctr_drbg,
    //     mbedtls_entropy_func,
    //     &ytls->entropy,
    //     (const unsigned char *)pers,
    //     strlen(pers)
    // ) != 0) {
    //     gobj_log_error(gobj, 0,
    //         "function",         "%s", __FUNCTION__,
    //         "msgset",           "%s", MSGSET_MBEDTLS_ERROR,
    //         "msg",              "%s", "mbedtls_ctr_drbg_seed() FAILED",
    //         NULL
    //     );
    //     cleanup((hytls)ytls);
    //     return NULL;
    // }

    // Set default SSL configuration
    if(mbedtls_ssl_config_defaults(
        &ytls->conf,
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
        cleanup((hytls)ytls);
        return NULL;
    }

    // Load own certificate and private key (required for server; optional for mutual-TLS client)
    const char *ssl_certificate = kw_get_str(
        gobj, jn_config, "ssl_certificate", "", server ? KW_REQUIRED : 0
    );
    const char *ssl_certificate_key = kw_get_str(
        gobj, jn_config, "ssl_certificate_key", "", server ? KW_REQUIRED : 0
    );

    if(ssl_certificate && ssl_certificate[0] != '\0') {
        if(mbedtls_x509_crt_parse_file(&ytls->cert, ssl_certificate) != 0) {
            gobj_log_error(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_MBEDTLS_ERROR,
                "msg",              "%s", "mbedtls_x509_crt_parse_file() FAILED",
                "cert",             "%s", ssl_certificate,
                NULL
            );
            cleanup((hytls)ytls);
            return NULL;
        }

        if(mbedtls_pk_parse_keyfile(
            &ytls->pkey,
            ssl_certificate_key,
            NULL   /* password: NULL for unencrypted keys; f_rng/p_rng removed in v4.0 */
        ) != 0) {
            gobj_log_error(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_MBEDTLS_ERROR,
                "msg",              "%s", "mbedtls_pk_parse_keyfile() FAILED",
                "cert_key",         "%s", ssl_certificate_key,
                NULL
            );
            cleanup((hytls)ytls);
            return NULL;
        }

        mbedtls_ssl_conf_own_cert(&ytls->conf, &ytls->cert, &ytls->pkey);
    }

    // Configure peer certificate verification
    const char *ssl_trusted_certificate = kw_get_str(
        gobj, jn_config, "ssl_trusted_certificate", "", 0
    );
    if(ssl_trusted_certificate && ssl_trusted_certificate[0] != '\0') {
        if(mbedtls_x509_crt_parse_file(&ytls->ca_cert, ssl_trusted_certificate) != 0) {
            gobj_log_error(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_MBEDTLS_ERROR,
                "msg",              "%s", "mbedtls_x509_crt_parse_file() FAILED for trusted cert",
                "ssl_trusted_certificate", "%s", ssl_trusted_certificate,
                NULL
            );
            cleanup((hytls)ytls);
            return NULL;
        }
        mbedtls_ssl_conf_ca_chain(&ytls->conf, &ytls->ca_cert, NULL);
        mbedtls_ssl_conf_authmode(&ytls->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    } else {
        // No trusted CA configured — disable peer verification
        // (suitable for self-signed certs in development/testing)
        mbedtls_ssl_conf_authmode(&ytls->conf, MBEDTLS_SSL_VERIFY_NONE);
    }

    // Set the API dispatch table (MUST be first field, required by __ytls_t__ cast)
    ytls->api_tls = &api_tls;

    // Configure other parameters
    ytls->server = server;
    ytls->rx_buffer_size = kw_get_int(
        gobj, jn_config, "rx_buffer_size", 32*1024, 0
    );
    ytls->trace_tls = kw_get_bool(
        gobj, jn_config, "trace_tls", 0, KW_WILD_NUMBER
    );
    ytls->gobj = gobj;
    snprintf(ytls->ssl_server_name, sizeof(ytls->ssl_server_name), "%s",
        kw_get_str(gobj, jn_config, "ssl_server_name", "", 0)
    );

    return (hytls)ytls;
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

    // Free mbedTLS structures
    mbedtls_ssl_config_free(&ytls->conf);
    mbedtls_x509_crt_free(&ytls->cert);
    mbedtls_pk_free(&ytls->pkey);
    mbedtls_x509_crt_free(&ytls->ca_cert);
    // mbedtls_ctr_drbg_free(&ytls->ctr_drbg);
    // mbedtls_entropy_free(&ytls->entropy);

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

static int mbedtls_ssl_send_callback(
    void *ctx,
    const unsigned char *buf,
    size_t len
) {
    sskt_t *sskt = (sskt_t *)ctx;
    hgobj gobj = NULL;

    // Create a buffer for the encrypted data
    gbuffer_t *gbuf = gbuffer_create(len, len);
    if(!gbuf) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_MEMORY_ERROR,
            "msg",              "%s", "no memory for gbuffer",
            "size",             "%d", (int)len,
            NULL
        );
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR; // Out of memory
    }

    // Copy the encrypted data into the buffer
    char *dest = gbuffer_cur_wr_pointer(gbuf);
    memcpy(dest, buf, len);
    gbuffer_set_wr(gbuf, len);

    // Invoke the callback to handle the encrypted data
    if(sskt->on_encrypted_data_cb(sskt->user_data, gbuf) < 0) {
        gbuffer_decref(gbuf);
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR; // Callback failed
    }

    return (int)len; // Return the number of bytes processed
}

static int mbedtls_ssl_recv_callback(void *ctx, unsigned char *buf, size_t len) {
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

    // Initialize the mbedTLS SSL context
    mbedtls_ssl_init(&sskt->ssl);

    if(mbedtls_ssl_setup(&sskt->ssl, &ytls->conf) != 0) {
        gobj_log_error(ytls->gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_MBEDTLS_ERROR,
            "msg",              "%s", "mbedtls_ssl_setup() FAILED",
            NULL
        );
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

    // Set SNI hostname for client connections (enables server_name extension in ClientHello)
    if(!ytls->server && ytls->ssl_server_name[0] != '\0') {
        mbedtls_ssl_set_hostname(&sskt->ssl, ytls->ssl_server_name);
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
        // send_callback was invoked by mbedTLS to transmit the Client Hello;
        // on_encrypted_data_cb has already queued it for writing to the socket.
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
        gobj_log_error(sskt->ytls->gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_MBEDTLS_ERROR,
            "msg",              "%s", "mbedtls_ssl_close_notify() FAILED",
            "error",            "%d", ret,
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

    // Free the mbedTLS SSL context
    mbedtls_ssl_free(&sskt->ssl);

    // Free the encrypted buffer if it exists
    if(sskt->encrypted_buffer) {
        gbuffer_decref(sskt->encrypted_buffer);
    }

    // Free the sskt structure itself
    GBMEM_FREE(sskt);
}

/***************************************************************************
 *
 ***************************************************************************/
static void mbedtls_debug_callback(
    void *ctx,
    int level,
    const char *file,
    int line,
    const char *str)
{
    ytls_t *ytls = (ytls_t *)ctx;

    // Log the debug message
    gobj_log_info(ytls->gobj, 0,
        "level",          "%d", level,
        "file",           "%s", file,
        "line",           "%d", line,
        "msg",            "%s", str,
        NULL
    );
}

PRIVATE void set_trace(hsskt sskt_, BOOL set)
{
    sskt_t *sskt = (sskt_t *)sskt_;
    ytls_t *ytls = sskt->ytls;
    ytls->trace_tls = set ? TRUE : FALSE;

    if(ytls->trace_tls) {
        mbedtls_ssl_conf_dbg(&ytls->conf, mbedtls_debug_callback, ytls);
    } else {
        mbedtls_ssl_conf_dbg(&ytls->conf, NULL, NULL);
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
            return -1;
        }
    }

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
    /*
     * In the custom BIO model, mbedTLS calls send_callback synchronously
     * whenever it wants to output encrypted bytes. The send_callback already
     * invokes on_encrypted_data_cb. There is nothing more to flush here.
     */
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
                    GBUFFER_DECREF(gbuf);
                    return -1;
                }
                flush_encrypted_data(sskt);
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
                GBUFFER_DECREF(gbuf);
                return -1;
            }
        }

        want_retries = 0;
        gbuffer_get(gbuf, written); // Pop data

        if(sskt->ytls->trace_tls) {
            gobj_trace_dump(gobj, p, written, "------- ==> encrypt_data DATA, userp %p", sskt->user_data);
        }

        if(flush_encrypted_data(sskt) < 0) {
            // Error already logged
            GBUFFER_DECREF(gbuf);
            return -1;
        }
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
            gobj_trace_msg(gobj, "------- decrypt_data: fed %zu bytes into encrypted_buffer, userp %p",
                appended, sskt->user_data);
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
            /* Handshake just completed */
            if(!sskt->handshake_informed) {
                sskt->handshake_informed = TRUE;
                sskt->on_handshake_done_cb(sskt->user_data, 0);
            }
        } else if(ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            /* Normal: need more data from the network or waiting for write */
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
            return -1;
        }
    } else {
        /* Handshake already done — read decrypted application data */
        int ret = flush_clear_data(sskt);
        if(ret < 0) {
            gobj_log_error(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_MBEDTLS_ERROR,
                "msg",              "%s", "flush_clear_data() FAILED",
                NULL
            );
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
    flush_clear_data(sskt);
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
