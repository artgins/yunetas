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
#include <ctype.h>

#include "yuno_config_file.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define TEMP_CONFIG_PREFIX  ".config."  // + the six characters of mkostemp()

/***************************************************************************
 *  See yuno_config_file.h
 *
 *  The content goes to a new file in the same directory, and that file is
 *  renamed over `path`. So the file is the agent's own, created with its
 *  final mode, whoever owned the one it replaces: a file of another user
 *  that the agent can write only through the group is replaced, not
 *  truncated and then refused because its mode cannot be changed. A
 *  symbolic link at `path` is replaced too, never followed. And a failure
 *  leaves the old file whole. No fsync(): the file is written again from
 *  the agent's treedb at every launch.
 *
 *  The new file is TEMP_CONFIG_PREFIX "XXXXXX", a name of its own size: the
 *  bin directory is the yuno's, and the agent writes one file at a time.
 *  Up to 7.25.21 it was the name of `path` with 8 bytes more, and a name a
 *  few bytes under NAME_MAX could not be written.
 ***************************************************************************/
PUBLIC int write_yuno_config_file(
    hgobj gobj,
    gbuffer_t *gbuf,
    const char *path
)
{
    char tmp_path[PATH_MAX];
    const char *slash = strrchr(path, '/');
    int written;
    if(slash) {
        written = snprintf(tmp_path, sizeof(tmp_path), "%.*s/%sXXXXXX",
            (int)(slash - path), path, TEMP_CONFIG_PREFIX
        );
    } else {
        written = snprintf(tmp_path, sizeof(tmp_path), "%sXXXXXX", TEMP_CONFIG_PREFIX);
    }
    if(written < 0 || (size_t)written >= sizeof(tmp_path)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The path of the configuration file of a yuno is too long",
            "path",         "%s", path,
            NULL
        );
        gbuffer_decref(gbuf);
        return -1;
    }

    /*
     *  mkostemp() creates the file with O_EXCL: it never opens a file or a
     *  link that exists
     */
    int fd = mkostemp(tmp_path, O_CLOEXEC);
    if(fd<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot create the configuration file of a yuno",
            "path",         "%s", path,
            "tmp_path",     "%s", tmp_path,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        gbuffer_decref(gbuf);
        return -1;
    }

    int ret = 0;
    if(fchmod(fd, YUNO_CONFIG_FILE_PERMISSION)<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot set the mode of the configuration file of a yuno, not written",
            "path",         "%s", path,
            "tmp_path",     "%s", tmp_path,
            "mode",         "0%o", YUNO_CONFIG_FILE_PERMISSION,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        ret = -1;
    }

    size_t len;
    while(ret == 0 && (len=gbuffer_chunk(gbuf))>0) {
        char *p = gbuffer_get(gbuf, len);
        errno = 0;
        ssize_t w = write(fd, p, len);
        if(w != (ssize_t)len) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot write the configuration file of a yuno",
                "path",         "%s", path,
                "tmp_path",     "%s", tmp_path,
                "len",          "%lu", (unsigned long)len,
                "written",      "%ld", (long)w,
                "errno",        "%d", errno,
                "strerror",     "%s", strerror(errno),
                NULL
            );
            ret = -1;
        }
    }
    gbuffer_decref(gbuf);

    if(close(fd)<0 && ret == 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot close the configuration file of a yuno",
            "path",         "%s", path,
            "tmp_path",     "%s", tmp_path,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        ret = -1;
    }

    if(ret == 0 && rename(tmp_path, path)<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot replace the configuration file of a yuno",
            "path",         "%s", path,
            "tmp_path",     "%s", tmp_path,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        ret = -1;
    }

    if(ret < 0) {
        if(unlink(tmp_path)<0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot remove the temporary configuration file of a yuno",
                "tmp_path",     "%s", tmp_path,
                "errno",        "%d", errno,
                "strerror",     "%s", strerror(errno),
                NULL
            );
        }
    }
    return ret;
}

/***************************************************************************
 *  Are `p` the six characters that mkostemp() chose, and nothing more?
 ***************************************************************************/
PRIVATE BOOL is_mkostemp_tail(const char *p)
{
    if(strlen(p) != 6) {
        return FALSE;
    }
    for(int i=0; i<6; i++) {
        if(!isalnum((unsigned char)p[i])) {
            return FALSE;
        }
    }
    return TRUE;
}

