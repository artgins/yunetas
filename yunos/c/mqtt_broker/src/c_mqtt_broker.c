/***********************************************************************
 *          C_MQTT_BROKER.C
 *          Mqtt_broker GClass.
 *
 *          Mqtt broker
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <stdio.h>
#include <limits.h>
#include <string.h>

#include <gobj.h>
#include <g_ev_kernel.h>
#include <g_st_kernel.h>
#include <helpers.h>

#ifdef __linux__
#if defined(CONFIG_HAVE_OPENSSL)
    #include <openssl/ssl.h>
    #include <openssl/rand.h>
#elif defined(CONFIG_HAVE_MBEDTLS)
    #include <mbedtls/md.h>
    #include <mbedtls/ctr_drbg.h>
    #include <mbedtls/pkcs5.h>
#else
    #error "No crypto library defined"
#endif
#endif

#include "c_mqtt_broker.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "command search level in childs"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_authzs[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "authz",        0,              0,          "permission to search"),
SDATAPM (DTP_STRING,    "service",      0,              0,          "Service where to search the permission. If empty print all service's permissions"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias---------------items-----------json_fn---------description---------- */
SDATACM (DTP_SCHEMA,    "help",             a_help,             pm_help,        cmd_help,       "Command's help"),
SDATACM2 (DTP_SCHEMA,   "authzs",           0,                  0,              pm_authzs,      cmd_authzs,     "Authorization's help"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name----------------flag--------default-----description---------- */
SDATA (DTP_STRING,      "tranger_path",     SDF_RD,     "/yuneta/store/(^^__yuno_role__^^)/(^^__node_owner__^^)", "tranger path"),
SDATA (DTP_STRING,      "tranger_database", SDF_RD,     "(^^__yuno_role__^^)^(^^__yuno_name__^^)", "tranger database"),
SDATA (DTP_STRING,      "filename_mask",    SDF_RD|SDF_REQUIRED,"%Y-%m-%d", "Organization of tables (file name format, see strftime())"),
SDATA (DTP_INTEGER,     "on_critical_error",SDF_RD,     "0x0010",   "LOG_OPT_TRACE_STACK"),

SDATA (DTP_BOOLEAN,     "allow_anonymous",  SDF_PERSIST, "1",       "Boolean value that determines whether clients that connect without providing a username are allowed to connect. If set to FALSE then another means of connection should be created to control authenticated client access. Defaults to TRUE, (TODO but connections are only allowed from the local machine)."),

SDATA (DTP_POINTER,     "subscriber",       0,          0,          "Subscriber of output-events. If it's null then the subscriber is the parent."),
SDATA (DTP_INTEGER,     "timeout",          SDF_RD,     "1000",     "Timeout"),
SDATA (DTP_POINTER,     "user_data",        0,          0,          "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,          0,          "more user data"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",        "Trace messages"},
{0, 0},
};

/*---------------------------------------------*
 *      GClass authz levels
 *---------------------------------------------*/
