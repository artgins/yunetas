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
#include <yuneta_config.h>
// #define CONFIG_HAVE_OPENSSL // TODO TEST remove

#ifdef CONFIG_HAVE_OPENSSL

#define OPENSSL_API_COMPAT 30100
#include <openssl/ssl.h>
#include <openssl/engine.h>
#include <openssl/err.h>
#include <openssl/bn.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <time.h>

#include <kwid.h>
#include <helpers.h>
#include "../ytls.h"
#include "openssl.h"

/***************************************************************
 *              Constants
 ***************************************************************/
#define HANDSHAKE_TRANSCRIPT_MAX (16*1024) // Max peer bytes retained for forensic dump on handshake failure

/***************************************************************
 *              Structures
 ***************************************************************/
typedef struct ytls_s {
    api_tls_t *api_tls;     // HACK must be the first item
    BOOL server;
    SSL_CTX *ctx;
    BOOL trace_tls;
    size_t rx_buffer_size;
    char ssl_server_name[256]; // Server name for SNI (client-side TLS only)
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
    unsigned long error; // holds ERR_get_error() (unsigned long per OpenSSL API)
    BOOL *alive; // Points to stack var in flush_clear_data; set to FALSE when freed mid-callback
    gbuffer_t *handshake_transcript; // Raw peer bytes captured during handshake; dumped on failure, released on success
} sskt_t;

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE int flush_clear_data(sskt_t *sskt);
PRIVATE void capture_handshake_bytes(sskt_t *sskt, const void *bytes, size_t len);
PRIVATE void dump_handshake_transcript_on_fail(sskt_t *sskt);

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
PRIVATE json_t *get_cert_info(hytls ytls);
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
    reload_certificates,
    get_cert_info,
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

        gobj_trace_msg(gobj, "%s (%s), %s, %s (%d), userp %p, len %zu",
            verstr, direction?"OUT":"IN",
            tls_rt_name, msg_name, msg_type, userp, len
        );
    } else {
        gobj_trace_msg(gobj, "%s ssl_ver %d, content_type %d, userp %p, len %zu",
            direction?"OUT":"IN", ssl_ver, content_type, userp, len
        );
    }
}

/***************************************************************************
 *  Verify callbacks. Both log the chain error (no silent verify failures).
 *  The strict one (REQUIRED) propagates preverify_ok so a bad cert aborts the
 *  handshake; the tolerant one (OPTIONAL) returns 1 so the handshake completes
 *  and the failure is surfaced post-handshake via SSL_get_verify_result().
 ***************************************************************************/
PRIVATE void log_chain_verify_error(X509_STORE_CTX *x509_ctx)
{
    int err = X509_STORE_CTX_get_error(x509_ctx);
    if(err == X509_V_OK) {
        return;
    }
    gobj_log_warning(0, 0,
        "function",     "%s", "ytls_verify_callback",
        "msgset",       "%s", MSGSET_OPENSSL,
        "msg",          "%s", "TLS peer certificate chain verify error",
        "error",        "%s", X509_verify_cert_error_string(err),
        "depth",        "%d", X509_STORE_CTX_get_error_depth(x509_ctx),
        NULL
    );
}
PRIVATE int verify_cb_strict(int preverify_ok, X509_STORE_CTX *x509_ctx)
{
    if(!preverify_ok) {
        log_chain_verify_error(x509_ctx);
    }
    return preverify_ok;
}
PRIVATE int verify_cb_tolerant(int preverify_ok, X509_STORE_CTX *x509_ctx)
{
    if(!preverify_ok) {
        log_chain_verify_error(x509_ctx);
    }
    return 1;   /* tolerate; reported via SSL_get_verify_result() post-handshake */
}

/***************************************************************************
 *  Build a fully-configured SSL_CTX from jn_config.
 *
 *  If fatal is TRUE, fatal errors use LOG_OPT_EXIT_ZERO (used on first init
 *  at yuno startup). For hot-reload, set fatal=FALSE so that a failure does
 *  not kill the yuno — the caller keeps the previous context intact.
 *
 *  Returns a new SSL_CTX * (refcount=1) on success, or NULL on failure.
 ***************************************************************************/