/***************************************************************************
 *  Is `name` a temporary file of write_yuno_config_file()?
 *      TEMP_CONFIG_PREFIX "XXXXXX"
 *      ".<n>-<role>^<name>.json.XXXXXX", the name of 7.25.21: of the yuno of
 *      `suffix` ("-<role>^<name>.json"), or of any yuno if `suffix` is NULL
 ***************************************************************************/
PRIVATE BOOL is_temp_config_file(const char *name, const char *suffix, size_t suffix_len)
{
    size_t prefix_len = strlen(TEMP_CONFIG_PREFIX);
    if(strncmp(name, TEMP_CONFIG_PREFIX, prefix_len) == 0) {
        return is_mkostemp_tail(name + prefix_len);
    }

    if(name[0] != '.') {
        return FALSE;
    }
    const char *p = name + 1;
    size_t digits = strspn(p, "0123456789");
    if(digits == 0 || digits > 9) {
        return FALSE;
    }
    p += digits;
    if(suffix) {
        if(strncmp(p, suffix, suffix_len) != 0) {
            return FALSE;
        }
        p += suffix_len;
    } else {
        const char *json = strstr(p, ".json.");
        if(p[0] != '-' || !json) {
            return FALSE;
        }
        p = json + strlen(".json");
    }
    if(p[0] != '.') {
        return FALSE;
    }
    return is_mkostemp_tail(p + 1);
}

/***************************************************************************
 *  A temporary file of an earlier write that did not reach its rename (the
 *  agent died in between): removed if it is a regular file, not followed
 *  if it is a link. Returns 0, or -1 (logged).
 ***************************************************************************/
PRIVATE int remove_temp_config_file(hgobj gobj, DIR *dir, const char *bin_path, const char *name)
{
    char path[PATH_MAX];
    build_path(path, sizeof(path), bin_path, name, NULL);

    struct stat st;
    if(fstatat(dirfd(dir), name, &st, AT_SYMLINK_NOFOLLOW) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot stat a temporary configuration file of a yuno",
            "path",         "%s", path,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        return -1;
    }
    if(!S_ISREG(st.st_mode)) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "A temporary configuration file of a yuno is not a regular file, left as it is",
            "path",         "%s", path,
            NULL
        );
        return 0;
    }
    if(unlinkat(dirfd(dir), name, 0) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot remove a temporary configuration file of a yuno",
            "path",         "%s", path,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        return -1;
    }
    gobj_log_warning(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_SYSTEM,
        "msg",          "%s", "A temporary configuration file of a yuno, left by an interrupted write, removed",
        "path",         "%s", path,
        NULL
    );
    return 0;
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
    int written = snprintf(suffix, sizeof(suffix), "-%s.json", role_plus_name);
    if(written < 0 || (size_t)written >= sizeof(suffix)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The name of a yuno is too long to name its old configuration files, not narrowed",
            "path",         "%s", bin_path,
            "role_plus_name", "%s", role_plus_name,
            NULL
        );
        closedir(dir);
        return -1;
    }
    size_t suffix_len = (size_t)written;

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
        if(is_temp_config_file(name, suffix, suffix_len)) {
            if(remove_temp_config_file(gobj, dir, bin_path, name) < 0) {
                ret = -1;   // Error already logged
            }
            continue;
        }
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

/***************************************************************************
 *  See yuno_config_file.h
 ***************************************************************************/
PUBLIC int remove_temp_yuno_config_files(
    hgobj gobj,
    const char *bin_path
)
{
    DIR *dir = opendir(bin_path);
    if(!dir) {
        if(errno == ENOENT) {
            return 0;   // a yuno never launched has no bin directory
        }
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot open the bin directory of a yuno to remove its temporary configuration files",
            "path",         "%s", bin_path,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        return -1;
    }

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
                    "msg",          "%s", "Cannot read the bin directory of a yuno, temporary configuration files may stay",
                    "path",         "%s", bin_path,
                    "errno",        "%d", errno,
                    "strerror",     "%s", strerror(errno),
                    NULL
                );
                ret = -1;
            }
            break;
        }
        if(is_temp_config_file(de->d_name, NULL, 0)) {
            if(remove_temp_config_file(gobj, dir, bin_path, de->d_name) < 0) {
                ret = -1;   // Error already logged
            }
        }
    }
    closedir(dir);
    return ret;
}
