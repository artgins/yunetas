/****************************************************************************
 *          test_register_refused_gclass.c
 *
 *          A gclass of the yuno that is refused stops the yuno before it
 *          runs, as a refused gclass of the core does: yuneta_entry_point()
 *          logs an ERROR and exits with -1 when register_yuno_and_more()
 *          answers a negative number. Up to 7.26.9 that answer was added to
 *          the exit code and the yuno RAN, and its services failed later
 *          with "gclass NOT FOUND".
 *
 *          The test forks. The child is the yuno: its
 *          register_yuno_and_more() registers a driver gclass, and then one
 *          with a SDF_SECRET attribute named `vault`, which gclass_create()
 *          refuses. Had the yuno run, the driver would have played and
 *          stopped it, and the child would have returned YUNO_RAN. The
 *          parent checks that the child exited with 255, the exit(-1) of
 *          the entry point.
 *
 *          A second child calls yunetas_register_c_core() from its
 *          register_yuno_and_more(), which the entry point has already
 *          called: refused, and the yuno stops too. The parent reads the
 *          child's stdout for the ERROR that says why: up to 7.26.9 the
 *          second call answered -1 with no log at all, and the only line
 *          was the entry point's, naming no cause.
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
#include <sys/types.h>
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
#define OUTPUT_MAX      (256*1024)  // what the parent keeps of a child's stdout

/***************************************************************
 *              Data
 ***************************************************************/
GOBJ_DEFINE_GCLASS(C_TEST_REGISTER_DRIVER);
GOBJ_DEFINE_GCLASS(C_TEST_REGISTER_REFUSED);

PRIVATE BOOL yuno_played = FALSE;
PRIVATE BOOL register_core_again = FALSE;   // the second child
PRIVATE char child_output[OUTPUT_MAX];      // the stdout of a child

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
    if(register_core_again) {
        result += yunetas_register_c_core();
    } else {
        result += register_gclass(C_TEST_REGISTER_REFUSED, refused_attrs_table);
    }
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
 *  Fork the yuno, with its stdout into `output` (NUL-terminated, capped).
 *  Return its wait status, or -1.
 ***************************************************************************/
PRIVATE int run_child(int argc, char *argv[], BOOL core_again, char *output, size_t output_size)
{
    int fds[2];
    if(pipe(fds) < 0) {
        printf("FAIL cannot make a pipe: %s\n", strerror(errno));
        return -1;
    }
    fflush(stdout);
    pid_t pid = fork();
    if(pid < 0) {
        printf("FAIL cannot fork: %s\n", strerror(errno));
        close(fds[0]);
        close(fds[1]);
        return -1;
    }
    if(pid == 0) {
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        close(fds[1]);
        register_core_again = core_again;
        exit(run_the_yuno(argc, argv));
    }

    close(fds[1]);
    size_t len = 0;
    char bf[4096];
    ssize_t n;
    while((n = read(fds[0], bf, sizeof(bf))) > 0) {
        size_t room = output_size - 1 - len;
        size_t take = (size_t)n < room? (size_t)n : room;
        memcpy(output + len, bf, take);
        len += take;
    }
    output[len] = 0;
    close(fds[0]);

    int status = 0;
    if(waitpid(pid, &status, 0) < 0) {
        printf("FAIL cannot wait for the yuno: %s\n", strerror(errno));
        return -1;
    }
    return status;
}

/***************************************************************************
 *  The child must have exited with 255, the exit(-1) of the entry point
 ***************************************************************************/
PRIVATE int check_exit(int status, const char *what)
{
    if(status < 0) {
        return -1;  // Error already printed
    }
    if(WIFEXITED(status) && WEXITSTATUS(status) == 255) {
        printf("ok   %s exits with -1 before it runs\n", what);
        return 0;
    }
    if(WIFEXITED(status)) {
        printf("FAIL %s exited with %d%s\n",
            what,
            WEXITSTATUS(status),
            WEXITSTATUS(status) == YUNO_RAN? ": it RAN" : ""
        );
    } else {
        printf("FAIL %s ended by the signal %d\n",
            what,
            WIFSIGNALED(status)? WTERMSIG(status) : -1
        );
    }
    return -1;
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    int result = 0;

    int status = run_child(argc, argv, FALSE, child_output, sizeof(child_output));
    fputs(child_output, stdout);
    result += check_exit(status, "a yuno whose gclass is refused");

    status = run_child(argc, argv, TRUE, child_output, sizeof(child_output));
    fputs(child_output, stdout);
    result += check_exit(status, "a yuno that registers the core gclasses again");
    if(strstr(child_output, "yunetas_register_c_core() called twice")) {
        printf("ok   the second registration of the core gclasses says why it is refused\n");
    } else {
        printf("FAIL the second registration of the core gclasses was refused with no log of why\n");
        result = -1;
    }

    printf("\n%s: %s\n", APP, result == 0? "PASS" : "FAIL");
    return result;
}
