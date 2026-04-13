/****************************************************************************
 *          MAIN.C  —  c_llhttp_parser
 *
 *          Sanity / regression tests for the vendored llhttp parser
 *          (kernel/c/gobj-c/src/llhttp*) and the gobj wrapper around it
 *          (kernel/c/root-linux/src/ghttp_parser.{h,c}).
 *
 *          Why this test exists
 *          --------------------
 *          The c_auth_bff persistent-IdP-connection refactor (intended fix
 *          for the per-request outbound HTTP_CL leak) hit a parser bug:
 *          a long-lived C_PROT_HTTP_CL whose llhttp_t parser sat idle for
 *          ~1 s between init and first execute returned HPE_INVALID_CONSTANT
 *          on the very first valid HTTP/1.1 response.  The same code path
 *          on a per-request HTTP_CL (legacy flow) worked fine.  Yuneta
 *          swapped http-parser → llhttp years ago and the integration has
 *          been a recurring source of trouble.
 *
 *          What this test covers
 *          ---------------------
 *          Two layers, both expected to PASS today:
 *            (a) bare llhttp:        init -> [sleep N] -> execute(valid resp)
 *                                    init -> reset -> [sleep N] -> execute
 *                                    pipelined responses on one parser
 *            (b) ghttp_parser:       create -> [sleep N] -> received(valid)
 *                                    create -> reset  -> [sleep N] -> received
 *                                    pipelined responses through the wrapper
 *
 *          Reproducing the production bug requires the full
 *          c_prot_http_cl + io_uring + c_auth_bff context — outside this
 *          isolated test.  When the bug is reproduced & root-caused, add
 *          a regression case here.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <locale.h>

#include <yunetas.h>


#define TEST_OK   0
#define TEST_FAIL 1

PRIVATE int s_failures = 0;

#define EXPECT(cond, msg) do { \
    if(!(cond)) { \
        fprintf(stderr, "  ✗ FAIL: %s  (at %s:%d)\n", msg, __FILE__, __LINE__); \
        s_failures++; \
    } else { \
        fprintf(stderr, "  ✓ ok:   %s\n", msg); \
    } \
} while(0)


/****************************************************************
 *      Sample HTTP/1.1 responses (what mock-KC and real KC send)
 ****************************************************************/
PRIVATE const char *RESP_204 =
    "HTTP/1.1 204 No Content\r\n"
    "Content-Type: application/json; charset=utf-8\r\n"
    "Content-Length: 0\r\n"
    "\r\n";

PRIVATE const char *RESP_200 =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json; charset=utf-8\r\n"
    "Content-Length: 27\r\n"
    "\r\n"
    "{\"access_token\":\"abc-1234\"}";


/****************************************************************
 *      (a) Bare llhttp tests — no gobj wrapper involved
 ****************************************************************/
PRIVATE int s_llhttp_msgs = 0;
PRIVATE int llhttp_on_msg_complete(llhttp_t *parser) { (void)parser; s_llhttp_msgs++; return 0; }


PRIVATE void llhttp_init_with_lenient(llhttp_t *parser, llhttp_settings_t *settings) {
    llhttp_init(parser, HTTP_RESPONSE, settings);
    llhttp_set_lenient_headers(parser, 1);
    llhttp_set_lenient_data_after_close(parser, 1);
}

PRIVATE void test_bare_llhttp_immediate(void) {
    fprintf(stderr, "\n== bare llhttp: init + immediate execute ==\n");

    llhttp_settings_t settings;
    llhttp_settings_init(&settings);
    settings.on_message_complete = llhttp_on_msg_complete;

    llhttp_t parser;
    llhttp_init_with_lenient(&parser, &settings);

    s_llhttp_msgs = 0;
    llhttp_errno_t err = llhttp_execute(&parser, RESP_204, strlen(RESP_204));

    EXPECT(err == HPE_OK, "execute returns HPE_OK");
    EXPECT(s_llhttp_msgs == 1, "exactly 1 on_message_complete fired");
}