PRIVATE sdata_desc_t authz_table[] = {
/*-AUTHZ-- type---------name----------------flag----alias---items---description--*/
SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj timer;
    int32_t timeout;
    hgobj gobj_input_side;
    hgobj gobj_tranger_broker;

    BOOL allow_anonymous;

} PRIVATE_DATA;





                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);

    /*
     *  SERVICE subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    } else if(gobj_is_pure_child(gobj)) {
        subscriber = gobj_parent(gobj);
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    }

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout,                   gobj_read_integer_attr)
    SET_PRIV(allow_anonymous,           gobj_read_bool_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(timeout,             gobj_read_integer_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    // set_timeout(priv->timer, priv->timeout);

    /*-------------------------*
     *      Start services
     *-------------------------*/
    priv->gobj_input_side = gobj_find_service("__input_side__", TRUE);
    gobj_subscribe_event(priv->gobj_input_side, 0, 0, gobj);
    gobj_start_tree(priv->gobj_input_side);

    /*--------------------------------*
     *      Tranger database
     *--------------------------------*/
    const char *path = gobj_read_str_attr(gobj, "tranger_path");
    if(empty_string(path)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "tranger path EMPTY",
            NULL
        );
        return -1;
    }

    const char *database = gobj_read_str_attr(gobj, "tranger_database");
    if(empty_string(database)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "tranger database EMPTY",
            NULL
        );
        return -1;
    }

    /*---------------------------------*
     *      Open Timeranger
     *---------------------------------*/
    json_t *kw_tranger = json_pack("{s:s, s:s, s:s, s:b, s:I, s:i}",
        "path", path,
        "database", database,
        "filename_mask", gobj_read_str_attr(gobj, "filename_mask"),
        "master", 1,
        "subscriber", (json_int_t)(uintptr_t)gobj,
        "on_critical_error", (int)gobj_read_integer_attr(gobj, "on_critical_error")
    );
    char name[NAME_MAX];
    snprintf(name, sizeof(name), "tranger_%s", gobj_name(gobj));
    priv->gobj_tranger_broker = gobj_create_service(
        name,
        C_TRANGER,
        kw_tranger,
        gobj
    );
    gobj_start(priv->gobj_tranger_broker);

    // TODO
    // priv->trq_msgs = trq_open(
    //     priv->tranger,
    //     topic_name,
    //     gobj_read_str_attr(gobj, "tkey"),
    //     tranger2_str2system_flag(gobj_read_str_attr(gobj, "system_flag")),
    //     gobj_read_integer_attr(gobj, "backup_queue_size")
    // );

    // TODO trq_load(priv->trq_msgs);

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*---------------------------------------*
     *      Stop services
     *---------------------------------------*/
    if(priv->gobj_input_side) {
        if(gobj_is_playing(priv->gobj_input_side)) {
            gobj_pause(priv->gobj_input_side);
        }
        gobj_stop_tree(priv->gobj_input_side);
    }

    /*----------------------------------*
     *      Close Timeranger
     *----------------------------------*/
    // TODO
    // EXEC_AND_RESET(trq_close, priv->trq_msgs);

    gobj_stop(priv->gobj_tranger_broker);

    EXEC_AND_RESET(gobj_destroy, priv->gobj_tranger_broker);

    clear_timeout(priv->timer);

    return 0;
}




                    /***************************
                     *      Commands
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    KW_INCREF(kw);
    json_t *jn_resp = gobj_build_cmds_doc(gobj, kw);
    return msg_iev_build_response(
        gobj,
        0,
        jn_resp,
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    return gobj_build_authzs_doc(gobj, cmd, kw);
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int open_queue(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int check_passwd(
    hgobj gobj,
    const char *password,
    const char *hash,
    const char *salt,
    const char *algorithm, // hashtype
    json_int_t iterations
)
{
#if defined(__linux__)
    #if defined(CONFIG_HAVE_OPENSSL)
    const EVP_MD *digest;
    unsigned char hash_[EVP_MAX_MD_SIZE+1];
    unsigned int hash_len_ = EVP_MAX_MD_SIZE;

    if(empty_string(algorithm)) {
        algorithm = "sha512";
    }
    digest = EVP_get_digestbyname(algorithm);
    if(!digest) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Unable to get openssl digest",
            "digest",       "%s", algorithm,
            NULL
        );
        return -1;
    }

    if(0) { //strcasecmp(algorithm, "sha512")==0) {
        EVP_MD_CTX *context = EVP_MD_CTX_new();
        EVP_DigestInit_ex(context, digest, NULL);
        EVP_DigestUpdate(context, password, strlen(password));
        EVP_DigestUpdate(context, salt, (size_t)strlen(salt));
        EVP_DigestFinal_ex(context, hash_, &hash_len_);
        EVP_MD_CTX_free(context);
    } else {
        PKCS5_PBKDF2_HMAC(password, (int)strlen(password),
            (const unsigned char *)salt, (int)strlen(salt), iterations,
            digest, (int)hash_len_, hash_
        );
    }

    if(hash_len_ == strlen(hash) && memcmp(hash, hash_, hash_len_)==0) {
        return 0;
    }
#elif defined(CONFIG_HAVE_MBEDTLS)
    unsigned char derived_hash[64]; // SHA512 max size
    size_t hash_len;
    const mbedtls_md_info_t *md_info = NULL;

    if(empty_string(algorithm)) {
        algorithm = "sha512";
    }

    md_info = mbedtls_md_info_from_string(algorithm);
    if(!md_info) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL_ERROR,
            "msg",      "%s", "Unable to get mbedtls digest",
            "digest",   "%s", algorithm,
            NULL
        );
        return -1;
    }

    hash_len = mbedtls_md_get_size(md_info);

    /* Decode salt from base64 */
    gbuffer_t *gbuf_salt = gbuffer_base64_to_string(salt, strlen(salt));
    if(!gbuf_salt) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL_ERROR,
            "msg",      "%s", "Invalid base64 salt",
            NULL
        );
        return -1;
    }

    if(mbedtls_pkcs5_pbkdf2_hmac(
            md_info,
            (const unsigned char *)password, strlen(password),
            (const unsigned char *)gbuffer_cur_rd_pointer(gbuf_salt), gbuffer_leftbytes(gbuf_salt),
            iterations,
            hash_len,
            derived_hash
        ) != 0) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL_ERROR,
            "msg",      "%s", "mbedtls_pbkdf2_hmac() FAILED",
            NULL
        );
        GBUFFER_DECREF(gbuf_salt);
        return -1;
    }

    /* Decode input hash from base64 */
    gbuffer_t *gbuf_hash = gbuffer_base64_to_string(hash, strlen(hash));
    if(!gbuf_hash) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL_ERROR,
            "msg",      "%s", "Invalid base64 hash",
            NULL
        );
        GBUFFER_DECREF(gbuf_salt);
        return -1;
    }

    const unsigned char *stored_hash = (const unsigned char *)gbuffer_cur_rd_pointer(gbuf_hash);
    size_t stored_hash_len = gbuffer_leftbytes(gbuf_hash);

    int result = -1;
    if(stored_hash_len == hash_len && memcmp(derived_hash, stored_hash, hash_len) == 0) {
        result = 0; // Match
    }

    GBUFFER_DECREF(gbuf_salt);
    GBUFFER_DECREF(gbuf_hash);
    return result;
