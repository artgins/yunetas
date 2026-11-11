/****************************************************************************
 *          TEST_EXIT_ON_ERROR.C
 *
 *          exitOnError of C_TCP_S, C_UDP_S and C_QIOGATE: by default a
 *          listener or a queue that fails at its start ENDS the yuno, with
 *          the codes of 7.30.17; 0 (the agent's choice) degrades it instead
 *          (tested in test_c_tcp_s_addr_in_use, c_tcps/test14,
 *          c_udp_s_self_stop and c_qiogate_stats). 7.30.18 degraded every
 *          yuno and ignored exitOnError: a yuno whose role is its listener or
 *          its queue ran without it (red: each case below is killed by its
 *          alarm, it never exits).
 *
 *          Each case is a yuno of its own, in a child process whose output
 *          the parent reads: the exit code and the line said before the exit
 *          are checked.
 *
 *              1. C_TCP_S, its port held past listen_retry_msec (0): ERROR
 *                 "yev_create_accept_event() FAILED", WARNING "Exiting",
 *                 code 255 (-1, the default: the watcher launches it again)
 *              2. C_TCP_S, a TLS url whose certificate is not there: code 0
 *                 (not launched again), "TLS server without its TLS
 *                 context: exiting", as up to 7.30.17
 *              3. C_TCP_S, a url of port 0, exitOnError 7: code 7
 *              4. C_UDP_S, its port held: code 0, "yev_create_accept_event()
 *                 FAILED" (exitOnError TRUE, the default)
 *              5. C_QIOGATE, its queue under a regular file: code 255,
 *                 "Queue cannot be opened: exiting", at the start (up to
 *                 7.30.17 the yuno went on and aborted at its first message)
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <yunetas.h>

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_exit_on_error"
#define APP_DOC         "exitOnError of C_TCP_S, C_UDP_S and C_QIOGATE"

#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DATETIME    __DATE__ " " __TIME__

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           0
#define MEM_MAX_BLOCK           0
#define MEM_SUPERBLOCK          0
#define MEM_MAX_SYSTEM_MEMORY   0

#define TCP_HELD_PORT   7784
#define TLS_PORT        7792
#define UDP_HELD_PORT   34301
#define QUEUES_PATH     "/tmp/test_exit_on_error"

#define NOT_EXITED      3       // the code of a yuno that reached its end: it did not exit

/***************************************************************************
 *                      Configs
 ***************************************************************************/
PRIVATE char fixed_config[]= "\
{                                                                   \n\
    'yuno': {                                                       \n\
        'yuno_role': '"APP_NAME"',                                  \n\
        'tags': ['test', 'yunetas']                                 \n\
    }                                                               \n\
}                                                                   \n\
";

