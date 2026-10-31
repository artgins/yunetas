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
/***************************************************************************
 *  Regression for f001: on_url() accumulates the URL across chunks and does
 *  `pos = strlen(parser->url)` on every chunk, so each chunk MUST leave the
 *  accumulator NUL-terminated. Feeding a request one byte at a time fires
 *  on_url repeatedly, exercising the gbmem_realloc grow path; without the
 *  terminator write the next strlen() over-reads adjacent heap and corrupts
 *  `pos`, mangling the reassembled URL. Assert it comes back byte-for-byte.
 *  (on_message_event is NULL, so the wrapper keeps parser->url and
 *  message_completed instead of freeing/resetting them on completion.)
 ***************************************************************************/
PRIVATE void test_ghttp_parser_fragmented_url(void) {
    fprintf(stderr, "\n== ghttp_parser: fragmented URL reassembled (f001 regression) ==\n");

    static const char EXPECTED_URL[] = "/api/v1/aaaa/bbbb/cccc/dddd/eeee/ffff/gggg";
    char req[256];
    int reqlen = snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\nHost: h\r\n\r\n", EXPECTED_URL);

    GHTTP_PARSER *parser = ghttp_parser_create(NULL, HTTP_REQUEST, NULL, NULL, NULL, FALSE);
    EXPECT(parser != NULL, "ghttp_parser_create (request) returns non-NULL");

    int consumed = 0;
    for(int i = 0; i < reqlen; i++) {       /* one byte per call => on_url fires repeatedly */
        int rc = ghttp_parser_received(parser, req + i, 1);
        if(rc < 0) {
            break;
        }
        consumed += rc;
    }
    EXPECT(consumed == reqlen, "byte-by-byte feed consumed the whole request");
    EXPECT(parser && parser->message_completed == 1, "wrapper saw message_completed");
    EXPECT(parser && parser->url && strcmp(parser->url, EXPECTED_URL) == 0,
        "fragmented URL reassembled exactly (no over-read / pos corruption)");

    ghttp_parser_destroy(parser);
}

/***************************************************************************
 *  Feed `len` bytes to the parser in pieces of `chunk` bytes, as a socket
 *  hands them. Returns the bytes consumed, or -1 at the first refusal.
 ***************************************************************************/
PRIVATE int feed_in_chunks(GHTTP_PARSER *parser, char *bf, size_t len, size_t chunk)
{
    size_t consumed = 0;
    while(consumed < len) {
        size_t n = MIN(chunk, len - consumed);
        int rc = ghttp_parser_received(parser, bf + consumed, n);
        if(rc < 0) {
            return -1;
        }
        consumed += (size_t)rc;
    }
    return (int)consumed;
}

/***************************************************************************
 *  A request whose one header holds `value_len` bytes
 ***************************************************************************/
PRIVATE char *build_big_header_request(size_t value_len, size_t *len)
{
    const char *head = "GET / HTTP/1.1\r\nX-Big: ";
    const char *tail = "\r\n\r\n";
    *len = strlen(head) + value_len + strlen(tail);
    char *req = gbmem_malloc(*len + 1);
    if(!req) {
        return NULL;
    }
    memcpy(req, head, strlen(head));
    memset(req + strlen(head), 'a', value_len);
    memcpy(req + strlen(head) + value_len, tail, strlen(tail) + 1);
    return req;
}

/***************************************************************************
 *  The best of three times, in ns, of parsing a header of `value_len`
 *  bytes that arrives in pieces of 4 KB (the rx buffer of C_TCP)
 ***************************************************************************/
PRIVATE uint64_t time_big_header(size_t value_len, BOOL *whole)
{
    size_t len;
    char *req = build_big_header_request(value_len, &len);
    if(!req) {
        *whole = FALSE;
        return 0;
    }
    uint64_t best = 0;
    *whole = TRUE;
    for(int i = 0; i < 3; i++) {
        GHTTP_PARSER *parser = ghttp_parser_create(NULL, HTTP_REQUEST, NULL, NULL, NULL, FALSE);
        parser->max_header_size = 16*1024*1024;    // capped to half the max block: 8 MB
        time_measure_t tm;
        MT_START_TIME(tm)
        int rc = feed_in_chunks(parser, req, len, 4096);
        clock_gettime(CLOCK_MONOTONIC_RAW, &tm.end);
        uint64_t ns = mt_get_time_ns(&tm);
        const char *v = json_string_value(json_object_get(parser->jn_headers, "X-BIG"));
        if(rc != (int)len || !parser->message_completed || !v || strlen(v) != value_len) {
            *whole = FALSE;
        }
        if(best == 0 || ns < best) {
            best = ns;
        }
        ghttp_parser_destroy(parser);
    }
    gbmem_free(req);
    return best;
}

/***************************************************************************
 *  A header that arrives in many pieces is put together in linear time:
 *  each byte copied once. Eight times the bytes must cost about eight
 *  times the time; a header copied whole again for each piece costs
 *  about sixty-four times.
 ***************************************************************************/
PRIVATE void test_ghttp_parser_header_time_linear(void)
{
    fprintf(stderr, "\n== ghttp_parser: a big header costs linear time ==\n");

    BOOL whole1, whole8;
    uint64_t t1 = time_big_header(512*1024, &whole1);
    uint64_t t8 = time_big_header(4*1024*1024, &whole8);
    double ratio = t1? (double)t8 / (double)t1 : 0;
    fprintf(stderr, "  512 KB: %.3f ms, 4 MB: %.3f ms, ratio %.1f (linear 8, quadratic 64)\n",
        (double)t1/1e6, (double)t8/1e6, ratio);
    EXPECT(whole1 && whole8, "the big header arrives whole into jn_headers");
    EXPECT(t1 > 0 && ratio < 24.0, "eight times the bytes cost less than 24 times the time");
}