#else
    #error "No crypto library defined"
#endif
#endif

    return -1;
}

/***************************************************************************
    "credentials" : [
        {
            "type": "password",
            "createdDate": 1581316153674,
            "secretData": {
                "value": "???",
                "salt": "???=="
            },
            "credentialData" : {
                "hashIterations": 27500,
                "algorithm": "sha512",
                "additionalParameters": {
                }
            }
        }
    ]
 ***************************************************************************/
#define PW_DEFAULT_ITERATIONS 101

PRIVATE json_t *hash_password(
    hgobj gobj,
    const char *password,
    const char *algorithm,
    int iterations
) {
#if defined(__linux__)
    #if defined(CONFIG_HAVE_OPENSSL)
    #define SALT_LEN 12
    unsigned int hash_len;
    unsigned char hash[64]; /* For SHA512 */
    unsigned char salt[SALT_LEN];

    if(empty_string(algorithm)) {
        algorithm = "sha512";
    }
    if(iterations < 1) {
        iterations = PW_DEFAULT_ITERATIONS;
    }
    if(RAND_bytes(salt, sizeof(salt))<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "RAND_bytes() FAILED",
            "digest",       "%s", algorithm,
            NULL
        );
        return 0;
    }

    const EVP_MD *digest = EVP_get_digestbyname(algorithm);
    if(!digest) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Unable to get openssl digest",
            "digest",       "%s", algorithm,
            NULL
        );
        return 0;
    }

    hash_len = sizeof(hash);
    PKCS5_PBKDF2_HMAC(password, (int)strlen(password),
        salt, sizeof(salt), iterations,
        digest, (int)hash_len, hash
    );

    gbuffer_t *gbuf_hash = gbuffer_string_to_base64((const char *)hash, hash_len);
    gbuffer_t *gbuf_salt = gbuffer_string_to_base64((const char *)salt, sizeof(salt));
    char *hash_b64 = gbuffer_cur_rd_pointer(gbuf_hash);
    char *salt_b64 = gbuffer_cur_rd_pointer(gbuf_salt);

    json_t *credentials = json_object();
    json_t *credential_list = kw_get_list(gobj, credentials, "credentials", json_array(), KW_CREATE);
    json_t *credential = json_pack("{s:s, s:I, s:{s:s, s:s}, s:{s:I, s:s, s:{}}}",
        "type", "password",
        "createdDate", (json_int_t)time_in_milliseconds(),
        "secretData",
            "value", hash_b64,
            "salt", salt_b64,
        "credentialData",
            "hashIterations", iterations,
            "algorithm", algorithm,
            "additionalParameters"
    );
    json_array_append_new(credential_list, credential);

    GBUFFER_DECREF(gbuf_hash);
    GBUFFER_DECREF(gbuf_salt);

    return credentials;
