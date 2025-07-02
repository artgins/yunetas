/* Copyright (C) 2015-2025 maClara, LLC <info@maclara-llc.com>
   This file is part of the JWT C Library

   SPDX-License-Identifier:  MPL-2.0
   This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include <string.h>

#include "jwt.h"

#include "jwt-private.h"

void jwt_checker_free(jwt_checker_t *__cmd)
{
    if(__cmd == NULL)
        return;

    json_decref(__cmd->c.payload);
    json_decref(__cmd->c.headers);

    memset(__cmd, 0, sizeof(*__cmd));

    jwt_freemem(__cmd);
}

jwt_checker_t *jwt_checker_new(void)
{
    jwt_checker_t *__cmd = jwt_malloc(sizeof(*__cmd));

    if(__cmd == NULL)
        return NULL;

    memset(__cmd, 0, sizeof(*__cmd));

    __cmd->c.payload = json_object();
    __cmd->c.headers = json_object();
    __cmd->c.claims = (JWT_CLAIM_EXP | JWT_CLAIM_NBF);

    if(!__cmd->c.payload || !__cmd->c.headers)
        jwt_freemem(__cmd);

    return __cmd;
}

static int __setkey_check(jwt_checker_t *__cmd, const jwt_alg_t alg,
                          const jwk_item_t *key)
{
    if(__cmd == NULL)
        return 1;
    if(key == NULL) {
        if(alg == JWT_ALG_NONE)
            return 0;

        jwt_write_error(__cmd, "Cannot set alg without a key");
    } else if(key->alg == JWT_ALG_NONE) {
        if(alg != JWT_ALG_NONE)
            return 0;

        jwt_write_error(__cmd, "Key provided, but could not find alg");
    } else {
        if(alg == JWT_ALG_NONE)
            return 0;

        if(alg == key->alg)
            return 0;

        jwt_write_error(__cmd, "Alg mismatch");
    }

    return 1;
}

int jwt_checker_setkey(jwt_checker_t *__cmd, const jwt_alg_t alg,
                       const jwk_item_t *key)
{
    if(__setkey_check(__cmd, alg, key))
        return 1;

    __cmd->c.alg = alg;
    __cmd->c.key = key;

    return 0;
}

int jwt_checker_error(const jwt_checker_t *__cmd)
{
    if(__cmd == NULL)
        return 1;

    return __cmd->error ? 1 : 0;
}

const char *jwt_checker_error_msg(const jwt_checker_t *__cmd)
{
    if(__cmd == NULL)
        return NULL;

    return __cmd->error_msg;
}

void jwt_checker_error_clear(jwt_checker_t *__cmd)
{
    if(__cmd == NULL)
        return;

    __cmd->error = 0;
    __cmd->error_msg[0] = '\0';
}

int jwt_checker_setcb(jwt_checker_t *__cmd, jwt_callback_t cb, void *ctx)
{
    if(__cmd == NULL)
        return 1;


    if(cb == NULL && __cmd->c.cb != NULL && ctx != NULL) {
        __cmd->c.cb_ctx = ctx;
        return 0;
    }

    if(cb == NULL && ctx != NULL) {
        jwt_write_error(__cmd, "Setting ctx without a cb won't work");
        return 1;
    }

    __cmd->c.cb = cb;
    __cmd->c.cb_ctx = ctx;

    return 0;
}

void *jwt_checker_getctx(jwt_checker_t *__cmd)
{
    if(__cmd == NULL)
        return NULL;

    return __cmd->c.cb_ctx;
}

typedef enum {
    __HEADER,
    __CLAIM,
} _setget_type_t;

typedef jwt_value_error_t (*__doer_t)(json_t *, jwt_value_t *);

static jwt_value_error_t __run_it(jwt_checker_t *__cmd, _setget_type_t type,
                                  jwt_value_t *value, __doer_t doer)
{
    json_t *which = NULL;


    switch(type) {
        case __CLAIM:
            which = __cmd->c.payload;
            break;

        default:
            return value->error = JWT_VALUE_ERR_INVALID;
    }

    return doer(which, value);
}

static const char *__get_name(jwt_claims_t type)
{
    if(type == JWT_CLAIM_ISS)
        return "iss";
    else if(type == JWT_CLAIM_AUD)
        return "aud";
    else if(type == JWT_CLAIM_SUB)
        return "sub";
    return NULL;
}

const char *jwt_checker_claim_get(jwt_checker_t *__cmd, jwt_claims_t type)
{
    const char *name = NULL;
    jwt_value_t jval;

    if(!__cmd)
        return NULL;

    name = __get_name(type);
    if(name == NULL)
        return NULL;

    jwt_set_GET_STR(&jval, name);
    __run_it(__cmd, __CLAIM, &jval, __getter);


    return jval.str_val;
}

int jwt_checker_claim_set(jwt_checker_t *__cmd, jwt_claims_t type, const char *value)
{
    const char *name = NULL;
    jwt_value_t jval;

    if(!__cmd || !value)
        return 1;

    name = __get_name(type);
    if(name == NULL)
        return 1;

    __cmd->c.claims |= type;

    jwt_set_SET_STR(&jval, name, value);
    jval.replace = 1;

    return __run_it(__cmd, __CLAIM, &jval, __setter) ? 1 : 0;
}

int jwt_checker_claim_del(jwt_checker_t *__cmd, jwt_claims_t type)
{
    const char *name = NULL;

    if(!__cmd)
        return 1;

    name = __get_name(type);
    if(name == NULL)
        return 1;

    __cmd->c.claims &= ~type;

    return __deleter(__cmd->c.payload, name);
}




int jwt_checker_time_leeway(jwt_checker_t *__cmd, jwt_claims_t claim, time_t secs)
{
    if(!__cmd)
        return 1;

    switch(claim) {
        case JWT_CLAIM_EXP:
            __cmd->c.exp = secs;
            break;

        case JWT_CLAIM_NBF:
            __cmd->c.nbf = secs;
            break;

        default:
            return 1;
    }

    if(secs <= -1)
        __cmd->c.claims &= ~claim;
    else
        __cmd->c.claims |= claim;

    return 0;
}


int jwt_checker_verify(jwt_checker_t *__cmd, const char *token)
{
    JWT_CONFIG_DECLARE(config);
    unsigned int payload_len;
    jwt_auto_t *jwt = NULL;

    if(__cmd == NULL)
        return 1;

    if(token == NULL || !strlen(token)) {
        jwt_write_error(__cmd, "Must pass a token");
        return 1;
    }

    jwt = jwt_new();
    if(jwt == NULL) {
        jwt_write_error(__cmd, "Could not allocate JWT object");
        return 1;
    }


    if(jwt_parse(jwt, token, &payload_len)) {
        jwt_copy_error(__cmd, jwt);
        return 1;
    };

    config.key = __cmd->c.key;
    config.alg = __cmd->c.alg;
    config.ctx = __cmd->c.cb_ctx;


    if(__cmd->c.cb && __cmd->c.cb(jwt, &config)) {
        jwt_write_error(__cmd, "User callback returned error");
        return 1;
    }


    if(__setkey_check(__cmd, config.alg, config.key))
        return 1;

    jwt->key = config.key;
    jwt->checker = __cmd;


    jwt = jwt_verify_complete(jwt, &config, token, payload_len);


    jwt_copy_error(__cmd, jwt);

    return __cmd->error;
}

json_t *jwt_checker_verify2(jwt_checker_t *__cmd, const char *token)
{
    JWT_CONFIG_DECLARE(config);
    unsigned int payload_len;
    jwt_auto_t *jwt = NULL;

    if(__cmd == NULL)
        return NULL;

    if(token == NULL || !strlen(token)) {
        jwt_write_error(__cmd, "Must pass a token");
        return NULL;
    }

    jwt = jwt_new();
    if(jwt == NULL) {
        jwt_write_error(__cmd, "Could not allocate JWT object");
        return NULL;
    }


    if(jwt_parse(jwt, token, &payload_len)) {
        jwt_copy_error(__cmd, jwt);
        jwt_free(jwt);
        return NULL;
    };

    config.key = __cmd->c.key;
    config.alg = __cmd->c.alg;
    config.ctx = __cmd->c.cb_ctx;

    if(__setkey_check(__cmd, config.alg, config.key)) {
        jwt_free(jwt);
        return NULL;
    }

    jwt->key = config.key;
    jwt->checker = __cmd;


    jwt = jwt_verify_complete(jwt, &config, token, payload_len);

    jwt_copy_error(__cmd, jwt);

    if(__cmd->error) {
        return NULL;
    } else {
        json_t *payload = json_incref(jwt->claims);
        return payload;
    }
}