PRIVATE void test_bare_llhttp_idle(int seconds) {
    fprintf(stderr, "\n== bare llhttp: init + sleep %d s + execute ==\n", seconds);

    llhttp_settings_t settings;
    llhttp_settings_init(&settings);
    settings.on_message_complete = llhttp_on_msg_complete;

    llhttp_t parser;
    llhttp_init_with_lenient(&parser, &settings);

    sleep(seconds);

    s_llhttp_msgs = 0;
    llhttp_errno_t err = llhttp_execute(&parser, RESP_204, strlen(RESP_204));

    EXPECT(err == HPE_OK, "execute returns HPE_OK after idle");
    EXPECT(s_llhttp_msgs == 1, "exactly 1 on_message_complete fired");
}

PRIVATE void test_bare_llhttp_init_then_reinit_then_execute(void) {
    fprintf(stderr, "\n== bare llhttp: init + reinit + execute (mirrors connection reuse) ==\n");

    llhttp_settings_t settings;
    llhttp_settings_init(&settings);
    settings.on_message_complete = llhttp_on_msg_complete;

    llhttp_t parser;
    llhttp_init_with_lenient(&parser, &settings);
    /* Second init mirrors the destroy+create cycle done on every new
     * connection (ac_connected) now that ghttp_parser_reset is gone. */
    llhttp_init_with_lenient(&parser, &settings);

    s_llhttp_msgs = 0;
    llhttp_errno_t err = llhttp_execute(&parser, RESP_204, strlen(RESP_204));

    EXPECT(err == HPE_OK, "execute returns HPE_OK after re-init");
    EXPECT(s_llhttp_msgs == 1, "exactly 1 on_message_complete fired");
}

PRIVATE void test_bare_llhttp_pipelined(void) {
    fprintf(stderr, "\n== bare llhttp: 3 pipelined responses on one parser ==\n");

    llhttp_settings_t settings;
    llhttp_settings_init(&settings);
    settings.on_message_complete = llhttp_on_msg_complete;

    llhttp_t parser;
    llhttp_init_with_lenient(&parser, &settings);

    /* Build the buffer: three responses back to back, as a keep-alive
     * connection would deliver them. */
    char buf[4096];
    int n = snprintf(buf, sizeof(buf), "%s%s%s", RESP_200, RESP_204, RESP_200);
    s_llhttp_msgs = 0;
    llhttp_errno_t err = llhttp_execute(&parser, buf, (size_t)n);

    EXPECT(err == HPE_OK, "execute returns HPE_OK on pipelined input");
    EXPECT(s_llhttp_msgs == 3, "all 3 on_message_complete fired");
}


/****************************************************************
 *      (b) ghttp_parser wrapper tests
 *
 *      The wrapper takes an hgobj; we pass NULL for these isolated
 *      tests AND pass NULL for every event name, so none of the
 *      gobj_publish_event / gobj_send_event calls inside the
 *      wrapper's callbacks ever fire.  All we exercise is
 *      create / reset / received with the same data shapes the
 *      production C_PROT_HTTP_CL feeds in.
 ****************************************************************/
PRIVATE void test_ghttp_parser_create_destroy(void) {
    fprintf(stderr, "\n== ghttp_parser: create + destroy ==\n");

    GHTTP_PARSER *parser = ghttp_parser_create(
        NULL,                   /* gobj */
        HTTP_RESPONSE,
        NULL, NULL, NULL,       /* on_header / on_body / on_message events */
        FALSE
    );
    EXPECT(parser != NULL, "ghttp_parser_create returns non-NULL");
    EXPECT(parser && parser->type == HTTP_RESPONSE, "type is HTTP_RESPONSE");
    EXPECT(parser && parser->message_completed == 0, "fresh state: message_completed=0");
    EXPECT(parser && parser->headers_completed == 0, "fresh state: headers_completed=0");
    EXPECT(parser && parser->body_size == 0, "fresh state: body_size=0");
    if(parser) {
        ghttp_parser_destroy(parser);
    }
}

PRIVATE void test_ghttp_parser_received_immediate(void) {
    fprintf(stderr, "\n== ghttp_parser: create + immediate received ==\n");

    GHTTP_PARSER *parser = ghttp_parser_create(NULL, HTTP_RESPONSE, NULL, NULL, NULL, FALSE);
    EXPECT(parser != NULL, "ghttp_parser_create returns non-NULL");

    char buf[2048];
    size_t n = (size_t)snprintf(buf, sizeof(buf), "%s", RESP_204);
    int rc = ghttp_parser_received(parser, buf, n);
    EXPECT(rc == (int)n, "ghttp_parser_received consumed all bytes");
    EXPECT(parser && parser->message_completed == 1, "wrapper saw message_completed");

    ghttp_parser_destroy(parser);
}

