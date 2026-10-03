/****************************************************************************
 *          test_secret_attrs.c
 *
 *          Regression test for the SDF_SECRET attrs (7.25.19) and the file
 *          that persists them.
 *
 *          A secret is read, written and saved as usual and SHOWN masked.
 *          The places that show it, and what is checked in each:
 *
 *            1. view-gobj (gobj2json): C_IEVENT_SRV's `http_cookie`, the
 *               whole Cookie header of the websocket upgrade -- the BFF's
 *               httpOnly access_token included.
 *            2. view-config: the value a config gives to a SDF_SECRET attr,
 *               wherever the config gives it: a service's kw, a child's kw
 *               and a 'global' key ("<service>.<attr>").
 *            3. The create_delete2 trace: the kw of a service being built
 *               and its final sdata.
 *
 *            4. The commands trace (with ev_kw): the command line and
 *               the kw of a command whose parameter is SDF_SECRET, given
 *               as key=value, as a required positional parameter and in
 *               the kw -- and the kw the command parser expands. A
 *               positional secret with a '=' in it (abc==, 'q=...') is
 *               masked and the rest of the line still shown; a secret
 *               that is a number is masked; the error a malformed line
 *               answers ("extra parameters") does not echo a secret.
 *            4b. The command-yuno shape: a SDF_WILD_CMD command forwards
 *               free keys to a table that is not known where it is traced;
 *               a key with a secret's name (password=...) is masked, in
 *               the line, in the kw, and inside a quoted `command`.
 *            2b. view-config also masks: a secret that is a number, a
 *               'global' key whose prefix is the name of a child (not a
 *               gclass, not a service), and the __json_config_variables__
 *               that feed a secret or have a secret's name (also in the
 *               create_delete2 trace).
 *
 *          And the persistent-attrs file (dbsimple.c):
 *
 *            5. a file written before 7.25.19 (0664) is made 0600 when it
 *               is LOADED, not only at its next save -- a password set once
 *               is never saved again.
 *            6. a symlink planted in place of the file (the data dirs are
 *               02775) is not read, and a save replaces it with a file of
 *               its own, 0600, without writing through it; the same for a
 *               hard link (an inode that is not the yuno's). The save
 *               writes a new file and renames it over the old one: no
 *               temporary file is left, and a save that fails leaves the
 *               old file as it was -- and write-attr answers the failure.
 *            7. a file of another user (fstat() of the test tells it so,
 *               __wrap_fstat) is not loaded, a save is refused and leaves
 *               it as it was: a member of the group (the data dirs are
 *               02775) could plant it. Root's is loaded, unless the group
 *               or others can write it. Up to 7.25.21 it was loaded, with
 *               a warning. Run as root (__wrap_geteuid), the yuno's user is
 *               the owner of the chain closed from "/" down, so a closed
 *               data directory planted under the 02775 parent names nobody
 *               (directories told by __wrap_fstat and __wrap_stat). Up to
 *               7.25.22 it named its maker, whose file was loaded. A realm
 *               reached through a symlink of the closed chain is trusted
 *               (its first form stopped at the symlink: refused).
 *            8. an in-place save whose write stops half way (a short
 *               pwrite(), then ENOSPC, as on NFSv3 or a copy-on-write
 *               filesystem: __wrap_pwrite) writes the old content back: the
 *               file is byte for byte what it was, and the ERROR says so.
 *               When the write back fails too, the ERROR says the file is
 *               left unparsable. Up to 7.25.21 one pwrite() was made and a
 *               short one left the new start over the old tail.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <stddef.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <dirent.h>
#include <yunetas.h>

/*
 *  An fstat() that tells the inode marked here as another user's (the test
 *  runs as one user: a file of another one cannot be made)
 */
int __real_fstat(int fd, struct stat *st);
int __wrap_fstat(int fd, struct stat *st);
static ino_t foreign_ino = 0;
static uid_t foreign_uid = 0;

/*
 *  Directories told with another owner and mode (fake_dirs[], ino 0 ends
 *  it), to build the chain a yuno run as root walks
 */
typedef struct {
    ino_t ino;
    uid_t uid;
    mode_t mode;
} fake_dir_t;
static fake_dir_t fake_dirs[4] = {{0}};

PRIVATE void tell_faked(struct stat *st)
{
    if(foreign_ino && st->st_ino == foreign_ino) {
        st->st_uid = foreign_uid;
    }
    for(int i = 0; i < (int)(sizeof(fake_dirs)/sizeof(fake_dirs[0])); i++) {
        if(!fake_dirs[i].ino) {
            break;
        }
        if(st->st_ino == fake_dirs[i].ino && S_ISDIR(st->st_mode)) {
            st->st_uid = fake_dirs[i].uid;
            st->st_mode = (st->st_mode & S_IFMT) | fake_dirs[i].mode;
        }
    }
}

int __wrap_fstat(int fd, struct stat *st)
{
    int ret = __real_fstat(fd, st);
    if(ret == 0) {
        tell_faked(st);
    }
    return ret;
}

/*
 *  And stat(), told the same: the chain walked by a release that used it
 *  (the walk up from the file, up to 7.25.22) is judged on the same
 *  directories, so the planted case fails on it
 */
int __real_stat(const char *path, struct stat *st);
int __wrap_stat(const char *path, struct stat *st);

int __wrap_stat(const char *path, struct stat *st)
{
    int ret = __real_stat(path, st);
    if(ret == 0) {
        tell_faked(st);
    }
    return ret;
}

/*
 *  A geteuid() that answers root while armed: the yuno run as root
 */
uid_t __real_geteuid(void);
uid_t __wrap_geteuid(void);
static BOOL fake_root = FALSE;

uid_t __wrap_geteuid(void)
{
    if(fake_root) {
        return 0;
    }
    return __real_geteuid();
}

/*
 *  A pwrite() that stops half way: the first call writes 16 bytes, the next
 *  ones fail with ENOSPC -- the next one only (pwrite_fail = 1, the write
 *  back then works) or all of them (pwrite_fail = -1)
 */
ssize_t __real_pwrite(int fd, const void *bf, size_t count, off_t offset);
ssize_t __wrap_pwrite(int fd, const void *bf, size_t count, off_t offset);
static int pwrite_short = 0;    // armed: the next pwrite() is short
static int pwrite_fail = 0;

ssize_t __wrap_pwrite(int fd, const void *bf, size_t count, off_t offset)
{
    if(pwrite_short) {
        pwrite_short = 0;
        return __real_pwrite(fd, bf, count < 16? count : 16, offset);
    }
    if(pwrite_fail) {
        if(pwrite_fail > 0) {
            pwrite_fail--;
        }
        errno = ENOSPC;
        return -1;
    }
    return __real_pwrite(fd, bf, count, offset);
}

#define APP             "test_secret_attrs"
#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DOC         "SDF_SECRET masking and persistent-attrs file regression test"
#define APP_DATETIME    ""

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           0
#define MEM_MAX_BLOCK           0
#define MEM_SUPERBLOCK          0
#define MEM_MAX_SYSTEM_MEMORY   0

#define MASK                    "********"
#define COOKIE                  "theme=dark; access_token=eyJ.cookie-secret.sig"

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE int s_result = 0;   /* accumulated check result, read after entry_point */

PRIVATE BOOL s_capturing = FALSE;
PRIVATE int s_final_kw_seen = 0;
PRIVATE int s_final_hs_seen = 0;
PRIVATE int s_trace_secret_seen = 0;
PRIVATE int s_command_seen = 0;
PRIVATE int s_command_kw_seen = 0;
PRIVATE int s_expanded_kw_seen = 0;
PRIVATE int s_command_secret_seen = 0;
PRIVATE int s_wild_secret_seen = 0;
PRIVATE int s_wild_line_seen = 0;
PRIVATE int s_tail_line_seen = 0;
PRIVATE int s_watch_seen = 0;           // the watched secret, anywhere in a log
PRIVATE int s_watch_msg_seen = 0;       // the watched message
PRIVATE const char *s_watch = NULL;
PRIVATE const char *s_watch_msg = NULL;

GOBJ_DEFINE_GCLASS(C_TEST_SECRET_DRIVER);
GOBJ_DEFINE_EVENT(EV_TEST_SECRET_KW);
GOBJ_DEFINE_EVENT(EV_TEST_SECRET_PUB);
GOBJ_DEFINE_GCLASS(C_TEST_SECRET_HOLDER);

typedef struct {
    hgobj   timer;
} PRIVATE_DATA;

/***************************************************************
 *              Config (single quotes -> double at runtime)
 ***************************************************************/