#elif defined(CONFIG_HAVE_MBEDTLS)
    #define SALT_LEN 12
    unsigned char hash[64];  // Support up to SHA512
    unsigned char salt[SALT_LEN];
    const mbedtls_md_info_t *md_info = NULL;

    if(empty_string(algorithm)) {
        algorithm = "sha512";
    }
    if(iterations < 1) {
        iterations = PW_DEFAULT_ITERATIONS;
    }

    /* Resolve hash algorithm */
    md_info = mbedtls_md_info_from_string(algorithm);
    if(!md_info) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL_ERROR,
            "msg",      "%s", "Unable to get mbedtls digest",
            "digest",   "%s", algorithm,
            NULL
        );
        return 0;
    }
    size_t hash_len = mbedtls_md_get_size(md_info);

    /* Initialize RNG */
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    const char *pers = "hash_password";

    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    if(mbedtls_ctr_drbg_seed(
            &ctr_drbg,
            mbedtls_entropy_func,
            &entropy,
            (const unsigned char *)pers,
            strlen(pers)
        ) != 0) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL_ERROR,
            "msg",      "%s", "mbedtls_ctr_drbg_seed() FAILED",
            NULL
        );
        goto error;
    }

    if(mbedtls_ctr_drbg_random(&ctr_drbg, salt, SALT_LEN) != 0) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL_ERROR,
            "msg",      "%s", "mbedtls_ctr_drbg_random() FAILED",
            NULL
        );
        goto error;
    }

    if(mbedtls_pkcs5_pbkdf2_hmac(
            md_info,
            (const unsigned char *)password, strlen(password),
            salt, SALT_LEN,
            iterations,
            hash_len,
            hash
        ) != 0) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL_ERROR,
            "msg",      "%s", "mbedtls_pkcs5_pbkdf2_hmac() FAILED",
            NULL
        );
        goto error;
    }

    gbuffer_t *gbuf_hash = gbuffer_string_to_base64((const char *)hash, hash_len);
    gbuffer_t *gbuf_salt = gbuffer_string_to_base64((const char *)salt, SALT_LEN);
    char *hash_b64 = gbuffer_cur_rd_pointer(gbuf_hash);
    char *salt_b64 = gbuffer_cur_rd_pointer(gbuf_salt);

    json_t *credential_list = json_array();
    json_t *credentials = json_pack("{s:o}", "credentials", credential_list);
    json_t *credential = json_pack("{s:s, s:I, s:{s:s, s:s}, s:{s:I, s:s, s:{}}}",
        "type", "password",
        "createdDate", (json_int_t)time_in_milliseconds(),
        "secretData",
            "value", hash_b64,
            "salt", salt_b64,
        "credentialData",
            "hashIterations", iterations,
            "algorithm", algorithm,
            "additionalParameters"
    );
    json_array_append_new(credential_list, credential);

    GBUFFER_DECREF(gbuf_hash);
    GBUFFER_DECREF(gbuf_salt);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    return credentials;