PRIVATE void test_ghttp_parser_received_after_idle(int seconds) {
    fprintf(stderr, "\n== ghttp_parser: create + sleep %d s + received ==\n", seconds);

    GHTTP_PARSER *parser = ghttp_parser_create(NULL, HTTP_RESPONSE, NULL, NULL, NULL, FALSE);
    EXPECT(parser != NULL, "ghttp_parser_create returns non-NULL");

    sleep(seconds);

    char buf[2048];
    size_t n = (size_t)snprintf(buf, sizeof(buf), "%s", RESP_204);
    int rc = ghttp_parser_received(parser, buf, n);
    EXPECT(rc == (int)n, "ghttp_parser_received consumed all bytes after idle");
    EXPECT(parser && parser->message_completed == 1, "wrapper saw message_completed after idle");

    ghttp_parser_destroy(parser);
}

PRIVATE void test_ghttp_parser_create_recreate_received(void) {
    fprintf(stderr, "\n== ghttp_parser: create + destroy+create + received (mirrors ac_connected) ==\n");

    GHTTP_PARSER *parser = ghttp_parser_create(NULL, HTTP_RESPONSE, NULL, NULL, NULL, FALSE);
    EXPECT(parser != NULL, "ghttp_parser_create returns non-NULL");

    /* This is exactly what c_prot_http_cl::ac_connected does on every
     * EV_CONNECTED now that ghttp_parser_reset is gone — destroy the
     * old parser and create a fresh one.  Make sure parsing still
     * works right after the recreate. */
    ghttp_parser_destroy(parser);
    parser = ghttp_parser_create(NULL, HTTP_RESPONSE, NULL, NULL, NULL, FALSE);
    EXPECT(parser != NULL, "re-created ghttp_parser is non-NULL");

    char buf[2048];
    size_t n = (size_t)snprintf(buf, sizeof(buf), "%s", RESP_204);
    int rc = ghttp_parser_received(parser, buf, n);
    EXPECT(rc == (int)n, "received OK after recreate");
    EXPECT(parser && parser->message_completed == 1, "message_completed after recreate");

    ghttp_parser_destroy(parser);
}

PRIVATE void test_ghttp_parser_create_idle_recreate_received(int seconds) {
    fprintf(stderr, "\n== ghttp_parser: create + sleep %d s + destroy+create + received ==\n", seconds);

    GHTTP_PARSER *parser = ghttp_parser_create(NULL, HTTP_RESPONSE, NULL, NULL, NULL, FALSE);
    EXPECT(parser != NULL, "ghttp_parser_create returns non-NULL");

    sleep(seconds);

    /* Recreate right before the first parse, simulating the real flow:
     *   create at mt_create → wait for TCP connect (variable time) →
     *   ac_connected destroys+creates parser → first response arrives → parse. */
    ghttp_parser_destroy(parser);
    parser = ghttp_parser_create(NULL, HTTP_RESPONSE, NULL, NULL, NULL, FALSE);
    EXPECT(parser != NULL, "re-created ghttp_parser is non-NULL");

    char buf[2048];
    size_t n = (size_t)snprintf(buf, sizeof(buf), "%s", RESP_204);
    int rc = ghttp_parser_received(parser, buf, n);
    EXPECT(rc == (int)n, "received OK after idle + recreate");
    EXPECT(parser && parser->message_completed == 1, "message_completed after idle + recreate");

    ghttp_parser_destroy(parser);
}

