/***********************************************************************
 *          OPENSSL.C
 *
 *          OpenSSL-specific code for the TLS/SSL layer
 *
 *          Copyright (c) 2018 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 *

HOWTO from https://gist.github.com/darrenjs/4645f115d10aa4b5cebf57483ec82eca

Running
-------

Running the program requires that a SSL certificate and private key are
available to be loaded. These can be generated using the 'openssl' program using
these steps:

1. Generate the private key, this is what we normally keep secret:

    openssl genrsa -des3 -passout pass:x -out server.pass.key 2048
    openssl rsa -passin pass:x -in server.pass.key -out server.key
    rm -f server.pass.key

2. Next generate the CSR.  We can leave the password empty when prompted
   (because this is self-sign):

    openssl req -new -key server.key -out server.csr

3. Next generate the self signed certificate:

    openssl x509 -req -sha256 -days 365 -in server.csr -signkey server.key -out server.crt
    rm -f server.csr

The openssl program can also be used to connect to this program as an SSL
client. Here's an example command (assuming we're using port 55555):

    openssl s_client -connect 127.0.0.1:55555 -msg -debug -state -showcerts


Flow of encrypted & unencrypted bytes
-------------------------------------

This diagram shows how the read and write memory BIO's (rbio & wbio) are
associated with the socket read and write respectively.  On the inbound flow
(data into the program) bytes are read from the socket and copied into the rbio
via BIO_write.  This represents the the transfer of encrypted data into the SSL
object. The unencrypted data is then obtained through calling SSL_read.  The
reverse happens on the outbound flow to convey unencrypted user data into a
socket write of encrypted data.


  +------+                                    +-----+
  |......|--> read(fd) --> BIO_write(rbio) -->|.....|--> SSL_read(ssl)  --> IN
  |......|                                    |.....|
  |.sock.|                                    |.SSL.|
  |......|                                    |.....|
  |......|<-- write(fd) <-- BIO_read(wbio) <--|.....|<-- SSL_write(ssl) <-- OUT
  +------+                                    +-----+

          |                                  |       |                     |
          |<-------------------------------->|       |<------------------->|
          |         encrypted bytes          |       |  unencrypted bytes  |


***********************************************************************/
#define OPENSSL_API_COMPAT 30100
#include <openssl/ssl.h>
#include <openssl/engine.h>
#include <openssl/rand.h>
#include <openssl/err.h>

#include <kwid.h>
#include "../ytls.h"
#include "openssl.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Structures
 ***************************************************************/
typedef struct ytls_s {
    api_tls_t *api_tls;     // HACK must be the first item
    BOOL server;
    SSL_CTX *ctx;
    BOOL trace;
    size_t rx_buffer_size;
    hgobj gobj;
} ytls_t;