error:
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    return NULL;
#else
    #error "No crypto library defined"
#endif
#endif
    return NULL;
}

/***************************************************************************
 *  Return -2 if username is not authorized
 ***************************************************************************/
PRIVATE int check_password(
    hgobj gobj,
    const char *client_id,
    const char *username,
    const char *password
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(empty_string(username)) {
        // TODO a client_id can have permitted usernames in blank
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_AUTH_ERROR,
            "msg",          "%s", "No username given to check password",
            "client_id",    "%s", client_id,
            NULL
        );
        return -2;
    }
    json_t *user = gobj_get_resource(0, username, 0, 0);
    if(!user) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_AUTH_ERROR,
            "msg",          "%s", "Username not exist",
            "client_id",    "%s", client_id,
            "username",     "%s", username,
            NULL
        );
        return -2;
    }

    json_t *credentials = kw_get_list(gobj, user, "credentials", 0, KW_REQUIRED);

    int idx; json_t *credential;
    json_array_foreach(credentials, idx, credential) {
        const char *password_saved = kw_get_str(
            gobj,
            credential,
            "secretData`value",
            "",
            KW_REQUIRED
        );
        const char *salt = kw_get_str(
            gobj,
            credential,
            "secretData`salt",
            "",
            KW_REQUIRED
        );
        json_int_t hashIterations = kw_get_int(
            gobj,
            credential,
            "credentialData`hashIterations",
            0,
            KW_REQUIRED
        );
        const char *algorithm = kw_get_str(
            gobj,
            credential,
            "credentialData`algorithm",
            "",
            KW_REQUIRED
        );

        if(check_passwd(
            gobj,
            password,
            password_saved,
            salt,
            algorithm,
            hashIterations
        )==0) {
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "Username authorized",
                "client_id",    "%s", client_id,
                "username",     "%s", username,
                NULL
            );
            return 0;
        }
    }

    gobj_log_warning(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_AUTH_ERROR,
        "msg",          "%s", "Username not authorized",
        "client_id",    "%s", client_id,
        "username",     "%s", username,
        NULL
    );

    return -2;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  Identity_card on from
 *      mqtt clients (__input_side__)
 *
    {
        "client_id": "DVES_40AC66",
        "assigned_id": false,       #^^ if assigned_id is true the client_id is temporary.
        "username": "DVES_USER",
        "password": "DVES_PASS",
        "clean_start": true,
        "protocol_version": 2,
        "protocol_name": "MQTT",
        "keepalive": 30,
        "session_expiry_interval": 0,
        "max_qos": 2,

        "will": true,
        "will_retain": true,
        "will_qos": 1,
        "will_topic": "tele/tasmota_40AC66/LWT",    #^^ these will fields are optionals
        "will_delay_interval": 0,
        "will_expiry_interval": 0,
        "gbuffer": 95091873745312,  #^^ it contents the will payload

        "__temp__": {
            "channel": "input-1",
            "channel_gobj": 95091872991280
        }
    }
 *
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