PRIVATE char fixed_config[]= "\
{                                                                   \n\
    'yuno': {                                                       \n\
        'yuno_role': 'test_secret_attrs',                           \n\
        'tags': ['test', 'yunetas']                                 \n\
    }                                                               \n\
}                                                                   \n\
";
PRIVATE char variable_config[]= "\
{                                                                   \n\
    'environment': {                                                \n\
        'work_dir': '/tmp',                                         \n\
        'domain_dir': 'test_secret_attrs',                          \n\
        'console_log_handlers': {},                                 \n\
        'daemon_log_handlers': {}                                   \n\
    },                                                              \n\
    'global': {                                                     \n\
        'secret-other.password': 'glob-hunter2',                    \n\
        'secret-child.password': 'gname-hunter2',                   \n\
        'secret-vars.__json_config_variables__': {                  \n\
            'smtp_pw': 'var-hunter2',                               \n\
            'api_token': 'tok-hunter2',                             \n\
            'plain_var': 'visible-var'                              \n\
        },                                                          \n\
        'secret-traced.__json_config_variables__': {                \n\
            'traced_pw': 'trace-hunter2'                            \n\
        }                                                           \n\
    },                                                              \n\
    'yuno': {                                                       \n\
        'autoplay': true,                                           \n\
        'required_services': [],                                    \n\
        'public_services': [],                                      \n\
        'service_descriptor': {},                                   \n\
        'realm_owner': 'test',                                      \n\
        'realm_id':    'test_secret_attrs',                         \n\
        'trace_levels': {}                                          \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'secret-driver',                                \n\
            'gclass': 'C_TEST_SECRET_DRIVER',                       \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': true                                        \n\
        },                                                          \n\
        {                                                           \n\
            'name': 'secret-holder',                                \n\
            'gclass': 'C_TEST_SECRET_HOLDER',                       \n\
            'autostart': false,                                     \n\
            'kw': {                                                 \n\
                'password': 'cfg-hunter2',                          \n\
                'pin': 7777777777777,                               \n\
                'note': 'visible-note'                              \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': 'secret-vars',                                  \n\
            'gclass': 'C_TEST_SECRET_HOLDER',                       \n\
            'autostart': false,                                     \n\
            'kw': {                                                 \n\
                'password': '(^^smtp_pw^^)',                        \n\
                'note': '(^^plain_var^^)'                           \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': 'secret-other',                                 \n\
            'gclass': 'C_TEST_SECRET_HOLDER',                       \n\
            'autostart': false,                                     \n\
            '[^^children^^]': {                                     \n\
                '__range__': 1,                                     \n\
                '__vars__': {                                       \n\
                    'tpl_token': 'tplv-hunter2'                   \n\
                },                                                  \n\
                '__content__': {                                    \n\
                    'name': 'tpl-(^^__range__^^)',                  \n\
                    'gclass': 'C_TEST_SECRET_HOLDER',               \n\
                    'kw': {                                         \n\
                        'password': 'tpl-hunter2'                   \n\
                    }                                               \n\
                }                                                   \n\
            },                                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'secret-child',                         \n\
                    'gclass': 'C_TEST_SECRET_HOLDER',               \n\
                    'kw': {                                         \n\
                        'password': 'child-hunter2'                 \n\
                    }                                               \n\
                }                                                   \n\
            ]                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE void check_int(const char *name, int got, int expected)
{
    if(got != expected) {
        printf("FAIL %-52s got %d expected %d\n", name, got, expected);
        s_result += -1;
    } else {
        printf("ok   %-52s (%d)\n", name, got);
    }
}

PRIVATE void check_true(const char *name, BOOL got)
{
    check_int(name, got?1:0, 1);
}

PRIVATE void check_str(const char *name, const char *got, const char *expected)
{
    if(strcmp(got?got:"", expected) != 0) {
        printf("FAIL %-52s got '%s' expected '%s'\n", name, got?got:"", expected);
        s_result += -1;
    } else {
        printf("ok   %-52s ('%s')\n", name, got);
    }
}

/*
 *  A log handler that only looks: the create_delete2 trace goes through
 *  the log handlers, so what it prints can be searched here.
 */
PRIVATE int capture_logs(void *h, int priority, const char *bf, size_t len)
{
    if(!s_capturing) {
        return 0;
    }
    if(strstr(bf, "final kw")) {
        s_final_kw_seen++;
    }
    if(strstr(bf, "final hs")) {
        s_final_hs_seen++;
    }
    if(strstr(bf, "trace-hunter2")) {
        s_trace_secret_seen++;
    }
    if(strstr(bf, "set-password")) {
        s_command_seen++;
    }
    if(strstr(bf, "command kw")) {
        s_command_kw_seen++;
    }
    if(strstr(bf, "expanded_command")) {
        s_expanded_kw_seen++;
    }
    if(strstr(bf, "cmd-hunter2") || strstr(bf, "kw-hunter2") || strstr(bf, "pos-hunter2") ||
        strstr(bf, "6666666666666") || strstr(bf, "err-hunter2")) {
        s_command_secret_seen++;
    }
    if(strstr(bf, "wild-hunter2") || strstr(bf, "wildkw-hunter2") || strstr(bf, "nest-hunter2")) {
        s_wild_secret_seen++;
    }
    if(strstr(bf, "command-yuno") && strstr(bf, "service=visible-service")) {
        s_wild_line_seen++;
    }
    if(strstr(bf, "set-password-pos") && strstr(bf, "note=visible-tail")) {
        s_tail_line_seen++;
    }
    if(s_watch && strstr(bf, s_watch)) {
        s_watch_seen++;
    }
    if(s_watch_msg && strstr(bf, s_watch_msg)) {
        s_watch_msg_seen++;
    }
    return 0;
}

PRIVATE BOOL file_contains(const char *path, const char *text)
{
    char bf[1024] = {0};
    int fd = open(path, O_RDONLY|O_NOFOLLOW);
    if(fd < 0) {
        return FALSE;
    }
    ssize_t n = read(fd, bf, sizeof(bf)-1);
    close(fd);
    if(n <= 0) {
        return FALSE;
    }
    return strstr(bf, text)?TRUE:FALSE;
}

PRIVATE int read_file(const char *path, char *bf, size_t size)
{
    int fd = open(path, O_RDONLY|O_NOFOLLOW);
    if(fd < 0) {
        return -1;
    }
    ssize_t n = read(fd, bf, size - 1);
    close(fd);
    return (int)n;
}

PRIVATE int write_file(const char *path, const char *content, mode_t mode)
{
    unlink(path);
    int fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, mode);
    if(fd < 0) {
        printf("FAIL cannot create %s\n", path);
        s_result += -1;
        return -1;
    }
    if(write(fd, content, strlen(content)) < 0) {
        printf("FAIL cannot write %s\n", path);
        s_result += -1;
    }
    fchmod(fd, mode);   // not subject to the umask
    close(fd);
    return 0;
}

PRIVATE BOOL is_link(const char *path)
{
    struct stat st;
    if(lstat(path, &st) < 0) {
        return FALSE;
    }
    return S_ISLNK(st.st_mode)? TRUE : FALSE;
}

PRIVATE int file_links(const char *path)
{
    struct stat st;
    if(lstat(path, &st) < 0) {
        return -1;
    }
    return (int)st.st_nlink;
}

PRIVATE BOOL is_regular_not_link(const char *path)
{
    struct stat st;
    if(lstat(path, &st) < 0) {
        return FALSE;
    }
    return S_ISREG(st.st_mode)? TRUE : FALSE;
}

/*
 *  The "<file>.tmp-XXXXXX" a save writes before its rename()
 */
PRIVATE int count_temp_files(const char *path)
{
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", path);
    char *slash = strrchr(dir, '/');
    if(!slash) {
        return -1;
    }
    *slash = 0;
    const char *base = slash + 1;
    size_t base_len = strlen(base);
    int count = 0;
    DIR *d = opendir(dir);
    if(!d) {
        return -1;
    }
    struct dirent *de;
    while((de = readdir(d))) {
        if(strncmp(de->d_name, base, base_len)==0 && strncmp(de->d_name + base_len, ".tmp-", 5)==0) {
            count++;
        }
    }
    closedir(d);
    return count;
}

PRIVATE int test_file_size(const char *path)
{
    struct stat st;
    if(lstat(path, &st) < 0) {
        return -1;
    }
    return (int)st.st_size;
}

PRIVATE int last_byte(const char *path)
{
    int fd = open(path, O_RDONLY);
    if(fd < 0) {
        return -1;
    }
    char c = 0;
    off_t size = lseek(fd, 0, SEEK_END);
    ssize_t n = size > 0? pread(fd, &c, 1, size - 1) : 0;
    close(fd);
    return n == 1? (unsigned char)c : -1;
}

/*
 *  The syscall `nr` answers `err` from now on (a filesystem without
 *  fallocate: EOPNOTSUPP; a disk that fills: ENOSPC)
 */
PRIVATE void deny_syscall(int nr, int err)
{
    struct sock_filter f[] = {
        BPF_STMT(BPF_LD|BPF_W|BPF_ABS, offsetof(struct seccomp_data, nr)),
        BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, (unsigned)nr, 0, 1),
        BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ERRNO|((unsigned)err & SECCOMP_RET_DATA)),
        BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
    };
    struct sock_fprog prog = {.len = sizeof(f)/sizeof(f[0]), .filter = f};
    prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
    if(prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog) < 0) {
        printf("FAIL cannot install the seccomp filter\n");
    }
}

PRIVATE int file_mode(const char *path)
{
    struct stat st;
    if(lstat(path, &st) < 0) {
        return -1;
    }
    return (int)(st.st_mode & 07777);
}

/***************************************************************
 *              Checks
 ***************************************************************/