#define VARIABLE_CONFIG_HEAD "\
{                                                                   \n\
    'environment': {                                                \n\
        'console_log_handlers': {                                   \n\
        },                                                          \n\
        'daemon_log_handlers': {                                    \n\
        }                                                           \n\
    },                                                              \n\
    'yuno': {                                                       \n\
        'autoplay': true,                                           \n\
        'timeout_periodic': 50,                                     \n\
        'required_services': [],                                    \n\
        'public_services': [],                                      \n\
        'service_descriptor': {                                     \n\
        },                                                          \n\
        'trace_levels': {                                           \n\
        }                                                           \n\
    },                                                              \n\
    'global': {                                                     \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'checked',                                      \n\
            'autostart': true,                                      \n\
            'autoplay': false,                                      \n\
"
#define VARIABLE_CONFIG_TAIL "\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
"

PRIVATE char config_tcp_held[] = VARIABLE_CONFIG_HEAD "\
            'gclass': 'C_TCP_S',                                    \n\
            'kw': {                                                 \n\
                'url': 'tcp://127.0.0.1:7784',                      \n\
                'listen_retry_msec': 0                              \n\
            }                                                       \n\
" VARIABLE_CONFIG_TAIL;

PRIVATE char config_tls_no_cert[] = VARIABLE_CONFIG_HEAD "\
            'gclass': 'C_TCP_S',                                    \n\
            'kw': {                                                 \n\
                'url': 'tcps://127.0.0.1:7792',                     \n\
                'crypto': {                                         \n\
                    'ssl_certificate': '" QUEUES_PATH "/none.crt',  \n\
                    'ssl_certificate_key': '" QUEUES_PATH "/none.key' \n\
                }                                                   \n\
            }                                                       \n\
" VARIABLE_CONFIG_TAIL;

PRIVATE char config_tcp_port0[] = VARIABLE_CONFIG_HEAD "\
            'gclass': 'C_TCP_S',                                    \n\
            'kw': {                                                 \n\
                'url': 'tcp://127.0.0.1:0',                         \n\
                'exitOnError': 7                                    \n\
            }                                                       \n\
" VARIABLE_CONFIG_TAIL;

PRIVATE char config_udp_held[] = VARIABLE_CONFIG_HEAD "\
            'gclass': 'C_UDP_S',                                    \n\
            'kw': {                                                 \n\
                'url': 'udp://127.0.0.1:34301'                      \n\
            }                                                       \n\
" VARIABLE_CONFIG_TAIL;

PRIVATE char config_queue_blocked[] = VARIABLE_CONFIG_HEAD "\
            'gclass': 'C_QIOGATE',                                  \n\
            'kw': {                                                 \n\
                'tranger_path': '" QUEUES_PATH "/blocked/q',        \n\
                'tranger_database': 'q',                            \n\
                'topic_name': 'msgs',                               \n\
                'tkey': 'tm',                                       \n\
                'system_flag': 'sf_string_key'                      \n\
            }                                                       \n\
" VARIABLE_CONFIG_TAIL;

typedef struct {
    const char *what;
    char *variable_config;
    int expected_code;
    const char *expected_line;
} exit_case_t;

PRIVATE exit_case_t cases[] = {
{"1. C_TCP_S, its port held, exitOnError -1 (default)", config_tcp_held,      255, "Exiting"},
{"2. C_TCP_S, a TLS url without its certificate",      config_tls_no_cert,   0,   "TLS server without its TLS context: exiting"},
{"3. C_TCP_S, a url of port 0, exitOnError 7",          config_tcp_port0,     7,   "Cannot Listen on port 0"},
{"4. C_UDP_S, its port held, exitOnError TRUE (default)", config_udp_held,    0,   "exiting with code 0 (exitOnError)"},
{"5. C_QIOGATE, its queue cannot be opened",            config_queue_blocked, 255, "Queue cannot be opened: exiting"},
};

/***************************************************************************
 *  The yuno of a case, in the child: it must exit before its end
 ***************************************************************************/
PRIVATE int register_nothing(void)
{
    return 0;
}

PRIVATE void run_yuno(int argc, char *argv[], char *variable_config)
{
    glog_init();
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    set_auto_kill_time(5);  // a yuno that does not exit is killed: SIGALRM

    helper_quote2doublequote(fixed_config);
    helper_quote2doublequote(variable_config);
    yuneta_setup(
        NULL,       // persistent_attrs
        NULL,       // command_parser
        NULL,       // stats_parser
        NULL,       // authz_checker
        NULL,       // authentication_parser
        MEM_MAX_BLOCK,
        MEM_MAX_SYSTEM_MEMORY,
        USE_OWN_SYSTEM_MEMORY,
        MEM_MIN_BLOCK,
        MEM_SUPERBLOCK
    );
    yuneta_entry_point(
        argc, argv,
        APP_NAME, APP_VERSION, APP_SUPPORT, APP_DOC, APP_DATETIME,
        fixed_config,
        variable_config,
        register_nothing,
        NULL
    );
    fflush(stdout);
    _exit(NOT_EXITED);
}

/***************************************************************************
 *  A socket that holds a port, of `type` (SOCK_STREAM listens)
 ***************************************************************************/
PRIVATE int hold_port(int type, int port)
{
    int fd = socket(AF_INET, type, 0);
    if(fd < 0) {
        return -1;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if(bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    if(type == SOCK_STREAM && listen(fd, 1) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

/***************************************************************************
 *  One case: its yuno in a child, its output read here
 ***************************************************************************/
PRIVATE int check_case(int argc, char *argv[], exit_case_t *c)
{
    int pipefd[2];
    if(pipe(pipefd) < 0) {
        printf("FAIL %s: pipe() %s\n", c->what, strerror(errno));
        return -1;
    }
    fflush(stdout);
    pid_t pid = fork();
    if(pid < 0) {
        printf("FAIL %s: fork() %s\n", c->what, strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }
    if(pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        run_yuno(argc, argv, c->variable_config);
    }
    close(pipefd[1]);

    char output[64*1024];
    size_t len = 0;
    ssize_t n;
    while((n = read(pipefd[0], output + len, sizeof(output) - 1 - len)) > 0) {
        len += (size_t)n;
        if(len >= sizeof(output) - 1) {
            break;
        }
    }
    output[len] = 0;
    close(pipefd[0]);

    int status = 0;
    waitpid(pid, &status, 0);

    int ret = 0;
    if(!WIFEXITED(status)) {
        printf("FAIL %s: the yuno did not exit (killed by signal %d)\n",
            c->what, WIFSIGNALED(status)? WTERMSIG(status) : 0
        );
        ret = -1;
    } else if(WEXITSTATUS(status) != c->expected_code) {
        printf("FAIL %s: exit code %d, expected %d%s\n",
            c->what, WEXITSTATUS(status), c->expected_code,
            WEXITSTATUS(status) == NOT_EXITED? " (it did not exit, it reached its end)" : ""
        );
        ret = -1;
    }
    if(!strstr(output, c->expected_line)) {
        printf("FAIL %s: no line says \"%s\"\n", c->what, c->expected_line);
        ret = -1;
    }
    if(ret < 0) {
        printf("---- output of the yuno ----\n%s\n----\n", output);
    } else {
        printf("ok   %s: exit code %d, \"%s\"\n", c->what, c->expected_code, c->expected_line);
    }
    return ret;
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    int result = 0;

    /*
     *  The ports held, inherited by the children; the queue's path is
     *  under a regular file
     */
    int tcp_fd = hold_port(SOCK_STREAM, TCP_HELD_PORT);
    int udp_fd = hold_port(SOCK_DGRAM, UDP_HELD_PORT);
    if(tcp_fd < 0 || udp_fd < 0) {
        printf("FAIL the ports %d (tcp) and %d (udp) cannot be held\n", TCP_HELD_PORT, UDP_HELD_PORT);
        result = -1;
    }
    rmrdir(QUEUES_PATH);
    mkrdir(QUEUES_PATH, 02770);
    int fd = open(QUEUES_PATH "/blocked", O_CREAT|O_WRONLY, 0660);
    if(fd < 0) {
        printf("FAIL the file that blocks the path of the queue\n");
        result = -1;
    } else {
        close(fd);
    }

    if(result == 0) {
        for(size_t i = 0; i < ARRAY_SIZE(cases); i++) {
            result += check_case(argc, argv, &cases[i]);
        }
    }

    if(tcp_fd >= 0) {
        close(tcp_fd);
    }
    if(udp_fd >= 0) {
        close(udp_fd);
    }
    rmrdir(QUEUES_PATH);

    printf("\n%s: %s\n", APP_NAME, result == 0? "PASS" : "FAIL");
    return result == 0? 0 : -1;
}