typedef struct sskt_s {
    ytls_t *ytls;
    SSL *ssl;
    //BIO *internal_bio, *network_bio;
    BIO *rbio; /* SSL reads from, we write to. */
    BIO *wbio; /* SSL writes to, we read from. */
    BOOL handshake_informed;
    int (*on_handshake_done_cb)(void *user_data, int error);
    int (*on_clear_data_cb)(void *user_data, gbuffer_t *gbuf);
    int (*on_encrypted_data_cb)(void *user_data, gbuffer_t *gbuf);
    void *user_data;
    char last_error[256];
    int error;
    char rx_bf[16*1024];
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
    "OPENSSL",
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
BOOL __initialized__ = FALSE;

/***************************************************************************
 *
 ***************************************************************************/
static const char *ssl_msg_type(int ssl_ver, int msg)
{
    if(ssl_ver == SSL3_VERSION_MAJOR) {
        switch(msg) {
            case SSL3_MT_HELLO_REQUEST:
                return "Hello request";
            case SSL3_MT_CLIENT_HELLO:
                return "Client hello";
            case SSL3_MT_SERVER_HELLO:
                return "Server hello";
            case SSL3_MT_NEWSESSION_TICKET:
                return "Newsession Ticket";
            case SSL3_MT_CERTIFICATE:
                return "Certificate";
            case SSL3_MT_SERVER_KEY_EXCHANGE:
                return "Server key exchange";
            case SSL3_MT_CLIENT_KEY_EXCHANGE:
                return "Client key exchange";
            case SSL3_MT_CERTIFICATE_REQUEST:
                return "Request CERT";
            case SSL3_MT_SERVER_DONE:
                return "Server finished";
            case SSL3_MT_CERTIFICATE_VERIFY:
                return "CERT verify";
            case SSL3_MT_FINISHED:
                return "Finished";
            case SSL3_MT_CERTIFICATE_STATUS:
                return "Certificate Status";
            case SSL3_MT_ENCRYPTED_EXTENSIONS:
                return "Encrypted Extensions";
            case SSL3_MT_END_OF_EARLY_DATA:
                return "End of early data";
            case SSL3_MT_KEY_UPDATE:
                return "Key update";
            case SSL3_MT_NEXT_PROTO:
                return "Next protocol";
            case SSL3_MT_MESSAGE_HASH:
                return "Message hash";
        }
    }
    return "Unknown";
}

static const char *tls_rt_type(int type)
{
    switch(type) {
    case SSL3_RT_HEADER:
        return "TLS header";
    case SSL3_RT_CHANGE_CIPHER_SPEC:
        return "TLS change cipher";
    case SSL3_RT_ALERT:
        return "TLS alert";
    case SSL3_RT_HANDSHAKE:
        return "TLS handshake";
    case SSL3_RT_APPLICATION_DATA:
        return "TLS app data";
    default:
        return "TLS Unknown";
    }
}

/*
 *  Our callback from the SSL/TLS layers.
 *  Copied from curl
 */
PRIVATE void ssl_tls_trace(
    int direction,
    int ssl_ver,
    int content_type,
    const void *buf,
    size_t len,
    SSL *ssl,
    void *userp
)
{
    hgobj gobj = 0;
    char unknown[32];
    const char *verstr = NULL;

    switch(ssl_ver) {
    case SSL2_VERSION:
        verstr = "SSLv2";
        break;
    case SSL3_VERSION:
        verstr = "SSLv3";
        break;
    case TLS1_VERSION:
        verstr = "TLSv1.0";
        break;
    case TLS1_1_VERSION:
        verstr = "TLSv1.1";
        break;
    case TLS1_2_VERSION:
        verstr = "TLSv1.2";
        break;
    case TLS1_3_VERSION:
        verstr = "TLSv1.3";
        break;
    case 0:
        break;
    default:
        snprintf(unknown, sizeof(unknown), "(%x)", ssl_ver);
        verstr = unknown;
        break;
    }

    /* Log progress for interesting records only (like Handshake or Alert), skip
     * all raw record headers (content_type == SSL3_RT_HEADER or ssl_ver == 0).
     * For TLS 1.3, skip notification of the decrypted inner Content Type.
     */
    if(ssl_ver
#ifdef SSL3_RT_INNER_CONTENT_TYPE
         && content_type != SSL3_RT_INNER_CONTENT_TYPE
#endif
        ) {
        const char *msg_name, *tls_rt_name;
        int msg_type;

        /* the info given when the version is zero is not that useful for us */

        ssl_ver >>= 8; /* check the upper 8 bits only below */

        /* SSLv2 doesn't seem to have TLS record-type headers, so OpenSSL
         * always pass-up content-type as 0. But the interesting message-type
         * is at 'buf[0]'.
         */
        if(ssl_ver == SSL3_VERSION_MAJOR && content_type)
            tls_rt_name = tls_rt_type(content_type);
        else
            tls_rt_name = "";

        if(content_type == SSL3_RT_CHANGE_CIPHER_SPEC) {
            msg_type = *(char *)buf;
            msg_name = "Change cipher spec";
        }
        else if(content_type == SSL3_RT_ALERT) {
            msg_type = (((char *)buf)[0] << 8) + ((char *)buf)[1];
            msg_name = SSL_alert_desc_string_long(msg_type);
        }
        else {
            msg_type = *(char *)buf;
            msg_name = ssl_msg_type(ssl_ver, msg_type);
        }

        gobj_trace_dump(gobj, buf, len,
            "%s (%s), %s, %s (%d), userp %p",
            verstr, direction?"OUT":"IN",
            tls_rt_name, msg_name, msg_type, userp
        );
    } else {
        gobj_trace_dump(gobj, buf, len,
            "%s ssl_ver %d, content_type %d, userp %p",
            direction?"OUT":"IN", ssl_ver, content_type, userp
        );
    }
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

    /*--------------------------------*
     *      Init OPENSSL
     *--------------------------------*/
    if(!__initialized__) {
        __initialized__ = TRUE;
        SSL_library_init();
        OpenSSL_add_all_algorithms();
    }
    const SSL_METHOD *method = 0;
    if(server) {
        method = TLS_server_method();        /* Create new server-method instance */
    } else {
        method = TLS_client_method();        /* Create new client-method instance */
    }
    SSL_CTX *ctx = SSL_CTX_new(method);         /* Create new context */
    if(!ctx) {
        unsigned long err = ERR_get_error();
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "SSL_CTX_new() FAILED",
            "error",        "%s", ERR_error_string(err, NULL),
            NULL
        );
        return 0;
    }

    /*--------------------------------*
     *      Options
     *  Lo dejaré tal cual nginx
     *--------------------------------*/
    SSL_CTX_set_options(ctx, SSL_OP_SINGLE_DH_USE);
    /* only in 0.9.8m+ */
    SSL_CTX_clear_options(ctx, SSL_OP_NO_SSLv2|SSL_OP_NO_SSLv3|SSL_OP_NO_TLSv1);
    SSL_CTX_set_min_proto_version(ctx, 0);
    SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);
    SSL_CTX_set_read_ahead(ctx, 1);

    /*--------------------------------*
     *      Alloc memory
     *--------------------------------*/
    ytls_t *ytls = GBMEM_MALLOC(sizeof(ytls_t));
    if(!ytls) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_MEMORY_ERROR,
            "msg",              "%s", "no memory for sizeof(ytls_t)",
            "sizeof(ytls_t)",   "%d", sizeof(ytls_t),
            NULL
        );
        return 0;
    }

    ytls->api_tls = &api_tls;
    ytls->server = server;
    ytls->ctx = ctx;
    ytls->gobj = gobj;

    /* the SSL trace callback is only used for verbose logging */
    ytls->trace = kw_get_bool(gobj, jn_config, "trace", 0, KW_WILD_NUMBER);

    if(ytls->trace) {
        SSL_CTX_set_msg_callback(ytls->ctx, ssl_tls_trace);
        SSL_CTX_set_msg_callback_arg(ytls->ctx, ytls);
    }

    const char *ssl_certificate = kw_get_str(gobj,
        jn_config, "ssl_certificate", "", server?KW_REQUIRED:0
    );
    const char *ssl_certificate_key = kw_get_str(gobj,
        jn_config, "ssl_certificate_key", "", server?KW_REQUIRED:0
    );
    const char *ssl_ciphers = kw_get_str(gobj,
        jn_config,
        "ssl_ciphers",
        "HIGH:!aNULL:!kRSA:!PSK:!SRP:!MD5:!RC4",
        0
    );
    ytls->rx_buffer_size = kw_get_int(gobj,jn_config, "rx_buffer_size", 32*1024, 0);

    const char *ssl_trusted_certificate = kw_get_str(gobj,
        jn_config, "ssl_trusted_certificate", "", 0
    );
    int ssl_verify_depth = kw_get_int(gobj,jn_config, "ssl_verify_depth", 1, 0);

    if(SSL_CTX_set_cipher_list(ytls->ctx, ssl_ciphers)<0) {
        unsigned long err = ERR_get_error();
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "SSL_CTX_set_cipher_list() FAILED",
            "error",        "%s", ERR_error_string(err, NULL),
            NULL
        );
    }

    if(server) {
        if(SSL_CTX_use_certificate_file(ytls->ctx, ssl_certificate, SSL_FILETYPE_PEM)!=1) {
            unsigned long err = ERR_get_error();
            gobj_log_error(gobj, LOG_OPT_EXIT_ZERO,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "SSL_CTX_use_certificate_file() FAILED",
                "error",        "%s", ERR_error_string(err, NULL),
                "cert",         "%s", ssl_certificate,
                NULL
            );
        }
        if(SSL_CTX_use_PrivateKey_file(ytls->ctx, ssl_certificate_key, SSL_FILETYPE_PEM)!=1) {
            unsigned long err = ERR_get_error();
            gobj_log_error(gobj, LOG_OPT_EXIT_ZERO,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "SSL_CTX_use_PrivateKey_file() FAILED",
                "error",        "%s", ERR_error_string(err, NULL),
                "cert",         "%s", ssl_certificate_key,
                NULL
            );
        }

        if(!empty_string(ssl_trusted_certificate)) {
            SSL_CTX_set_verify_depth(ctx, ssl_verify_depth);
            if(SSL_CTX_load_verify_locations(ctx, ssl_trusted_certificate, NULL)!=1) {
                unsigned long err = ERR_get_error();
                gobj_log_error(gobj, LOG_OPT_EXIT_ZERO,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "SSL_CTX_load_verify_locations() FAILED",
                    "error",        "%s", ERR_error_string(err, NULL),
                    "cert",         "%s", ssl_trusted_certificate,
                    NULL
                );
            }
        }

        /*
         * SSL_CTX_load_verify_locations() may leave errors in the error queue
         * while returning success
         */
        ERR_clear_error();

    } else {
        // TODO SSL_set_tlsext_host_name : "yuneta.io"
    }

    return (hytls)ytls;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void cleanup(hytls ytls_)
{
    ytls_t *ytls = ytls_;

    // TODO manten una lista de sskt y cierralos

    SSL_CTX_free(ytls->ctx);

    /* Removes all digests and ciphers */
    EVP_cleanup();

    GBMEM_FREE(ytls);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE const char *version(hytls ytls)
{
    return OpenSSL_version(OPENSSL_VERSION);
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
    ytls_t *ytls = ytls_;
    hgobj gobj = ytls->gobj;

    /*--------------------------------*
     *      Alloc memory
     *--------------------------------*/
    sskt_t *sskt = GBMEM_MALLOC(sizeof(sskt_t));
    if(!sskt) {
        gobj_log_error(gobj, 0,
            "gobj",             "%s", __FILE__,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_MEMORY_ERROR,
            "msg",              "%s", "no memory for sizeof(sskt_t)",
            "sizeof(sskt_t)",   "%d", sizeof(sskt_t),
            NULL
        );
        return 0;
    }

    sskt->ytls = ytls;
    sskt->on_handshake_done_cb = on_handshake_done_cb;
    sskt->on_clear_data_cb = on_clear_data_cb;
    sskt->on_encrypted_data_cb = on_encrypted_data_cb;
    sskt->user_data = user_data;

    sskt->ssl = SSL_new(ytls->ctx);
    if(!sskt->ssl) {
        sskt->error = ERR_get_error();
        ERR_error_string_n(sskt->error, sskt->last_error, sizeof(sskt->last_error));
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "SSL_new() FAILED",
            "error",        "%d", (int)sskt->error,
            "serror",       "%s", sskt->last_error,
            NULL
        );
        GBMEM_FREE(sskt)
        return 0;
    }

    if(ytls->trace) {
        SSL_set_msg_callback(sskt->ssl, ssl_tls_trace);
        SSL_set_msg_callback_arg(sskt->ssl, ytls);
    } else {
        SSL_set_msg_callback(sskt->ssl, 0);
    }

    if(ytls->server) {
        SSL_set_accept_state(sskt->ssl);
    } else {
        SSL_set_connect_state(sskt->ssl);
    }

// Esto jode?    SSL_set_options(sskt->ssl, SSL_OP_NO_RENEGOTIATION); // New to openssl 1.1.1

    sskt->rbio = BIO_new(BIO_s_mem());
    sskt->wbio = BIO_new(BIO_s_mem());

    SSL_set_bio(sskt->ssl, sskt->rbio, sskt->wbio);

    if(do_handshake(sskt)<0) {
        SSL_free(sskt->ssl);   /* free the SSL object and its BIO's */
        GBMEM_FREE(sskt)
        return 0;
    }

    return sskt;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void shutdown_sskt(hsskt sskt_)
{
    sskt_t *sskt = sskt_;

    SSL_shutdown(sskt->ssl);
    flush_encrypted_data(sskt);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void free_secure_filter(hsskt sskt_)
{
    sskt_t *sskt = sskt_;

    SSL_free(sskt->ssl);   /* free the SSL object and its BIO's */
    GBMEM_FREE(sskt)
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void set_trace(hsskt sskt_, BOOL set)
{
    sskt_t *sskt = sskt_;
    hgobj gobj = sskt->ytls->gobj;
    sskt->ytls->trace = set?TRUE:FALSE;

    if(sskt->ytls->trace) {
        SSL_CTX_set_msg_callback(sskt->ytls->ctx, ssl_tls_trace);
        SSL_CTX_set_msg_callback_arg(sskt->ytls->ctx, sskt->ytls);
        if(sskt->ssl) {
            SSL_set_msg_callback(sskt->ssl, ssl_tls_trace);
            SSL_set_msg_callback_arg(sskt->ssl, sskt->ytls);
        }

    } else {
        SSL_CTX_set_msg_callback(sskt->ytls->ctx, 0);
        SSL_set_msg_callback(sskt->ssl, 0);
    }
}

/***************************************************************************
    Do handshake
 ***************************************************************************/
PRIVATE int do_handshake(hsskt sskt_)
{
    sskt_t *sskt = sskt_;
    hgobj gobj = sskt->ytls->gobj;

    if(sskt->ytls->trace) {
        gobj_trace_msg(gobj, "------- do_handshake, userp %p", sskt->user_data);
    }

    int ret = SSL_do_handshake(sskt->ssl);
    if(ret <= 0)  {
        /*
        - return 0
            The TLS/SSL handshake was not successful but was shut down controlled
            and by the specifications of the TLS/SSL protocol.
            Call SSL_get_error() with the return value ret to find out the reason.

        - return < 0
            The TLS/SSL handshake was not successful because a fatal error occurred
            either at the protocol level or a connection failure occurred.
            The shutdown was not clean.
            It can also occur if action is needed to continue the operation for non-blocking BIOs.
            Call SSL_get_error() with the return value ret to find out the reason.
        */
        int detail = SSL_get_error(sskt->ssl, ret);
        switch(detail) {
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            if(sskt->ytls->trace) {
                gobj_trace_msg(gobj, "------- encrypt_data: %s, userp %p",
                    ret==SSL_ERROR_WANT_READ?"SSL_ERROR_WANT_READ":"SSL_ERROR_WANT_WRITE",
                    sskt->user_data
                );
            }
            flush_encrypted_data(sskt);
            flush_clear_data(sskt);
            break;

        default:
            sskt->error = ERR_get_error();
            ERR_error_string_n(sskt->error, sskt->last_error, sizeof(sskt->last_error));
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "SSL_do_handshake() FAILED",
                "ret",          "%d", (int)detail,
                "error",        "%d", (int)sskt->error,
                "serror",       "%s", sskt->last_error,
                NULL
            );
            sskt->on_handshake_done_cb(sskt->user_data, -1);
            return -1;
        }
    }

    flush_encrypted_data(sskt);

    BOOL handshake_end = SSL_is_init_finished(sskt->ssl);
    if(ret==1 || handshake_end) { // Viene los dos a la vez
        /*
        - return 1
            The TLS/SSL handshake was successfully completed,
            a TLS/SSL connection has been established.
        */
        if(!sskt->handshake_informed) {
            sskt->handshake_informed = TRUE;
            sskt->on_handshake_done_cb(sskt->user_data, 0);
        }
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int flush_encrypted_data(sskt_t *sskt)
{
    hgobj gobj = sskt->ytls->gobj;

    if(sskt->ytls->trace) {
        gobj_trace_msg(gobj, "------- flush_encrypted_data(), userp %p", sskt->user_data);
    }
    /*
    BIO_read() return
    All these functions return either the amount of data successfully read or written
    (if the return value is positive)
    or that no data was successfully read or written if the result is 0 or -1.
    If the return value is -2 then the operation is not implemented in the specific BIO type.

    A 0 or -1 return is not necessarily an indication of an error.
    In particular when the source/sink is non-blocking or of a certain type
    it may merely be an indication that no data is currently available and
    that the application should retry the operation later.
    */

    long pending;
    while((pending = BIO_pending(sskt->wbio))>0) {
        gbuffer_t *gbuf = gbuffer_create(pending, pending);
        if(!gbuf) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MEMORY_ERROR,
                "msg",          "%s", "No memory for BIO_pending",
                NULL
            );
            return -1;
        }
        char *p = gbuffer_cur_wr_pointer(gbuf);
        int ret = BIO_read(sskt->wbio, p, pending);
        if(sskt->ytls->trace) {
            gobj_trace_msg(gobj, "------- flush_encrypted_data() %d, userp %p", ret, sskt->user_data);
        }
        if(ret > 0) {
            gbuffer_set_wr(gbuf, ret);
            sskt->on_encrypted_data_cb(sskt->user_data, gbuf);
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
)
{
    sskt_t *sskt = sskt_;
    hgobj gobj = sskt->ytls->gobj;

    if(!SSL_is_init_finished(sskt->ssl)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "TLS handshake PENDING",
            NULL
        );
        GBUFFER_DECREF(gbuf)
        return -1;
    }

    size_t len;
    while(sskt->ssl && (len = gbuffer_chunk(gbuf))>0) {
        char *p = gbuffer_cur_rd_pointer(gbuf);    // Don't pop data, be sure it's written
        int written = SSL_write(sskt->ssl, p, len);
        if(written <= 0) {
            int ret = SSL_get_error(sskt->ssl, written);
            switch(ret) {
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE:
                if(sskt->ytls->trace) {
                    gobj_trace_msg(gobj, "------- encrypt_data: %s, userp %p",
                        ret==SSL_ERROR_WANT_READ?"SSL_ERROR_WANT_READ":"SSL_ERROR_WANT_WRITE",
                        sskt->user_data
                    );
                }
                flush_encrypted_data(sskt);
                flush_clear_data(sskt);
                continue;

            default:
                sskt->error = ERR_get_error();
                ERR_error_string_n(sskt->error, sskt->last_error, sizeof(sskt->last_error));
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                    "msg",          "%s", "SSL_write() FAILED",
                    "ret",          "%d", (int)ret,
                    "error",        "%d", (int)sskt->error,
                    "serror",       "%s", sskt->last_error,
                    NULL
                );
                GBUFFER_DECREF(gbuf)
                return -1;
            }
            break;
        }
        gbuffer_get(gbuf, written);    // Pop data

        if(sskt->ytls->trace) {
            gobj_trace_dump(gobj, p, len, "------- ==> encrypt_data DATA, userp %p", sskt->user_data);
        }
        gbuffer_get(gbuf, written);    // Pop data
        if(flush_encrypted_data(sskt)<0) {
            // Error already logged
            GBUFFER_DECREF(gbuf)
            return -1;
        }
    }
    GBUFFER_DECREF(gbuf)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int flush_clear_data(sskt_t *sskt)
{
    hgobj gobj = sskt->ytls->gobj;

    if(sskt->ytls->trace) {
        gobj_trace_msg(gobj, "------- flush_clear_data(), userp %p", sskt->user_data);
    }
    while(sskt->ssl) {
        gbuffer_t *gbuf = gbuffer_create(sskt->ytls->rx_buffer_size, sskt->ytls->rx_buffer_size);
        char *p = gbuffer_cur_wr_pointer(gbuf);
        int nread = SSL_read(sskt->ssl, p, sskt->ytls->rx_buffer_size);
        if(sskt->ytls->trace) {
            gobj_trace_msg(gobj, "------- flush_clear_data() %d, userp %p", nread, sskt->user_data);
        }
        if(nread <= 0) {
            sskt->error = ERR_get_error();
            if(sskt->error < 0) {
                ERR_error_string_n(sskt->error, sskt->last_error, sizeof(sskt->last_error));
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                    "msg",          "%s", "SSL_read() FAILED",
                    "ret",          "%d", (int)nread,
                    "error",        "%d", (int)sskt->error,
                    "serror",       "%s", sskt->last_error,
                    NULL
                );
                GBUFFER_DECREF(gbuf)
                return -1;
            } else {
                // no more data
                GBUFFER_DECREF(gbuf)
                break;
            }
        }

        // Callback clear data
        gbuffer_set_wr(gbuf, nread);
        sskt->on_clear_data_cb(sskt->user_data, gbuf);
    }
    return 0;
}

/***************************************************************************
    Use this function decrypt encrypted data.
    The clear data will be returned in on_clear_data_cb callback.
 ***************************************************************************/
PRIVATE int decrypt_data(
    hsskt sskt_,
    gbuffer_t *gbuf // owned
)
{
    sskt_t *sskt = sskt_;
    hgobj gobj = sskt->ytls->gobj;

    size_t len;
    while((len = gbuffer_chunk(gbuf))>0) {
        char *p = gbuffer_cur_rd_pointer(gbuf);    // Don't pop data, be sure it's written

        int written = BIO_write(sskt->rbio, p, len);
        if(written < 0) {
            sskt->error = ERR_get_error();
            ERR_error_string_n(sskt->error, sskt->last_error, sizeof(sskt->last_error));
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "BIO_write() FAILED",
                "ret",          "%d", (int)written,
                "error",        "%d", (int)sskt->error,
                "serror",       "%s", sskt->last_error,
                NULL
            );
            GBUFFER_DECREF(gbuf)
            return -1;
        }

        gbuffer_get(gbuf, written);    // Pop data

        if(sskt->ytls->trace) {
            gobj_trace_dump(gobj, p, len, "------- <== decrypt_data, userp %p", sskt->user_data);
        }
        if(!SSL_is_init_finished(sskt->ssl)) {
            if(do_handshake(sskt)<0) {
                // Error already logged
                GBUFFER_DECREF(gbuf)
                return -1;
            }
        } else {
            if(flush_clear_data(sskt)<0) {
                // Error already logged
                GBUFFER_DECREF(gbuf)
                return -1;
            }
        }
    }
    GBUFFER_DECREF(gbuf)
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
PUBLIC api_tls_t *openssl_api_tls(void)
{
    return &api_tls;
}