PRIVATE void check_http_cookie(hgobj gobj)
{
    hgobj ievent = gobj_create(
        "ievent",
        C_IEVENT_SRV,
        json_pack("{s:s}", "http_cookie", COOKIE),
        gobj
    );
    check_true("C_IEVENT_SRV created", ievent?TRUE:FALSE);
    if(!ievent) {
        return;
    }
    check_str("http_cookie is kept as it came",
        gobj_read_str_attr(ievent, "http_cookie"), COOKIE
    );

    json_t *jn_gobj = gobj2json(ievent, json_pack("[s]", "attrs"));
    check_str("view-gobj shows http_cookie masked",
        kw_get_str(gobj, jn_gobj, "attrs`http_cookie", "", 0), MASK
    );
    JSON_DECREF(jn_gobj)

    gobj_destroy(ievent);
}

PRIVATE void check_view_config(hgobj gobj)
{
    json_t *resp = gobj_command(gobj_yuno(), "view-config", json_object(), gobj);
    check_int("view-config answers result=0",
        (int)kw_get_int(gobj, resp, "result", -999, 0), 0
    );
    json_t *jn_data = kw_get_dict(gobj, resp, "data", 0, 0);
    char *s = json2uglystr(jn_data);
    check_true("view-config has a config", !empty_string(s));
    check_true("view-config hides a service's kw secret",
        s && !strstr(s, "cfg-hunter2")
    );
    check_true("view-config hides a child's kw secret",
        s && !strstr(s, "child-hunter2")
    );
    check_true("view-config hides a 'global' secret",
        s && !strstr(s, "glob-hunter2")
    );
    check_true("view-config still shows what is not secret",
        s && strstr(s, "visible-note")
    );
    check_true("view-config hides a secret that is a number",
        s && !strstr(s, "7777777777777")
    );
    check_true("view-config hides a 'global' secret named by a child's name",
        s && !strstr(s, "gname-hunter2")
    );
    check_true("view-config hides a config variable that feeds a secret",
        s && !strstr(s, "var-hunter2")
    );
    check_true("view-config hides a config variable with a secret's name",
        s && !strstr(s, "tok-hunter2")
    );
    check_true("view-config still shows a plain config variable",
        s && strstr(s, "visible-var")
    );
    check_true("view-config hides a secret of a [^^children^^] template",
        s && !strstr(s, "tpl-hunter2")
    );
    check_true("view-config hides a secret variable of a [^^children^^] template",
        s && !strstr(s, "tplv-hunter2")
    );
    GBMEM_FREE(s)
    JSON_DECREF(resp)

    check_str("the config secret still reaches the attr",
        gobj_read_str_attr(gobj_find_service("secret-holder", TRUE), "password"),
        "cfg-hunter2"
    );
    check_str("the config variable still reaches the attr",
        gobj_read_str_attr(gobj_find_service("secret-vars", TRUE), "password"),
        "var-hunter2"
    );

    hgobj holder = gobj_find_service("secret-holder", TRUE);
    json_t *jn_attrs = gobj_read_attrs(holder, SDF_PERSIST, gobj);
    gobj_mask_secret_attrs(holder, jn_attrs);
    check_str("a secret that is a number is shown masked",
        kw_get_str(gobj, jn_attrs, "pin", "", 0), MASK
    );
    JSON_DECREF(jn_attrs)

    resp = gobj_command(gobj_yuno(),
        "view-attrs gobj_name=secret-holder attribute=pin", json_object(), gobj
    );
    json_t *jn_pin = json_object_get(kw_get_dict(gobj, resp, "data", 0, 0), gobj_short_name(holder));
    check_str("view-attrs of one secret that is a number shows it masked",
        json_is_string(jn_pin)? json_string_value(jn_pin) : "(not a string)", MASK
    );
    JSON_DECREF(resp)
}

PRIVATE void check_create_delete2_trace(void)
{
    s_final_kw_seen = 0;
    s_final_hs_seen = 0;
    s_trace_secret_seen = 0;

    gobj_set_global_trace("create_delete2", TRUE);
    s_capturing = TRUE;
    hgobj traced = gobj_service_factory(
        "secret-traced",
        json_pack("{s:s, s:s, s:{s:s}}",
            "name", "secret-traced",
            "gclass", "C_TEST_SECRET_HOLDER",
            "kw",
                "password", "(^^traced_pw^^)"  // 'trace-hunter2', from the config variables
        )
    );
    s_capturing = FALSE;
    gobj_set_global_trace("create_delete2", FALSE);

    check_true("the traced service was created", traced?TRUE:FALSE);
    check_true("the create_delete2 trace printed a final kw", s_final_kw_seen > 0);
    check_true("the create_delete2 trace printed a final hs", s_final_hs_seen > 0);
    check_int("the create_delete2 trace hides the secret", s_trace_secret_seen, 0);
    if(traced) {
        check_str("the traced secret still reaches the attr",
            gobj_read_str_attr(traced, "password"), "trace-hunter2"
        );
        gobj_destroy(traced);
    }
}

PRIVATE void check_command_trace(void)
{
    hgobj holder = gobj_find_service("secret-holder", TRUE);
    s_command_seen = 0;
    s_command_kw_seen = 0;
    s_expanded_kw_seen = 0;
    s_command_secret_seen = 0;

    gobj_set_global_trace("commands", TRUE);
    gobj_set_global_trace("ev_kw", TRUE);
    s_capturing = TRUE;
    json_t *resp = gobj_command(holder,
        "set-password password=cmd-hunter2 note=visible", 0, holder
    );
    JSON_DECREF(resp)
    resp = gobj_command(holder,
        "set-password",
        json_pack("{s:s}", "password", "kw-hunter2"),
        holder
    );
    JSON_DECREF(resp)
    resp = gobj_command(holder, "set-password-pos abc==pos-hunter2==", 0, holder);
    JSON_DECREF(resp)
    resp = gobj_command(holder, "set-password-pos 'q=pos-hunter2' note=visible-tail", 0, holder);
    JSON_DECREF(resp)
    resp = gobj_command(holder,
        "set-password",
        json_pack("{s:I}", "password", (json_int_t)6666666666666),
        holder
    );
    JSON_DECREF(resp)
    resp = gobj_command(holder, "set-password password= err-hunter2", 0, holder);
    check_int("a malformed line is refused",
        (int)kw_get_int(0, resp, "result", 0, 0), -1
    );
    check_true("its answer does not echo the secret",
        !strstr(kw_get_str(0, resp, "comment", "", 0), "err-hunter2")
    );
    JSON_DECREF(resp)
    resp = gobj_command(holder, "set-password-pos pos-hunter2", 0, holder);
    JSON_DECREF(resp)
    s_capturing = FALSE;
    gobj_set_global_trace("ev_kw", FALSE);
    gobj_set_global_trace("commands", FALSE);

    check_true("the commands trace printed the command", s_command_seen > 0);
    check_true("the commands trace printed the command kw", s_command_kw_seen > 0);
    check_true("the parser printed the expanded kw", s_expanded_kw_seen > 0);
    check_int("the commands trace hides the secret parameter", s_command_secret_seen, 0);
    check_true("the line after a positional with a '=' is still shown", s_tail_line_seen > 0);
    check_str("the secret parameter still reaches the command",
        gobj_read_str_attr(holder, "password"), "pos-hunter2"
    );
}

PRIVATE void check_wild_command_trace(void)
{
    hgobj holder = gobj_find_service("secret-holder", TRUE);
    s_wild_secret_seen = 0;
    s_wild_line_seen = 0;

    gobj_set_global_trace("commands", TRUE);
    gobj_set_global_trace("ev_kw", TRUE);
    s_capturing = TRUE;
    json_t *resp = gobj_command(holder,
        "command-yuno id=x service=visible-service command=set-user-pwd password=wild-hunter2",
        0, holder
    );
    JSON_DECREF(resp)
    resp = gobj_command(holder,
        "command-yuno id=x service=visible-service command=set-user-pwd",
        json_pack("{s:s}", "password", "wildkw-hunter2"),
        holder
    );
    JSON_DECREF(resp)
    resp = gobj_command(holder,
        "command-yuno id=x service=visible-service command=\"set-user-pwd password=nest-hunter2\"",
        0, holder
    );
    JSON_DECREF(resp)
    s_capturing = FALSE;
    gobj_set_global_trace("ev_kw", FALSE);
    gobj_set_global_trace("commands", FALSE);

    check_true("the commands trace printed the wild command", s_wild_line_seen > 0);
    check_int("the commands trace hides a free key with a secret's name", s_wild_secret_seen, 0);
    check_str("the free key still reaches the command",
        gobj_read_str_attr(holder, "note"), "set-user-pwd password=nest-hunter2"
    );
}

/*
 *  Watch one secret, and one message that must be printed, around `what`
 */
PRIVATE void watch(const char *secret, const char *msg)
{
    s_watch = secret;
    s_watch_msg = msg;
    s_watch_seen = 0;
    s_watch_msg_seen = 0;
    s_capturing = TRUE;
}

PRIVATE void unwatch(const char *what)
{
    char name[128];
    s_capturing = FALSE;
    snprintf(name, sizeof(name), "%s: printed", what);
    check_true(name, s_watch_msg_seen > 0);
    snprintf(name, sizeof(name), "%s: the secret hidden", what);
    check_int(name, s_watch_seen, 0);
    s_watch = NULL;
    s_watch_msg = NULL;
}

