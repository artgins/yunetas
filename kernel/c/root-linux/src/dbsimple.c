/***********************************************************************
 *          dbsimple.c
 *
 *          Simple DB file for persistent attributes
 *
 *          Copyright (c) 2015 Niyamaka.
 *          Copyright (c) 2024-2026, ArtGins.
 *          All Rights Reserved.
***********************************************************************/
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>

#include <kwid.h>
#include "yunetas_environment.h"
#include "dbsimple.h"

/***************************************************************
 *              Constants
 ***************************************************************/
/*
 *  The name of the file a save writes before its rename(): "<file>.tmp-"
 *  and six letters of mkostemp(), a name nobody gives a backup
 */
#define TMP_INFIX   ".tmp-"

/***************************************************************
 *              Structures
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE int save_json(
    hgobj gobj,
    json_t *jn  // owned
);

/***************************************************************
 *              Data
 ***************************************************************/

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE char *get_persist_filename(
    hgobj gobj,
    char *bf,
    int bflen,
    const char *label,
    BOOL create_directories)
{
    char filename[NAME_MAX];
    snprintf(filename, sizeof(filename), "%s-%s-%s.json",
        gobj_gclass_name(gobj),
        gobj_name(gobj),
        label
    );

    return yuneta_realm_file(bf, bflen, "data", filename, create_directories);
}

/***************************************************************************
 *  Who the yuno's user is, for a yuno run as root: the owner of the lowest
 *  directory of the CLOSED chain that starts at "/" -- every directory from
 *  "/" down to it nobody else can write, and each one owned by root or by
 *  that user (/yuneta/realms, 0755, on a node). The chain is walked down
 *  with openat(O_NOFOLLOW) and ends at the first directory others can write
 *  (the realm's, 02775) or of a third user: what lies below it can be
 *  renamed away and replaced by someone else, so nothing found there says
 *  who the yuno's user is. Up to 7.25.22 the walk went UP from the file and
 *  took the first closed directory: one planted under the 02775 parent, or
 *  a symlink to one, named its maker as the yuno's user -- the file was
 *  loaded, and the next save given to them with the yuno's secrets.
 *
 *  A symlink met INSIDE the closed chain (/yuneta -> /srv/yuneta) is
 *  followed: only root or the chain's user can have put it there. Its
 *  target is walked from "/" again, under the same rules, and the user the
 *  chain named so far still holds: the chain behind the link may be root's
 *  or theirs, nobody else's. Its first form stopped at any symlink, and a
 *  root yuno under a linked /yuneta refused its own files. A ".." is the
 *  parent of the real directories walked, and the path is walked again
 *  without what the directories it leaves had named (a first form carried
 *  it: /a/X/../W trusted X, whose directory the path does not go through).
 *  (uid_t)-1 if it cannot be known (logged).
 ***************************************************************************/
#define TRUST_WALK_MAX_LINKS    40      // as the kernel's limit of symlinks in a path

/*
 *  One walk of `dir` from "/" (see trusted_dir_owner()). 0 when the chain
 *  ends, `*trusted` its user; 1 when it met a symlink, 2 a "..": `dir` is
 *  rewritten with what to walk instead; -1 on error (logged).
 */
