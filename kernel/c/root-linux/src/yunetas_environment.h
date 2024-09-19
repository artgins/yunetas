/****************************************************************************
 *              YUNETA_ENVIRONMENT.H
 *              Includes
 *              Copyright (c) 2014-2023 Niyamaka, 2024 ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
#pragma once

#ifdef __cplusplus
extern "C"{
#endif

#include <gobj.h>

/*********************************************************************
 *  Prototypes
 *********************************************************************/
PUBLIC int register_yuneta_environment(
    const char *root_dir,
    const char *domain_dir,
    int xpermission,    // permission for directories and executable files
    int rpermission,    // permission for regular files
    json_t *jn_config
);
PUBLIC json_t *yuneta_json_config(void);
PUBLIC int yuneta_xpermission(void);    // permission for directories and executable files
PUBLIC int yuneta_rpermission(void);    // permission for regular files

PUBLIC const char *yuneta_root_dir(void);   // from environment.root_dir json config, it's the main ROOT dir
PUBLIC const char *yuneta_domain_dir(void); // from environment.domain_dir, it's the path for the realms

/*------------------------------------*
 *      root_dir + domain_dir
 *------------------------------------*/
PUBLIC char *yuneta_realm_dir(
    char *bf,
    int bfsize,
    const char *subdomain,
    BOOL create
);
PUBLIC char *yuneta_realm_file(
    char *bf,
    int bfsize,
    const char *subdomain,
    const char *filename,
    BOOL create
);

PUBLIC char *yuneta_log_dir(
    char *bf,
    int bfsize,
    BOOL create
);
PUBLIC char *yuneta_log_file(
    char *bf,
    int bfsize,
    const char *filename,
    BOOL create
);
PUBLIC char *yuneta_bin_dir(
    char *bf,
    int bfsize,
    BOOL create
);
PUBLIC char *yuneta_bin_file(
    char *bf,
    int bfsize,
    const char *filename,
    BOOL create
);

/*------------------------------------*
 *      root_dir + "store"
 *------------------------------------*/
PUBLIC char *yuneta_store_dir(
    char *bf,
    int bfsize,
    const char *dir,
    const char *subdir,
    BOOL create
);
PUBLIC char *yuneta_store_file(
    char *bf,
    int bfsize,
    const char *dir,
    const char *subdir,
    const char *filename,
    BOOL create
);
PUBLIC char *yuneta_realm_store_dir(
    char *bf,
    int bfsize,
    const char *service,
    const char *owner,
    const char *realm_id,
    const char *dir,
    BOOL create
);

#ifdef __cplusplus
}
#endif