/*
 *  The other dumps of a kw: the kw_get_* errors, the machine trace with
 *  ev_kw, the ievents traces, and a line the parser refuses
 */
PRIVATE void check_log_dumps(hgobj gobj)
{
    hgobj holder = gobj_find_service("secret-holder", TRUE);
    json_t *resp;

    json_t *kw = json_pack("{s:I, s:s}", "password", (json_int_t)5555555555555, "note", "x");
    watch("5555555555555", "path MUST BE a json str");
    kw_get_str(gobj, kw, "password", "", 0);
    unwatch("a kw_get_str() error dump");
    JSON_DECREF(kw)

    gobj_set_global_trace("machine", TRUE);
    gobj_set_global_trace("ev_kw", TRUE);
    watch("evkw-hunter2", "kw exec event");
    gobj_send_event(gobj, EV_TEST_SECRET_KW,
        json_pack("{s:s, s:s}", "access_token", "evkw-hunter2", "x", "y"), gobj
    );
    gobj_set_global_trace("ev_kw", FALSE);
    gobj_set_global_trace("machine", FALSE);
    unwatch("the machine trace with ev_kw");

    /*
     *  A subscription whose __filter__ / __global__ carry a credential: the
     *  traces of its subscribe, of the filter at a publish, and of its
     *  unsubscribe mask it. Up to 7.25.21 the three printed it as it was.
     */
    gobj_set_global_trace("machine", TRUE);
    gobj_set_global_trace("ev_kw", TRUE);
    gobj_set_global_trace("subscriptions", TRUE);
    json_t *kw_subs = json_pack("{s:{s:s}, s:{s:s}}",
        "__filter__", "api_token", "subfilt-hunter2",
        "__global__", "password", "subglob-hunter2"
    );
    watch("-hunter2\"", "subscribing event");
    gobj_subscribe_event(holder, EV_TEST_SECRET_PUB, json_incref(kw_subs), gobj);
    unwatch("the trace of a subscription with credentials");
    watch("subfilt-hunter2", "publishing with filter");
    gobj_publish_event(holder, EV_TEST_SECRET_PUB, json_pack("{s:s}", "api_token", "other"));
    unwatch("the filter trace of a publish");
    watch("-hunter2\"", "unsubscribing event");
    gobj_unsubscribe_event(holder, EV_TEST_SECRET_PUB, kw_subs, gobj);
    unwatch("the trace of an unsubscription with credentials");
    gobj_set_global_trace("subscriptions", FALSE);
    gobj_set_global_trace("ev_kw", FALSE);
    gobj_set_global_trace("machine", FALSE);

    kw = json_pack("{s:s, s:s, s:{s:[{s:s}]}}",
        "__command__", "set-password-pos iev-hunter2",
        "note", "visible",
        "__md_iev__", "ievent_gate_stack", "dst_service", "secret-holder"
    );
    watch("iev-hunter2", "ievents2-v6");
    trace_inter_event2(gobj, "ievents2-v6", "EV_MT_COMMAND", kw);
    unwatch("the ievents2 trace of a command to a local service");
    JSON_DECREF(kw)

    kw = json_pack("{s:s, s:{s:[{s:s}], s:[{s:s}]}}",
        "password", "iev7kw-hunter2",
        "__md_iev__",
            "ievent_gate_stack", "dst_service", "remote-service",
            "__command__", "command", "set-user-pwd username=bob password=iev7-hunter2"
    );
    watch("hunter2", "ievents2-v7");
    trace_inter_event2(gobj, "ievents2-v7", "EV_MT_COMMAND", kw);
    unwatch("the ievents2 trace of a command to a remote service");
    JSON_DECREF(kw)

    /*
     *  The ievents trace shows only result and __md_iev__, where a v7
     *  command lives: a positional secret in it is masked by the command
     *  table. Up to 7.25.21 it was masked by names only, and went in clear.
     */
    kw = json_pack("{s:{s:[{s:s}], s:[{s:s}]}}",
        "__md_iev__",
            "ievent_gate_stack", "dst_service", "secret-holder",
            "__command__", "command", "set-password-pos ievpos-hunter2"
    );
    watch("ievpos-hunter2", "ievents-v7");
    trace_inter_event(gobj, "ievents-v7", "EV_MT_COMMAND", kw);
    unwatch("the ievents trace of a positional secret");
    JSON_DECREF(kw)

    gobj_set_global_trace("commands", TRUE);
    watch("wa-hunter2", "write-attr");
    resp = gobj_command(gobj_yuno(),
        "write-attr gobj_name=secret-holder attribute=password value=wa-hunter2",
        json_object(), gobj
    );
    gobj_set_global_trace("commands", FALSE);
    unwatch("the commands trace of a write-attr of a secret");
    JSON_DECREF(resp)
    check_str("the write-attr of a secret still writes it",
        gobj_read_str_attr(holder, "password"), "wa-hunter2"
    );

    /*
     *  The names: what names something ABOUT a credential is not one
     */
    check_true("access_token is a secret's name", is_secret_name("access_token", 12));
    check_true("token_endpoint is not", !is_secret_name("token_endpoint", 14));
    check_true("token_endpoint is, for is_secret_name_any()", is_secret_name_any("token_endpoint", 14));
    check_true("api_key_max is, for is_secret_name_any()", is_secret_name_any("api_key_max", 11));
    check_true("hostname is not, for is_secret_name_any()", !is_secret_name_any("hostname", 8));
    check_true("public_url is not, for is_secret_name_any()", !is_secret_name_any("public_url", 10));
    check_true("cookie_domain is not", !is_secret_name("cookie_domain", 13));
    check_true("jwt_public_keys is not", !is_secret_name("jwt_public_keys", 15));
    check_true("refresh_token_count is not", !is_secret_name("refresh_token_count", 19));

    /*
     *  The quote that closes a command carried in a value survives
     */
    char *line = command_mask_secret_line(holder,
        "command-yuno id=x command=\"set-user-pwd password=hunter2\""
    );
    check_str("a masked command in a value keeps its closing quote", line,
        "command-yuno id=x command='set-user-pwd password=********'"
    );
    GBMEM_FREE(line)

    /*
     *  An extra word that is not a secret is shown
     */
    resp = gobj_command(holder, "set-password note=x stray-word", 0, holder);
    check_true("an extra plain word is shown in the refusal",
        strstr(kw_get_str(0, resp, "comment", "", 0), "stray-word")? TRUE : FALSE
    );
    JSON_DECREF(resp)

    /*
     *  The rest of a secret written with blanks, and the word after a
     *  "password= " read as part of the next key: never shown
     */
    gobj_set_global_trace("commands", TRUE);
    watch("horse", "set-password");
    resp = gobj_command(holder, "set-password password=correct horse battery", 0, holder);
    check_int("a secret with blanks refuses the command", (int)kw_get_int(0, resp, "result", 0, 0), -1);
    check_true("its refusal shows no word of it",
        !strstr(kw_get_str(0, resp, "comment", "", 0), "horse")
    );
    JSON_DECREF(resp)
    unwatch("the commands trace of a secret with blanks");
    /*
     *  The same, positional: the required secret takes "correct" and the
     *  rest has no '='. Up to 7.25.21 the refusal showed 'horse battery'.
     */
    watch("horse", "set-password-pos");
    resp = gobj_command(holder, "set-password-pos correct horse battery", 0, holder);
    check_int("a positional secret with blanks refuses the command", (int)kw_get_int(0, resp, "result", 0, 0), -1);
    check_true("its refusal shows no word of it",
        !strstr(kw_get_str(0, resp, "comment", "", 0), "horse")
    );
    JSON_DECREF(resp)
    unwatch("the commands trace of a positional secret with blanks");
    /*
     *  A secret json parameter that does not parse: refused, and its text
     *  is in no log. Up to 7.25.21 the verbose parser logged it in clear.
     */
    watch("jsonbad-hunter2", "set-password");
    resp = gobj_command(holder, "set-password credentials=[jsonbad-hunter2", 0, holder);
    check_int("a secret json that does not parse refuses the command", (int)kw_get_int(0, resp, "result", 0, 0), -1);
    check_true("its refusal does not show it",
        !strstr(kw_get_str(0, resp, "comment", "", 0), "jsonbad-hunter2")
    );
    JSON_DECREF(resp)
    unwatch("the log of a secret json that does not parse");
    watch("blank-hunter2", "set-password");
    resp = gobj_command(holder, "set-password password= blank-hunter2 note=1", 0, holder);
    check_int("a key with a blank refuses the command", (int)kw_get_int(0, resp, "result", 0, 0), -1);
    check_true("its refusal does not echo the key",
        !strstr(kw_get_str(0, resp, "comment", "", 0), "blank-hunter2")
    );
    JSON_DECREF(resp)
    unwatch("the commands trace of a key with a blank");
    gobj_set_global_trace("commands", FALSE);

    /*
     *  A quote inside an unquoted value is part of it
     */
    struct {
        const char *in;
        const char *out;
    } inline_cases[] = {
        {"write-attr attribute=password value=ab'cd", "write-attr attribute=password value=********"},
        {"command=\"set-user password=p'q\"", "command=\"set-user password=********\""},
        {"token=abc\"def\"ghi x=1", "token=******** x=1"},
        {"command='set-user-pwd password=hunter2'", "command='set-user-pwd password=********'"},
        {"command='set-email-user password=correct horse battery'", "command='set-email-user password=********'"},
        {"command=\"set-user-pwd password= hunter2\"", "command=\"set-user-pwd password=********\""},
        {"set-password password= note=x", "set-password password= note=x"},
        {"x password=a b c user=bob", "x password=******** user=bob"},
        {"'password'=x", "'password'=********"},
        {"password'=x", "password'=********"},
        {"\"password\"=x", "\"password\"=********"},
        {"user[password]=x", "user[password]=********"},
        {"kw[\"password\"]=x", "kw[\"password\"]=********"},
        {"password[0]=x", "password[0]=********"},
        {"attribute=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx_password value=v",
         "attribute=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx_password value=********"},
        {0, 0}
    };
    for(int i=0; inline_cases[i].in; i++) {
        char *m = mask_secrets_inline(inline_cases[i].in);   // NULL: nothing to mask
        check_str("a value is masked whole, quotes in it included",
            m? m : inline_cases[i].in, inline_cases[i].out
        );
        GBMEM_FREE(m)
    }
    check_true("authorization_header is a secret's name", is_secret_name("authorization_header", 20));

    /*
     *  Masking is linear and capped: a 1 MB "a=a=a=..." word, a 1 MB
     *  secret value and a 1 MB received text take milliseconds (it was
     *  quadratic: seconds for 60 KB); 5 MB is not shown at all
     */
    {
        size_t big_len = 1024*1024;
        char *big = gbmem_malloc(big_len + 16);
        for(size_t i=0; i<big_len; i++) {
            big[i] = (i & 1)? '=' : 'a';
        }
        big[big_len] = 0;

        uint64_t t0 = time_in_milliseconds_monotonic();
        char *m = mask_secrets_inline(big);
        check_true("a 1 MB a=a=... word has nothing to mask", m == NULL);
        GBMEM_FREE(m)
        json_t *jn_big = json_pack("{s:s}", "anything", big);
        json_t *jn_shown = json_mask_secrets(jn_big);
        JSON_DECREF(jn_shown)
        JSON_DECREF(jn_big)
        mask_secrets_in_text(big, big_len);
        uint64_t t1 = time_in_milliseconds_monotonic();
        check_true("a 1 MB a=a=... word is masked in under 500 ms (quadratic: minutes)", t1 - t0 < 500);

        memcpy(big, "password=", 9);
        memset(big + 9, 'x', big_len - 9);
        t0 = time_in_milliseconds_monotonic();
        m = mask_secrets_inline(big);
        check_str("a 1 MB secret value is masked whole", m, "password=********");
        GBMEM_FREE(m)
        t1 = time_in_milliseconds_monotonic();
        check_true("a 1 MB secret value is masked in under 500 ms", t1 - t0 < 500);
        GBMEM_FREE(big)

        size_t huge_len = 5*1024*1024;
        char *huge = gbmem_malloc(huge_len + 1);
        memset(huge, 'x', huge_len);
        memcpy(huge, "a=b ", 4);
        huge[huge_len] = 0;
        m = mask_secrets_inline(huge);
        check_str("a text over 4 MB is not shown", m, "<not shown: too large to mask>");
        GBMEM_FREE(m)
        GBMEM_FREE(huge)

        /*
         *  The cap counts NODES too, and the walk stops at it: a frame of
         *  "[{},{},...]" or "[[],...]" (no bytes of keys or strings) is not
         *  walked whole, and is one placeholder past the cap
         */
        for(int kind=0; kind<2; kind++) {
            json_t *jn_many = json_array();
            for(int i=0; i<200000; i++) {
                json_array_append_new(jn_many, kind? json_array() : json_object());
            }
            json_t *jn_kw2 = json_pack("{s:o}", "list", jn_many);
            t0 = time_in_milliseconds_monotonic();
            jn_shown = json_mask_secrets_capped(jn_kw2, 1024);
            t1 = time_in_milliseconds_monotonic();
            json_t *jn_list = json_object_get(jn_shown, "list");
            check_true(kind? "[[],...] past the cap: the walk stops" : "[{},...] past the cap: the walk stops",
                json_is_array(jn_list) && json_array_size(jn_list) <= 1025
            );
            check_true("past the cap, one placeholder for the rest",
                json_is_array(jn_list) &&
                strcmp(json_string_value(json_array_get(jn_list, json_array_size(jn_list)-1))?
                    json_string_value(json_array_get(jn_list, json_array_size(jn_list)-1)) : "",
                    "<not shown: too large to mask>")==0
            );
            check_true("a capped walk takes milliseconds", t1 - t0 < 50);
            JSON_DECREF(jn_shown)
            JSON_DECREF(jn_kw2)
        }

        /*
         *  A name longer than the bound is judged by its LAST bytes
         */
        char long_key[160];
        memset(long_key, 'x', 130);
        strcpy(long_key + 130, "_password");
        json_t *jn_long = json_pack("{s:s}", long_key, "long-hunter2");
        jn_shown = json_mask_secrets(jn_long);
        check_str("a key of 139 bytes ending in _password is masked",
            json_string_value(json_object_get(jn_shown, long_key)), "********"
        );
        JSON_DECREF(jn_shown)
        JSON_DECREF(jn_long)
        char long_text[256];
        snprintf(long_text, sizeof(long_text), "{\"%s\": \"text-hunter2\"}", long_key);
        mask_secrets_in_text(long_text, strlen(long_text));
        check_true("a json key of 139 bytes in a dump is masked", !strstr(long_text, "text-hunter2"));

        json_t *jn_kw = json_pack("{s:s, s:s}", "a", "small", "b", "small");
        jn_shown = json_mask_secrets_capped(jn_kw, 6);
        char *ss2 = json2uglystr(jn_shown);
        check_true("past the cap a string is not shown",
            ss2 && strstr(ss2, "too large to mask")? TRUE : FALSE
        );
        GBMEM_FREE(ss2)
        JSON_DECREF(jn_shown)
        JSON_DECREF(jn_kw)
    }

    /*
     *  A dict shared (the C twin of a bug gobj-js had: a dict met twice was
     *  masked once), a cycle, and a fan-out that would be exponential
     *  without the memo (the test's ctest TIMEOUT makes that fail fast)
     */
    json_t *creds = json_pack("{s:s}", "password", "shared-hunter2");
    json_t *kw2 = json_pack("{s:O, s:O, s:[O,O]}", "first", creds, "second", creds, "list", creds, creds);
    json_t *shown = json_mask_secrets(kw2);
    char *ss = json2uglystr(shown);
    check_true("a dict met twice is masked both times", ss && !strstr(ss, "shared-hunter2"));
    GBMEM_FREE(ss)
    JSON_DECREF(shown)
    JSON_DECREF(kw2)
    JSON_DECREF(creds)

    json_t *a = json_pack("{s:s}", "token", "cyc-hunter2");
    json_t *b = json_object();
    json_object_set(a, "b", b);
    json_object_set(b, "a", a);     // a cycle, which jansson lets build
    shown = json_mask_secrets(a);
    check_str("a cycle is shown as <cycle>",
        json_string_value(json_object_get(json_object_get(shown, "b"), "a")), "<cycle>"
    );
    JSON_DECREF(shown)
    json_object_del(b, "a");        // break it, or it never frees
    JSON_DECREF(b)
    JSON_DECREF(a)

    json_t *top = json_object();
    json_t *level = top;
    for(int i=0; i<40; i++) {
        json_t *next = json_object();
        json_object_set(level, "l", next);
        json_object_set(level, "r", next);
        json_decref(next);
        level = next;
    }
    json_object_set_new(level, "password", json_string("deep"));
    shown = json_mask_secrets(top);     // 2^40 walks without the memo
    check_true("a shared fan-out is masked in linear time", shown? TRUE : FALSE);
    JSON_DECREF(shown)
    JSON_DECREF(top)

    /*
     *  A gbuffer in a kw, with the gbuffers trace: dumped, masked
     */
    gbuffer_t *gbuf = gbuffer_create(256, 256);
    gbuffer_append_string(gbuf, "{\"password\": \"gb-hunter2\", \"note\": \"gbvisible\"}");
    gobj_set_global_trace("machine", TRUE);
    gobj_set_global_trace("ev_kw", TRUE);
    gobj_set_global_trace("gbuffers", TRUE);
    watch("gb-hunter2", "gbvisible");
    gobj_send_event(gobj, EV_TEST_SECRET_KW,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),     // the kw takes it
        gobj
    );
    gobj_set_global_trace("gbuffers", FALSE);
    gobj_set_global_trace("ev_kw", FALSE);
    gobj_set_global_trace("machine", FALSE);
    unwatch("the gbuffer of an event with the gbuffers trace");

    gobj_write_str_attr(holder, "password", "before-unterminated");
    resp = gobj_command(holder, "set-password note=a password='unterm-hunter2", 0, holder);
    check_int("a value with no closing quote refuses the command",
        (int)kw_get_int(0, resp, "result", 0, 0), -1
    );
    const char *comment = kw_get_str(0, resp, "comment", "", 0);
    check_true("the refusal says why", strstr(comment, "no closing quote")?TRUE:FALSE);
    check_true("the refusal does not echo the secret", !strstr(comment, "unterm-hunter2"));
    JSON_DECREF(resp)
    check_str("the command did not run",
        gobj_read_str_attr(holder, "password"), "before-unterminated"
    );
}

