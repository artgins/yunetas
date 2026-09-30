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
 *               the kw -- and the kw the command parser expands.
 *
 *          And the persistent-attrs file (dbsimple.c):
 *
 *            5. a file written before 7.25.19 (0664) is made 0600 when it
 *               is LOADED, not only at its next save -- a password set once
 *               is never saved again.
 *            6. a symlink planted in place of the file (the data dirs are
 *               02775) is neither read nor written through.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
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
        'secret-other.password': 'glob-hunter2'                     \n\
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
                'note': 'visible-note'                              \n\
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
    if(strstr(bf, "cmd-hunter2") || strstr(bf, "kw-hunter2") || strstr(bf, "pos-hunter2")) {
        s_command_secret_seen++;
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
    GBMEM_FREE(s)
    JSON_DECREF(resp)

    check_str("the config secret still reaches the attr",
        gobj_read_str_attr(gobj_find_service("secret-holder", TRUE), "password"),
        "cfg-hunter2"
    );
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
                "password", "trace-hunter2"
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
    resp = gobj_command(holder, "set-password-pos pos-hunter2", 0, holder);
    JSON_DECREF(resp)
    s_capturing = FALSE;
    gobj_set_global_trace("ev_kw", FALSE);
    gobj_set_global_trace("commands", FALSE);

    check_true("the commands trace printed the command", s_command_seen > 0);
    check_true("the commands trace printed the command kw", s_command_kw_seen > 0);
    check_true("the parser printed the expanded kw", s_expanded_kw_seen > 0);
    check_int("the commands trace hides the secret parameter", s_command_secret_seen, 0);
    check_str("the secret parameter still reaches the command",
        gobj_read_str_attr(holder, "password"), "pos-hunter2"
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
    check_true("a save through a symlink fails", ret < 0);
    check_true("the symlink target is not written",
        !file_contains(planted, "through-the-link")
    );
    check_int("the symlink target keeps its mode", file_mode(planted), 0644);

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
    gobj_write_str_attr(gobj, "password", kw_get_str(gobj, kw, "password", "", 0));
    KW_DECREF(kw)
    return build_command_response(gobj, 0, 0, 0, 0);
}

/*-PM----type-----------name------------flag--------------------default-description--*/
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
