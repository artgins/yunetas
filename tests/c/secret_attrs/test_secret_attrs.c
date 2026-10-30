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
 *               write-attr reads its value as the attribute's type (a
 *               json one a bare word as the text, a dict or a list its
 *               json, a pointer refused) and its refusal does not repeat
 *               the value: a secret json's was echoed.
 *            4b. The command-yuno shape: a SDF_WILD_CMD command forwards
 *               free keys to a table that is not known where it is traced;
 *               a key with a secret's name (password=...) is masked, in
 *               the line, in the kw, and inside a quoted `command`.
 *            4c. A secret by its VALUE: a JWT and the token
 *               after "Bearer " are masked under any name or none, in a
 *               text and in a json; pass, pin and otp are secrets' names
 *               as whole segments (not passive, ping, otpx), key and cert
 *               are not; a string with a NUL is masked past it; a
 *               write-attr of a secret written with blanks does not repeat
 *               the rest of it; the first word of a line out of session
 *               (C_IEVENT_SRV, C_IEVENT_CLI) is masked; the log line of a
 *               peer's app (EV_REMOTE_LOG) is capped and masked, its kw not
 *               dumped; and the frame of an event that is not public is
 *               one WARNING, masked.
 *            4d. A json that does not parse is never logged as it came
 *               (anystring2json(), string2json(), json_config_string2json(),
 *               a dict or list attr written as text): not its bytes, not
 *               the bytes jansson quotes near the error. An HTTP json body
 *               that breaks (ghttp_parser) and an inter-event without its
 *               routing (msg_iev_get_stack(), msg_iev_pop_stack()) are the
 *               peer's: a WARNING, masked. Up to 7.26.9 each was an ERROR
 *               with the text, or the kw, in clear.
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
static fake_dir_t fake_dirs[8] = {{0}};

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
 *  directories, so the planted case fails on it. Only with glibc 2.33 or
 *  later: before, stat() was an inline over __xstat(), which --wrap=stat
 *  does not reach (the code under test uses fstat() and is not affected)
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
PRIVATE int s_watch_errors = 0;         // ERRORs (or worse) logged while watching
PRIVATE size_t s_remote_log_longest = 0;    // the longest "Remote Error" line
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