PRIVATE int walk_closed_chain(
    hgobj gobj,
    const char *filename,
    char *dir,
    size_t dirsize,
    uid_t *trusted
)
{
    int fd = open("/", O_PATH|O_DIRECTORY|O_CLOEXEC);
    struct stat st;
    if(fd < 0 || fstat(fd, &st) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot stat the root directory",
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        if(fd >= 0) {
            close(fd);
        }
        return -1;
    }
    if(st.st_mode & (S_IWGRP|S_IWOTH)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "The root directory is open to others: no owner can be trusted",
            "path",         "%s", filename,
            NULL
        );
        close(fd);
        return -1;
    }
    uid_t at_start = *trusted;  // carried by a symlink (its owner chose the target), or none
    if(*trusted == (uid_t)-1) {
        *trusted = st.st_uid;
    }

    char walked[PATH_MAX];  // the real directories of the chain, from "/"
    walked[0] = 0;
    const char *p = dir;
    while(*p) {
        if(*p == '/') {
            p++;
            continue;
        }
        size_t len = strcspn(p, "/");
        const char *rest = p + len;     // "" or "/..."
        char seg[NAME_MAX + 1];
        if(len > NAME_MAX) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "A name above the persistent attrs file is too long",
                "path",         "%s", filename,
                NULL
            );
            close(fd);
            return -1;
        }
        memcpy(seg, p, len);
        seg[len] = 0;

        if(strcmp(seg, ".") == 0) {
            p = rest;
            continue;
        }

        char again[PATH_MAX];
        int n = -1;
        BOOL is_link = TRUE;
        if(strcmp(seg, "..") == 0) {
            char *slash = strrchr(walked, '/');
            if(slash) {
                *slash = 0;     // the parent of a real directory ("/" stays "/")
            }
            n = snprintf(again, sizeof(again), "%s%s", walked, rest);
            /*
             *  What this walk took from the directories it leaves is not
             *  carried: /a/X/../W names nobody X owns, the path does not go
             *  through X. Only what a symlink carried in holds
             */
            *trusted = at_start;
            is_link = FALSE;
        } else {
            int next = openat(fd, seg, O_PATH|O_NOFOLLOW|O_CLOEXEC);
            if(next < 0 || fstat(next, &st) < 0) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM,
                    "msg",          "%s", "Cannot stat a directory above the persistent attrs file",
                    "path",         "%s", filename,
                    "segment",      "%s", seg,
                    "errno",        "%d", errno,
                    "serrno",       "%s", strerror(errno),
                    NULL
                );
                if(next >= 0) {
                    close(next);
                }
                close(fd);
                return -1;
            }
            if(S_ISLNK(st.st_mode)) {
                char target[PATH_MAX];
                ssize_t tl = readlinkat(next, "", target, sizeof(target) - 1);
                close(next);
                if(tl <= 0) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_SYSTEM,
                        "msg",          "%s", "Cannot read a symlink above the persistent attrs file",
                        "path",         "%s", filename,
                        "segment",      "%s", seg,
                        "errno",        "%d", errno,
                        "serrno",       "%s", strerror(errno),
                        NULL
                    );
                    close(fd);
                    return -1;
                }
                target[tl] = 0;
                if(target[0] == '/') {
                    n = snprintf(again, sizeof(again), "%s%s", target, rest);
                } else {
                    n = snprintf(again, sizeof(again), "%s/%s%s", walked, target, rest);
                }
            } else {
                close(fd);
                fd = next;
                if(!S_ISDIR(st.st_mode)) {
                    break;  // not a directory: the open of the file says it
                }
                if(st.st_mode & (S_IWGRP|S_IWOTH)) {
                    break;  // open to others: what is below can be replaced
                }
                if(*trusted == 0) {
                    *trusted = st.st_uid;
                } else if(st.st_uid != 0 && st.st_uid != *trusted) {
                    break;  // a third user's: what is below can be replaced
                }
                size_t wl = strlen(walked);
                snprintf(walked + wl, sizeof(walked) - wl, "/%s", seg);
                p = rest;
                continue;
            }
        }

        close(fd);
        if(n < 0 || (size_t)n >= sizeof(again) || (size_t)n >= dirsize) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "The path above the persistent attrs file, a symlink followed, is too long",
                "path",         "%s", filename,
                "segment",      "%s", seg,
                NULL
            );
            return -1;
        }
        snprintf(dir, dirsize, "%s", again);
        return is_link? 1 : 2;
    }
    close(fd);
    return 0;
}

PRIVATE uid_t trusted_dir_owner(hgobj gobj, const char *filename)
{
    if(filename[0] != '/') {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The persistent attrs file is not a full path",
            "path",         "%s", filename,
            NULL
        );
        return (uid_t)-1;
    }

    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", filename);
    char *slash = strrchr(dir, '/');
    *slash = 0;     // the directory of the file

    /*
     *  Only the symlinks are counted: a ".." always walks a shorter path
     *  again, a link can make it longer and loop
     */
    uid_t trusted = (uid_t)-1;
    int links = 0;
    while(links <= TRUST_WALK_MAX_LINKS) {
        int ret = walk_closed_chain(gobj, filename, dir, sizeof(dir), &trusted);
        if(ret < 0) {
            return (uid_t)-1;   // Error already logged
        }
        if(ret == 0) {
            return trusted;
        }
        if(ret == 1) {
            links++;
        }
    }
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_SYSTEM,
        "msg",          "%s", "Too many symlinks above the persistent attrs file: no owner can be trusted",
        "path",         "%s", filename,
        "max",          "%d", TRUST_WALK_MAX_LINKS,
        NULL
    );
    return (uid_t)-1;
}