print_json2("XXXXXXXXX", kw); // TODO TEST

    if(src != priv->gobj_input_side) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "on_open NOT from input_size",
            "src",          "%s", gobj_full_name(src),
            NULL
        );
        KW_DECREF(kw);
        return -1;
    }

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_json(
            gobj,
            kw, // not own
            "ON_OPEN %s", gobj_short_name(src)
        );
    }

    /*---------------------------------------------*
     *      Check user/password
     *---------------------------------------------*/
    const char *client_id = kw_get_str(gobj, kw, "client_id", "", KW_REQUIRED);
    const char *username = kw_get_str(gobj, kw, "username", "", KW_REQUIRED);
    const char *password = kw_get_str(gobj, kw, "password", "", KW_REQUIRED);

    int authorized = 0;
    if(priv->allow_anonymous) {
        username = "yuneta";
    } else {
        authorized = check_password(gobj, client_id, username, password);
    }

    if(authorized < 0) {
        KW_DECREF(kw);
        return authorized;
    }

    /*---------------------------------------------*
     *  MQTT Client ID <==> topic in Timeranger
     *---------------------------------------------*/
    BOOL clean_start = kw_get_bool(gobj, kw, "clean_start", 0, KW_REQUIRED);
    BOOL will = kw_get_bool(gobj, kw, "will", 0, KW_REQUIRED);
    if(will) {
        // TODO read the remain will fields
    }

    /*----------------------------------------------------------------*
     *  Open the topic (client_id) or create it if it doesn't exist
     *----------------------------------------------------------------*/
    json_t *jn_response = gobj_command(
        priv->gobj_tranger_broker,
        "open-topic",
        json_pack("{s:s, s:s}",
            "topic_name", client_id,
            "__username__", username
        ),
        gobj
    );

    int result = COMMAND_RESULT(gobj, jn_response);
    if(result < 0) {
        JSON_DECREF(jn_response)
        jn_response = gobj_command(
            priv->gobj_tranger_broker,
            "create-topic", // idempotent function
            json_pack("{s:s, s:s}",
                "topic_name", client_id,
                "__username__", username
            ),
            gobj
        );
        result = COMMAND_RESULT(gobj, jn_response);
    }
    if(result < 0) {
        const char *comment = COMMAND_COMMENT(gobj, jn_response);
        JSON_DECREF(jn_response)
        KW_DECREF(kw);
        return -1;
    }

    json_t *topic = COMMAND_DATA(gobj, jn_response);

    // TODO if session already exists with below conditions return 1!
    // if(priv->clean_start == FALSE && prev_session_expiry_interval > 0) {
    //     if(priv->protocol_version == mosq_p_mqtt311 || priv->protocol_version == mosq_p_mqtt5) {
    //         connect_ack |= 0x01;
    //          result = 1;
    //     }
    //     // copia client session TODO
    // }

    JSON_DECREF(jn_response)
    KW_DECREF(kw);
    return result;
}

/***************************************************************************
 *  Identity_card off from
 *      mqtt clients (__input_side__)
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    // PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_json(
            gobj,
            kw, // not own
            "ON_CLOSE %s", gobj_short_name(src)
        );
    }

    // TODO do will job ?

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
//    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    // printf("Timeout\n");

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create                  = mt_create,
    .mt_destroy                 = mt_destroy,
    .mt_start                   = mt_start,
    .mt_stop                    = mt_stop,
    .mt_play                    = mt_play,
    .mt_pause                   = mt_pause,
    .mt_writing                 = mt_writing,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_MQTT_BROKER);

/*------------------------*
 *      Events
 *------------------------*/

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
        {EV_ON_OPEN,                ac_on_open,              0},
        {EV_ON_CLOSE,               ac_on_close,             0},
        {EV_TIMEOUT,                ac_timeout,              0},
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
        {EV_ON_OPEN,                0},
        {EV_ON_CLOSE,               0},
        {EV_TIMEOUT,                0},
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
        0, // local methods
        tattr_desc,
        sizeof(PRIVATE_DATA),
        authz_table,
        command_table,
        s_user_trace_level,
        0 // gcflags
    );
    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC int register_c_mqtt_broker(void)
{
    return create_gclass(C_MQTT_BROKER);
}