PRIVATE void check_persistent_file(void)
{
    hgobj holder = gobj_find_service("secret-holder", TRUE);
    char path[PATH_MAX];
    char planted[PATH_MAX];
    yuneta_realm_file(path, sizeof(path), "data",
        "C_TEST_SECRET_HOLDER-secret-holder-persistent-attrs.json", TRUE
    );
    yuneta_realm_file(planted, sizeof(planted), "data", "planted.json", TRUE);

    /*
     *  A file left 0664 by a release before 7.25.19
     */
    write_file(path, "{\"password\": \"disk-secret\"}", 0664);
    check_int("the old file is 0664", file_mode(path), 0664);
    gobj_load_persistent_attrs(holder, 0);
    check_str("the old file is loaded",
        gobj_read_str_attr(holder, "password"), "disk-secret"
    );
    check_int("loading the old file makes it 0600", file_mode(path), 0600);

    /*
     *  A hard link: the load replaces it, and the other name is untouched
     */
    unlink(path);
    write_file(planted, "{\"password\": \"linked-secret\"}", 0644);
    if(link(planted, path) < 0) {
        printf("FAIL cannot create the hard link %s\n", path);
        s_result += -1;
    }
    gobj_load_persistent_attrs(holder, 0);
    check_str("a hard-linked file is loaded",
        gobj_read_str_attr(holder, "password"), "linked-secret"
    );
    check_int("the other name keeps its mode", file_mode(planted), 0644);
    check_int("the file is replaced by a 0600 one", file_mode(path), 0600);
    check_int("the file is a name of its own", file_links(path), 1);

    /*
     *  What a save that died left: removed when the file is loaded, a
     *  symlink of that name is not followed and is left
     */
    char stale[PATH_MAX+16];
    char stale_link[PATH_MAX+16];
    char backup[PATH_MAX+16];
    snprintf(stale, sizeof(stale), "%s.tmp-Ab3xYz", path);
    snprintf(stale_link, sizeof(stale_link), "%s.tmp-Lk3xYz", path);
    snprintf(backup, sizeof(backup), "%s.backup", path);
    write_file(stale, "{\"password\": \"stale\"}", 0600);
    write_file(backup, "{\"password\": \"a backup\"}", 0600);
    unlink(stale_link);
    if(symlink(planted, stale_link) < 0) {
        printf("FAIL cannot create the symlink %s\n", stale_link);
        s_result += -1;
    }
    gobj_load_persistent_attrs(holder, 0);
    check_true("a stale temporary file is removed", access(stale, F_OK) != 0);
    check_true("a symlink of that name is left", is_link(stale_link));
    check_true("its target is untouched", file_contains(planted, "linked-secret"));
    check_true("a backup of the file (.backup) is left", access(backup, F_OK) == 0);
    unlink(backup);
    unlink(stale_link);
    unlink(planted);

    /*
     *  A symlink planted in place of the file
     */
    unlink(path);
    write_file(planted, "{\"password\": \"planted\"}", 0644);
    if(symlink(planted, path) < 0) {
        printf("FAIL cannot create the symlink %s\n", path);
        s_result += -1;
    }
    gobj_write_str_attr(holder, "password", "before");
    gobj_load_persistent_attrs(holder, 0);
    check_str("a symlinked file is not loaded",
        gobj_read_str_attr(holder, "password"), "before"
    );
    gobj_write_str_attr(holder, "password", "through-the-link");
    int ret = gobj_save_persistent_attrs(holder, json_string("password"));
    check_int("a save replaces a planted symlink", ret, 0);
    check_true("the saved file is a regular file, not the link", is_regular_not_link(path));
    check_int("the saved file is 0600", file_mode(path), 0600);
    check_true("the saved file has the value", file_contains(path, "through-the-link"));
    check_true("the symlink target is not written",
        !file_contains(planted, "through-the-link")
    );
    check_int("the symlink target keeps its mode", file_mode(planted), 0644);

    /*
     *  A hard link in place of the file (as a file of another user, it is
     *  an inode that is not the yuno's to write): the save makes a new one
     */
    unlink(path);
    write_file(planted, "{\"password\": \"other-inode\"}", 0644);
    if(link(planted, path) < 0) {
        printf("FAIL cannot create the hard link %s\n", path);
        s_result += -1;
    }
    gobj_write_str_attr(holder, "password", "new-inode");
    ret = gobj_save_persistent_attrs(holder, json_string("password"));
    check_int("a save over a hard link succeeds", ret, 0);
    check_true("the other inode is not written", file_contains(planted, "other-inode"));
    check_true("the other inode is not written (2)", !file_contains(planted, "new-inode"));
    check_int("the saved file is a new 0600 inode", file_mode(path), 0600);
    check_int("no temporary file is left", count_temp_files(path), 0);

    /*
     *  A directory the yuno cannot write (not checked as root, who writes
     *  anything): the file of its own is written in place; with no file of
     *  its own the save fails, write-attr says so, and nothing is left
     */
    json_t *resp;
    if(geteuid() != 0) {
        gobj_write_str_attr(holder, "password", "kept-on-disk");
        gobj_write_str_attr(holder, "note",
            "a-long-note-to-make-the-old-file-longer-than-the-new-one-"
            "a-long-note-to-make-the-old-file-longer-than-the-new-one"
        );
        gobj_save_persistent_attrs(holder, json_pack("[s,s]", "password", "note"));
        int old_size = test_file_size(path);
        char dir[PATH_MAX];
        snprintf(dir, sizeof(dir), "%s", path);
        *strrchr(dir, '/') = 0;
        struct stat st_dir;
        stat(dir, &st_dir);
        chmod(path, 0640);
        chmod(dir, 0555);
        resp = gobj_command(gobj_yuno(),
            "write-attr gobj_name=secret-holder attribute=note value=in-place", json_object(), holder
        );
        chmod(dir, st_dir.st_mode & 07777);
        check_int("a save in a directory that cannot be written goes in place",
            (int)kw_get_int(0, resp, "result", -1, 0), 0
        );
        JSON_DECREF(resp)
        check_true("in place: on disk", file_contains(path, "in-place"));
        json_error_t jerr;
        json_t *jn_disk = json_load_file(path, 0, &jerr);
        check_true("in place: the file parses", jn_disk? TRUE : FALSE);
        JSON_DECREF(jn_disk)
        check_true("in place: the rest kept", file_contains(path, "kept-on-disk"));
        check_int("in place: made 0600", file_mode(path), 0600);
        check_true("in place: a shorter content is cut to its size",
            test_file_size(path) > 0 && test_file_size(path) < old_size && last_byte(path) == '}'
        );

        /*
         *  A filesystem with no fallocate (seccomp answers EOPNOTSUPP, as
         *  NFSv3 or FUSE): the in-place save goes on without the
         *  reservation. In a child: the filter cannot be removed.
         */
        chmod(dir, 0555);
        fflush(stdout);     // the child would print the buffer again
        pid_t pid = fork();
        if(pid == 0) {
            deny_syscall(__NR_fallocate, EOPNOTSUPP);
            gobj_write_str_attr(holder, "note",     // longer than the file: it grows
                "no-fallocate-and-a-longer-note-than-the-file-has-now-"
                "no-fallocate-and-a-longer-note-than-the-file-has-now-"
                "no-fallocate-and-a-longer-note-than-the-file-has-now"
            );
            int r = gobj_save_persistent_attrs(holder, json_string("note"));
            _exit(r == 0? 0 : 1);
        }
        int wstatus = 0;
        waitpid(pid, &wstatus, 0);
        chmod(dir, st_dir.st_mode & 07777);
        check_true("in place with no fallocate: saved",
            WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0
        );
        check_true("in place with no fallocate: on disk", file_contains(path, "no-fallocate"));

        /*
         *  The write fails (ENOSPC) for a content longer than the file: the
         *  room taken did not grow the file (no NULs after the old json),
         *  so the old file is left as it was, and parses
         */
        int size_before = test_file_size(path);
        chmod(dir, 0555);
        fflush(stdout);
        pid = fork();
        if(pid == 0) {
            deny_syscall(__NR_pwrite64, ENOSPC);
            gobj_write_str_attr(holder, "note",
                "a-note-that-does-not-fit-a-note-that-does-not-fit-a-note-that-does-not-fit-"
                "a-note-that-does-not-fit-a-note-that-does-not-fit-a-note-that-does-not-fit-"
                "a-note-that-does-not-fit-a-note-that-does-not-fit-a-note-that-does-not-fit"
            );
            int r = gobj_save_persistent_attrs(holder, json_string("note"));
            _exit(r == 0? 0 : 1);
        }
        waitpid(pid, &wstatus, 0);
        chmod(dir, st_dir.st_mode & 07777);
        check_true("a write that fails: the save fails",
            WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 1
        );
        check_int("a write that fails: the file keeps its size", test_file_size(path), size_before);
        jn_disk = json_load_file(path, 0, &jerr);
        check_true("a write that fails: the file still parses", jn_disk? TRUE : FALSE);
        JSON_DECREF(jn_disk)

        /*
         *  The write stops half way (16 bytes, then ENOSPC): the old content
         *  is written back, byte for byte
         */
        char before[4096];
        int before_len = read_file(path, before, sizeof(before));
        chmod(dir, 0555);
        gobj_write_str_attr(holder, "note",
            "a-note-written-short-a-note-written-short-a-note-written-short-"
            "a-note-written-short-a-note-written-short-a-note-written-short"
        );
        s_watch_msg = "written back as it was";
        s_watch_msg_seen = 0;
        s_capturing = TRUE;
        pwrite_short = 1;
        pwrite_fail = 1;
        int r_short = gobj_save_persistent_attrs(holder, json_string("note"));
        pwrite_short = 0;
        pwrite_fail = 0;
        s_capturing = FALSE;
        chmod(dir, st_dir.st_mode & 07777);
        char after[4096];
        int after_len = read_file(path, after, sizeof(after));
        check_true("a write stopped half way: the save fails", r_short < 0);
        check_true("a write stopped half way: the old content is written back",
            before_len > 0 && after_len == before_len && memcmp(before, after, (size_t)before_len) == 0
        );
        check_true("a write stopped half way: the ERROR says it was written back", s_watch_msg_seen > 0);

        /*
         *  And the write back fails too: the ERROR says the file is left
         *  unparsable (put back by the test)
         */
        chmod(dir, 0555);
        s_watch_msg = "UNPARSABLE";
        s_watch_msg_seen = 0;
        s_capturing = TRUE;
        pwrite_short = 1;
        pwrite_fail = -1;
        r_short = gobj_save_persistent_attrs(holder, json_string("note"));
        pwrite_short = 0;
        pwrite_fail = 0;
        s_capturing = FALSE;
        s_watch_msg = NULL;
        chmod(dir, st_dir.st_mode & 07777);
        check_true("a write back that fails: the save fails", r_short < 0);
        check_true("a write back that fails: the ERROR says the file is unparsable", s_watch_msg_seen > 0);
        before[before_len] = 0;
        write_file(path, before, 0600);

        unlink(path);
        chmod(dir, 0555);
        resp = gobj_command(gobj_yuno(),
            "write-attr gobj_name=secret-holder attribute=note value=unsaved", json_object(), holder
        );
        chmod(dir, st_dir.st_mode & 07777);
        check_int("write-attr answers a save that failed", (int)kw_get_int(0, resp, "result", 0, 0), -1);
        JSON_DECREF(resp)
        check_str("the attr is written anyway", gobj_read_str_attr(holder, "note"), "unsaved");
        check_int("a failed save leaves no temporary file", count_temp_files(path), 0);

        /*
         *  A file there that cannot be read: the save is refused, the file
         *  kept (its other attrs would be lost)
         */
        gobj_write_str_attr(holder, "password", "kept-unreadable");
        gobj_save_persistent_attrs(holder, json_string("password"));
        chmod(path, 0000);
        gobj_write_str_attr(holder, "note", "must-not-replace");
        int ret2 = gobj_save_persistent_attrs(holder, json_string("note"));
        chmod(path, 0600);
        check_true("a save over a file that cannot be read is refused", ret2 < 0);
        check_true("the file is kept as it was", file_contains(path, "kept-unreadable"));
        check_true("the file is not replaced", !file_contains(path, "must-not-replace"));
    }

    /*
     *  An empty file holds no attrs, and does not refuse a save
     */
    write_file(path, "", 0600);
    gobj_write_str_attr(holder, "note", "over-empty");
    check_int("a save over an empty file is done",
        gobj_save_persistent_attrs(holder, json_string("note")), 0
    );
    check_true("over an empty file: on disk", file_contains(path, "over-empty"));

    /*
     *  write-attr of a persistent attr of a gobj that is no service: it
     *  says that it is not persisted
     */
    resp = gobj_command(gobj_yuno(),
        "write-attr gobj_name=secret-other`secret-child attribute=note value=child-note",
        json_object(), holder
    );
    check_int("write-attr of a child answers", (int)kw_get_int(0, resp, "result", -1, 0), 0);
    check_true("and says it is not persisted",
        strstr(kw_get_str(0, resp, "comment", "", 0), "NOT persisted")? TRUE : FALSE
    );
    JSON_DECREF(resp)

    resp = gobj_command(gobj_yuno(),
        "write-attr gobj_name=secret-holder attribute=note value=saved", json_object(), holder
    );
    check_int("write-attr answers a save that worked", (int)kw_get_int(0, resp, "result", -1, 0), 0);
    JSON_DECREF(resp)
    check_true("and it is on disk", file_contains(path, "saved"));

    /*
     *  SDF_PERSIST without SDF_WR is not writable at run time: up to
     *  7.25.22 write-attr took SDF_PERSIST alone as writable
     */
    resp = gobj_command(gobj_yuno(),
        "write-attr gobj_name=secret-holder attribute=config_only value=planted", json_object(), holder
    );
    check_int("write-attr of an attr SDF_PERSIST without SDF_WR is refused",
        (int)kw_get_int(0, resp, "result", 0, 0), -1
    );
    JSON_DECREF(resp)
    check_str("and it is not written", gobj_read_str_attr(holder, "config_only"), "");

    /*
     *  A file of another user, not root: not loaded, a save refused
     */
    unlink(path);
    write_file(path, "{\"password\": \"planted-by-another\"}", 0644);
    struct stat st_file;
    stat(path, &st_file);
    foreign_ino = st_file.st_ino;
    foreign_uid = geteuid() + 4242;
    gobj_write_str_attr(holder, "password", "mine");
    gobj_load_persistent_attrs(holder, 0);
    check_str("a file of another user is not loaded",
        gobj_read_str_attr(holder, "password"), "mine"
    );
    int ret2 = gobj_save_persistent_attrs(holder, json_string("password"));
    check_true("a save over a file of another user is refused", ret2 < 0);
    check_true("and leaves it as it was", file_contains(path, "planted-by-another"));

    /*
     *  Root's is trusted (the yuno run once as root)
     */
    foreign_uid = 0;
    gobj_load_persistent_attrs(holder, 0);
    check_str("a file of root is loaded",
        gobj_read_str_attr(holder, "password"), "planted-by-another"
    );

    /*
     *  But not one that others can write: root's file left 0664 by a
     *  release before 7.25.19, in the group-writable data dir, can be
     *  edited by any member of the group. Up to 7.25.21 it was loaded
     */
    chmod(path, 0664);
    gobj_write_str_attr(holder, "password", "mine-again");
    gobj_load_persistent_attrs(holder, 0);
    check_str("a file of root that the group can write is not loaded",
        gobj_read_str_attr(holder, "password"), "mine-again"
    );
    foreign_ino = 0;

    /*
     *  Run as root, the trust comes from the chain closed from "/" down:
     *  here /tmp (told 0755) and the realm directory (told 0755, of the
     *  yuno's user). A member of the group renames the data directory away
     *  -- its parent is 02775 on a node -- and makes one of their own, 0755,
     *  with their file in it. Up to 7.25.22 the walk went up from the file,
     *  and that closed data directory named its maker as the yuno's user:
     *  the file was loaded, and the next save given to them
     */
    char data_dir[PATH_MAX];
    snprintf(data_dir, sizeof(data_dir), "%s", path);
    *strrchr(data_dir, '/') = 0;
    char realm_dir[PATH_MAX];
    snprintf(realm_dir, sizeof(realm_dir), "%s", data_dir);
    *strrchr(realm_dir, '/') = 0;
    struct stat st_dir;
    stat("/tmp", &st_dir);
    fake_dirs[0] = (fake_dir_t){st_dir.st_ino, 0, 0755};
    stat(realm_dir, &st_dir);
    fake_dirs[1] = (fake_dir_t){st_dir.st_ino, geteuid(), 0755};
    uid_t intruder = geteuid() + 4243;
    stat(data_dir, &st_dir);
    fake_dirs[2] = (fake_dir_t){st_dir.st_ino, intruder, 0755};

    write_file(path, "{\"password\": \"planted-in-a-dir-of-mine\"}", 0600);
    stat(path, &st_file);
    foreign_ino = st_file.st_ino;
    foreign_uid = intruder;
    gobj_write_str_attr(holder, "password", "mine-as-root");
    fake_root = TRUE;
    gobj_load_persistent_attrs(holder, 0);
    fake_root = FALSE;
    check_str("run as root, a file in a planted closed directory is not loaded",
        gobj_read_str_attr(holder, "password"), "mine-as-root"
    );

    /*
     *  While the file of the yuno's user, there, is
     */
    foreign_ino = 0;
    write_file(path, "{\"password\": \"of-the-yunos-user\"}", 0600);
    fake_root = TRUE;
    gobj_load_persistent_attrs(holder, 0);
    fake_root = FALSE;
    check_str("run as root, the file of the owner of the closed chain is loaded",
        gobj_read_str_attr(holder, "password"), "of-the-yunos-user"
    );

    /*
     *  And so it is when the realm is reached through a symlink inside the
     *  closed chain (/yuneta -> /srv/yuneta on a node; here
     *  /tmp/test_secret_attrs -> ../tmp/test_secret_attrs.real, a ".."
     *  too): only root or the chain's user can have put it there, so it is
     *  followed. Its first form stopped at the symlink and named root: a
     *  root yuno refused its own files
     */
    char moved_realm[PATH_MAX];
    build_path(moved_realm, sizeof(moved_realm), "/tmp", "test_secret_attrs.real", NULL);
    if(rename(realm_dir, moved_realm) < 0 ||
            symlink("../tmp/test_secret_attrs.real", realm_dir) < 0) {
        printf("FAIL cannot put the realm behind a symlink: %s\n", strerror(errno));
        s_result += -1;
    }
    write_file(path, "{\"password\": \"through-a-linked-realm\"}", 0600);
    fake_root = TRUE;
    gobj_load_persistent_attrs(holder, 0);
    fake_root = FALSE;
    check_str("run as root, a realm reached through a symlink of the closed chain is trusted",
        gobj_read_str_attr(holder, "password"), "through-a-linked-realm"
    );
    unlink(realm_dir);
    rename(moved_realm, realm_dir);
    memset(fake_dirs, 0, sizeof(fake_dirs));

    /*
     *  An attr of the file that is not persistent (any more: the jwks that
     *  add-jwk persisted before 7.25.22) is not loaded, and it is said,
     *  without its value. Up to 7.25.22 it was dropped in silence
     */
    write_file(path, "{\"note\": \"n\", \"jwks\": \"jwks-value-x\"}", 0600);
    watch("jwks-value-x", "holds an attr that is not persistent");
    gobj_load_persistent_attrs(holder, 0);
    unwatch("an attr of the file that is not persistent");

    unlink(path);
    unlink(planted);
}