/***************************************************************************
 *  Is the persistent attrs file one to read, and one the yuno can keep?
 *  Return -1 when it is not a regular file, or one of another user not to
 *  be trusted (logged). Else 0, and
 *  `*must_replace` TRUE when it is the yuno's own and not as save_json()
 *  writes it: 0600 and one name only. Such a file (one left 0664 by a
 *  release before 7.25.19, a hard link) is not changed in place -- a
 *  fchmod() through a hard link changes another name -- it is replaced by
 *  load_json(). A file of another user is never replaced at a load (a
 *  warning).
 ***************************************************************************/
PRIVATE int check_persist_file(hgobj gobj, int fd, const char *filename, BOOL *must_replace)
{
    *must_replace = FALSE;

    struct stat st;
    if(fstat(fd, &st) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot stat the persistent attrs file",
            "path",         "%s", filename,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }
    if(!S_ISREG(st.st_mode)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "The persistent attrs file is not a regular file",
            "path",         "%s", filename,
            NULL
        );
        return -1;
    }
    if((st.st_mode & 07777) == 0600 && st.st_nlink == 1 && st.st_uid == geteuid()) {
        return 0;
    }

    char mode[16];
    snprintf(mode, sizeof(mode), "0%o", (unsigned)(st.st_mode & 07777));
    if(st.st_uid != geteuid()) {
        /*
         *  Another user's. Root's is trusted (the yuno run once as root),
         *  and so, for a yuno run as root, is the one of the yuno's own
         *  user (trusted_dir_owner()). Any other is refused:
         *  the data directory is group-writable (02775), so a member of the
         *  group could plant it, and the persistent attrs of the agent and
         *  of logcenter held commands run with system(). Up to 7.25.21 it
         *  was loaded, with a warning. Refused, nothing is loaded and the
         *  saves are refused (they would lose its other attrs).
         */
        BOOL trusted = (st.st_uid == 0)? TRUE : FALSE;
        if(!trusted && geteuid() == 0) {
            trusted = (st.st_uid == trusted_dir_owner(gobj, filename))? TRUE : FALSE;
        }
        if(trusted && (st.st_mode & (S_IWGRP|S_IWOTH))) {
            /*
             *  Trusted by its owner, but writable by others: one left 0664
             *  by a release before 7.25.19, in the group-writable data dir,
             *  can have been edited by any member of the group. It is not
             *  the yuno's, so it is not replaced either (up to 7.25.21 it
             *  was loaded)
             */
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Persistent attrs file of another user that others can write refused: not loaded, saves refused until it is made 0600 or given to the yuno's user",
                "path",         "%s", filename,
                "mode",         "%s", mode,
                "uid",          "%d", (int)st.st_uid,
                "euid",         "%d", (int)geteuid(),
                NULL
            );
            return -1;
        }
        if(!trusted) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Persistent attrs file of another user refused: not loaded, saves refused until it is removed or given to the yuno's user",
                "path",         "%s", filename,
                "mode",         "%s", mode,
                "uid",          "%d", (int)st.st_uid,
                "euid",         "%d", (int)geteuid(),
                NULL
            );
            return -1;
        }
        /*
         *  A load only reads, and never takes it. Replacing it here made
         *  the next start, as its own user, read nothing, and its next save
         *  write only the attrs given -- every other one lost.
         */
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Persistent attrs file of another user, left as it is",
            "path",         "%s", filename,
            "mode",         "%s", mode,
            "uid",          "%d", (int)st.st_uid,
            "euid",         "%d", (int)geteuid(),
            NULL
        );
        return 0;
    }
    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_SYSTEM,
        "msg",          "%s", "Persistent attrs file replaced by a 0600 one of the yuno's own",
        "path",         "%s", filename,
        "old_mode",     "%s", mode,
        "links",        "%d", (int)st.st_nlink,
        "uid",          "%d", (int)st.st_uid,
        NULL
    );
    *must_replace = TRUE;
    return 0;
}

