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
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <yunetas.h>

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

GOBJ_DEFINE_GCLASS(C_TEST_SECRET_DRIVER);
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

PRIVATE BOOL is_regular_not_link(const char *path)
{
    struct stat st;
    if(lstat(path, &st) < 0) {
        return FALSE;
    }
    return S_ISREG(st.st_mode)? TRUE : FALSE;
}

/*
 *  The "<file>.XXXXXX" a save writes before its rename()
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
        if(strncmp(de->d_name, base, base_len)==0 && de->d_name[base_len] == '.') {
            count++;
        }
    }
    closedir(d);
    return count;
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
     *  A save that fails: write-attr says so, the old file stays as it was
     */
    gobj_write_str_attr(holder, "password", "kept-on-disk");
    gobj_save_persistent_attrs(holder, json_string("password"));
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", path);
    *strrchr(dir, '/') = 0;
    struct stat st_dir;
    stat(dir, &st_dir);
    chmod(dir, 0555);
    json_t *resp = gobj_command(gobj_yuno(),
        "write-attr gobj_name=secret-holder attribute=note value=unsaved", json_object(), holder
    );
    chmod(dir, st_dir.st_mode & 07777);
    check_int("write-attr answers a save that failed", (int)kw_get_int(0, resp, "result", 0, 0), -1);
    JSON_DECREF(resp)
    check_str("the attr is written anyway", gobj_read_str_attr(holder, "note"), "unsaved");
    check_true("the old file is left as it was", file_contains(path, "kept-on-disk"));
    check_int("a failed save leaves no temporary file", count_temp_files(path), 0);

    resp = gobj_command(gobj_yuno(),
        "write-attr gobj_name=secret-holder attribute=note value=saved", json_object(), holder
    );
    check_int("write-attr answers a save that worked", (int)kw_get_int(0, resp, "result", -1, 0), 0);
    JSON_DECREF(resp)
    check_true("and it is on disk", file_contains(path, "saved"));

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
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    check_http_cookie(gobj);
    check_view_config(gobj);
    check_create_delete2_trace();
    check_command_trace();
    check_wild_command_trace();
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
SDATA (DTP_STRING,      "password",         SDF_PERSIST|SDF_SECRET,   "",     "A secret"),
SDATA (DTP_STRING,      "note",             SDF_PERSIST,              "",     "Not a secret"),
SDATA (DTP_INTEGER,     "pin",              SDF_PERSIST|SDF_SECRET,   "0",    "A secret that is a number"),
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
        {0, 0}
    };

    ev_action_t st_idle[] = {
        {EV_TIMEOUT,    ac_timeout,     0},
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
