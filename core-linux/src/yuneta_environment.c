/****************************************************************************
 *              YUNETA_ENVIRONMENT.C
 *              Yuneta
 *
 *              Copyright (c) 2014-2015 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <helpers.h>
#include "yuneta_environment.h"

/***************************************************************************
 *  Prototypes
 ***************************************************************************/

/***************************************************************************
 *  Data
 ***************************************************************************/
PRIVATE json_t *__jn_config__ = 0;
PRIVATE int __xpermission__ = 0;    // permission for directories and executable files
PRIVATE int __rpermission__ = 0;    // permission for regular files
PRIVATE char __work_dir__[PATH_MAX] = {0};
PRIVATE char __domain_dir__[PATH_MAX] = {0};
PRIVATE char global_uuid[256] = {0};

/***************************************************************************
 *  Register environment
 ***************************************************************************/
PUBLIC int register_yuneta_environment(
    const char *work_dir,
    const char *domain_dir,
    int xpermission,
    int rpermission,
    json_t *jn_config)
{
    __xpermission__ = xpermission;
    __rpermission__ = rpermission;

    snprintf(__work_dir__, sizeof(__work_dir__), "%s", work_dir?work_dir:"");
    snprintf(__domain_dir__, sizeof(__domain_dir__), "%s", domain_dir?domain_dir:"");

    strntolower(__work_dir__, strlen(__work_dir__));
    strntolower(__domain_dir__, strlen(__domain_dir__));

    __jn_config__ = jn_config;

    return 0;
}


/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *yuneta_json_config(void)
{
    return __jn_config__;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int yuneta_xpermission(void)
{
    return __xpermission__;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int yuneta_rpermission(void)
{
    return __rpermission__;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC const char *yuneta_work_dir(void)
{
    return __work_dir__;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC const char *yuneta_domain_dir(void)
{
    return __domain_dir__;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC char *yuneta_realm_dir(
    char *bf,
    int bfsize,
    const char *subdomain,
    BOOL create
)
{
    if(empty_string(yuneta_work_dir()) || empty_string(yuneta_domain_dir())) {
        *bf = 0;
        return 0;
    }

    build_path3(bf, bfsize, yuneta_work_dir(), yuneta_domain_dir(), subdomain);

    if(create) {
        if(access(bf, 0)!=0) {
            mkrdir(bf, 0, yuneta_xpermission());
            if(access(bf, 0)!=0) {
                *bf = 0;
                return 0;
            }
        }
    }
    return bf;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC char *yuneta_realm_file(
    char *bf,
    int bfsize, const char *subdomain,
    const char *filename,
    BOOL create
)
{
    char realm_path[PATH_MAX];
    if(!yuneta_realm_dir(realm_path, sizeof(realm_path), subdomain, create)) {
        *bf = 0;
        return 0;
    }

    build_path2(bf, bfsize, realm_path, filename);

    return bf;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC char *yuneta_store_dir(
    char *bf,
    int bfsize,
    const char *dir,
    const char *subdir,
    BOOL create
)
{
    if(empty_string(yuneta_work_dir())) {
        *bf = 0;
        return 0;
    }

    build_path4(bf, bfsize, yuneta_work_dir(), "store", dir, subdir);

    if(create) {
        if(access(bf, 0)!=0) {
            mkrdir(bf, 0, yuneta_xpermission());
            if(access(bf, 0)!=0) {
                *bf = 0;
                return 0;
            }
        }
    }
    return bf;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC char *yuneta_store_file(
    char *bf,
    int bfsize,
    const char *dir,
    const char *subdir,
    const char *filename,
    BOOL create)    // from environment.global_store_dir json config
{
    char store_path[PATH_MAX];
    if(!yuneta_store_dir(store_path, sizeof(store_path), dir, subdir, create)) {
        *bf = 0;
        return 0;
    }

    build_path2(bf, bfsize, store_path, filename);

    return bf;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC char *yuneta_log_dir(char *bf, int bfsize, BOOL create)
{
    return yuneta_realm_dir(bf, bfsize, "logs", create);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC char *yuneta_log_file(
    char *bf,
    int bfsize,
    const char *filename,
    BOOL create)    // from environment.global_store_dir json config
{
    char log_path[PATH_MAX];
    if(!yuneta_log_dir(log_path, sizeof(log_path), create)) {
        *bf = 0;
        return 0;
    }

    build_path2(bf, bfsize, log_path, filename);

    return bf;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC char *yuneta_bin_dir(char *bf, int bfsize, BOOL create)
{
    return yuneta_realm_dir(bf, bfsize, "bin", create);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC char *yuneta_bin_file(
    char *bf,
    int bfsize,
    const char *filename,
    BOOL create)    // from environment.global_store_dir json config
{
    char log_path[PATH_MAX];
    if(!yuneta_bin_dir(log_path, sizeof(log_path), create)) {
        *bf = 0;
        return 0;
    }

    build_path2(bf, bfsize, log_path, filename);

    return bf;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC char *yuneta_realm_store_dir(
    char *bf,
    int bfsize,
    const char *service,
    const char *owner,
    const char *realm_id,
    const char *dir,
    BOOL create
)
{
    if(empty_string(yuneta_work_dir())) {
        *bf = 0;
        return 0;
    }

    build_path6(bf, bfsize, yuneta_work_dir(), "store", service, owner, realm_id, dir);

    if(create) {
        if(access(bf, 0)!=0) {
            mkrdir(bf, 0, yuneta_xpermission());
            if(access(bf, 0)!=0) {
                *bf = 0;
                return 0;
            }
        }
    }
    return bf;
}