/***************************************************************************
 *  Remove what a save that died between its create and its rename() left:
 *  "<file>.tmp-XXXXXX", a regular file (a symlink is never followed, and one
 *  of that name is left, logged)
 ***************************************************************************/
PRIVATE void remove_stale_temp_files(hgobj gobj, const char *filename)
{
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", filename);
    char *slash = strrchr(dir, '/');
    if(!slash) {
        return;     // get_persist_filename() gives a full path
    }
    *slash = 0;
    const char *base = slash + 1;
    size_t base_len = strlen(base);

    DIR *d = opendir(dir);
    if(!d) {
        return;     // no directory: no file, nothing left there
    }
    int dfd = dirfd(d);
    struct dirent *de;
    while((de = readdir(d))) {
        const char *name = de->d_name;
        size_t infix_len = strlen(TMP_INFIX);
        if(strncmp(name, base, base_len)!=0 ||
                strncmp(name + base_len, TMP_INFIX, infix_len)!=0 ||
                strlen(name + base_len + infix_len) != 6) {
            continue;
        }
        BOOL pattern = TRUE;
        for(const char *c = name + base_len + infix_len; *c; c++) {
            if(!isalnum((unsigned char)*c)) {
                pattern = FALSE;
                break;
            }
        }
        if(!pattern) {
            continue;
        }
        struct stat st;
        if(fstatat(dfd, name, &st, AT_SYMLINK_NOFOLLOW) < 0) {
            continue;   // gone meanwhile
        }
        if(!S_ISREG(st.st_mode)) {
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "A temporary persistent attrs name that is not a regular file, left",
                "path",         "%s", dir,
                "name",         "%s", name,
                NULL
            );
            continue;
        }
        if(unlinkat(dfd, name, 0) < 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot remove a stale temporary persistent attrs file",
                "path",         "%s", dir,
                "name",         "%s", name,
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
            continue;
        }
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Stale temporary persistent attrs file removed (a save that did not end)",
            "path",         "%s", dir,
            "name",         "%s", name,
            NULL
        );
    }
    closedir(d);
}

/***************************************************************************
 *  O_NOFOLLOW: the data dirs are 02775, so a member of the group could
 *  plant a symlink in place of the file.
 ***************************************************************************/
PRIVATE int open_persist_file(hgobj gobj, const char *filename, int flags)
{
    int fd = open(filename, flags|O_NOFOLLOW|O_CLOEXEC, 0600);
    if(fd < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", errno==ELOOP?
                "Refused the persistent attrs file: it is a symlink" :
                "Cannot open the persistent attrs file",
            "path",         "%s", filename,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }
    return fd;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *load_json(
    hgobj gobj,
    BOOL *failed    // out, may be NULL: TRUE when a file is there and cannot be read
)
{
    if(failed) {
        *failed = FALSE;
    }
    char filename[PATH_MAX];
    get_persist_filename(gobj, filename, sizeof(filename), "persistent-attrs", FALSE);

    remove_stale_temp_files(gobj, filename);

    struct stat st;
    if(lstat(filename, &st) < 0) {
        if(errno != ENOENT) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot stat the persistent attrs file",
                "path",         "%s", filename,
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
        }
        // No persistent attrs saved
        return 0;
    }

    if(S_ISREG(st.st_mode) && st.st_size == 0) {
        /*
         *  An empty file holds nothing: no attrs, and nothing a save could
         *  lose. It is not "a file that cannot be parsed", which refuses saves.
         */
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Persistent attrs file empty: no attrs in it",
            "path",         "%s", filename,
            NULL
        );
        return 0;
    }

    int fd = open_persist_file(gobj, filename, O_RDONLY|O_NONBLOCK);
    if(fd < 0) {
        // Error already logged
        if(failed && !S_ISLNK(st.st_mode)) {
            *failed = TRUE;     // a planted symlink is not data of the yuno: a save replaces it
        }
        return 0;
    }

    /*
     *  A file written before 7.25.19 is 0664 with the secrets in it, and a
     *  secret set once (the emailsender password) is never saved again: it
     *  is replaced now, not at a save that may never come. It is still
     *  loaded: refusing it would not hide it.
     */
    BOOL must_replace = FALSE;
    if(check_persist_file(gobj, fd, filename, &must_replace) < 0) {
        // Error already logged
        close(fd);
        if(failed) {
            *failed = TRUE;
        }
        return 0;
    }

    size_t flags = 0;
    json_error_t error;
    json_t *jn_device = json_loadfd(fd, flags, &error);
    close(fd);
    if(!jn_device) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_JSON,
            "msg",          "%s", "Persistent attrs file cannot be parsed: saves are refused until it is removed (its attrs go back to their defaults) or repaired",
            "path",         "%s", filename,
            "error",        "%s", error.text,
            "line",         "%d", error.line,
            NULL
        );
        if(failed) {
            *failed = TRUE;
        }
    } else if(must_replace) {
        save_json(gobj, json_incref(jn_device));    // Error logged if it fails
    }
    return jn_device;
}