PRIVATE void make_link(const char *target, const char *path)
{
    if(symlink(target, path) < 0) {
        printf("FAIL cannot make the link %s -> %s: %s\n", path, target, strerror(errno));
        s_result += -1;
    }
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
 *  Masking is linear, measured by its SLOPE, not by a time limit: a limit
 *  failed under ASan with the machine loaded, and says nothing of the
 *  shape of the cost. Each text is masked at 16 KB, 64 KB and 256 KB; the
 *  slope of log(time) on log(size) over those three, equally spaced, is
 *  log(t_256KB / t_16KB) / log(16): 1 for a linear masking, 2 for a
 *  quadratic one. Below 1.15 it is linear, a ratio below 16^1.15 = 24.25,
 *  the limit of gobj-js' test (secret_masking.test.js). A time is the best
 *  of three passes, the three sizes taken in turn in each pass, so that a
 *  load slows a pass and not one size; a call is repeated for 10 ms and
 *  its time is the mean, so that the grain of the clock makes no slope. A
 *  text is measured up to three times and fails only if every measure is
 *  steep: under load a large text pays the cache that a small one does
 *  not (with twelve memory-bound processes on eight cores one measure in
 *  ten was steep, up to 51, and the next one was not; at 1 MB it was one
 *  in five, and a run under ASan came close to the TIMEOUT of 60 s), and
 *  a quadratic masking is steep every time. The masking gives 15 to 17
 *  on an idle machine, plain and under ASan; a copy of it that also reads
 *  the text to its end from every 25th byte (about the masking's cost at
 *  256 KB) gives 24 to 46, and fails on 14 of the 17 texts; a quadratic
 *  masking gives 256.
 */
#define MASKED_AS_TEXT  0   // mask_secrets_inline() and mask_secrets_in_text() of a copy
#define MASKED_AS_JSON  1   // json_mask_secrets() of {"anything": text}
#define LINEAR_RATIO    24.25

PRIVATE double ms_of_masking(int how, const char *text, size_t len, char *work)
{
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    int calls = 0;
    double elapsed = 0;
    do {
        if(how == MASKED_AS_JSON) {
            json_t *jn = json_pack("{s:s}", "anything", text);
            json_t *jn_shown = json_mask_secrets(jn);
            JSON_DECREF(jn_shown)
            JSON_DECREF(jn)
        } else {
            char *m = mask_secrets_inline(text);
            GBMEM_FREE(m)
            memcpy(work, text, len);
            mask_secrets_in_text(work, len);
        }
        calls++;
        clock_gettime(CLOCK_MONOTONIC, &t1);
        elapsed = (double)(t1.tv_sec - t0.tv_sec)*1000.0 + (double)(t1.tv_nsec - t0.tv_nsec)/1e6;
    } while(elapsed < 10);
    return elapsed / calls;
}

PRIVATE void check_masking_is_linear(const char *head, const char *unit, int how)
{
    size_t sizes[3] = {16*1024, 64*1024, 256*1024};
    char *texts[3] = {0};
    size_t lens[3] = {0};
    char *work = gbmem_malloc(sizes[2] + 1);
    size_t hl = strlen(head);
    size_t ul = strlen(unit);
    for(int s = 0; s < 3; s++) {
        texts[s] = gbmem_malloc(sizes[s] + 1);
        memcpy(texts[s], head, hl);
        size_t n = hl;
        while(n + ul <= sizes[s]) {
            memcpy(texts[s] + n, unit, ul);
            n += ul;
        }
        texts[s][n] = 0;
        lens[s] = n;
        ms_of_masking(how, texts[s], lens[s], work);    // the first calls warm the caches
    }

    double best[3];
    double ratio = 0;
    char seen[256] = "";
    for(int attempt = 0; attempt < 3; attempt++) {
        best[0] = best[1] = best[2] = 1e12;
        for(int pass = 0; pass < 3; pass++) {
            for(int s = 0; s < 3; s++) {
                double ms = ms_of_masking(how, texts[s], lens[s], work);
                if(ms < best[s]) {
                    best[s] = ms;
                }
            }
        }
        ratio = best[2] / best[0];
        size_t used = strlen(seen);
        snprintf(seen + used, sizeof(seen) - used, "%s%.1f (%.2f / %.2f / %.2f ms)",
            attempt? ", " : "", ratio, best[0], best[1], best[2]);
        if(ratio < LINEAR_RATIO) {
            break;
        }
    }

    char what[256];
    snprintf(what, sizeof(what), "%s of '%s%s': linear, 256 KB / 16 KB = %s",
        how == MASKED_AS_JSON? "json_mask_secrets()" : "mask_secrets_inline() + mask_secrets_in_text()",
        head, unit, seen);
    check_true(what, ratio < LINEAR_RATIO);

    for(int s = 0; s < 3; s++) {
        GBMEM_FREE(texts[s])
    }
    GBMEM_FREE(work)
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
    if(strstr(bf, "Remote Error") && len > s_remote_log_longest) {
        s_remote_log_longest = len;
    }
    if(s_watch_msg && strstr(bf, s_watch_msg)) {
        s_watch_msg_seen++;
    }
    if(s_watch && priority <= LOG_ERR) {
        s_watch_errors++;
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
        /*
         *  Called in a child only: without the filter its case would run
         *  untested and pass (the no-fallocate one does). Exit 2, which no
         *  case takes for its answer
         */
        printf("FAIL cannot install the seccomp filter: errno %d %s\n", errno, strerror(errno));
        fflush(stdout);
        _exit(2);
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
    s_watch_errors = 0;
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
 *  unwatch() of what a peer sent: a WARNING, never an ERROR
 */
PRIVATE void unwatch_peer(const char *what)
{
    char name[128];
    int errors = s_watch_errors;
    unwatch(what);
    snprintf(name, sizeof(name), "%s: no ERROR", what);
    check_int(name, errors, 0);
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
     *  write-attr reads its value text AS the attribute's type, as the
     *  parser reads a parameter of that type: strtoll()
     *  wrote "12abc" as 12
     */
    json_int_t pin_before = gobj_read_integer_attr(holder, "pin");
    resp = gobj_command(gobj_yuno(),
        "write-attr gobj_name=secret-holder attribute=pin value=12abc",
        json_object(), gobj
    );
    check_int("write-attr of a text that is no integer: refused",
        (int)kw_get_int(0, resp, "result", 0, 0), -1);
    JSON_DECREF(resp)
    check_int("write-attr refused: the attribute unchanged",
        (int)gobj_read_integer_attr(holder, "pin"), (int)pin_before);
    resp = gobj_command(gobj_yuno(),
        "write-attr gobj_name=secret-holder attribute=pin value=4242",
        json_object(), gobj
    );
    check_int("write-attr of an integer text: written",
        (int)gobj_read_integer_attr(holder, "pin"), 4242);
    JSON_DECREF(resp)

    /*
     *  A json attr takes a bare word as the text, a dict or a list its
     *  json, a pointer is refused, and a refusal never repeats the value:
     *  a json one was echoed whole, a secret's too
     */
    resp = gobj_command(gobj_yuno(),
        "write-attr gobj_name=secret-holder attribute=vault_secret value={bad-secret-hunter2",
        json_object(), gobj
    );
    check_int("write-attr of a json that breaks: refused",
        (int)kw_get_int(0, resp, "result", 0, 0), -1);
    check_true("write-attr refused: the value not repeated",
        strstr(kw_get_str(0, resp, "comment", "", 0), "hunter2") == NULL);
    JSON_DECREF(resp)
    resp = gobj_command(gobj_yuno(),
        "write-attr gobj_name=secret-holder attribute=vault_secret value=word",
        json_object(), gobj
    );
    check_int("write-attr of a json given a bare word: written",
        (int)kw_get_int(0, resp, "result", 0, 0), 0);
    JSON_DECREF(resp)
    check_str("write-attr of a json: a bare word is the text",
        json_string_value(gobj_read_json_attr(holder, "vault_secret")), "word");
    resp = gobj_command(gobj_yuno(),
        "write-attr gobj_name=secret-holder attribute=settings value={\"a\":1}",
        json_object(), gobj
    );
    JSON_DECREF(resp)
    check_int("write-attr of a dict: written",
        (int)json_integer_value(json_object_get(gobj_read_json_attr(holder, "settings"), "a")), 1);
    resp = gobj_command(gobj_yuno(),
        "write-attr gobj_name=secret-holder attribute=settings value=[1]",
        json_object(), gobj
    );
    check_int("write-attr of a dict given a list: refused",
        (int)kw_get_int(0, resp, "result", 0, 0), -1);
    JSON_DECREF(resp)
    check_true("write-attr refused: the dict unchanged",
        json_is_object(gobj_read_json_attr(holder, "settings")));
    resp = gobj_command(gobj_yuno(),
        "write-attr gobj_name=secret-holder attribute=items value=[1,2]",
        json_object(), gobj
    );
    JSON_DECREF(resp)
    check_int("write-attr of a list: written",
        (int)json_array_size(gobj_read_json_attr(holder, "items")), 2);
    resp = gobj_command(gobj_yuno(),
        "write-attr gobj_name=secret-holder attribute=handle value=5",
        json_object(), gobj
    );
    check_int("write-attr of a pointer: refused",
        (int)kw_get_int(0, resp, "result", 0, 0), -1);
    check_true("write-attr of a pointer: says why",
        strstr(kw_get_str(0, resp, "comment", "", 0), "is a pointer") != NULL);
    JSON_DECREF(resp)

    /*
     *  The `set` of a trace command takes text or json (DTP_JSON): `set=yes`
     *  was atoi()'d to a RESET, and a GUI's `set: 1` was refused for not
     *  being a text
     */
    resp = gobj_command(gobj_yuno(), "set-global-trace",
        json_pack("{s:s, s:i}", "level", "start_stop", "set", 1), gobj);
    check_int("set-global-trace set: 1 (json) sets",
        (gobj_global_trace_level() & TRACE_START_STOP)? 1 : 0, 1);
    JSON_DECREF(resp)
    resp = gobj_command(gobj_yuno(), "set-global-trace level=start_stop set=no", 0, gobj);
    check_int("set-global-trace set=no resets",
        (gobj_global_trace_level() & TRACE_START_STOP)? 1 : 0, 0);
    JSON_DECREF(resp)
    resp = gobj_command(gobj_yuno(), "set-global-trace level=start_stop set=yes", 0, gobj);
    check_int("set-global-trace set=yes sets",
        (gobj_global_trace_level() & TRACE_START_STOP)? 1 : 0, 1);
    JSON_DECREF(resp)
    resp = gobj_command(gobj_yuno(), "set-global-trace level=start_stop set=maybe", 0, gobj);
    check_int("set-global-trace set=maybe: refused",
        (int)kw_get_int(0, resp, "result", 0, 0), -1);
    JSON_DECREF(resp)
    resp = gobj_command(gobj_yuno(), "set-global-trace level=start_stop set=reset", 0, gobj);
    check_int("set-global-trace set=reset resets",
        (gobj_global_trace_level() & TRACE_START_STOP)? 1 : 0, 0);
    JSON_DECREF(resp)

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
     *  A value too long to show is shown as its size, keyed or positional.
     *  Up to 7.26.5 the content64 of an install-binary (tens of MB) overflowed
     *  the 64 KB buffer of the line: "MAXIMUM SPACE REACHED", with its stack.
     */
    {
        size_t big_len = 100*1024;
        char *big = GBMEM_MALLOC(big_len + 1);
        memset(big, 'A', big_len);
        big[big_len] = 0;
        gbuffer_t *gbuf_cmd = gbuffer_create(big_len + 256, big_len + 256);
        gbuffer_printf(gbuf_cmd, "set-password note=%s password=hunter2", big);
        line = command_mask_secret_line(holder, gbuffer_cur_rd_pointer(gbuf_cmd));
        check_str("a keyed value over 1 KB is shown as its size", line,
            "set-password note=<102400 bytes> password=********"
        );
        GBMEM_FREE(line)
        gbuffer_clear(gbuf_cmd);
        gbuffer_printf(gbuf_cmd, "set-password-pos %s note=x", big);
        line = command_mask_secret_line(holder, gbuffer_cur_rd_pointer(gbuf_cmd));
        check_str("a positional secret over 1 KB is still masked", line,
            "set-password-pos ******** note=x"
        );
        GBMEM_FREE(line)
        gbuffer_clear(gbuf_cmd);
        gbuffer_printf(gbuf_cmd, "set-note-pos %s", big);
        line = command_mask_secret_line(holder, gbuffer_cur_rd_pointer(gbuf_cmd));
        check_str("a positional value over 1 KB is shown as its size", line,
            "set-note-pos <102400 bytes>"
        );
        GBMEM_FREE(line)
        gbuffer_decref(gbuf_cmd);
        GBMEM_FREE(big)
    }

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
     *  Masking is linear (check_masking_is_linear(): by its slope, it was
     *  quadratic: seconds for 60 KB) and capped: a 1 MB "a=a=a=..." word, a
     *  1 MB secret value, and the shapes that each round found quadratic;
     *  5 MB is not shown at all
     */
    {
        size_t big_len = 1024*1024;
        char *big = gbmem_malloc(big_len + 16);
        for(size_t i=0; i<big_len; i++) {
            big[i] = (i & 1)? '=' : 'a';
        }
        big[big_len] = 0;

        char *m = mask_secrets_inline(big);
        check_true("a 1 MB a=a=... word has nothing to mask", m == NULL);
        GBMEM_FREE(m)

        memcpy(big, "password=", 9);
        memset(big + 9, 'x', big_len - 9);
        m = mask_secrets_inline(big);
        check_str("a 1 MB secret value is masked whole", m, "password=********");
        GBMEM_FREE(m)
        GBMEM_FREE(big)

        const char *words[] = {"a=", "abcd=", " attribute=", "pin=\"", "abc?token=x&", 0};
        for(int k=0; words[k]; k++) {
            check_masking_is_linear("", words[k], MASKED_AS_TEXT);
            check_masking_is_linear("", words[k], MASKED_AS_JSON);
        }
        check_masking_is_linear("password=", "x", MASKED_AS_TEXT);

        /*
         *  And the shapes a review found: a scheme word repeated ("Bearer Bearer
         *  ..." looked ahead from each of its words: quadratic), a text of
         *  ':' after secrets' names, and a container that does not close;
         *  and of 7.27.0: names in brackets, %-encoded, as labels, and the
         *  userinfo of a URL, and %-encoded queries read decoded, twice
         */
        const char *patterns[] = {
            "Bearer ", "basic ", "token:", "\"pin\": {[",
            "[pin]=\"", "t%6Fken=x&", "x[pin]: ", "a://b:c@",
            "p%3Dx%26", "%2541", "%25253D", 0
        };
        for(int k=0; patterns[k]; k++) {
            check_masking_is_linear("", patterns[k], MASKED_AS_TEXT);
        }

        uint64_t t0, t1;
        json_t *jn_shown;
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

/*
 *  A json text that does not parse is never logged as it came: not its
 *  bytes, not the bytes jansson quotes near the error ("near '\"S'"). The
 *  error says where (position, line, column); a dump of the bytes is
 *  masked. What a peer sent (an HTTP body, an inter-event without its
 *  routing) is a WARNING.
 */
PRIVATE void check_json_parse_logs(hgobj gobj)
{
    hgobj holder = gobj_find_service("secret-holder", TRUE);
    const char *trailing_comma = "{\"username\":\"bob\",\"password\":\"parse-hunter2\",}";
    const char *cut = "{\"password\": \"parse-hunter2";

    watch("parse-hunter2", "json_loadb() FAILED");
    json_t *jn = anystring2json(trailing_comma, strlen(trailing_comma), TRUE);
    check_true("anystring2json() of a json that breaks: NULL", jn == NULL);
    jn = anystring2json(cut, strlen(cut), TRUE);
    check_true("anystring2json() of a json cut: NULL", jn == NULL);
    unwatch("anystring2json() of a json that breaks");

    watch("parse-hunter2", "json_loads() FAILED");
    jn = string2json(trailing_comma, TRUE);
    check_true("string2json() of a json that breaks: NULL", jn == NULL);
    jn = string2json(cut, TRUE);
    check_true("string2json() of a json cut: NULL", jn == NULL);
    unwatch("string2json() of a json that breaks");

    watch("parse-hunter2", "json_loads() FAILED");
    jn = json_config_string2json(trailing_comma, TRUE);
    check_true("json_config_string2json() of a json that breaks: NULL", jn == NULL);
    jn = json_config_string2json(cut, TRUE);
    check_true("json_config_string2json() of a json cut: NULL", jn == NULL);
    unwatch("json_config_string2json() of a json that breaks");

    watch("parse-hunter2", "Attribute value of the wrong type");
    gobj_write_attr(holder, "settings", json_string(trailing_comma), gobj);
    unwatch("a dict attr written as text that breaks");
    watch("parse-hunter2", "Attribute value of the wrong type");
    gobj_write_attr(holder, "items", json_string(cut), gobj);
    unwatch("a list attr written as text that breaks");

    // The json body of an HTTP message (the BFF's /auth/login, an IdP's answer), as ghttp_parser reads it
    GHTTP_PARSER *parser = ghttp_parser_create(gobj, HTTP_REQUEST, 0, 0, EV_TEST_SECRET_KW, TRUE);
    char request[512];
    int len = snprintf(request, sizeof(request),
        "POST /auth/login HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "\r\n"
        "%s",
        (int)strlen(trailing_comma), trailing_comma
    );
    watch("parse-hunter2", "is not json");
    ghttp_parser_received(parser, request, (size_t)len);
    unwatch_peer("an HTTP json body that breaks");
    len = snprintf(request, sizeof(request),
        "POST /auth/login HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "\r\n"
        "%s",
        (int)strlen(cut), cut
    );
    watch("parse-hunter2", "is not json");
    ghttp_parser_received(parser, request, (size_t)len);
    unwatch_peer("an HTTP json body cut");
    ghttp_parser_destroy(parser);

    // An inter-event without its routing (__md_iev__, or its stack): the peer's frame
    json_t *kw = json_pack("{s:s, s:s}",
        "__command__", "set-password password=stack-hunter2",
        "password", "stack-hunter2"
    );
    watch("stack-hunter2", "__md_iev__ NOT FOUND");
    check_true("msg_iev_get_stack() without __md_iev__: NULL",
        msg_iev_get_stack(gobj, kw, IEVENT_STACK_ID, TRUE) == NULL);
    unwatch_peer("msg_iev_get_stack() of a kw without __md_iev__");
    watch("stack-hunter2", "__md_iev__ NOT FOUND");
    check_true("msg_iev_pop_stack() without __md_iev__: NULL",
        msg_iev_pop_stack(gobj, kw, IEVENT_STACK_ID) == NULL);
    unwatch_peer("msg_iev_pop_stack() of a kw without __md_iev__");
    json_object_set_new(kw, "__md_iev__", json_pack("{s:s}", "__msg_type__", "__message__"));
    watch("stack-hunter2", "stack NOT FOUND");
    check_true("msg_iev_get_stack() without the stack: NULL",
        msg_iev_get_stack(gobj, kw, IEVENT_STACK_ID, TRUE) == NULL);
    unwatch_peer("msg_iev_get_stack() of a kw without its stack");
    watch("stack-hunter2", "stack NOT FOUND");
    check_true("msg_iev_pop_stack() without the stack: NULL",
        msg_iev_pop_stack(gobj, kw, IEVENT_STACK_ID) == NULL);
    unwatch_peer("msg_iev_pop_stack() of a kw without its stack");
    JSON_DECREF(kw)
}

/*
 *  A secret by its VALUE, and what still reached the log or an answer
 */
PRIVATE void check_secret_values(hgobj gobj)
{
    json_t *resp;

    /*
     *  pass, pin and otp are secrets' names as WHOLE segments only; key
     *  and cert are not
     */
    struct {
        const char *name;
        BOOL secret;
    } names[] = {
        {"pass", TRUE}, {"db_pass", TRUE}, {"user-pin", TRUE}, {"otp", TRUE},
        {"sim_pin", TRUE}, {"PIN", TRUE}, {"user[pin]", TRUE}, {"otp.code", TRUE},
        {"passive", FALSE}, {"bypass", FALSE}, {"compass", FALSE}, {"ping", FALSE},
        {"mapping", FALSE}, {"spinner", FALSE}, {"otpx", FALSE}, {"pin2", FALSE},
        {"pin_count", FALSE},   // a segment about it
        {"key", FALSE}, {"cert", FALSE}, {"cert_pem", FALSE},
        {0, 0}
    };
    for(int i=0; names[i].name; i++) {
        char what[128];
        snprintf(what, sizeof(what), "%s is %sa secret's name",
            names[i].name, names[i].secret? "" : "not ");
        check_true(what, is_secret_name(names[i].name, strlen(names[i].name)) == names[i].secret);
    }
    check_true("pin_count is, for is_secret_name_any()", is_secret_name_any("pin_count", 9));

    /*
     *  A JWT, and the token after "Bearer ", by their shape: under any name
     *  or none
     */
    struct {
        const char *in;
        const char *out;    // NULL: nothing to mask
    } shapes[] = {
        {"note=eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiJ4In0.c2lnLWh1bnRlcjI x=1", "note=******** x=1"},
        {"curl -H 'Authorization: Bearer abc-hunter2' x", "curl -H 'Authorization: ********' x"},
        {"BEARER   tok-hunter2", "BEARER   ********"},
        {"the jwt eyJhbGciOiJub25lIn0.eyJzdWIiOiJ4In0.c2ln.", "the jwt ********."},
        {"pin=1234 sim_pin=5678 ping=1", "pin=******** sim_pin=******** ping=1"},
        {"eyJabc.def x=eyJabc", "******** x=eyJabc"},  // two parts: a JWT a cap cut
        {"xeyJa.b.c", NULL},               // inside a word
        {"bearer=x", "bearer=********"},   // its name, as before

        /*
         *  After "Bearer" or "Basic" the word is masked
         *  when it can be a credential, never when it reads as prose
         */
        {"the bearer of this letter", NULL},
        {"Pay the Bearer Bond.", NULL},
        {"the bearer self-service and/or e.g. holder", NULL},
        {"bearer BOND", NULL},
        {"el portador bearer señor", NULL},
        {"a basic plan, BASIC programs, Basic Operations", NULL},
        {"Basic YWRtaW4=", NULL},          // the base64 of "admin": no ':', no user:password
        {"Bearer mF_9.B5f-4.1JqM", "Bearer ********"},     // RFC 6750
        {"bearer abcdEFGHijkl", "bearer ********"},        // its case mixed
        {"Bearer 12345678", "Bearer ********"},
        {"Bearer abcdefghijklmnopqrstuvwxyz", "Bearer ********"},  // long
        {"Bearer eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiJ4In0.c2ln", "Bearer ********"},
        {"Bearer short1", NULL},           // shorter than a token
        {"x Basic dXNlcjpwYXNz y", "x Basic ******** y"},  // "user:pass"
        {"basic YTpi", "basic ********"},  // "a:b"

        /*
         *  The false negatives
         */
        {"Bearer Bearer TOKEN", "Bearer Bearer ********"},
        {"Bearer \"tok\"", "Bearer ********"},
        {"bearer: tok", "bearer: ********"},
        {"x eyJhbGciOiJSU0EtT0FFUCJ9.OKOawDo.48V1_ALb.5eym8TW.XFBoMYUZ y", "x ******** y"},  // a JWE
        {"eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiJ0ZXN0ZX", "********"},   // cut by a cap
        {"the jwt eyJhbGciOiJIUzI1NiJ9.", NULL},  // its only dot ends the sentence: a header
        {"the jwt eyJhbGciOiJIUzI1NiJ9.eyJ", "the jwt ********"},  // cut after its header
        {"eyJhbGciOiJIUzI1NiJ9", NULL},    // a header and no '.': no JWT
        {"token=Bearer abc x=1", "token=******** x=1"},
        {"password=Basic xyz", "password=********"},
        {"\"password\": Bearer abc, \"a\": 1", "\"password\": ********, \"a\": 1"},
        {"{\"password\": \"pw-hunter2\", \"user\": \"bob\"}", "{\"password\": ********, \"user\": \"bob\"}"},
        {"\"password\": hunter2", "\"password\": ********"},
        {"Authorization: Basic dXNlcjpwYXNz", "Authorization: ********"},
        {"X-Auth-Token: xat-hunter2; next", "X-Auth-Token: ********; next"},
        {"{'token': 'py-hunter2'}", "{'token': ********}"},
        {"token_endpoint: https://idp/token", NULL},   // about a token
        {"at 12:30:45 on host:8080", NULL},
        {"Wrong token: try again", "Wrong token: ******** again"},  // a secret's name and a ':' is a label

        /*
         *  Each rule judges
         *  the text as it came, and 7.26.9's reading of the parameters is
         *  kept whole -- a review showed what 7.26.9 masked
         */
        {"eyJzdWIiOiJ4In0.bearer12345678=sr1-hunter2", "********=********"},  // a JWT of two parts took the name
        {"Bearer Bearer pwd=x \"sr2-hunter2\"", "Bearer Bearer ********"},   // the doubled scheme took the '='
        {"x-auth-token: password=sr3,hunter2 c", "x-auth-token: ********"},  // the label's value hid a name=value
        {"x Bearer abc%2Bsr4hunter2 y", "x Bearer ******** y"},              // a token cut at a byte of no token68
        {"password12:30command='sr5-hunter2' y", "password12:******** y"},   // a label inside a name
        {"update-node x='{\\\"password\\\":\\\"sr6-hunter2\\\"}'",
         "update-node x='{\\\"password\\\":********}'"},                     // a json text inside a json text

        /*
         *  7.27.0: a secret's name in a query string, after a word that is
         *  no secret's name ("file" is about a credential); what 7.26.9
         *  masked after the value of a name of a segment (pass, pin); a
         *  scheme and its credential under a secret's name, whole
         */
        {"GET https://my-file-server.com/get?token=qs1-hunter2&x=1",
         "GET https://my-file-server.com/get?token=********&x=1"},
        {"https://cdn.public.example/x?access_token=qs2-hunter2", "https://cdn.public.example/x?access_token=********"},
        {"db_pass='password'=qs3-hunter2", "db_pass=********=********"},
        {"pin='token=qs4'hunter2 x", "pin=********"},
        {"password=Basic qs5hunter2==", "password=********"},
        {"eyJhIjoxfQ. eyJhIjoxfQ..", NULL},  // headers whose dots end them

        /*
         *  7.27.0: a query name as a form writes it -- in brackets,
         *  %-encoded -- after a word that is no secret's name, and the
         *  password of the userinfo of a URL
         */
        {"GET https://my-file-server.com/get?user[password]=wn1-hunter2&x=1",
         "GET https://my-file-server.com/get?user[password]=********&x=1"},
        {"GET https://my-file-server.com/get?t%6Fken=wn2-hunter2", "GET https://my-file-server.com/get?t%6Fken=********"},
        {"GET https://my-file-server.com/f?user%5Bpin%5D=1234&a%5B0%5D=5",
         "GET https://my-file-server.com/f?user%5Bpin%5D=********&a%5B0%5D=5"},
        {"clone https://bob:wn3-hunter2@git.example/x.git", "clone https://bob:********@git.example/x.git"},
        {"redis://:wn4-hunter2@[::1]:6379/0", "redis://:********@[::1]:6379/0"},
        {"ssh://git@github.com:22/x http://[::1]:8080/y", NULL},    // a user, a port: no password

        /*
         *  7.27.0: a URL or a query %-encoded inside the value of a
         *  parameter (redirect_uri=, next=, url=), and a name encoded twice,
         *  are read decoded too; a word of a value is not the name of the
         *  parameter after it
         */
        {"GET /cb?next=%2Flogin%3Fpassword%3Dnx1-hunter2", "GET /cb?next=%2Flogin%3Fpassword%3D********"},
        {"url=https%3A%2F%2Fbob%3Anx2-hunter2%40host", "url=https%3A%2F%2Fbob%3A********%40host"},
        {"redirect_uri=https%3A%2F%2Fapp%2Fcb%3Ftoken%3Dnx3-hunter2&state=1",
         "redirect_uri=https%3A%2F%2Fapp%2Fcb%3Ftoken%3D********"},
        {"%2570assword=nx4-hunter2", "%2570assword=********"},
        {"state=%7B%22token%22%3A%22nx5-hunter2%22%7D", "state=%7B%22token%22%3A********%7D"},
        {"next=https%253A%252F%252Fapp%252Fcb%253Fpassword%253Dnx6-hunter2",
         "next=https%253A%252F%252Fapp%252Fcb%253Fpassword%253D********"},
        {"GET /search?q=boarding+pass&lang=es", NULL},
        {"GET /x?q=1&db_pass=nx7-hunter2", "GET /x?q=1&db_pass=********"},
        {0, 0}
    };
    for(int i=0; shapes[i].in; i++) {
        char *m = mask_secrets_inline(shapes[i].in);   // NULL: nothing to mask
        check_str("a credential by its shape is masked",
            m? m : "(nothing to mask)", shapes[i].out? shapes[i].out : "(nothing to mask)"
        );

        /*
         *  mark_secrets_inline() marks what mask_secrets_inline() masks: the
         *  same text, each run of marks as one "********"
         */
        const char *in = shapes[i].in;
        size_t len = strlen(in);
        uint8_t marks[256] = {0};
        size_t n_marked = mark_secrets_inline(in, len, marks);
        char rebuilt[1024];
        size_t r = 0;
        for(size_t k = 0; k < len && r + 9 < sizeof(rebuilt); k++) {
            BOOL marked = (marks[k >> 3] >> (k & 7)) & 1;
            if(!marked) {
                rebuilt[r++] = in[k];
            } else if(k == 0 || !((marks[(k-1) >> 3] >> ((k-1) & 7)) & 1)) {
                memcpy(rebuilt + r, "********", 8);
                r += 8;
            }
        }
        rebuilt[r] = 0;
        check_str("mark_secrets_inline() marks what mask_secrets_inline() masks",
            n_marked? rebuilt : "(nothing to mask)", m? m : "(nothing to mask)"
        );
        GBMEM_FREE(m)
    }

    /*
     *  mask_secrets_inline_capped(): masked FIRST, then cut. Cut first,
     *  "Bearer abcd" was all of a token of 15 that a cap left: it showed
     */
    struct {
        const char *in;
        size_t bfsize;
        const char *out;
    } capped[] = {
        {"EV_X Bearer abcdefghij12345", 17, "EV_X Bearer ****"},
        {"EV_X password=hunter2 x=1", 64, "EV_X password=******** x=1"},
        {"eyJhbGciOiJIUzI1NiJ9AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA.eyJzdWIi.c2ln", 33,
         "********"},       // its '.' beyond the cap
        {"señor password=x", 4, "se"},  // a whole character
        {"plain text", 64, "plain text"},
        {0, 0, 0}
    };
    for(int i=0; capped[i].in; i++) {
        char bf[128];
        size_t n = mask_secrets_inline_capped(bf, capped[i].bfsize, capped[i].in, strlen(capped[i].in));
        check_str("masked, then capped", bf, capped[i].out);
        check_int("its length", (int)n, (int)strlen(capped[i].out));
    }

    /*
     *  A masked copy that fits in the cap ends on a whole character too: the
     *  window it was masked from cut one ("EV_pin=", 300 x, " next=1 ", and
     *  the window of 320 bytes ends in the 3rd e-acute)
     */
    char cut_text[1024];
    size_t cut_len = (size_t)snprintf(cut_text, sizeof(cut_text), "EV_pin=%300s next=1 ", "");
    memset(cut_text + 7, 'x', 300);
    for(int i = 0; i < 300 && cut_len + 2 < sizeof(cut_text); i++) {
        cut_text[cut_len++] = (char)0xC3;
        cut_text[cut_len++] = (char)0xA9;
    }
    cut_text[cut_len] = 0;
    char cut_bf[65];
    size_t cut_n = mask_secrets_inline_capped(cut_bf, sizeof(cut_bf), cut_text, cut_len);
    check_str("a masked copy shorter than the cap ends on a whole character",
        cut_bf, "EV_pin=******** next=1 \xC3\xA9\xC3\xA9");
    check_int("its length", (int)cut_n, 27);

    /*
     *  The password of a URL whose '@' lies past the window judged: the
     *  authority reaches the cut, so it is masked from its first ':' to the
     *  cut, at every cap -- plain, %-encoded, with an '@' of the password
     *  or of the user (an email address) in the window: the userinfo ends
     *  at the LAST '@' --, and after a value masked short, which leaves the end
     *  of the window inside the copy shown. A userinfo that begins with '['
     *  is no IP literal followed by a port: its password is masked too --
     *  and so is one whose "zone" is longer than any, which put the start
     *  of a literal and its port farther apart than the bytes judged past
     *  the cap
     */
    {
        char url_text[1024];
        char url_bf[512];
        const char *url_shapes[][5] = {
            {"url=https://bob:", "P", "@host/p", "plain", "P"},
            {"url=https%3A%2F%2Fbob%3A", "%50", "%40host", "%-encoded", "%5"},
            {"url=https://bob:p@", "P", "@host/p", "an '@' of the password in the window", "P"},
            {"url=imap://bob@example.com:", "P", "@mail/p", "an '@' of the user in the window", "P"},
            {"password=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx url=https://bob:",
             "P", "@host/p", "after a value masked short", "P"},
            {"x https://[a%x:SuperSecret1]:", "z", "@h/p", "a userinfo [a%x:password]:", "Secret"},
            {"x https://[dead:beef]:", "z", "@h/p", "a userinfo [user:password]:", "beef"},
            {"x https://[::1]:", "1", "@h/p", "a userinfo [::1]:digits, more than a port", "1]"},
            {"x https://[fe80::1%25", "z", "]:80@h/p", "a userinfo [fe80::1%25zone]:80, a zone too long", "::1"},
        };
        for(size_t k = 0; k < sizeof(url_shapes)/sizeof(url_shapes[0]); k++) {
            size_t ul = (size_t)snprintf(url_text, sizeof(url_text), "%s", url_shapes[k][0]);
            size_t unit = strlen(url_shapes[k][1]);
            while(ul + unit + strlen(url_shapes[k][2]) < 300 + strlen(url_shapes[k][0])) {
                memcpy(url_text + ul, url_shapes[k][1], unit);
                ul += unit;
            }
            ul += (size_t)snprintf(url_text + ul, sizeof(url_text) - ul, "%s", url_shapes[k][2]);
            size_t leaks = 0;
            for(size_t bfsize = 2; bfsize <= sizeof(url_bf); bfsize++) {
                mask_secrets_inline_capped(url_bf, bfsize, url_text, ul);
                if(strstr(url_bf, url_shapes[k][4])) {
                    if(leaks++ == 0) {
                        printf("     cap %zu shows %s\n", bfsize, url_bf);
                    }
                }
            }
            char what[160];
            snprintf(what, sizeof(what), "a URL password cut by the window is masked at every cap (%s)",
                url_shapes[k][3]);
            check_true(what, leaks == 0);
        }
    }

    /*
     *  The same userinfo shaped like a literal, %-encoded byte by byte once
     *  and twice: the margin past the cap counts the bytes WRITTEN, three
     *  times those of the literal per encoding
     */
    {
        static char enc[2][2048];
        static char enc_bf[1400];
        size_t el = (size_t)snprintf(enc[0], sizeof(enc[0]),
            "https://[ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255%%25%60s]:80@h/p", "");
        memset(enc[0] + 57, 'z', 60);
        const char *needles[2] = {"%3A%66", "%25%33%41%25%36%36"};    // ":f" after "[ffff"
        for(int t = 0; t < 2; t++) {
            char *src = enc[t & 1];
            char *dst = enc[(t + 1) & 1];
            size_t dl = (size_t)snprintf(dst, sizeof(enc[0]), "next=");
            for(size_t i = (t == 0)? 0 : 5; i < el; i++) {
                dl += (size_t)snprintf(dst + dl, sizeof(enc[0]) - dl, "%%%02X", (unsigned char)src[i]);
            }
            el = dl;
            size_t leaks = 0;
            for(size_t bfsize = 2; bfsize <= sizeof(enc_bf); bfsize++) {
                mask_secrets_inline_capped(enc_bf, bfsize, dst, el);
                if(strstr(enc_bf, needles[t]) && leaks++ == 0) {
                    printf("     cap %zu shows %s\n", bfsize, enc_bf);
                }
            }
            check_true(t == 0? "a URL password shaped like a literal, %-encoded, is masked at every cap" :
                "a URL password shaped like a literal, %-encoded twice, is masked at every cap", leaks == 0);
        }
    }

    /*
     *  An IP literal followed by a port at the cut of a capped line ("password=",
     *  300 x, the URL, 300 z) is kept, the port masked: in the bytes as
     *  written, and in the decoded view built when the window holds a %XX
     *  elsewhere, or in the zone (RFC 6874 "%25"). A literal whose own bytes
     *  are %-encoded, and an IPv4 quad with a leading zero, are not kept.
     */
    {
        struct {
            const char *head;
            const char *url;
            size_t bfsize;
            const char *out;
        } at_cut[] = {
            {"", " url=https://[2001:db8::1]:8080 ", 82,
             "password=******** url=https://[2001:db8::1]:********"},
            {"GET /a%20b ", " url=https://[2001:db8::1]:1600 ", 94,
             "GET /a%20b password=******** url=https://[2001:db8::1]:********"},
            {"", " url=https://[fe80::1%25eth0]:1600 ", 86,
             "password=******** url=https://[fe80::1%25eth0]:********"},
            {"", " url=https://[::01.2.3.4]:8080 ", 82,
             "password=******** url=https://[:********"},
            {"", " next=https%3A%2F%2F%5B2001%3Adb8%3A%3A1%5D%3A1600 ", 102,
             "password=******** next=https%3A%2F%2F%5B2001%3A********"},
            /*  The bound, in the bytes as WRITTEN (here the colons and the zone are
             *  %-encoded, so the raw bytes hold no ':' to mask from): from the '['
             *  to the cut, 118 kept, 119 not  */
            {"", " url=https://[2001%3Adb8%3A%3A1%25%65%74%68%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30]%3A16000 ", 185,
             "password=******** url=https://[2001%3Adb8%3A%3A1%25%65%74%68%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30]%3A********"},
            {"", " url=https://[2001%3Adb8%3A%3A1%25%65%74%68%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30%30]%3A16000 ", 186,
             "password=******** url=https://[2001%3A********"},
            /*  A '[' encoded twice is no raw '[' in the text as written: the map
             *  composes through both decodings  */
            {"", " url=https://%255B2001%253Adb8%253A%253A1%255D%253A1600 ", 108,
             "password=******** url=https://%255B2001%253A********"},
            /*  ... and a map of the first view only would point at a raw '[' of the
             *  text twenty bytes earlier (ten %XX before it), and grant it  */
            {"GET /a%20%20%20%20%20%20%20%20%20%20b ", "[xxxxxx url=https://%255B2001%253Adb8%253A%253A1%255D%253A1600 ", 153,
             "GET /a%20%20%20%20%20%20%20%20%20%20b password=******** url=https://%255B2001%253A********"},
        };
        for(size_t k = 0; k < sizeof(at_cut)/sizeof(at_cut[0]); k++) {
            char cut_url[1024];
            char cut_url_bf[256];
            size_t hl = (size_t)snprintf(cut_url, sizeof(cut_url), "%spassword=", at_cut[k].head);
            memset(cut_url + hl, 'x', 300);
            hl += 300;
            hl += (size_t)snprintf(cut_url + hl, sizeof(cut_url) - hl, "%s", at_cut[k].url);
            memset(cut_url + hl, 'z', 300);
            hl += 300;
            cut_url[hl] = 0;
            mask_secrets_inline_capped(cut_url_bf, at_cut[k].bfsize, cut_url, hl);
            check_str("an IP literal at the cut of a capped line", cut_url_bf, at_cut[k].out);
        }
    }

    /*
     *  A dump of a few MiB with a %XX is decoded with no map of where each
     *  byte was written: only the cut of a capped line reads it, and at 8
     *  bytes a byte its allocation failed and the whole dump was masked
     */
    {
        size_t big_len = 3*1024*1024;
        char *big = gbmem_malloc(big_len + 1);
        if(big) {
            memset(big, 'x', big_len);
            memcpy(big, "GET /a%20b ", 11);
            memcpy(big + big_len - 18, " password=hunter2 ", 18);
            big[big_len] = 0;
            size_t masked = mask_secrets_in_text(big, big_len);
            check_true("a dump of 3 MiB with a %XX masks its password, not all of it",
                masked < 100 && strncmp(big, "GET /a%20b xxx", 14) == 0 && !strstr(big, "hunter2"));
            GBMEM_FREE(big)
        }
    }

    json_t *kw = json_pack("{s:s, s:s, s:I, s:s, s:s, s:s%, s:s}",
        "note", "eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiJ4In0.jwtnote-hunter2",
        "headers", "Authorization: Bearer hdr-hunter2",
        "sim_pin", (json_int_t)4242,
        "db_pass", "dbpass-hunter2",
        "passive", "visible-passive",
        "nul", "a\0password=nul-hunter2", (size_t)22,
        "url", "https://cdn.public.example/x?access_token=urljson-hunter2"
    );
    json_t *shown = json_mask_secrets(kw);
    char *ss = json2uglystr(shown);
    check_true("a JWT under a plain key is masked", ss && !strstr(ss, "jwtnote-hunter2"));
    check_true("a Bearer token in a string is masked", ss && !strstr(ss, "hdr-hunter2"));
    check_true("sim_pin and db_pass are masked", ss && !strstr(ss, "4242") && !strstr(ss, "dbpass-hunter2"));
    check_true("passive is not a secret", ss && strstr(ss, "visible-passive"));
    check_true("a string with a NUL is masked past it", ss && !strstr(ss, "nul-hunter2"));
    check_true("a token in the query of a url is masked", ss && !strstr(ss, "urljson-hunter2") &&
        strstr(ss, "https://cdn.public.example/x?access_token=********"));
    GBMEM_FREE(ss)
    JSON_DECREF(shown)
    JSON_DECREF(kw)

    /*
     *  write-attr of a secret written with blanks: the parser refuses the
     *  rest of it as extra text, and repeated it -- `value` is no secret
     *  parameter (it answered 'horse battery')
     */
    gobj_set_global_trace("commands", TRUE);
    watch("horse", "write-attr");
    resp = gobj_command(gobj_yuno(),
        "write-attr gobj_name=secret-holder attribute=password value=correct horse battery",
        json_object(), gobj
    );
    check_int("a write-attr of a secret with blanks is refused",
        (int)kw_get_int(0, resp, "result", 0, 0), -1);
    check_true("its refusal shows no word of it",
        !strstr(kw_get_str(0, resp, "comment", "", 0), "horse"));
    JSON_DECREF(resp)
    unwatch("the commands trace of a write-attr of a secret with blanks");
    gobj_set_global_trace("commands", FALSE);
    resp = gobj_command(gobj_yuno(),
        "write-attr gobj_name=secret-holder attribute=note value=hello stray-word",
        json_object(), gobj
    );
    check_true("a write-attr of a plain attr shows its extra word",
        strstr(kw_get_str(0, resp, "comment", "", 0), "stray-word")? TRUE : FALSE);
    JSON_DECREF(resp)

    /*
     *  Out of session the first word of the line is logged, masked: it can
     *  be a secret itself
     */
    hgobj srv = gobj_create("ievent-out-of-session", C_IEVENT_SRV, 0, gobj);
    watch("oos-hunter2", "Not in session");
    resp = gobj_command(srv, "password=oos-hunter2 user=x", 0, gobj);
    JSON_DECREF(resp)
    unwatch("the first word of a line out of session (C_IEVENT_SRV)");

    hgobj cli = gobj_create("ievent-cli-out-of-session", C_IEVENT_CLI, 0, gobj);
    watch("oos2-hunter2", "Not in session");
    resp = gobj_command(cli, "token=oos2-hunter2 x=1", 0, gobj);
    JSON_DECREF(resp)
    unwatch("the first word of a line out of session (C_IEVENT_CLI)");

    /*
     *  And the name of a stats out of session: it was logged whole
     */
    watch("oos3-hunter2", "Not in session");
    resp = gobj_stats(srv, "password=oos3-hunter2 x=1", 0, gobj);
    JSON_DECREF(resp)
    unwatch("the stats asked out of session (C_IEVENT_SRV)");
    watch("oos4-hunter2", "Not in session");
    resp = gobj_stats(cli, "token=oos4-hunter2", 0, gobj);
    JSON_DECREF(resp)
    unwatch("the stats asked out of session (C_IEVENT_CLI)");

    /*
     *  Masked BEFORE they are capped: capped first, the token that the cap
     *  cut showed its first bytes, too short to be judged one.
     *  49 bytes, " Bearer ", and
     *  the cap at 64 leaves 7 of the token.
     */
    char line[256];
    snprintf(line, sizeof(line), "%s Bearer sr7tokenvalue12345", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    watch("sr7tok", "Not in session");
    resp = gobj_stats(srv, line, 0, gobj);
    JSON_DECREF(resp)
    unwatch("a token the cap cuts, in the stats asked out of session (C_IEVENT_SRV)");
    watch("sr7tok", "Not in session");
    resp = gobj_stats(cli, line, 0, gobj);
    JSON_DECREF(resp)
    unwatch("a token the cap cuts, in the stats asked out of session (C_IEVENT_CLI)");
    watch("eyJsr10", "Not in session");
    resp = gobj_command(srv,
        "eyJsr10AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA.eyJzdWIiOiJ4In0.c2ln x=1",
        0, gobj
    );
    JSON_DECREF(resp)
    unwatch("a JWT whose '.' is beyond the cap, as the first word out of session");
    gobj_destroy(cli);

    /*
     *  The log line of a peer's app (EV_REMOTE_LOG, in session): capped
     *  and masked, its kw not dumped
     */
    gobj_change_state(srv, ST_SESSION);
    size_t text_len = 4096;
    char *text = gbmem_malloc(text_len + 1);
    memset(text, 'x', text_len);
    text[text_len] = 0;
    memcpy(text, "ERROR password=rl-hunter2 ", 26);
    s_remote_log_longest = 0;
    watch("rl-hunter2", "Remote Error");
    gobj_send_event(srv, EV_REMOTE_LOG,
        json_pack("{s:s, s:s}", "msg", text, "token", "rl-hunter2-kw"),
        gobj
    );
    unwatch("the log line of a peer's app");
    check_true("the log line of a peer's app is capped", s_remote_log_longest > 0 &&
        s_remote_log_longest < 2048);
    memset(text, 'x', text_len);
    memcpy(text + 1023 - 8 - 6, " Bearer sr9tokenvalue12345", 26);  // the cap leaves 6 of the token
    watch("sr9to", "Remote Error");
    gobj_send_event(srv, EV_REMOTE_LOG, json_pack("{s:s}", "msg", text), gobj);
    unwatch("a token the cap cuts, in the log line of a peer's app");
    GBMEM_FREE(text)
    gobj_destroy(srv);

    /*
     *  The frame of an event that is not public: one WARNING, the name and
     *  the kw masked -- the lookup was silent, and its callers traced the
     *  kw raw
     */
    gbuffer_t *gbuf = gbuffer_create(1024, 1024);
    gbuffer_append_string(gbuf,
        "{\"event\": \"EV_NO_SUCH_EVENT_REV23\", \"kw\": {\"jwt\": \"ue-hunter2\","
        " \"auth\": {\"client_secret\": \"ue-hunter2\"}, \"note\": \"visible\"}}"
    );
    gobj_event_t iev_event = EV_TIMEOUT;
    watch("ue-hunter2", "Frame of an event that is not public");
    json_t *iev_kw = iev_create_from_gbuffer(gobj, &iev_event, gbuf, 0);
    unwatch("the frame of an event that is not public");
    check_true("an event that is not public is answered NULL", iev_event == NULL);
    check_true("and its kw", iev_kw? TRUE : FALSE);
    KW_DECREF(iev_kw)

    /*
     *  ... and its NAME is masked as its kw is: a password or a JWT sent as
     *  the name of the event showed
     */
    gbuf = gbuffer_create(1024, 1024);
    gbuffer_append_string(gbuf,
        "{\"event\": \"EV_X password=uename-hunter2\", \"kw\": {\"note\": \"visible\"}}"
    );
    watch("uename-hunter2", "Frame of an event that is not public");
    iev_kw = iev_create_from_gbuffer(gobj, &iev_event, gbuf, 0);
    unwatch("the name of an event that is not public");
    KW_DECREF(iev_kw)

    /*
     *  ... masked before it is capped: "EV_" and 47 bytes, "Bearer ", and
     *  the cap at 64 leaves 6 of the token
     */
    gbuf = gbuffer_create(1024, 1024);
    gbuffer_append_string(gbuf,
        "{\"event\": \"EV_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX Bearer sr8tokenvalue12345\","
        " \"kw\": {\"note\": \"visible\"}}"
    );
    watch("sr8tok", "Frame of an event that is not public");
    iev_kw = iev_create_from_gbuffer(gobj, &iev_event, gbuf, 0);
    unwatch("a token the cap cuts, in the name of an event that is not public");
    KW_DECREF(iev_kw)

    /*
     *  A frame that is not json: its WARNING says where it broke, not
     *  jansson's quote of the bytes there ("premature end of input near
     *  '\"sr12-hunter2'"): the dump of the frame is masked, and the error
     *  showed the secret
     */
    gbuf = gbuffer_create(1024, 1024);
    gbuffer_append_string(gbuf, "{\"event\": \"x\", \"kw\": {\"password\": \"sr12-hunter2");
    watch("sr12-hunter2", "frame is not json");
    iev_kw = iev_create_from_gbuffer(gobj, &iev_event, gbuf, 1);
    unwatch("the error of a frame that is not json");
    check_true("a frame that is not json is refused", iev_kw? FALSE : TRUE);
    KW_DECREF(iev_kw)

    /*
     *  A frame of json that is not an inter-event: its WARNING says so, with
     *  the frame masked
     */
    gbuf = gbuffer_create(1024, 1024);
    gbuffer_append_string(gbuf,
        "{\"note\": \"eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiJ4In0.frame-hunter2\", \"password\": \"frame-hunter2\"}"
    );
    watch("frame-hunter2", "Frame is not an inter-event");
    iev_kw = iev_create_from_gbuffer(gobj, &iev_event, gbuf, 1);
    unwatch("a frame of json that is not an inter-event");
    check_true("a frame that is not an inter-event is refused", iev_kw? FALSE : TRUE);

    /*
     *  The parser's refusals repeat no credential of the line:
     *  a JWT written as a key, an Authorization written as extra
     *  text, a json "password": x, a secret as the first word
     */
    struct {
        const char *line;
        const char *secret;
        const char *answer;     // a part of the answer that must be there
    } refusals[] = {
        {"help eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiJ4In0.keyjwt-hunter2=1", "keyjwt-hunter2", "has no option '********'"},
        {"help eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiJ4In0.quote-hunter2='x", "quote-hunter2", "value with no closing quote"},
        {"help Authorization: Basic dXNlcjpiYXNpYy1odW50ZXIy", "dXNlcjpiYXNpYy1odW50ZXIy", "'<...>'"},
        {"help \"password\": json-hunter2", "json-hunter2", "'<...>'"},
        {"password=first-hunter2 user=x", "first-hunter2", "command not available: 'password=********'"},
        {0, 0, 0}
    };
    for(int i=0; refusals[i].line; i++) {
        resp = gobj_command(gobj_yuno(), refusals[i].line, json_object(), gobj);
        const char *comment = kw_get_str(0, resp, "comment", "", 0);
        char what[128];
        snprintf(what, sizeof(what), "refused, without its secret (%d)", i);
        check_true(what, kw_get_int(0, resp, "result", 0, 0) < 0 &&
            !strstr(comment, refusals[i].secret) && strstr(comment, refusals[i].answer));
        if(strstr(comment, refusals[i].secret) || !strstr(comment, refusals[i].answer)) {
            printf("     the answer: %s\n", comment);
        }
        JSON_DECREF(resp)
    }

    /*
     *  A key with a NUL (C code can build one; a parser refuses it) is
     *  judged by all its bytes: it was judged up to the NUL and its value
     *  shown
     */
    json_t *jn_nul_key = json_object();
    json_object_setn_new(jn_nul_key, "a\0password", 10, json_string("nulkey-hunter2"));
    json_object_set_new(jn_nul_key, "note", json_string("visible-nulkey"));
    json_t *jn_nul_shown = json_mask_secrets(jn_nul_key);
    char *ss_nul = json2uglystr(jn_nul_shown);
    check_true("the value of a key with a NUL is masked", ss_nul && !strstr(ss_nul, "nulkey-hunter2"));
    check_true("the rest of that json shows", ss_nul && strstr(ss_nul, "visible-nulkey"));
    check_int("the masked copy keeps every key", (int)json_object_size(jn_nul_shown), 2);
    GBMEM_FREE(ss_nul)
    JSON_DECREF(jn_nul_shown)
    JSON_DECREF(jn_nul_key)
}

/***************************************************************************
 *  The persistent attrs file of a yuno run as root (__wrap_geteuid): whose
 *  file is trusted, by the chain closed from "/" down
 ***************************************************************************/
PRIVATE void check_root_trust_chain(hgobj holder, const char *path)
{
    struct stat st_file;

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

    /*
     *  A ".." does not carry the user of the directory it leaves: the link
     *  is test_secret_attrs.x/../test_secret_attrs.real, and .x is a closed
     *  directory of the intruder. The path does not go through it, so the
     *  intruder's file is refused. A first form carried the user across
     *  the "..": the intruder was trusted, and their file loaded
     */
    char x_dir[PATH_MAX];
    build_path(x_dir, sizeof(x_dir), "/tmp", "test_secret_attrs.x", NULL);
    mkdir(x_dir, 0755);
    stat(x_dir, &st_dir);
    fake_dirs[3] = (fake_dir_t){st_dir.st_ino, intruder, 0755};
    unlink(realm_dir);
    if(symlink("test_secret_attrs.x/../test_secret_attrs.real", realm_dir) < 0) {
        printf("FAIL cannot link the realm through a \"..\": %s\n", strerror(errno));
        s_result += -1;
    }
    write_file(path, "{\"password\": \"through-a-dotdot\"}", 0600);
    stat(path, &st_file);
    foreign_ino = st_file.st_ino;
    foreign_uid = intruder;
    gobj_write_str_attr(holder, "password", "mine-dotdot");
    fake_root = TRUE;
    gobj_load_persistent_attrs(holder, 0);
    fake_root = FALSE;
    check_str("run as root, a \"..\" does not trust the owner of the directory it leaves",
        gobj_read_str_attr(holder, "password"), "mine-dotdot"
    );

    /*
     *  A third user's directory met through a link ends the chain: /tmp is
     *  told the yuno user's here, the realm the link leads to the
     *  intruder's. Their file is refused; the yuno user stays the one
     *  trusted
     */
    unlink(realm_dir);
    make_link("test_secret_attrs.real", realm_dir);
    fake_dirs[0].uid = geteuid();
    fake_dirs[1].uid = intruder;
    gobj_write_str_attr(holder, "password", "mine-third");
    fake_root = TRUE;
    gobj_load_persistent_attrs(holder, 0);
    fake_root = FALSE;
    check_str("run as root, a third user's directory behind a link is not trusted",
        gobj_read_str_attr(holder, "password"), "mine-third"
    );
    fake_dirs[0].uid = 0;
    fake_dirs[1].uid = geteuid();
    foreign_ino = 0;

    /*
     *  A chain of 40 links (the kernel's own limit, which open() enforces
     *  first) is followed to its end
     */
    unlink(realm_dir);
    char link_path[PATH_MAX], link_target[NAME_MAX + 1];
    for(int i = 1; i < 40; i++) {
        char name[NAME_MAX + 1];
        snprintf(name, sizeof(name), "test_secret_attrs.l%d", i);
        build_path(link_path, sizeof(link_path), "/tmp", name, NULL);
        unlink(link_path);
        if(i < 39) {
            snprintf(link_target, sizeof(link_target), "test_secret_attrs.l%d", i + 1);
        } else {
            snprintf(link_target, sizeof(link_target), "test_secret_attrs.real");
        }
        make_link(link_target, link_path);
    }
    make_link("test_secret_attrs.l1", realm_dir);
    write_file(path, "{\"password\": \"through-40-links\"}", 0600);
    fake_root = TRUE;
    gobj_load_persistent_attrs(holder, 0);
    fake_root = FALSE;
    check_str("run as root, a realm behind 40 links of the closed chain is trusted",
        gobj_read_str_attr(holder, "password"), "through-40-links"
    );
    for(int i = 1; i < 40; i++) {
        char name[NAME_MAX + 1];
        snprintf(name, sizeof(name), "test_secret_attrs.l%d", i);
        build_path(link_path, sizeof(link_path), "/tmp", name, NULL);
        unlink(link_path);
    }
    rmdir(x_dir);

    unlink(realm_dir);
    rename(moved_realm, realm_dir);
    memset(fake_dirs, 0, sizeof(fake_dirs));
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
     *  A yuno as root is played by __wrap_geteuid(). The test itself runs
     *  as root only if root is in group yuneta (the entry point refuses any
     *  other user that is not yuneta): then the yuno's user here is root,
     *  whose files are trusted anyway, and the cases of the chain that
     *  expect a file to be loaded tell nothing
     */
    check_root_trust_chain(holder, path);

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
    check_json_parse_logs(gobj);
    check_secret_values(gobj);
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
PRIVATE sdata_desc_t pm_set_note_pos[] = {
SDATAPM (DTP_STRING,    "note",         SDF_REQUIRED,           0,      "Not a secret, positional"),
SDATA_END()
};

PRIVATE sdata_desc_t holder_command_table[] = {
/*-CMD---type-----------name----------------alias---items-------------------json_fn-------------description--*/
SDATACM (DTP_SCHEMA,    "set-password",     0,      pm_set_password,        cmd_set_password,   "Set the password"),
SDATACM (DTP_SCHEMA,    "set-password-pos", 0,      pm_set_password_pos,    cmd_set_password,   "Set the password, positional"),
SDATACM (DTP_SCHEMA,    "set-note-pos",     0,      pm_set_note_pos,        cmd_set_password,   "Set nothing, a positional plain value"),
SDATACM2 (DTP_SCHEMA,   "command-yuno",     SDF_WILD_CMD,   0,      pm_command_yuno,    cmd_command_yuno,   "Command to yuno, free keys"),
SDATA_END()
};

/***************************************************************
 *              GClass
 ***************************************************************/
PRIVATE sdata_desc_t driver_attrs_table[] = {
    SDATA_END()
};

/*
 *  A SDF_SECRET attr has a secret's name: gclass_create() refuses one that
 *  has not (tests/c/command_secret_positional), so a
 *  secret with a plain name -- this `vault_secret` was `vault` -- can no
 *  longer exist on Linux
 */
PRIVATE sdata_desc_t holder_attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------------default-description---------- */
SDATA (DTP_STRING,      "password",         SDF_WR|SDF_PERSIST|SDF_SECRET,"", "A secret"),
SDATA (DTP_STRING,      "note",             SDF_WR|SDF_PERSIST,       "",     "Not a secret"),
SDATA (DTP_INTEGER,     "pin",              SDF_WR|SDF_PERSIST|SDF_SECRET,"0","A secret that is a number"),
SDATA (DTP_STRING,      "config_only",      SDF_PERSIST,              "",     "Persistent, set by the config"),
SDATA (DTP_JSON,        "vault_secret",     SDF_WR|SDF_SECRET,        0,      "A secret json"),
SDATA (DTP_DICT,        "settings",         SDF_WR,                   "{}",   "A dict"),
SDATA (DTP_LIST,        "items",            SDF_WR,                   "[]",   "A list"),
SDATA (DTP_POINTER,     "handle",           SDF_WR,                   0,      "A pointer"),
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