PRIVATE SSL_CTX *build_ssl_ctx(
    hgobj gobj,
    json_t *jn_config,
    BOOL server,
    BOOL fatal,
    BOOL trace_tls,
    void *trace_arg
)
{
    const SSL_METHOD *method = server ? TLS_server_method() : TLS_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if(!ctx) {
        unsigned long err = ERR_get_error();
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPENSSL,
            "msg",          "%s", "SSL_CTX_new() FAILED",
            "error",        "%s", ERR_error_string(err, NULL),
            NULL
        );
        return NULL;
    }

    SSL_CTX_set_options(ctx, SSL_OP_SINGLE_DH_USE);
    SSL_CTX_clear_options(ctx, SSL_OP_NO_SSLv2|SSL_OP_NO_SSLv3|SSL_OP_NO_TLSv1);

    /*
     *  Protocol floor. Secure-by-default: when "ssl_min_version" is unset the
     *  floor is TLS1.2 (matches the mbedTLS backend, which cannot go lower).
     *  The knob can RAISE it (TLS1.3) or, for legacy IoT gates that need it,
     *  LOWER it to SSLv3 / TLS1.0 (alias TLS1) / TLS1.1 — paired with an
     *  ssl_ciphers "@SECLEVEL=0" so OpenSSL actually negotiates the old suite.
     *  A floor below TLS1.2 is an explicit, auditable downgrade and is logged
     *  (yuneta "no silent permissive defaults"); and any peer rejected by the
     *  floor leaves a trace in do_handshake() below.
     *  Accepts: SSLv3, TLS1.0 (alias TLS1), TLS1.1, TLS1.2, TLS1.3.
     */
    int min_proto_version = TLS1_2_VERSION;
    const char *ssl_min_version = kw_get_str(gobj, jn_config, "ssl_min_version", "", 0);
    if(!empty_string(ssl_min_version)) {
        if(strcasecmp(ssl_min_version, "SSLv3")==0) {
            min_proto_version = SSL3_VERSION;
        } else if(strcasecmp(ssl_min_version, "TLS1.0")==0 ||
                  strcasecmp(ssl_min_version, "TLS1")==0) {
            min_proto_version = TLS1_VERSION;
        } else if(strcasecmp(ssl_min_version, "TLS1.1")==0) {
            min_proto_version = TLS1_1_VERSION;
        } else if(strcasecmp(ssl_min_version, "TLS1.2")==0) {
            min_proto_version = TLS1_2_VERSION;
        } else if(strcasecmp(ssl_min_version, "TLS1.3")==0) {
            min_proto_version = TLS1_3_VERSION;
        } else {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_OPENSSL,
                "msg",          "%s", "Unknown ssl_min_version, keeping TLS1.2 floor",
                "ssl_min_version", "%s", ssl_min_version,
                NULL
            );
        }
    }
    if(min_proto_version != 0 && min_proto_version < TLS1_2_VERSION) {
        /*
         *  Explicit legacy downgrade. Make the gate enumerable in the logs so
         *  the reduced-security surface is auditable (not silent).
         */
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPENSSL,
            "msg",          "%s", "TLS gate configured with a legacy floor below TLS1.2 (IoT-compat downgrade)",
            "ssl_min_version", "%s", ssl_min_version,
            "server",       "%d", server?1:0,
            NULL
        );
    }
    SSL_CTX_set_min_proto_version(ctx, min_proto_version);
    SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);

    /*
     *  TLS renegotiation. Secure-by-default: DISABLED (closes the
     *  renegotiation-based DoS/abuse surface; TLS1.3 has no renegotiation at
     *  all). The "ssl_disable_renegotiation" knob defaults to true now; set it
     *  to false to re-enable renegotiation on a gate that genuinely needs it —
     *  that is an explicit, auditable downgrade and is logged (yuneta "no
     *  silent" norm).
     */
    if(kw_get_bool(gobj, jn_config, "ssl_disable_renegotiation", 1, 0)) {
        SSL_CTX_set_options(ctx, SSL_OP_NO_RENEGOTIATION);
    } else {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPENSSL,
            "msg",          "%s", "TLS renegotiation explicitly enabled (DoS/abuse surface)",
            "server",       "%d", server?1:0,
            NULL
        );
    }

    SSL_CTX_set_read_ahead(ctx, 1);

    if(trace_tls) {
        SSL_CTX_set_msg_callback(ctx, ssl_tls_trace);
        SSL_CTX_set_msg_callback_arg(ctx, trace_arg);
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
    const char *ssl_trusted_certificate = kw_get_str(gobj,
        jn_config, "ssl_trusted_certificate", "", 0
    );
    int ssl_verify_depth = kw_get_int(gobj, jn_config, "ssl_verify_depth", 1, 0);

    if(SSL_CTX_set_cipher_list(ctx, ssl_ciphers)<0) {
        unsigned long err = ERR_get_error();
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPENSSL,
            "msg",          "%s", "SSL_CTX_set_cipher_list() FAILED",
            "error",        "%s", ERR_error_string(err, NULL),
            NULL
        );
    }

    if(server) {
        log_opt_t log_fatal = fatal ? LOG_OPT_EXIT_ZERO : 0;

        if(SSL_CTX_use_certificate_chain_file(ctx, ssl_certificate)!=1) {
            unsigned long err = ERR_get_error();
            gobj_log_error(gobj, log_fatal,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_OPENSSL,
                "msg",          "%s", "SSL_CTX_use_certificate_chain_file() FAILED",
                "error",        "%s", ERR_error_string(err, NULL),
                "cert",         "%s", ssl_certificate,
                NULL
            );
            if(!fatal) {
                SSL_CTX_free(ctx);
                return NULL;
            }
        }
        if(SSL_CTX_use_PrivateKey_file(ctx, ssl_certificate_key, SSL_FILETYPE_PEM)!=1) {
            unsigned long err = ERR_get_error();
            gobj_log_error(gobj, log_fatal,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_OPENSSL,
                "msg",          "%s", "SSL_CTX_use_PrivateKey_file() FAILED",
                "error",        "%s", ERR_error_string(err, NULL),
                "cert",         "%s", ssl_certificate_key,
                NULL
            );
            if(!fatal) {
                SSL_CTX_free(ctx);
                return NULL;
            }
        }

        /* Sanity-check: private key must match certificate. */
        if(SSL_CTX_check_private_key(ctx) != 1) {
            unsigned long err = ERR_get_error();
            gobj_log_error(gobj, log_fatal,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_OPENSSL,
                "msg",          "%s", "SSL_CTX_check_private_key() FAILED: key does not match cert",
                "error",        "%s", ERR_error_string(err, NULL),
                "cert",         "%s", ssl_certificate,
                "cert_key",     "%s", ssl_certificate_key,
                NULL
            );
            if(!fatal) {
                SSL_CTX_free(ctx);
                return NULL;
            }
        }

        /* Certificate/key ops may leave non-fatal entries in the error queue. */
        ERR_clear_error();
    }
    /* (client SNI is per-connection: applied on the SSL object in
     *  new_secure_filter(), not here on the SSL_CTX.) */

    /*
     *  Peer verification (both client and server). Computed default mirrors
     *  the mbedTLS backend:
     *    - no CA configured -> NONE     (preserves historical behavior; IoT /
     *                                    self-signed / PSK keep working)
     *    - server + CA      -> OPTIONAL (request + verify the client cert if
     *                                    presented; tolerate absent)
     *    - client + CA      -> REQUIRED (validate the server cert + hostname;
     *                                    closes the MITM hole)
     *  The "ssl_verify_mode" knob (required/optional/none) overrides; an
     *  optional "ssl_use_system_ca" bool adds the OS trust store. Nothing is
     *  silent: a tolerated failure (OPTIONAL) is logged in do_handshake() via
     *  SSL_get_verify_result(), a rejected one by the verify callback, and a
     *  client left unverified is logged here. Hostname is checked
     *  per-connection in new_secure_filter() (SSL_set1_host).
     */
    {
        BOOL use_system_ca = kw_get_bool(gobj, jn_config, "ssl_use_system_ca", 0, 0);
        BOOL has_ca = !empty_string(ssl_trusted_certificate) || use_system_ca;

        int vmode; /* 0=NONE 1=OPTIONAL 2=REQUIRED */
        const char *ssl_verify_mode = kw_get_str(gobj, jn_config, "ssl_verify_mode", "", 0);
        if(!empty_string(ssl_verify_mode)) {
            if(strcasecmp(ssl_verify_mode, "none")==0) {
                vmode = 0;
            } else if(strcasecmp(ssl_verify_mode, "optional")==0) {
                vmode = 1;
            } else if(strcasecmp(ssl_verify_mode, "required")==0) {
                vmode = 2;
            } else {
                vmode = has_ca ? (server ? 1 : 2) : 0;
                gobj_log_error(gobj, 0,
                    "function",         "%s", __FUNCTION__,
                    "msgset",           "%s", MSGSET_OPENSSL,
                    "msg",              "%s", "Unknown ssl_verify_mode, keeping computed default",
                    "ssl_verify_mode",  "%s", ssl_verify_mode,
                    NULL
                );
            }
        } else {
            vmode = has_ca ? (server ? 1 : 2) : 0;
        }

        if(vmode != 0) {
            SSL_CTX_set_verify_depth(ctx, ssl_verify_depth);
            if(!empty_string(ssl_trusted_certificate)) {
                if(SSL_CTX_load_verify_locations(ctx, ssl_trusted_certificate, NULL)!=1) {
                    unsigned long err = ERR_get_error();
                    gobj_log_error(gobj, fatal?LOG_OPT_EXIT_ZERO:0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_OPENSSL,
                        "msg",          "%s", "SSL_CTX_load_verify_locations() FAILED",
                        "error",        "%s", ERR_error_string(err, NULL),
                        "cert",         "%s", ssl_trusted_certificate,
                        NULL
                    );
                    if(!fatal) {
                        SSL_CTX_free(ctx);
                        return NULL;
                    }
                }
            }
            if(use_system_ca) {
                /*
                 *  Portable system trust: probe the well-known CA bundle files
                 *  across distros and load the first one found. A fully-static
                 *  binary does NOT inherit the host OPENSSLDIR / SSL_CERT_FILE,
                 *  so SSL_CTX_set_default_verify_paths() loads an empty store
                 *  and a perfectly valid public cert fails to verify. Fall back
                 *  to the default paths for dynamic builds / hosts where they
                 *  are correct.
                 */
                const char *ca_bundle = ytls_get_system_ca_bundle();
                int loaded = ca_bundle &&
                    SSL_CTX_load_verify_locations(ctx, ca_bundle, NULL)==1;
                if(!loaded) {
                    SSL_CTX_set_default_verify_paths(ctx);
                    gobj_log_warning(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_OPENSSL,
                        "msg",          "%s", "ssl_use_system_ca: no portable CA bundle loaded, using default verify paths",
                        "probed",       "%s", ca_bundle?ca_bundle:"(none found)",
                        NULL
                    );
                }
            }
            ERR_clear_error();

            int vflags = SSL_VERIFY_PEER;
            if(vmode==2 && server) {
                vflags |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
            }
            SSL_CTX_set_verify(ctx, vflags, (vmode==1) ? verify_cb_tolerant : verify_cb_strict);
        } else {
            /*
             *  vmode==0 -> SSL_VERIFY_NONE. For a CLIENT this means the server
             *  certificate is never validated = a standing MITM hole. Fail
             *  closed by default (soft failure: return NULL so the connection
             *  is refused but the yuno stays up); an embedder that genuinely
             *  needs an unverified client (self-signed / PSK / IoT bring-up)
             *  must opt in explicitly with ssl_allow_insecure_client=true.
             *  Servers with no CA legitimately accept anonymous clients and
             *  are unaffected.
             */
            if(!server && !kw_get_bool(gobj, jn_config, "ssl_allow_insecure_client", 0, 0)) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_OPENSSL,
                    "msg",          "%s", "TLS client refused: no server-certificate validation. Set ssl_trusted_certificate / ssl_use_system_ca (recommended), or ssl_allow_insecure_client=true to accept the MITM risk",
                    NULL
                );
                SSL_CTX_free(ctx);
                return NULL;
            }
            SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
            if(!server) {
                gobj_log_warning(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_OPENSSL,
                    "msg",          "%s", "TLS client WITHOUT server-certificate validation (MITM surface): set ssl_trusted_certificate or ssl_use_system_ca",
                    NULL
                );
            }
        }
    }

    return ctx;
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
     *      Alloc memory
     *--------------------------------*/
    ytls_t *ytls = GBMEM_MALLOC(sizeof(ytls_t));
    if(!ytls) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_MEMORY,
            "msg",              "%s", "no memory for sizeof(ytls_t)",
            "sizeof(ytls_t)",   "%d", sizeof(ytls_t),
            NULL
        );
        return 0;
    }

    ytls->api_tls = &api_tls;
    ytls->server = server;
    ytls->gobj = gobj;
    ytls->trace_tls = kw_get_bool(gobj, jn_config, "trace_tls", 0, KW_WILD_NUMBER);
    ytls->rx_buffer_size = kw_get_int(gobj, jn_config, "rx_buffer_size", 32*1024, 0);
    snprintf(ytls->ssl_server_name, sizeof(ytls->ssl_server_name), "%s",
        kw_get_str(gobj, jn_config, "ssl_server_name", "", 0)
    );

    if(ytls->trace_tls) {
        gobj_log_debug(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INFO,
            "msg",              "%s", "OPENSSL: set trace TRUE",
            "ssl_server_name",  "%s", ytls->ssl_server_name,
            NULL
        );
    }

    /*--------------------------------*
     *      Build SSL_CTX (fatal on first init)
     *--------------------------------*/
    ytls->ctx = build_ssl_ctx(gobj, jn_config, server, TRUE, ytls->trace_tls, ytls);
    if(!ytls->ctx) {
        GBMEM_FREE(ytls)
        return 0;
    }

    return (hytls)ytls;
}