/***************************************************************************
 *  Take the room of `len` bytes for the file, keeping its size (the old
 *  content is not grown with NULs). A filesystem that cannot (NFSv3, FUSE,
 *  an old ZFS: EOPNOTSUPP) goes on without the reservation, logged.
 *  Return -1 (errno set, logged by the caller) when there is no room.
 ***************************************************************************/
PRIVATE int reserve_room(hgobj gobj, int fd, const char *filename, size_t len)
{
    if(fallocate(fd, FALLOC_FL_KEEP_SIZE, 0, (off_t)len) == 0) {
        return 0;
    }
    if(errno == EOPNOTSUPP || errno == ENOSYS) {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "The filesystem cannot reserve room: the persistent attrs are written without it",
            "path",         "%s", filename,
            NULL
        );
        return 0;
    }
    return -1;
}

/***************************************************************************
 *  pwrite()/pread() of all `len` bytes at `offset`, over short answers and
 *  EINTR: -1 with errno at the first failure (EIO for a write of 0 bytes,
 *  ENODATA for a file shorter than `len`). `*done` (if not NULL): the
 *  bytes written, a failure included.
 ***************************************************************************/
PRIVATE int pwrite_all(int fd, const char *bf, size_t len, off_t offset, size_t *done_)
{
    size_t done = 0;
    int ret = 0;
    while(done < len) {
        ssize_t n = pwrite(fd, bf + done, len - done, offset + (off_t)done);
        if(n < 0) {
            if(errno == EINTR) {
                continue;
            }
            ret = -1;
            break;
        }
        if(n == 0) {
            errno = EIO;
            ret = -1;
            break;
        }
        done += (size_t)n;
    }
    if(done_) {
        *done_ = done;
    }
    return ret;
}

PRIVATE int pread_all(int fd, char *bf, size_t len, off_t offset)
{
    size_t done = 0;
    while(done < len) {
        ssize_t n = pread(fd, bf + done, len - done, offset + (off_t)done);
        if(n < 0) {
            if(errno == EINTR) {
                continue;
            }
            return -1;
        }
        if(n == 0) {
            errno = ENODATA;
            return -1;
        }
        done += (size_t)n;
    }
    return 0;
}

/***************************************************************************
 *  The save of a file in a directory the yuno cannot write: in place, and
 *  only into a file of its own, regular and of one name (O_NOFOLLOW; made
 *  0600 before a byte is written). Anything else is refused, logged.
 ***************************************************************************/
