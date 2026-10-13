/****************************************************************************
 *          test_register_refused_gclass.c
 *
 *          A gclass of the yuno that is refused stops the yuno before it
 *          runs, as a refused gclass of the core does: yuneta_entry_point()
 *          logs an ERROR and exits with -1 when register_yuno_and_more()
 *          answers a negative number. Up to 7.26.9 that answer was added to
 *          the exit code and the yuno RAN, and its services failed later
 *          with "gclass NOT FOUND" (REV-CLOUD V24.4).
 *
 *          The test forks. The child is the yuno: its
 *          register_yuno_and_more() registers a driver gclass, and then one
 *          with a SDF_SECRET attribute named `vault`, which gclass_create()
 *          refuses. Had the yuno run, the driver would have played and
 *          stopped it, and the child would have returned YUNO_RAN. The
 *          parent checks that the child exited with 255, the exit(-1) of
 *          the entry point.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <yunetas.h>

#define APP             "test_register_refused_gclass"
#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DOC         "A refused gclass of the yuno stops the yuno"
#define APP_DATETIME    ""

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           0
#define MEM_MAX_BLOCK           0
#define MEM_SUPERBLOCK          0
#define MEM_MAX_SYSTEM_MEMORY   0

#define YUNO_RAN        7       // what the child answers when its yuno ran
#define CHILD_TIMEOUT   20      // seconds: the yuno that runs stops itself at once

/***************************************************************
 *              Data
 ***************************************************************/
GOBJ_DEFINE_GCLASS(C_TEST_REGISTER_DRIVER);
GOBJ_DEFINE_GCLASS(C_TEST_REGISTER_REFUSED);

PRIVATE BOOL yuno_played = FALSE;

/***************************************************************
 *              Config (single quotes -> double at runtime)
 ***************************************************************/
PRIVATE char fixed_config[]= "\
{                                                                   \n\
    'yuno': {                                                       \n\
        'yuno_role': 'test_register_refused_gclass',                \n\
        'tags': ['test', 'yunetas']                                 \n\
    }                                                               \n\
}                                                                   \n\
";
PRIVATE char variable_config[]= "\
{                                                                   \n\
    'environment': {                                                \n\
        'work_dir': '/tmp',                                         \n\
        'console_log_handlers': {},                                 \n\
        'daemon_log_handlers': {}                                   \n\
    },                                                              \n\
    'yuno': {                                                       \n\
        'autoplay': true,                                           \n\
        'required_services': [],                                    \n\
        'public_services': [],                                      \n\
        'service_descriptor': {},                                   \n\
        'realm_owner': 'test',                                      \n\
        'realm_id':    'test',                                      \n\
        'trace_levels': {}                                          \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'register-driver',                              \n\
            'gclass': 'C_TEST_REGISTER_DRIVER',                     \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': true                                        \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

/***************************************************************
 *              Framework Methods
 ***************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    yuno_played = TRUE;
    set_yuno_must_die();
    return 0;
}

/***************************************************************
 *              GClass
 ***************************************************************/
PRIVATE sdata_desc_t driver_attrs_table[] = {
    SDATA_END()
};

/*
 *  A SDF_SECRET attribute with a name that is no secret's: gclass_create()
 *  refuses the gclass (see test_command_secret_positional)
 */
PRIVATE sdata_desc_t refused_attrs_table[] = {
/*-ATTR-type------------name--------flag----------------default-description--*/
SDATA (DTP_STRING,      "vault",    SDF_WR|SDF_SECRET,  "",     "A secret with a plain name"),
SDATA_END()
};

PRIVATE int register_gclass(gclass_name_t gclass_name, sdata_desc_t *attrs_table)
{
    /*  Not static: the event and state names are runtime pointers
     *  (GOBJ_DEFINE_EVENT), so they cannot initialize a static table.  */
    static const GMETHODS gmt = {
        .mt_play = mt_play,
    };

    event_type_t event_types[] = {
        {0, 0}
    };

    ev_action_t st_idle[] = {
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE,   st_idle},
        {0, 0}
    };

    hgclass gc = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,                          // lmt
        attrs_table,
        0,                          // priv_size
        0,                          // authz_table
        0,                          // command_table
        0,                          // s_user_trace_level
        0                           // gclass_flag
    );
    return gc ? 0 : -1;     // Error already logged
}

PRIVATE int register_yuno_and_more(void)
{
    int result = 0;
    result += register_gclass(C_TEST_REGISTER_DRIVER, driver_attrs_table);
    result += register_gclass(C_TEST_REGISTER_REFUSED, refused_attrs_table);
    return result;
}

/***************************************************************************
 *  The child: the yuno
 ***************************************************************************/
PRIVATE int run_the_yuno(int argc, char *argv[])
{
    glog_init();
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    helper_quote2doublequote(fixed_config);
    helper_quote2doublequote(variable_config);

    yuneta_setup(
        NULL,                   // persistent_attrs
        NULL,                   // command_parser
        NULL,                   // stats_parser
        NULL,                   // authz_checker
        NULL,                   // authentication_parser
        MEM_MAX_BLOCK,
        MEM_MAX_SYSTEM_MEMORY,
        USE_OWN_SYSTEM_MEMORY,
        MEM_MIN_BLOCK,
        MEM_SUPERBLOCK
    );

    alarm(CHILD_TIMEOUT);   // the parent sees a signal, never a hang
    yuneta_entry_point(
        argc, argv,
        APP, APP_VERSION, APP_SUPPORT, APP_DOC, APP_DATETIME,
        fixed_config,
        variable_config,
        register_yuno_and_more,
        NULL                    // cleaning
    );
    return yuno_played? YUNO_RAN : YUNO_RAN + 1;
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    fflush(stdout);
    pid_t pid = fork();
    if(pid < 0) {
        printf("FAIL cannot fork: %s\n", strerror(errno));
        return -1;
    }
    if(pid == 0) {
        exit(run_the_yuno(argc, argv));
    }

    int status = 0;
    if(waitpid(pid, &status, 0) < 0) {
        printf("FAIL cannot wait for the yuno: %s\n", strerror(errno));
        return -1;
    }
    int result = 0;
    if(WIFEXITED(status) && WEXITSTATUS(status) == 255) {
        printf("ok   a yuno whose gclass is refused exits with -1 before it runs\n");
    } else if(WIFEXITED(status)) {
        printf("FAIL a yuno whose gclass is refused exited with %d%s\n",
            WEXITSTATUS(status),
            WEXITSTATUS(status) == YUNO_RAN? ": it RAN" : ""
        );
        result = -1;
    } else {
        printf("FAIL a yuno whose gclass is refused ended by the signal %d\n",
            WIFSIGNALED(status)? WTERMSIG(status) : -1
        );
        result = -1;
    }

    printf("\n%s: %s\n", APP, result == 0? "PASS" : "FAIL");
    return result;
}
