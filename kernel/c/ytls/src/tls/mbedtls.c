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

#ifdef CONFIG_YTLS_USE_MBEDTLS

#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/error.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/pk.h>

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
    mbedtls_x509_crt cert;    // Server certificate
    mbedtls_pk_context pkey;  // Private key

    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;

    BOOL trace;
    size_t rx_buffer_size;
    hgobj gobj;
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
    ytls_t *ytls = GBMEM_MALLOC(sizeof(ytls_t));
    if (!ytls) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_MEMORY_ERROR,
            "msg",              "%s", "no memory for sizeof(ytls_t)",
            "sizeof(ytls_t)",   "%d", sizeof(ytls_t),
            NULL
        );
        return 0;
    }

    // Initialize mbedTLS structures
    mbedtls_ssl_config_init(&ytls->conf);
    mbedtls_x509_crt_init(&ytls->cert);
    mbedtls_pk_init(&ytls->pkey);
    mbedtls_ctr_drbg_init(&ytls->ctr_drbg);
    mbedtls_entropy_init(&ytls->entropy);

    // Seed the CTR-DRBG
    const char *pers = "tls";
    if (mbedtls_ctr_drbg_seed(
        &ytls->ctr_drbg,
        mbedtls_entropy_func,
        &ytls->entropy,
        (const unsigned char *)pers,
        strlen(pers)
    ) != 0) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_AUTH_ERROR,
            "msg",              "%s", "mbedtls_ctr_drbg_seed() FAILED",
            NULL
        );
        cleanup((hytls)ytls);
        return 0;
    }

    // Set default SSL configuration
    if (mbedtls_ssl_config_defaults(
        &ytls->conf,
        server ? MBEDTLS_SSL_IS_SERVER : MBEDTLS_SSL_IS_CLIENT,
        MBEDTLS_SSL_TRANSPORT_STREAM,
        MBEDTLS_SSL_PRESET_DEFAULT
    ) != 0) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_AUTH_ERROR,
            "msg",              "%s", "mbedtls_ssl_config_defaults() FAILED",
            NULL
        );
        cleanup((hytls)ytls);
        return 0;
    }

    // Load certificates and keys
    const char *ssl_certificate = kw_get_str(
        gobj, jn_config, "ssl_certificate", "", server ? KW_REQUIRED : 0
    );
    const char *ssl_certificate_key = kw_get_str(
        gobj, jn_config, "ssl_certificate_key", "", server ? KW_REQUIRED : 0
    );

    if (mbedtls_x509_crt_parse_file(&ytls->cert, ssl_certificate) != 0) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_AUTH_ERROR,
            "msg",              "%s", "mbedtls_x509_crt_parse_file() FAILED",
            "cert",             "%s", ssl_certificate,
            NULL
        );
        cleanup((hytls)ytls);
        return 0;
    }

    if (mbedtls_pk_parse_keyfile(
        &ytls->pkey,
        ssl_certificate_key,
        NULL,
        mbedtls_ctr_drbg_random,
        &ytls->ctr_drbg
    ) != 0) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_AUTH_ERROR,
            "msg",              "%s", "mbedtls_pk_parse_keyfile() FAILED",
            "cert_key",         "%s", ssl_certificate_key,
            NULL
        );
        cleanup((hytls)ytls);
        return 0;
    }

    mbedtls_ssl_conf_own_cert(&ytls->conf, &ytls->cert, &ytls->pkey);

    // Configure other parameters
    ytls->server = server;
    ytls->rx_buffer_size = kw_get_int(
        gobj, jn_config, "rx_buffer_size", 32 * 1024, 0
    );
    ytls->trace = kw_get_bool(
        gobj, jn_config, "trace", 0, KW_WILD_NUMBER
    );
    ytls->gobj = gobj;

    if (ytls->trace) {
        gobj_log_info(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msg",              "%s", "TLS initialized with tracing enabled.",
            NULL
        );
    }

    return (hytls)ytls;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void cleanup(hytls ytls_)
{
    ytls_t *ytls = (ytls_t *)ytls_;
    if (!ytls) {
        return;
    }

    // Free mbedTLS structures
    mbedtls_ssl_config_free(&ytls->conf);
    mbedtls_x509_crt_free(&ytls->cert);
    mbedtls_pk_free(&ytls->pkey);
    mbedtls_ctr_drbg_free(&ytls->ctr_drbg);
    mbedtls_entropy_free(&ytls->entropy);

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

static int mbedtls_ssl_send_callback(void *ctx, const unsigned char *buf, size_t len) {
    sskt_t *sskt = (sskt_t *)ctx;

    // Create a buffer for the encrypted data
    gbuffer_t *gbuf = gbuffer_create(len, len);
    if (!gbuf) {
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR; // Out of memory
    }

    // Copy the encrypted data into the buffer
    char *dest = gbuffer_cur_wr_pointer(gbuf);
    memcpy(dest, buf, len);
    gbuffer_set_wr(gbuf, len);

    // Invoke the callback to handle the encrypted data
    if (sskt->on_encrypted_data_cb(sskt->user_data, gbuf) < 0) {
        gbuffer_decref(gbuf);
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR; // Callback failed
    }

    return (int)len; // Return the number of bytes processed
}

static int mbedtls_ssl_recv_callback(void *ctx, unsigned char *buf, size_t len) {
    sskt_t *sskt = (sskt_t *)ctx;

    // Check if there is encrypted data available
    size_t available = gbuffer_chunk(sskt->encrypted_buffer);
    if (available == 0) {
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
    if (!sskt) {
        gobj_log_error(ytls->gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_MEMORY_ERROR,
            "msg",              "%s", "no memory for sizeof(sskt_t)",
            "sizeof(sskt_t)",   "%d", sizeof(sskt_t),
            NULL
        );
        return 0;
    }

    // Initialize the mbedTLS SSL context
    mbedtls_ssl_init(&sskt->ssl);

    if (mbedtls_ssl_setup(&sskt->ssl, &ytls->conf) != 0) {
        gobj_log_error(ytls->gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_AUTH_ERROR,
            "msg",              "%s", "mbedtls_ssl_setup() FAILED",
            NULL
        );
        GBMEM_FREE(sskt);
        return 0;
    }

    // Set user-defined callbacks and user data
    sskt->ytls = ytls;
    sskt->on_handshake_done_cb = on_handshake_done_cb;
    sskt->on_clear_data_cb = on_clear_data_cb;
    sskt->on_encrypted_data_cb = on_encrypted_data_cb;
    sskt->user_data = user_data;

    // Initialize encrypted data buffer
    sskt->encrypted_buffer = gbuffer_create(32 * 1024, 32 * 1024);

    // Set BIO callbacks for mbedTLS
    mbedtls_ssl_set_bio(
        &sskt->ssl,
        sskt,
        mbedtls_ssl_send_callback,  // Function to send data
        mbedtls_ssl_recv_callback,  // Function to receive data
        NULL                        // Function for receive timeout (optional)
    );

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
    if (ret < 0) {
        gobj_log_error(sskt->ytls->gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_SYSTEM_ERROR,
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

    // Free the mbedTLS SSL context
    mbedtls_ssl_free(&sskt->ssl);

    // Free the encrypted buffer if it exists
    if (sskt->encrypted_buffer) {
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
    ytls->trace = set ? TRUE : FALSE;

    if (ytls->trace) {
        // Enable debug callback
        mbedtls_ssl_conf_dbg(&ytls->conf, mbedtls_debug_callback, ytls);
        gobj_log_info(ytls->gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msg",              "%s", "TLS tracing enabled.",
            NULL
        );
    } else {
        // Disable debug callback
        mbedtls_ssl_conf_dbg(&ytls->conf, NULL, NULL);
        gobj_log_info(ytls->gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msg",              "%s", "TLS tracing disabled.",
            NULL
        );
    }
}

/***************************************************************************
    Do handshake
 ***************************************************************************/
PRIVATE int do_handshake(hsskt sskt_)
{
    sskt_t *sskt = (sskt_t *)sskt_;
    hgobj gobj = sskt->ytls->gobj;

    if (sskt->ytls->trace) {
        gobj_trace_msg(gobj, "------- do_handshake, userp %p", sskt->user_data);
    }

    int ret = mbedtls_ssl_handshake(&sskt->ssl);

    if (ret != 0) {
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            if (sskt->ytls->trace) {
                gobj_trace_msg(gobj, "------- handshake: %s, userp %p",
                    ret == MBEDTLS_ERR_SSL_WANT_READ ? "WANT_READ" : "WANT_WRITE",
                    sskt->user_data
                );
            }
            flush_encrypted_data(sskt);
            flush_clear_data(sskt);
            return 1; // Indicate that the handshake is still ongoing
        } else {
            char error_buf[256];
            mbedtls_strerror(ret, error_buf, sizeof(error_buf));
            gobj_log_error(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_SYSTEM_ERROR,
                "msg",              "%s", "TLS handshake failed",
                "error_code",       "%d", ret,
                "error_message",    "%s", error_buf,
                NULL
            );

            sskt->on_handshake_done_cb(sskt->user_data, -1);
            return -1;
        }
    }

    flush_encrypted_data(sskt);

    if (!sskt->handshake_informed) {
        sskt->handshake_informed = TRUE;
        sskt->on_handshake_done_cb(sskt->user_data, 0); // Indicate success
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int flush_encrypted_data(sskt_t *sskt)
{
    hgobj gobj = sskt->ytls->gobj;

    if (sskt->ytls->trace) {
        gobj_trace_msg(gobj, "------- flush_encrypted_data(), userp %p", sskt->user_data);
    }

    int pending;
    while ((pending = mbedtls_ssl_get_bytes_avail(&sskt->ssl)) > 0) {
        gbuffer_t *gbuf = gbuffer_create(pending, pending);
        if (!gbuf) {
            gobj_log_error(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_MEMORY_ERROR,
                "msg",              "%s", "No memory for pending encrypted data",
                NULL
            );
            return -1;
        }

        char *p = gbuffer_cur_wr_pointer(gbuf);
        int ret = mbedtls_ssl_read(&sskt->ssl, (unsigned char *)p, pending);
        if (ret > 0) {
            gbuffer_set_wr(gbuf, ret);
            sskt->on_encrypted_data_cb(sskt->user_data, gbuf);
        } else {
            gbuffer_decref(gbuf);
            if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
                if (sskt->ytls->trace) {
                    gobj_trace_msg(gobj, "------- flush_encrypted_data WANT_READ/WANT_WRITE, userp %p", sskt->user_data);
                }
                return 1; // Indicate need for more I/O
            } else {
                char error_buf[256];
                mbedtls_strerror(ret, error_buf, sizeof(error_buf));
                gobj_log_error(gobj, 0,
                    "function",         "%s", __FUNCTION__,
                    "msgset",           "%s", MSGSET_SYSTEM_ERROR,
                    "msg",              "%s", "Error reading encrypted data",
                    "error_code",       "%d", ret,
                    "error_message",    "%s", error_buf,
                    NULL
                );
                return -1;
            }
        }
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

    if (mbedtls_ssl_handshake(&sskt->ssl)!=0) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INTERNAL_ERROR,
            "msg",              "%s", "TLS handshake PENDING",
            NULL
        );
        GBUFFER_DECREF(gbuf);
        return -1;
    }

    size_t len;
    while ((len = gbuffer_chunk(gbuf)) > 0) {
        const char *p = gbuffer_cur_rd_pointer(gbuf); // Don't pop data, be sure it's written
        int written = mbedtls_ssl_write(&sskt->ssl, (const unsigned char *)p, len);

        if (written <= 0) {
            if (written == MBEDTLS_ERR_SSL_WANT_READ || written == MBEDTLS_ERR_SSL_WANT_WRITE) {
                if (sskt->ytls->trace) {
                    gobj_trace_msg(gobj, "------- encrypt_data: WANT_READ/WANT_WRITE, userp %p", sskt->user_data);
                }
                flush_encrypted_data(sskt);
                flush_clear_data(sskt);
                continue;
            } else {
                char error_buf[256];
                mbedtls_strerror(written, error_buf, sizeof(error_buf));
                gobj_log_error(gobj, 0,
                    "function",         "%s", __FUNCTION__,
                    "msgset",           "%s", MSGSET_SYSTEM_ERROR,
                    "msg",              "%s", "mbedtls_ssl_write() FAILED",
                    "error_code",       "%d", written,
                    "error_message",    "%s", error_buf,
                    NULL
                );
                GBUFFER_DECREF(gbuf);
                return -1;
            }
        }

        gbuffer_get(gbuf, written); // Pop data

        if (sskt->ytls->trace) {
            gobj_trace_dump(gobj, p, written, "------- ==> encrypt_data DATA, userp %p", sskt->user_data);
        }

        if (flush_encrypted_data(sskt) < 0) {
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

    if (sskt->ytls->trace) {
        gobj_trace_msg(gobj, "------- flush_clear_data(), userp %p", sskt->user_data);
    }

    while (TRUE) {
        gbuffer_t *gbuf = gbuffer_create(sskt->ytls->rx_buffer_size, sskt->ytls->rx_buffer_size);
        if (!gbuf) {
            gobj_log_error(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_MEMORY_ERROR,
                "msg",              "%s", "Failed to create gbuffer",
                NULL
            );
            return -1;
        }

        char *p = gbuffer_cur_wr_pointer(gbuf);
        int nread = mbedtls_ssl_read(&sskt->ssl, (unsigned char *)p, sskt->ytls->rx_buffer_size);

        if (sskt->ytls->trace) {
            gobj_trace_msg(gobj, "------- flush_clear_data() %d, userp %p", nread, sskt->user_data);
        }

        if (nread > 0) {
            gbuffer_set_wr(gbuf, nread);
            ret += sskt->on_clear_data_cb(sskt->user_data, gbuf);
        } else {
            gbuffer_decref(gbuf);

            if (nread == MBEDTLS_ERR_SSL_WANT_READ || nread == MBEDTLS_ERR_SSL_WANT_WRITE) {
                if (sskt->ytls->trace) {
                    gobj_trace_msg(gobj, "------- flush_clear_data WANT_READ/WANT_WRITE, userp %p", sskt->user_data);
                }
                break; // No more data available at this moment
            } else if (nread < 0) {
                char error_buf[256];
                mbedtls_strerror(nread, error_buf, sizeof(error_buf));
                gobj_log_error(gobj, 0,
                    "function",         "%s", __FUNCTION__,
                    "msgset",           "%s", MSGSET_SYSTEM_ERROR,
                    "msg",              "%s", "mbedtls_ssl_read() FAILED",
                    "error_code",       "%d", nread,
                    "error_message",    "%s", error_buf,
                    NULL
                );
                return -1111; // Mark as TLS error
            } else {
                // No more data
                break;
            }
        }
    }

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

    size_t len;
    while ((len = gbuffer_chunk(gbuf)) > 0) {
        const unsigned char *p = (const unsigned char *)gbuffer_cur_rd_pointer(gbuf); // Don't pop data, be sure it's written

        int written = mbedtls_ssl_read(&sskt->ssl, (unsigned char *)p, len);
        if (written < 0) {
            if (written == MBEDTLS_ERR_SSL_WANT_READ || written == MBEDTLS_ERR_SSL_WANT_WRITE) {
                if (sskt->ytls->trace) {
                    gobj_trace_msg(gobj, "------- decrypt_data WANT_READ/WANT_WRITE, userp %p", sskt->user_data);
                }
                continue; // Retry on WANT_READ or WANT_WRITE
            } else {
                char error_buf[256];
                mbedtls_strerror(written, error_buf, sizeof(error_buf));
                gobj_log_error(gobj, 0,
                    "function",         "%s", __FUNCTION__,
                    "msgset",           "%s", MSGSET_SYSTEM_ERROR,
                    "msg",              "%s", "mbedtls_ssl_read() FAILED",
                    "error_code",       "%d", written,
                    "error_message",    "%s", error_buf,
                    NULL
                );
                GBUFFER_DECREF(gbuf);
                return -1111; // Mark as TLS error
            }
        }

        gbuffer_get(gbuf, written); // Pop data

        if (sskt->ytls->trace) {
            gobj_trace_dump(gobj, (char *)p, len, "------- <== decrypt_data, userp %p", sskt->user_data);
        }

        if (mbedtls_ssl_handshake(&sskt->ssl)!=0) {
            if (do_handshake(sskt) < 0) {
                // Error already logged
                GBUFFER_DECREF(gbuf);
                return -1111; // Mark as TLS error
            }
        } else {
            int ret = flush_clear_data(sskt);
            if (ret < 0) {
                // Error already logged
                GBUFFER_DECREF(gbuf);
                return ret;
            }
        }
    }

    GBUFFER_DECREF(gbuf);
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

#endif /* CONFIG_YTLS_USE_MBEDTLS */