PRIVATE int save_json_in_place(hgobj gobj, const char *filename, json_t *jn)
{
    int fd = open(filename, O_RDWR|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK);
    if(fd < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Persistent attrs NOT saved: the directory cannot be written, and there is no file of the yuno's own to write in place",
            "path",         "%s", filename,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }
    struct stat st;
    if(fstat(fd, &st) < 0 || !S_ISREG(st.st_mode) || st.st_nlink != 1 || st.st_uid != geteuid()) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Persistent attrs NOT saved: the directory cannot be written, and the file is not the yuno's own, regular and of one name",
            "path",         "%s", filename,
            NULL
        );
        close(fd);
        return -1;
    }

    /*
     *  The new content is written over the old one before the file is cut:
     *  room is taken first without changing the size (reserve_room(): no
     *  NULs added to the old content), a shorter content is padded with
     *  blanks (a json with blanks after it parses), and the file is cut to
     *  it only once it is on disk. The room taken does not stop an ENOSPC
     *  half way where the filesystem cannot reserve it (NFSv3, FUSE) or
     *  writes elsewhere (copy-on-write: btrfs, reflinked XFS): the old
     *  content, read first, is then written back. A crash from the first
     *  pwrite() until its fsync() returns, or a write back that fails too,
     *  can leave a file that cannot be parsed -- which refuses the next
     *  saves, and says so.
     */
    const char *failed = NULL;
    int last_errno = 0;
    BOOL written_over = FALSE;  // the old content may be partly gone
    size_t old_len = (size_t)st.st_size;
    char *old = old_len? gbmem_malloc(old_len) : NULL;
    char *content = json_dumps(jn, JSON_INDENT(4));
    size_t content_len = content? strlen(content) : 0;
    size_t write_len = MAX(content_len, old_len);
    char *bf = content? gbmem_malloc(write_len) : NULL;
    if(!content || !bf || (old_len && !old)) {
        failed = "Cannot dump the persistent attrs";
        last_errno = errno;
    } else if(old_len && pread_all(fd, old, old_len, 0) < 0) {
        failed = "Cannot read the persistent attrs file before writing over it, the file is left as it was";
        last_errno = errno;
    } else {
        memcpy(bf, content, content_len);
        memset(bf + content_len, ' ', write_len - content_len);
        if((st.st_mode & 07777) != 0600 && fchmod(fd, 0600) < 0) {
            failed = "Cannot make the persistent attrs file 0600";
            last_errno = errno;
        } else if(reserve_room(gobj, fd, filename, write_len) < 0) {
            failed = "No room for the persistent attrs, the file is left as it was";
            last_errno = errno;
        } else {
            size_t done = 0;
            if(pwrite_all(fd, bf, write_len, 0, &done) < 0) {
                failed = "Cannot write the persistent attrs file";
                last_errno = errno;
                written_over = done > 0;
            } else if(fsync(fd) < 0) {
                failed = "Cannot sync the persistent attrs file";
                last_errno = errno;
                written_over = TRUE;
            } else if(ftruncate(fd, (off_t)content_len) < 0 || fsync(fd) < 0) {
                failed = "Cannot cut the persistent attrs file (it is padded with blanks, and parses)";
                last_errno = errno;
            }
        }
    }
    const char *file_state = NULL;
    if(failed && written_over) {
        if((old_len == 0 || pwrite_all(fd, old, old_len, 0, NULL) == 0) &&
                ftruncate(fd, (off_t)old_len) == 0 && fsync(fd) == 0) {
            file_state = "written back as it was";
        } else {
            file_state = "UNPARSABLE: the old content could not be written back, the next start loads the defaults and refuses every save";
        }
    }
    GBMEM_FREE(old)
    GBMEM_FREE(bf)
    GBMEM_FREE(content)     // jansson allocates through gbmem
    if(close(fd) < 0 && !failed) {
        failed = "Cannot close the persistent attrs file";
        last_errno = errno;
    }
    if(failed) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", failed,
            "path",         "%s", filename,
            "errno",        "%d", last_errno,
            "serrno",       "%s", strerror(last_errno),
            "file",         "%s", file_state? file_state : "",
            NULL
        );
        return -1;
    }
    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_SYSTEM,
        "msg",          "%s", "Persistent attrs saved in place: the directory cannot be written",
        "path",         "%s", filename,
        NULL
    );
    return 0;
}

/***************************************************************************
 *  The directory of `filename` synced, so the rename() survives a crash
 ***************************************************************************/