/***************************************************************************
 *  Hot-reload certificates: build a fresh SSL_CTX, atomically swap it in,
 *  and release our reference to the old one. Existing SSL objects keep the
 *  old context alive via their own reference (SSL_new() up-refs on creation,
 *  SSL_free() releases it) — so live connections are NOT disrupted.
 *
 *  On failure, the previous context is kept intact.
 ***************************************************************************/
PRIVATE int reload_certificates(hytls ytls_, json_t *jn_config)
{
    ytls_t *ytls = ytls_;
    if(!ytls) {
        return -1;
    }
    hgobj gobj = ytls->gobj;

    BOOL trace_tls = kw_get_bool(gobj, jn_config, "trace_tls", ytls->trace_tls, KW_WILD_NUMBER);

    SSL_CTX *new_ctx = build_ssl_ctx(gobj, jn_config, ytls->server, FALSE, trace_tls, ytls);
    if(!new_ctx) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_OPENSSL,
            "msg",              "%s", "reload_certificates FAILED: keeping previous SSL_CTX",
            "ssl_server_name",  "%s", ytls->ssl_server_name,
            NULL
        );
        return -1;
    }

    SSL_CTX *old_ctx = ytls->ctx;
    ytls->ctx = new_ctx;
    ytls->trace_tls = trace_tls;

    if(old_ctx) {
        SSL_CTX_free(old_ctx);  // refcount--; live SSL objects keep it alive
    }

    gobj_log_info(gobj, 0,
        "function",         "%s", __FUNCTION__,
        "msgset",           "%s", MSGSET_INFO,
        "msg",              "%s", "TLS certificates reloaded",
        "ssl_server_name",  "%s", ytls->ssl_server_name,
        NULL
    );
    return 0;
}