/***************************************************************************
 *  The head of a request is refused past max_header_size (the target, the
 *  names and the values count): at the limit it passes, one byte more it
 *  is refused (headers_refused, -1), and nothing more is parsed.
 ***************************************************************************/
PRIVATE void test_ghttp_parser_header_cap(void)
{
    fprintf(stderr, "\n== ghttp_parser: the head is refused past its limit ==\n");

    /*  "/" + "X-Big" + value  */
    size_t at_limit = GHTTP_MAX_HEADER_SIZE - 1 - 5;
    for(int extra = 0; extra < 2; extra++) {
        size_t len;
        char *req = build_big_header_request(at_limit + (size_t)extra, &len);
        GHTTP_PARSER *parser = ghttp_parser_create(NULL, HTTP_REQUEST, NULL, NULL, NULL, FALSE);
        int rc = feed_in_chunks(parser, req, len, 4096);
        if(extra == 0) {
            EXPECT(rc == (int)len && parser->message_completed && !parser->headers_refused,
                "a head of exactly max_header_size is parsed");
        } else {
            EXPECT(rc == -1 && parser->headers_refused && !parser->message_completed,
                "a head one byte over max_header_size is refused");
            EXPECT(ghttp_parser_received(parser, "GET", 3) == -1,
                "a refused parser parses nothing more");
        }
        ghttp_parser_destroy(parser);
        gbmem_free(req);
    }
}

/***************************************************************************
 *  A header with an empty value is kept, as "", and a header split at
 *  every byte comes back whole. A name that comes again is one field, its
 *  values joined in order with ", " (RFC 9110 §5.3), a COOKIE with "; ";
 *  a SET-COOKIE is never joined: the last one is kept.
 ***************************************************************************/
PRIVATE void test_ghttp_parser_header_pieces(void)
{
    fprintf(stderr, "\n== ghttp_parser: empty values and headers in pieces ==\n");

    char req[] = "GET /x HTTP/1.1\r\nX-Empty:\r\nX-Two: one two\r\nx-two: three\r\nHost: h\r\n"
        "Retry-After: 120\r\nRetry-After: 60\r\nCookie: a=1\r\nCookie: b=2\r\n"
        "Set-Cookie: s=1; Expires=Wed, 21 Oct 2026 07:28:00 GMT\r\nSet-Cookie: s=2\r\n\r\n";
    GHTTP_PARSER *parser = ghttp_parser_create(NULL, HTTP_REQUEST, NULL, NULL, NULL, FALSE);
    int rc = feed_in_chunks(parser, req, strlen(req), 1);
    EXPECT(rc == (int)strlen(req) && parser->message_completed, "the request is parsed byte by byte");
    const char *empty = json_string_value(json_object_get(parser->jn_headers, "X-EMPTY"));
    EXPECT(empty && strcmp(empty, "") == 0, "an empty header value is kept as \"\"");
    const char *two = json_string_value(json_object_get(parser->jn_headers, "X-TWO"));
    EXPECT(two && strcmp(two, "one two, three") == 0, "a name that comes again joins its values with \", \"");
    const char *ra = json_string_value(json_object_get(parser->jn_headers, "RETRY-AFTER"));
    EXPECT(ra && strcmp(ra, "120, 60") == 0, "two Retry-After are one field \"120, 60\", not the last one");
    const char *cookie = json_string_value(json_object_get(parser->jn_headers, "COOKIE"));
    EXPECT(cookie && strcmp(cookie, "a=1; b=2") == 0, "two Cookie are joined with \"; \"");
    const char *set_cookie = json_string_value(json_object_get(parser->jn_headers, "SET-COOKIE"));
    EXPECT(set_cookie && strcmp(set_cookie, "s=2") == 0, "a Set-Cookie is never joined: the last one is kept");
    const char *host = json_string_value(json_object_get(parser->jn_headers, "HOST"));
    EXPECT(host && strcmp(host, "h") == 0, "a header given byte by byte comes back whole");
    ghttp_parser_destroy(parser);
}

/***************************************************************************
 *  A stopped parser takes every byte and parses none
 ***************************************************************************/
PRIVATE void test_ghttp_parser_stop(void)
{
    fprintf(stderr, "\n== ghttp_parser: a stopped parser parses nothing ==\n");

    GHTTP_PARSER *parser = ghttp_parser_create(NULL, HTTP_RESPONSE, NULL, NULL, NULL, FALSE);
    ghttp_parser_stop(parser);
    char buf[256];
    size_t n = (size_t)snprintf(buf, sizeof(buf), "%s", RESP_204);
    int rc = ghttp_parser_received(parser, buf, n);
    EXPECT(rc == (int)n, "a stopped parser takes every byte");
    EXPECT(parser->message_completed == 0, "a stopped parser completes no message");
    EXPECT(ghttp_parser_finish(parser) == 0, "finish on a stopped parser is quiet");
    ghttp_parser_destroy(parser);
}

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
    test_ghttp_parser_fragmented_url();
    test_ghttp_parser_header_pieces();
    test_ghttp_parser_header_cap();
    test_ghttp_parser_stop();
    test_ghttp_parser_header_time_linear();

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