PRIVATE void sync_parent_dir(hgobj gobj, const char *filename)
{
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", filename);
    char *slash = strrchr(dir, '/');
    if(!slash) {
        return;     // get_persist_filename() gives a full path
    }
    *slash = 0;
    int dfd = open(dir, O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    if(dfd < 0 || fsync(dfd) < 0) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot sync the directory of the persistent attrs file: the save may not survive a crash",
            "path",         "%s", dir,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
    }
    if(dfd >= 0) {
        close(dfd);
    }
}

/***************************************************************************
 *  Write the persistent attrs to a NEW file in the same directory and
 *  rename() it over the old one. The new file is created O_EXCL and 0600
 *  (mkostemp()), so a hard link or a symlink planted in the old one's
 *  place is replaced and nothing is written through it, and the old file
 *  is never truncated before the new one is complete: a failed save leaves
 *  it as it was. The new file is the yuno's user's; when root saves over a
 *  file of another user, it is given to that user (fchown), so root never
 *  takes the file from the yuno. A file there that cannot be read is never
 *  replaced: db_save/remove_persistent_attrs() refuse before coming here.
 *  A persistent attr can be a secret (the SMTP password of the
 *  emailsender, set with set-email-user). Up to 7.25.18 json_dump_file()
 *  created the file with the process umask, 0666 on every node; up to
 *  7.25.20 it was written in place.
 *  A crash between the create and the rename() leaves a
 *  "<file>.tmp-XXXXXX" of 0600 in the directory (the next load removes it).
 *  A directory the yuno cannot write: the save goes in place, into a file
 *  of its own only (save_json_in_place()), which a crash during its write
 *  can leave unparsable.
 ***************************************************************************/
PRIVATE int save_json(
    hgobj gobj,
    json_t *jn  // owned
)
{
    char filename[PATH_MAX];
    get_persist_filename(gobj, filename, sizeof(filename), "persistent-attrs", TRUE);

    char tmpname[PATH_MAX];
    if(snprintf(tmpname, sizeof(tmpname), "%s" TMP_INFIX "XXXXXX", filename) >= (int)sizeof(tmpname)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Path of the persistent attrs file too long",
            "path",         "%s", filename,
            NULL
        );
        JSON_DECREF(jn)
        return -1;
    }

    /*
     *  A file there of another user. Its attrs were loaded (a file that
     *  cannot be read, or is not trusted, refused the save before it came
     *  here), so nothing is lost by replacing it. Root (the yuno run once
     *  as root) gives the new file to the old owner when that is the yuno's
     *  user (trusted_dir_owner()): root never takes the file
     *  from the yuno, which would then read nothing. To anybody else root
     *  gives nothing: up to 7.25.21 it gave the new file, the secrets in
     *  it, to whoever owned the old one. Anyone else takes it over, logged.
     */
    struct stat st_old;
    BOOL foreign = (lstat(filename, &st_old) == 0 && S_ISREG(st_old.st_mode) &&
        st_old.st_uid != geteuid())? TRUE : FALSE;

    int fd = mkostemp(tmpname, O_CLOEXEC);
    if(fd < 0 && (errno == EACCES || errno == EPERM)) {
        int ret = save_json_in_place(gobj, filename, jn);  // Error logged if it fails
        JSON_DECREF(jn)
        return ret;
    }
    if(fd < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot create the new persistent attrs file",
            "path",         "%s", tmpname,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        JSON_DECREF(jn)
        return -1;
    }

    const char *failed = NULL;
    int last_errno = 0;
    BOOL give_back = (foreign && geteuid() == 0 &&
        st_old.st_uid == trusted_dir_owner(gobj, filename))? TRUE : FALSE;
    if(give_back && fchown(fd, st_old.st_uid, st_old.st_gid) < 0) {
        failed = "Cannot give the new persistent attrs file to the owner of the old one";
        last_errno = errno;
    } else if(json_dumpfd(jn, fd, JSON_INDENT(4)) < 0) {
        failed = "Cannot save device json database";
        last_errno = errno;
    } else if(fsync(fd) < 0) {
        failed = "Cannot sync the new persistent attrs file";
        last_errno = errno;
    }
    if(close(fd) < 0 && !failed) {
        failed = "Cannot close the new persistent attrs file";
        last_errno = errno;
    }
    if(!failed && rename(tmpname, filename) < 0) {
        failed = "Cannot rename the new persistent attrs file over the old one";
        last_errno = errno;
    }
    if(failed) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", failed,
            "path",         "%s", filename,
            "tmp",          "%s", tmpname,
            "errno",        "%d", last_errno,
            "serrno",       "%s", strerror(last_errno),
            NULL
        );
        unlink(tmpname);
        JSON_DECREF(jn)
        return -1;
    }

    sync_parent_dir(gobj, filename);
    if(foreign) {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", give_back?
                "Persistent attrs file of another user saved, and kept its owner" :
                "Persistent attrs file of another user taken over by the yuno's user",
            "path",         "%s", filename,
            "old_uid",      "%d", (int)st_old.st_uid,
            "euid",         "%d", (int)geteuid(),
            NULL
        );
    }
    JSON_DECREF(jn)
    return 0;
}