/***************************************************************
 *              Framework Methods
 ***************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
}

PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    set_timeout(priv->timer, 100);
    return 0;
}

PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    return 0;
}

/***************************************************************
 *              Actions
 ***************************************************************/
PRIVATE int ac_secret_kw(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}

PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    check_http_cookie(gobj);
    check_view_config(gobj);
    check_create_delete2_trace();
    check_command_trace();
    check_wild_command_trace();
    check_log_dumps(gobj);
    check_persistent_file();

    set_yuno_must_die();

    KW_DECREF(kw)
    return 0;
}

/***************************************************************
 *              Commands
 ***************************************************************/
PRIVATE json_t *cmd_set_password(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    json_t *jn_password = kw_get_dict_value(gobj, kw, "password", 0, 0);
    if(json_is_string(jn_password)) {   // a number is only for the trace check
        gobj_write_str_attr(gobj, "password", json_string_value(jn_password));
    }
    KW_DECREF(kw)
    return build_command_response(gobj, 0, 0, 0, 0);
}

/*
 *  The shape of the agent's command-yuno: the free keys go to a table
 *  that is not this one. It keeps what it got, to be checked.
 */
PRIVATE json_t *cmd_command_yuno(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *password = kw_get_str(gobj, kw, "password", "", 0);
    const char *command = kw_get_str(gobj, kw, "command", "", 0);
    gobj_write_str_attr(gobj, "note", empty_string(password)? command : password);
    KW_DECREF(kw)
    return build_command_response(gobj, 0, 0, 0, 0);
}

