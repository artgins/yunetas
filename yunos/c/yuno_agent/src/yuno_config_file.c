/****************************************************************************
 *          yuno_config_file.c
 *
 *          The configuration files the agent writes for a yuno. See
 *          yuno_config_file.h.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>

#include "yuno_config_file.h"

/***************************************************************************
 *  See yuno_config_file.h
 ***************************************************************************/
PUBLIC int write_yuno_config_file(
    hgobj gobj,
    gbuffer_t *gbuf,
    const char *path
)
{
    int fd = newfile(path, YUNO_CONFIG_FILE_PERMISSION, TRUE);
    if(fd<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot create the configuration file of a yuno",
            "path",         "%s", path,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        gbuffer_decref(gbuf);
        return -1;
    }

    /*
     *  open() keeps the mode of a file that exists: a wider one would
     *  leave the secrets of the configuration readable
     */
    if(fchmod(fd, YUNO_CONFIG_FILE_PERMISSION)<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot narrow the mode of the configuration file of a yuno, not written",
            "path",         "%s", path,
            "mode",         "0%o", YUNO_CONFIG_FILE_PERMISSION,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        close(fd);
        gbuffer_decref(gbuf);
        return -1;
    }

    int ret = 0;
    size_t len;
    while((len=gbuffer_chunk(gbuf))>0) {
        char *p = gbuffer_get(gbuf, len);
        if(write(fd, p, len) != (ssize_t)len) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot write the configuration file of a yuno",
                "path",         "%s", path,
                "errno",        "%d", errno,
                "strerror",     "%s", strerror(errno),
                NULL
            );
            ret = -1;
            break;
        }
    }
    close(fd);
    gbuffer_decref(gbuf);
    return ret;
}

/***************************************************************************
 *  See yuno_config_file.h
 ***************************************************************************/
PUBLIC int narrow_stale_yuno_config_files(
    hgobj gobj,
    const char *bin_path,
    const char *role_plus_name,
    int n_written
)
{
    DIR *dir = opendir(bin_path);
    if(!dir) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot open the bin directory of a yuno to narrow its old configuration files",
            "path",         "%s", bin_path,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    char suffix[NAME_MAX+1];
    snprintf(suffix, sizeof(suffix), "-%s.json", role_plus_name);
    size_t suffix_len = strlen(suffix);

    int ret = 0;
    struct dirent *de;
    while(1) {
        errno = 0;
        de = readdir(dir);
        if(!de) {
            if(errno != 0) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM,
                    "msg",          "%s", "Cannot read the bin directory of a yuno, old configuration files may stay readable",
                    "path",         "%s", bin_path,
                    "errno",        "%d", errno,
                    "strerror",     "%s", strerror(errno),
                    NULL
                );
                ret = -1;
            }
            break;
        }

        const char *name = de->d_name;
        size_t digits = strspn(name, "0123456789");
        if(digits == 0 || digits > 9 || strcmp(name + digits, suffix) != 0) {
            continue;
        }
        if(strlen(name) != digits + suffix_len) {
            continue;
        }
        if(atoi(name) <= n_written) {
            continue;
        }

        char path[PATH_MAX];
        build_path(path, sizeof(path), bin_path, name, NULL);

        int fd = openat(dirfd(dir), name, O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
        if(fd < 0) {
            if(errno == ELOOP) {
                gobj_log_warning(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM,
                    "msg",          "%s", "An old configuration file of a yuno is a symbolic link, left as it is",
                    "path",         "%s", path,
                    NULL
                );
                continue;
            }
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot open an old configuration file of a yuno to narrow its mode",
                "path",         "%s", path,
                "errno",        "%d", errno,
                "strerror",     "%s", strerror(errno),
                NULL
            );
            ret = -1;
            continue;
        }

        struct stat st;
        if(fstat(fd, &st) < 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot stat an old configuration file of a yuno",
                "path",         "%s", path,
                "errno",        "%d", errno,
                "strerror",     "%s", strerror(errno),
                NULL
            );
            close(fd);
            ret = -1;
            continue;
        }
        if(!S_ISREG(st.st_mode)) {
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "An old configuration file of a yuno is not a regular file, left as it is",
                "path",         "%s", path,
                NULL
            );
            close(fd);
            continue;
        }

        mode_t mode = st.st_mode & 07777;
        mode_t narrowed = mode & YUNO_CONFIG_FILE_PERMISSION;
        if(narrowed != mode) {
            if(fchmod(fd, narrowed) < 0) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM,
                    "msg",          "%s", "Cannot narrow the mode of an old configuration file of a yuno, it stays readable",
                    "path",         "%s", path,
                    "mode",         "0%o", (unsigned)mode,
                    "errno",        "%d", errno,
                    "strerror",     "%s", strerror(errno),
                    NULL
                );
                ret = -1;
            }
        }
        close(fd);
    }
    closedir(dir);
    return ret;
}