/***************************************************************************
 *  Convert an OpenSSL ASN1_TIME to Unix time_t.
 *  Returns 0 on failure.
 ***************************************************************************/
PRIVATE time_t asn1_time_to_unix(const ASN1_TIME *t)
{
    if(!t) {
        return 0;
    }
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    if(ASN1_TIME_to_tm(t, &tm) != 1) {
        return 0;
    }
    /* ASN1_TIME_to_tm fills tm in UTC; use timegm to avoid TZ shift. */
    return timegm(&tm);
}

/***************************************************************************
 *  Introspect the server certificate currently loaded in ytls->ctx.
 *  Caller owns the returned json_t; returns NULL when no cert is loaded
 *  (client-side ytls, or init failed).
 ***************************************************************************/
PRIVATE json_t *get_cert_info(hytls ytls_)
{
    ytls_t *ytls = ytls_;
    if(!ytls || !ytls->ctx) {
        return NULL;
    }

    X509 *cert = SSL_CTX_get0_certificate(ytls->ctx);
    if(!cert) {
        return NULL;
    }

    json_t *info = json_object();

    /* Subject / issuer as one-line DN. */
    char buf[512];
    X509_NAME *sn = X509_get_subject_name(cert);
    if(sn) {
        X509_NAME_oneline(sn, buf, sizeof(buf));
        json_object_set_new(info, "subject", json_string(buf));
    }
    X509_NAME *in = X509_get_issuer_name(cert);
    if(in) {
        X509_NAME_oneline(in, buf, sizeof(buf));
        json_object_set_new(info, "issuer", json_string(buf));
    }

    /* Validity window as Unix timestamps. */
    time_t nb = asn1_time_to_unix(X509_get0_notBefore(cert));
    time_t na = asn1_time_to_unix(X509_get0_notAfter(cert));
    json_object_set_new(info, "not_before", json_integer((json_int_t)nb));
    json_object_set_new(info, "not_after",  json_integer((json_int_t)na));

    /* Serial as uppercase hex. */
    ASN1_INTEGER *serial = X509_get_serialNumber(cert);
    if(serial) {
        BIGNUM *bn = ASN1_INTEGER_to_BN(serial, NULL);
        if(bn) {
            char *hex = BN_bn2hex(bn);
            if(hex) {
                json_object_set_new(info, "serial", json_string(hex));
                OPENSSL_free(hex);
            }
            BN_free(bn);
        }
    }

    return info;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void cleanup(hytls ytls_)
{
    ytls_t *ytls = ytls_;

    if(ytls && ytls->ctx) {
        SSL_CTX_free(ytls->ctx);
    }

    GBMEM_FREE(ytls)
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
            "msgset",           "%s", MSGSET_MEMORY,
            "msg",              "%s", "no memory for sizeof(sskt_t)",
            "sizeof(sskt_t)",   "%d", sizeof(sskt_t),
            "ssl_server_name",  "%s", ytls->ssl_server_name,
            NULL
        );
        return 0;
    }

    sskt->ytls = ytls;
    sskt->on_handshake_done_cb = on_handshake_done_cb;
    sskt->on_clear_data_cb = on_clear_data_cb;
    sskt->on_encrypted_data_cb = on_encrypted_data_cb;
    sskt->user_data = user_data;
    sskt->alive = NULL;

    /*
     *  Forensic transcript: capture raw peer bytes during handshake so that
     *  a failure can be investigated (SNI, cipher suites, fingerprint).
     *  Allocation failure is non-fatal: the filter still works without it.
     */
    sskt->handshake_transcript = gbuffer_create(HANDSHAKE_TRANSCRIPT_MAX, HANDSHAKE_TRANSCRIPT_MAX);
    if(!sskt->handshake_transcript) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_MEMORY,
            "msg",              "%s", "no memory for handshake_transcript",
            "ssl_server_name",  "%s", ytls->ssl_server_name,
            NULL
        );
    }

    sskt->ssl = SSL_new(ytls->ctx);
    if(!sskt->ssl) {
        sskt->error = ERR_get_error();
        ERR_error_string_n(sskt->error, sskt->last_error, sizeof(sskt->last_error));
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_OPENSSL,
            "msg",              "%s", "SSL_new() FAILED",
            "error",            "%lu", (unsigned long)sskt->error,
            "serror",           "%s", sskt->last_error,
            "ssl_server_name",  "%s", ytls->ssl_server_name,
            NULL
        );
        GBUFFER_DECREF(sskt->handshake_transcript)
        GBMEM_FREE(sskt)
        return 0;
    }

    if(ytls->trace_tls) {
        SSL_set_msg_callback(sskt->ssl, ssl_tls_trace);
        SSL_set_msg_callback_arg(sskt->ssl, ytls);
    } else {
        SSL_set_msg_callback(sskt->ssl, 0);
    }

    if(ytls->server) {
        SSL_set_accept_state(sskt->ssl);
    } else {
        SSL_set_connect_state(sskt->ssl);

        /*
         *  SNI: advertise the target hostname in the ClientHello. Required by
         *  virtual-hosted TLS endpoints (CDN/WAF front ends, e.g. ESIOS behind
         *  Imperva) which reject SNI-less handshakes with a 403.
         */
        if(ytls->ssl_server_name[0] != '\0') {
            if(SSL_set_tlsext_host_name(sskt->ssl, ytls->ssl_server_name) != 1) {
                gobj_log_error(gobj, 0,
                    "function",         "%s", __FUNCTION__,
                    "msgset",           "%s", MSGSET_OPENSSL,
                    "msg",              "%s", "SSL_set_tlsext_host_name() FAILED",
                    "ssl_server_name",  "%s", ytls->ssl_server_name,
                    NULL
                );
            }
        }

        /*
         *  Hostname verification. SSL_VERIFY_PEER validates the chain but NOT
         *  that it matches the target host — without this a chain-valid cert
         *  for the wrong host is still a MITM. Apply only when verification is
         *  on (mode inherited from the ctx).
         */
        if(SSL_get_verify_mode(sskt->ssl) != SSL_VERIFY_NONE) {
            if(ytls->ssl_server_name[0] != '\0') {
                X509_VERIFY_PARAM *vp = SSL_get0_param(sskt->ssl);
                X509_VERIFY_PARAM_set_hostflags(vp, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
                if(SSL_set1_host(sskt->ssl, ytls->ssl_server_name) != 1) {
                    gobj_log_warning(gobj, 0,
                        "function",         "%s", __FUNCTION__,
                        "msgset",           "%s", MSGSET_OPENSSL,
                        "msg",              "%s", "SSL_set1_host() FAILED (hostname not checked)",
                        "ssl_server_name",  "%s", ytls->ssl_server_name,
                        NULL
                    );
                }
            } else {
                gobj_log_warning(gobj, 0,
                    "function",         "%s", __FUNCTION__,
                    "msgset",           "%s", MSGSET_OPENSSL,
                    "msg",              "%s", "TLS client verify ON but no ssl_server_name: hostname NOT checked",
                    "ssl_server_name",  "%s", ytls->ssl_server_name,
                    NULL
                );
            }
        }
    }

    // Renegotiation is now controlled at ctx build time by the
    // "ssl_disable_renegotiation" config knob (SSL_OP_NO_RENEGOTIATION).

    sskt->rbio = BIO_new(BIO_s_mem());
    sskt->wbio = BIO_new(BIO_s_mem());

    SSL_set_bio(sskt->ssl, sskt->rbio, sskt->wbio);

    /*
     *  The filter is left "cold": no SSL_do_handshake() here. The caller is
     *  responsible for invoking ytls_do_handshake() once ready (e.g. c_tcp
     *  does so after changing to ST_WAIT_HANDSHAKE).
     */
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

    /*
     * If we are being freed from within the on_clear_data_cb callback
     * (re-entrant destruction), signal the alive marker on the caller's
     * stack so that flush_clear_data() can stop iterating after we return.
     */
    if(sskt->alive) {
        *sskt->alive = FALSE;
    }

    SSL_free(sskt->ssl);   /* free the SSL object and its BIO's */
    GBUFFER_DECREF(sskt->handshake_transcript)
    GBMEM_FREE(sskt)
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void set_trace(hsskt sskt_, BOOL set)
{
    sskt_t *sskt = sskt_;
    sskt->ytls->trace_tls = set?TRUE:FALSE;

    gobj_log_debug(sskt->ytls->gobj, 0,
        "function",         "%s", __FUNCTION__,
        "msgset",           "%s", MSGSET_INFO,
        "msg",              "%s", "OPENSSL: set trace",
        "trace",            "%d", set,
        NULL
    );

    if(sskt->ytls->trace_tls) {
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
    Append peer bytes to the forensic transcript (bounded, never grows).
 ***************************************************************************/
PRIVATE void capture_handshake_bytes(sskt_t *sskt, const void *bytes, size_t len)
{
    if(!sskt->handshake_transcript || len == 0) {
        return;
    }
    size_t room = gbuffer_freebytes(sskt->handshake_transcript);
    if(room == 0) {
        return;
    }
    size_t take = len < room ? len : room;
    gbuffer_append(sskt->handshake_transcript, (void *)bytes, take);
}

/***************************************************************************
    Dump the captured handshake transcript on failure, then release it.
    Called from do_handshake() fatal branch; safe when transcript is empty.
 ***************************************************************************/
PRIVATE void dump_handshake_transcript_on_fail(sskt_t *sskt)
{
    hgobj gobj = sskt->ytls->gobj;

    if(sskt->handshake_transcript && gbuffer_leftbytes(sskt->handshake_transcript) > 0) {
        gobj_trace_dump_gbuf(gobj, sskt->handshake_transcript,
            "TLS handshake FAILS: raw peer bytes (userp %p)", sskt->user_data
        );
    }
    GBUFFER_DECREF(sskt->handshake_transcript)
}

/***************************************************************************
    Do handshake
 ***************************************************************************/
PRIVATE int do_handshake(hsskt sskt_)
{
    sskt_t *sskt = sskt_;
    hgobj gobj = sskt->ytls->gobj;

    if(sskt->ytls->trace_tls) {
        gobj_trace_msg(gobj, "------- do_handshake, userp %p", sskt->user_data);
    }

    /*
     * SSL_get_error() consults the thread-local error queue to classify the
     * result of the SSL operation. Any stale error left by unrelated code
     * (e.g. libjwt RSA signature verification) will make SSL_get_error()
     * misreport a benign WANT_READ/WANT_WRITE as SSL_ERROR_SSL.
     */
    ERR_clear_error();
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
            if(sskt->ytls->trace_tls) {
                gobj_trace_msg(gobj, "------- do_handshake: %s, userp %p",
                    detail==SSL_ERROR_WANT_READ?"SSL_ERROR_WANT_READ":"SSL_ERROR_WANT_WRITE",
                    sskt->user_data
                );
            }
            break;

        default:
            sskt->error = ERR_get_error();
            ERR_error_string_n(sskt->error, sskt->last_error, sizeof(sskt->last_error));
            gobj_log_set_last_message("OPENSSL: %s", sskt->last_error);
            /*
             *  Leave a default-on trace of the rejected handshake (not just
             *  under trace_tls). A peer offering a TLS version below the
             *  configured floor lands here with reason "unsupported protocol"
             *  / "wrong version number" / "tlsv1 alert protocol version" — so
             *  the legacy IoT devices that need an explicit ssl_min_version
             *  opt-down are identifiable. The peer address is logged by the
             *  transport gobj on the matching connection drop.
             */
            gobj_log_warning(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_OPENSSL,
                "msg",              "%s", "TLS handshake rejected (check ssl_min_version for legacy peers)",
                "error",            "%s", sskt->last_error,
                "ssl_server_name",  "%s", sskt->ytls->ssl_server_name,
                "tls_version",      "%s", SSL_get_version(sskt->ssl),
                "userp",            "%p", sskt->user_data,
                NULL
            );
            dump_handshake_transcript_on_fail(sskt);
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
            /*
             *  No silent verification failures: under VERIFY_OPTIONAL the
             *  handshake completes even if the peer cert did not verify;
             *  SSL_get_verify_result() is the only way to learn it. (With
             *  VERIFY_NONE it returns X509_V_OK, so this never false-fires.)
             */
            long vr = SSL_get_verify_result(sskt->ssl);
            if(vr != X509_V_OK) {
                gobj_log_warning(gobj, 0,
                    "function",         "%s", __FUNCTION__,
                    "msgset",           "%s", MSGSET_OPENSSL,
                    "msg",              "%s", "TLS peer certificate did NOT verify (accepted under VERIFY_OPTIONAL; set ssl_verify_mode=required to reject)",
                    "error",            "%s", X509_verify_cert_error_string(vr),
                    "ssl_server_name",  "%s", sskt->ytls->ssl_server_name,
                    "userp",            "%p", sskt->user_data,
                    NULL
                );
            }
            sskt->on_handshake_done_cb(sskt->user_data, 0);
        }
        GBUFFER_DECREF(sskt->handshake_transcript) // handshake succeeded: transcript no longer needed
        return 1; // handshake done
    }

    return 0; // handshake in progress (WANT_READ/WANT_WRITE)
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int flush_encrypted_data(sskt_t *sskt)
{
    hgobj gobj = sskt->ytls->gobj;

    if(sskt->ytls->trace_tls) {
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

    int pending;
    while((pending = BIO_pending(sskt->wbio))>0) {
        gbuffer_t *gbuf = gbuffer_create(pending, pending);
        if(!gbuf) {
            gobj_log_error(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_MEMORY,
                "msg",              "%s", "No memory for BIO_pending",
                "ssl_server_name",  "%s", sskt->ytls->ssl_server_name,
                NULL
            );
            return -1;
        }
        char *p = gbuffer_cur_wr_pointer(gbuf);
        const int ret = BIO_read(sskt->wbio, p, pending);
        if(sskt->ytls->trace_tls) {
            gobj_trace_msg(gobj, "------- flush_encrypted_data() %d, userp %p", ret, sskt->user_data);
        }
        if(ret > 0) {
            gbuffer_set_wr(gbuf, ret);
            sskt->on_encrypted_data_cb(sskt->user_data, gbuf);
            // TODO check mbedtls does:
            // gbuffer_decref(gbuf);
            // return MBEDTLS_ERR_SSL_INTERNAL_ERROR; // Callback failed
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
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_OPENSSL,
            "msg",              "%s", "TLS handshake PENDING",
            "ssl_server_name",  "%s", sskt->ytls->ssl_server_name,
            NULL
        );
        GBUFFER_DECREF(gbuf)
        return -1;
    }

    size_t len;
    while(sskt->ssl && (len = gbuffer_chunk(gbuf))>0) {
        const char *p = gbuffer_cur_rd_pointer(gbuf);    // Don't pop data, be sure it's written
        ERR_clear_error(); // see do_handshake() note on stale error-queue entries
        const int written = SSL_write(sskt->ssl, p, len);
        if(written <= 0) {
            const int ret = SSL_get_error(sskt->ssl, written);
            switch(ret) {
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE:
                if(sskt->ytls->trace_tls) {
                    gobj_trace_msg(gobj, "------- encrypt_data: %s, userp %p",
                        ret==SSL_ERROR_WANT_READ?"SSL_ERROR_WANT_READ":"SSL_ERROR_WANT_WRITE",
                        sskt->user_data
                    );
                }
                flush_encrypted_data(sskt);
                if(flush_clear_data(sskt) == -2222) {
                    // on_clear_data_cb freed sskt re-entrantly inside flush_clear_data;
                    // do NOT touch sskt again (the loop re-test below would deref it).
                    GBUFFER_DECREF(gbuf)
                    return -1;
                }
                continue;

            default:
                sskt->error = ERR_get_error();
                ERR_error_string_n(sskt->error, sskt->last_error, sizeof(sskt->last_error));
                gobj_log_error(gobj, 0,
                    "function",         "%s", __FUNCTION__,
                    "msgset",           "%s", MSGSET_OPENSSL,
                    "msg",              "%s", "SSL_write() FAILED",
                    "ret",              "%d", (int)ret,
                    "error",            "%lu", (unsigned long)sskt->error,
                    "serror",           "%s", sskt->last_error,
                    "ssl_server_name",  "%s", sskt->ytls->ssl_server_name,
                    NULL
                );
                GBUFFER_DECREF(gbuf)
                return -1;
            }
            break;
        }
        gbuffer_get(gbuf, written);    // Pop data

        if(sskt->ytls->trace_tls) {
            gobj_trace_msg(gobj, "------- ==> encrypt_data DATA, userp %p, len %d", sskt->user_data, (int)len);
        }
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
    while(sskt->ssl) {
        gbuffer_t *gbuf = gbuffer_create(sskt->ytls->rx_buffer_size, sskt->ytls->rx_buffer_size);
        char *p = gbuffer_cur_wr_pointer(gbuf);
        ERR_clear_error(); // see do_handshake() note on stale error-queue entries
        int nread = SSL_read(sskt->ssl, p, sskt->ytls->rx_buffer_size);
        if(sskt->ytls->trace_tls) {
            gobj_trace_msg(gobj, "------- flush_clear_data() %d, userp %p", nread, sskt->user_data);
        }
        if(nread <= 0) {
            const int ssl_err = SSL_get_error(sskt->ssl, nread);
            if(ssl_err == SSL_ERROR_WANT_READ
                    || ssl_err == SSL_ERROR_WANT_WRITE
                    || ssl_err == SSL_ERROR_ZERO_RETURN) {
                // No more data right now (WANT_*) or peer sent close_notify
                // (ZERO_RETURN — clean TLS shutdown; TCP-level close is
                // reported separately by the read yev_event).
                GBUFFER_DECREF(gbuf)
                break;
            }
            sskt->error = ERR_get_error();
            ERR_error_string_n(sskt->error, sskt->last_error, sizeof(sskt->last_error));
            gobj_log_error(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_OPENSSL,
                "msg",              "%s", "SSL_read() FAILED",
                "ret",              "%d", (int)nread,
                "ssl_err",          "%d", ssl_err,
                "error",            "%lu", (unsigned long)sskt->error,
                "serror",           "%s", sskt->last_error,
                "ssl_server_name",  "%s", sskt->ytls->ssl_server_name,
                NULL
            );
            GBUFFER_DECREF(gbuf)
            sskt->alive = NULL;
            return -1111; // Mark as TLS error
        }

        // Callback clear data
        gbuffer_set_wr(gbuf, nread);
        ret += sskt->on_clear_data_cb(sskt->user_data, gbuf);
        /*
         * Check alive marker: if the callback caused sskt to be freed
         * (re-entrant destruction), sskt_alive is now FALSE.
         * Do NOT touch sskt after this point.
         */
        if(!sskt_alive) {
            return -2222; // sskt freed re-entrantly inside on_clear_data_cb; signal callers not to touch it
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
)
{
    sskt_t *sskt = sskt_;
    hgobj gobj = sskt->ytls->gobj;

    size_t len;
    while((len = gbuffer_chunk(gbuf))>0) {
        char *p = gbuffer_cur_rd_pointer(gbuf);    // Don't pop data, be sure it's written

        /*
         *  Capture raw peer bytes for forensic dump BEFORE feeding the BIO —
         *  once BIO_write runs, the bytes are owned by OpenSSL.
         */
        if(!SSL_is_init_finished(sskt->ssl)) {
            capture_handshake_bytes(sskt, p, len);
        }

        int written = BIO_write(sskt->rbio, p, len);
        if(written < 0) {
            sskt->error = ERR_get_error();
            ERR_error_string_n(sskt->error, sskt->last_error, sizeof(sskt->last_error));
            gobj_log_error(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_OPENSSL,
                "msg",              "%s", "BIO_write() FAILED",
                "ret",              "%d", (int)written,
                "error",            "%lu", (unsigned long)sskt->error,
                "serror",           "%s", sskt->last_error,
                "ssl_server_name",  "%s", sskt->ytls->ssl_server_name,
                NULL
            );
            GBUFFER_DECREF(gbuf)
            return -1111; // Mark as TLS error
        }

        gbuffer_get(gbuf, written);    // Pop data

        if(sskt->ytls->trace_tls) {
            gobj_trace_msg(gobj, "------- <== decrypt_data, userp %p, len %zu", sskt->user_data, len);
        }
        if(!SSL_is_init_finished(sskt->ssl)) {
            if(do_handshake(sskt)<0) {
                // Error already logged
                GBUFFER_DECREF(gbuf)
                return -1111; // Mark as TLS error
            }
        } else {
            int ret = flush_clear_data(sskt);
            if(ret < 0) {
                // Error already logged
                GBUFFER_DECREF(gbuf)
                return ret;
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
    int ret = flush_clear_data(sskt);
    if(ret < 0) {
        // Error already logged in flush_clear_data (incl. -2222 re-entrant-free sentinel)
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
PUBLIC api_tls_t *openssl_api_tls(void)
{
    return &api_tls;
}

#endif /* CONFIG_HAVE_OPENSSL */
