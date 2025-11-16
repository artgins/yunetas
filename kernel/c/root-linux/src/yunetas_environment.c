/****************************************************************************
 *              YUNETA_ENVIRONMENT.C
 *              Yuneta
 *
 *              Copyright (c) 2014-2023 Niyamaka.
 *              Copyright (c) 2024, ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <helpers.h>
#include "yunetas_environment.h"

/***************************************************************************
 *  Prototypes
 ***************************************************************************/

/***************************************************************************
 *  Data
 ***************************************************************************/
PRIVATE int __xpermission__ = 0;    // permission for directories and executable files
PRIVATE int __rpermission__ = 0;    // permission for regular files
PRIVATE char __root_dir__[PATH_MAX] = {0};
PRIVATE char __domain_dir__[PATH_MAX] = {0};

/***************************************************************************
 *  Register environment
 ***************************************************************************/
PUBLIC int register_yuneta_environment(
    const char *root_dir,
    const char *domain_dir,
    int xpermission,
    int rpermission)
{
    __xpermission__ = xpermission;
    __rpermission__ = rpermission;

    snprintf(__root_dir__, sizeof(__root_dir__), "%s", root_dir?root_dir:"");
    snprintf(__domain_dir__, sizeof(__domain_dir__), "%s", domain_dir?domain_dir:"");

    strntolower(__root_dir__, strlen(__root_dir__));
    strntolower(__domain_dir__, strlen(__domain_dir__));

    return 0;
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
PUBLIC const char *yuneta_root_dir(void)
{
    return __root_dir__;
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
    if(empty_string(yuneta_root_dir()) || empty_string(yuneta_domain_dir())) {
        *bf = 0;
        return 0;
    }

    build_path(bf, bfsize, yuneta_root_dir(), yuneta_domain_dir(), subdomain, NULL);

    if(create) {
        if(!is_directory(bf)) {
            mkrdir(bf, yuneta_xpermission());
            if(!is_directory(bf)) {
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

    build_path(bf, bfsize, realm_path, filename, NULL);

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

    build_path(bf, bfsize, log_path, filename, NULL);

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

    build_path(bf, bfsize, log_path, filename, NULL);

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
    if(empty_string(yuneta_root_dir())) {
        *bf = 0;
        return 0;
    }

    build_path(bf, bfsize, yuneta_root_dir(), "store", dir, subdir, NULL);

    if(create) {
        if(!is_directory(bf)) {
            mkrdir(bf, yuneta_xpermission());
            if(!is_directory(bf)) {
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

    build_path(bf, bfsize, store_path, filename, NULL);

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
    if(empty_string(yuneta_root_dir())) {
        *bf = 0;
        return 0;
    }

    build_path(bf, bfsize, yuneta_root_dir(), "store", service, owner, realm_id, dir, NULL);

    if(create) {
        if(!is_directory(bf)) {
            mkrdir(bf, yuneta_xpermission());
            if(!is_directory(bf)) {
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
PUBLIC char *yuneta_realm_store_tenant_dir(
    char *bf,
    int bfsize,
    const char *service,
    const char *owner,
    const char *realm_id,
    const char *tenant,
    const char *dir,
    BOOL create
)
{
    if(empty_string(yuneta_root_dir())) {
        *bf = 0;
        return 0;
    }

    build_path(bf, bfsize, yuneta_root_dir(), "store", service, owner, realm_id, tenant, dir, NULL);

    if(create) {
        if(!is_directory(bf)) {
            mkrdir(bf, yuneta_xpermission());
            if(!is_directory(bf)) {
                *bf = 0;
                return 0;
            }
        }
    }
    return bf;
}