/*-PM----type-----------name------------flag--------------------default-description--*/
PRIVATE sdata_desc_t pm_command_yuno[] = {
SDATAPM (DTP_STRING,    "id",           0,                      0,      "Id of yuno"),
SDATAPM (DTP_STRING,    "command",      0,                      0,      "Command to be executed in matched yunos"),
SDATAPM (DTP_STRING,    "service",      0,                      0,      "Service of yuno where execute the command"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_set_password[] = {
SDATAPM (DTP_STRING,    "password",     SDF_SECRET,             0,      "The new password"),
SDATAPM (DTP_STRING,    "note",         0,                      0,      "Not a secret"),
SDATAPM (DTP_JSON,      "credentials",  SDF_SECRET,             0,      "A secret json"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_set_password_pos[] = {
SDATAPM (DTP_STRING,    "password",     SDF_REQUIRED|SDF_SECRET, 0,     "The new password"),
SDATA_END()
};

PRIVATE sdata_desc_t holder_command_table[] = {
/*-CMD---type-----------name----------------alias---items-------------------json_fn-------------description--*/
SDATACM (DTP_SCHEMA,    "set-password",     0,      pm_set_password,        cmd_set_password,   "Set the password"),
SDATACM (DTP_SCHEMA,    "set-password-pos", 0,      pm_set_password_pos,    cmd_set_password,   "Set the password, positional"),
SDATACM2 (DTP_SCHEMA,   "command-yuno",     SDF_WILD_CMD,   0,      pm_command_yuno,    cmd_command_yuno,   "Command to yuno, free keys"),
SDATA_END()
};

/***************************************************************
 *              GClass
 ***************************************************************/
PRIVATE sdata_desc_t driver_attrs_table[] = {
    SDATA_END()
};

PRIVATE sdata_desc_t holder_attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------------default-description---------- */
SDATA (DTP_STRING,      "password",         SDF_WR|SDF_PERSIST|SDF_SECRET,"", "A secret"),
SDATA (DTP_STRING,      "note",             SDF_WR|SDF_PERSIST,       "",     "Not a secret"),
SDATA (DTP_INTEGER,     "pin",              SDF_WR|SDF_PERSIST|SDF_SECRET,"0","A secret that is a number"),
SDATA (DTP_STRING,      "config_only",      SDF_PERSIST,              "",     "Persistent, set by the config"),
SDATA_END()
};

PRIVATE int register_c_test_secret(void)
{
    static const GMETHODS gmt = {
        .mt_create = mt_create,
        .mt_start = mt_start,
        .mt_stop = mt_stop,
    };
    static const GMETHODS gmt_holder = {
        0
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,    0},
        {EV_TEST_SECRET_KW, 0},
        {EV_TEST_SECRET_PUB, 0},
        {0, 0}
    };

    ev_action_t st_idle[] = {
        {EV_TIMEOUT,    ac_timeout,     0},
        {EV_TEST_SECRET_KW, ac_secret_kw, 0},
        {EV_TEST_SECRET_PUB, ac_secret_kw, 0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE,   st_idle},
        {0, 0}
    };

    hgclass gc = gclass_create(
        C_TEST_SECRET_DRIVER,
        event_types,
        states,
        &gmt,
        0,                          // lmt
        driver_attrs_table,
        sizeof(PRIVATE_DATA),
        0,                          // authz_table
        0,                          // command_table
        0,                          // s_user_trace_level
        0                           // gclass_flag
    );
    if(!gc) {
        return -1;
    }

    event_type_t holder_event_types[] = {
        {EV_TEST_SECRET_PUB, EVF_OUTPUT_EVENT},
        {0, 0}
    };

    ev_action_t holder_st_idle[] = {
        {0, 0, 0}
    };

    states_t holder_states[] = {
        {ST_IDLE,   holder_st_idle},
        {0, 0}
    };

    gc = gclass_create(
        C_TEST_SECRET_HOLDER,
        holder_event_types,
        holder_states,
        &gmt_holder,
        0,                          // lmt
        holder_attrs_table,
        0,                          // priv size
        0,                          // authz_table
        holder_command_table,       // command_table
        0,                          // s_user_trace_level
        0                           // gclass_flag
    );
    return gc ? 0 : -1;
}

PRIVATE int register_yuno_and_more(void)
{
    return register_c_test_secret();
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    glog_init();
    gobj_log_register_handler("capture_logs", 0, capture_logs, 0);
    gobj_log_add_handler("capture_logs", "capture_logs", LOG_OPT_ALL, 0);
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

    /*
     *  A run killed half way (a chmod'ed directory, a file 0000) must not
     *  fail the next one: start from nothing
     */
    if(access("/tmp/test_secret_attrs", F_OK) == 0) {
        chmod("/tmp/test_secret_attrs/data", 0775);
        rmrdir("/tmp/test_secret_attrs");
    }

    helper_quote2doublequote(fixed_config);
    helper_quote2doublequote(variable_config);

    yuneta_setup(
        NULL,                   // persistent_attrs, default internal dbsimple
        command_parser,         // command_parser (needed for gobj_command)
        NULL,                   // stats_parser
        NULL,                   // authz_checker
        NULL,                   // authentication_parser
        MEM_MAX_BLOCK,
        MEM_MAX_SYSTEM_MEMORY,
        USE_OWN_SYSTEM_MEMORY,
        MEM_MIN_BLOCK,
        MEM_SUPERBLOCK
    );

    int result = yuneta_entry_point(
        argc, argv,
        APP, APP_VERSION, APP_SUPPORT, APP_DOC, APP_DATETIME,
        fixed_config,
        variable_config,
        register_yuno_and_more,
        NULL                    // cleaning
    );

    size_t leaked = get_cur_system_memory();
    check_int("no memory leak", (int)leaked, 0);

    printf("\n%s: %s\n", APP, (s_result == 0 && result == 0) ? "PASS" : "FAIL");
    return s_result + result;
}