PRIVATE void test_ghttp_parser_finish_eof(void) {
    fprintf(stderr, "\n== ghttp_parser: EOF-terminated response fires on_message_complete via finish ==\n");

    /* HTTP/1.0 response with no Content-Length — the message ends
     * only when the peer closes the socket.  Without ghttp_parser_finish()
     * on_message_complete never fires and EV_ON_MESSAGE is lost. */
    const char *RESP_10_EOF =
        "HTTP/1.0 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "hello world";

    GHTTP_PARSER *parser = ghttp_parser_create(NULL, HTTP_RESPONSE, NULL, NULL, NULL, FALSE);
    EXPECT(parser != NULL, "ghttp_parser_create returns non-NULL");

    char buf[256];
    size_t n = (size_t)snprintf(buf, sizeof(buf), "%s", RESP_10_EOF);
    int rc = ghttp_parser_received(parser, buf, n);
    EXPECT(rc == (int)n, "received consumed all bytes");
    EXPECT(parser && parser->message_completed == 0, "no message_completed yet (waiting for EOF)");

    int fr = ghttp_parser_finish(parser);
    EXPECT(fr == 0, "ghttp_parser_finish returns 0");
    EXPECT(parser && parser->message_completed == 1, "message_completed fired by finish (EOF)");

    ghttp_parser_destroy(parser);
}

PRIVATE void test_ghttp_parser_pipelined(void) {
    fprintf(stderr, "\n== ghttp_parser: pipelined responses on one parser ==\n");

    GHTTP_PARSER *parser = ghttp_parser_create(NULL, HTTP_RESPONSE, NULL, NULL, NULL, FALSE);
    EXPECT(parser != NULL, "ghttp_parser_create returns non-NULL");

    /* Send two responses in a row.  After each on_message_complete the
     * wrapper resets its per-message state internally (see comment in
     * ghttp_parser.c::on_message_complete) but does NOT re-init llhttp,
     * so the parser must stay healthy across messages.  This is the
     * keep-alive pump — the very pattern the persistent-IdP refactor
     * needs to work. */
    char buf[4096];
    size_t n = (size_t)snprintf(buf, sizeof(buf), "%s%s", RESP_200, RESP_204);
    int rc = ghttp_parser_received(parser, buf, n);
    EXPECT(rc == (int)n, "pipelined received consumed all bytes");
    /* After the FIRST on_message_complete the wrapper zeroes its
     * message_completed flag, so we can't easily check it here without
     * intercepting events.  The fact that rc == n and llhttp didn't
     * complain is the success criterion. */

    ghttp_parser_destroy(parser);
}


/****************************************************************
 *      Entry point
 ****************************************************************/
int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");

    /*----------------------------------*
     *      Startup gobj system
     *  ghttp_parser uses GBMEM_MALLOC + json calls; needs gobj alive.
     *----------------------------------*/
    sys_malloc_fn_t  malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t  calloc_func;
    sys_free_fn_t    free_func;

    gbmem_get_allocators(&malloc_func, &realloc_func, &calloc_func, &free_func);
    json_set_alloc_funcs(malloc_func, free_func);

    gobj_start_up(
        argc, argv,
        NULL, NULL, NULL, NULL, NULL, NULL
    );

    fprintf(stderr, "================================================\n");
    fprintf(stderr, "  c_llhttp_parser — sanity tests\n");
    fprintf(stderr, "================================================\n");

    /* (a) bare llhttp */
    test_bare_llhttp_immediate();
    test_bare_llhttp_idle(1);
    test_bare_llhttp_idle(2);
    test_bare_llhttp_init_then_reinit_then_execute();
    test_bare_llhttp_pipelined();

    /* (b) ghttp_parser wrapper */
    test_ghttp_parser_create_destroy();
    test_ghttp_parser_received_immediate();
    test_ghttp_parser_received_after_idle(1);
    test_ghttp_parser_received_after_idle(2);
    test_ghttp_parser_create_recreate_received();
    test_ghttp_parser_create_idle_recreate_received(1);
    test_ghttp_parser_create_idle_recreate_received(2);
    test_ghttp_parser_pipelined();
    test_ghttp_parser_finish_eof();

    gobj_end();

    fprintf(stderr, "\n================================================\n");
    if(s_failures == 0) {
        fprintf(stderr, "  ALL OK\n");
        fprintf(stderr, "================================================\n");
        return TEST_OK;
    } else {
        fprintf(stderr, "  %d FAILURE(S)\n", s_failures);
        fprintf(stderr, "================================================\n");
        return TEST_FAIL;
    }
}