/***************************************************************************
   Load writable persistent attributes from simple file db
 ***************************************************************************/
PUBLIC int db_load_persistent_attrs(
    hgobj gobj,
    json_t *keys  // owned
) {
    json_t *jn_file = load_json(gobj, NULL);
    if(jn_file) {
        json_t *attrs = kw_clone_by_keys(
            gobj,
            jn_file,    // owned
            keys,       // owned
            FALSE
        );

        /*
         *  An attr of the file that is not SDF_PERSIST (any more) is not
         *  loaded, and a save keeps it in the file: said, once per load.
         *  Up to 7.25.22 it was dropped in silence -- the jwks that
         *  add-jwk persisted before 7.25.22, now the config's: a node whose
         *  keys came only from there lost its JWT logins at the restart.
         */
        const char *key; json_t *jn_value;
        json_object_foreach(attrs, key, jn_value) {
            const sdata_desc_t *it = gobj_attr_desc(gobj, key, FALSE);
            if(!it || !(it->flag & SDF_PERSIST)) {
                gobj_log_warning(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_CONFIGURATION,
                    "msg",          "%s", "Persistent attrs file holds an attr that is not persistent: NOT loaded (set it in the config; remove-persistent-attrs drops it from the file)",
                    "attr",         "%s", key,
                    NULL
                );
            }
        }

        gobj_write_attrs(gobj, attrs, SDF_PERSIST, 0);
    } else {
        JSON_DECREF(keys)
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int db_save_persistent_attrs(
    hgobj gobj,
    json_t *keys  // owned
) {
    json_t *jn_attrs = gobj_read_attrs(gobj, SDF_PERSIST, 0);
    json_t *attrs = kw_clone_by_keys(
        gobj,
        jn_attrs,   // owned
        keys,       // owned
        FALSE
    );

    /*
     *  What is not saved now is kept from the file: a file there that
     *  cannot be read refuses the save, or every attr not given would be
     *  lost (a file of root, left by a run of the yuno as root).
     */
    BOOL load_failed = FALSE;
    json_t *jn_file = load_json(gobj, &load_failed);
    if(load_failed) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Persistent attrs NOT saved: the file there cannot be read, and its other attrs would be lost",
            NULL
        );
        JSON_DECREF(attrs)
        return -1;
    }
    if(jn_file) {
        json_object_update_missing(attrs, jn_file);
        JSON_DECREF(jn_file)
    }

    return save_json(
        gobj,
        attrs  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int db_remove_persistent_attrs(
    hgobj gobj,
    json_t *keys  // owned
) {
    BOOL load_failed = FALSE;
    json_t *jn_file = load_json(gobj, &load_failed);
    if(load_failed) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Persistent attrs NOT removed: the file there cannot be read, and its other attrs would be lost",
            NULL
        );
        JSON_DECREF(keys)
        return -1;
    }

    json_t *attrs = kw_clone_by_not_keys(
        gobj,
        jn_file,    // owned
        keys,       // owned
        FALSE
    );

    return save_json(
        gobj,
        attrs  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *db_list_persistent_attrs(
    hgobj gobj,
    json_t *keys  // owned
) {
    json_t *jn_file = load_json(gobj, NULL);

    json_t *attrs = kw_clone_by_keys(
        gobj,
        jn_file,    // owned
        keys,       // owned
        FALSE
    );
    return attrs;
}
