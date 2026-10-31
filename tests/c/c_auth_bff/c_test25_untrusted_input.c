/****************************************************************************
 *          C_TEST25_UNTRUSTED_INPUT.C
 *
 *          Driver for test25_untrusted_input: what the BFF takes from
 *          others. One plain socket, one request at a time (each answer is
 *          read whole before the next request), but the last four steps,
 *          which pipeline theirs. Before each step the mock IdP's `last_form` is
 *          cleared; after it, the form the IdP got is decoded and checked
 *          (each key named once, with its value), or must be empty when the
 *          BFF must not call the IdP.
 *
 *            1. login as "user+1@x.com" with the password
 *               "p+a%41&client_id=evil&grant_type=x": 200; the IdP gets
 *               ONE grant_type, ONE client_id, and the username and the
 *               password as they were typed;
 *            2. refresh with the cookie "refresh_token=RT&scope=evil%41+y":
 *               200; the IdP gets that refresh_token, and no scope;
 *            3. logout with the cookie "refresh_token=RT&client_id=evil":
 *               200, both cookies cleared; the IdP gets that
 *               refresh_token and ONE client_id;
 *          4-8. logout with a refresh_token cookie, and an Origin that is
 *               not the allowed_origin: another, none, "null", the allowed
 *               one with more after it ("http://localhost.evil.com"), the
 *               start of the allowed one ("http://localhos"): 403
 *               origin_not_allowed, no Set-Cookie, no call to the IdP;
 *         9-10. logout with only the access_token cookie, no Origin and
 *               "http://localhost:18888": the same;
 *           11. login, the IdP answers an access_token with
 *               "; Domain=evil.com" in it: 502 auth_unexpected_error, no
 *               Set-Cookie;
 *           12. the same with a refresh_token with
 *               "\r\nSet-Cookie: injected=1" in it;
 *           13. refresh with a refresh_token cookie of 4083 bytes (4096
 *               with its name, the longest the BFF writes): 200;
 *           14. the same of 4084 bytes: 400 token_too_large, its error
 *               saying "none longer than 4083", no call to the IdP;
 *           15. logout with that cookie of 4084 bytes: the same;
 *           16. logout with it of 4083 bytes: 200, the IdP called;
 *           17. /auth/token with an access_token cookie of 4084 bytes
 *               (4096 with its name): 200, the token in the answer;
 *           18. the same of 4085 bytes: 400 token_too_large;
 *           19. login, the IdP answers the JSON string "" as its body: 502
 *               auth_empty_response, its error saying a JSON string (not an
 *               empty body);
 *           20. login, the IdP answers a body of 1 MB and 1 byte (the
 *               limit is 1 MB): 502 auth_unexpected_error, its error
 *               saying the limit;
 *           21. login: 200 (the IdP connection dropped by step 20 is made
 *               again);
 *        22-25. refresh, the IdP answers 429 with a Retry-After: "120"
 *               and "Wed, 21 Oct 2026 07:28:00 GMT" are passed on in the
 *               429 auth_rate_limited, with Access-Control-Expose-Headers
 *               naming it (the BFF may be on another origin than the
 *               SPA); "soon" and "-5" are not passed on;
 *        26-31. login, the IdP answers an access_token with a byte over
 *               0x7E (0xC3, of a UTF-8 "é"), a DEL (0x7F), a space, a
 *               DQUOTE, a comma or a backslash: 502 auth_unexpected_error,
 *               the WARNING naming the byte in hex;
 *        32-45. refresh, the IdP answers 429 with a Retry-After that is not
 *               passed on: a day 99, a time 99:99:99, a day-name that is
 *               not the one of the date, a month "Foo", a day-name "Xyz",
 *               29 Feb of a year that is not leap, 11 digits, "120, 60";
 *               and one that is: 10 digits, an RFC 850 date and two
 *               asctime dates (passed on as IMF-fixdate), "120" with blanks
 *               after it (passed on without them), a leap second;
 *        46-51. a refresh with another Origin, a refresh with none, a login
 *               from the origin of a dev server ("http://localhost:5173"),
 *               a callback and a /auth/token with another Origin, a logout
 *               with a refresh_token cookie from the dev server: 403
 *               origin_not_allowed, its error naming the Origin received
 *               and allowed_origin, no call to the IdP;
 *           52. login with a username of 300 bytes and a password of 600:
 *               200, the IdP gets both whole;
 *           53. login, the IdP drops the connection without answering: 502
 *               auth_service_unavailable, and the login is not sent again
 *               on the next connection (the IdP would answer it 200);
 *           54. login: 200;
 *           55. three requests in one write: a login (the IdP answers it
 *               300 ms later), a POST to an unknown endpoint and a GET:
 *               their answers come in that order, 200, 404, 405;
 *        56-58. a login and 17, 40 and 600 requests to an unknown endpoint
 *               in one write, each on a new connection: 16 answers wait
 *               behind the login (the pending_queue_size), the 17th drops
 *               the connection, with no answer sent, and the requests after
 *               it are not read.
 *
 *          The logs are asserted by the test harness via
 *          set_expected_results in main_test25.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <yunetas.h>

#include "c_test25_untrusted_input.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define TEST_NAME       "test25_untrusted_input"
#define ALLOWED_ORIGIN  "http://localhost"  /* the allowed_origin of the BFF (main_test25) */
#define NO_ORIGIN       "@none"             /* a request with no Origin header */
#define BIG_BODY        "@big"              /* a raw body of BIG_BODY_LEN bytes */
#define BIG_BODY_LEN    (1024*1024 + 1)     /* one byte over the BFF's limit */
#define MAX_COOKIE_LEN  5000
#define MAX_PIPELINED   3
#define IDP_DROPS       (-1)                /* idp_status: the IdP drops the connection, no answer */

#define TEXT_50     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
#define TEXT_300    TEXT_50 TEXT_50 TEXT_50 TEXT_50 TEXT_50 TEXT_50
#define TEXT_600    TEXT_300 TEXT_300
#define DEV_ORIGIN  "http://localhost:5173"     /* the origin of a dev server proxying to the BFF */

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct {
    const char *method;         /* NULL: POST */
    const char *url;
    const char *cookie;         /* Cookie header, "@rt<N>"/"@at<N>" for a token of N bytes, NULL none */
    const char *body;           /* NULL: the test credentials for /auth/login, "{}" for the others */
    const char *origin;         /* NULL: ALLOWED_ORIGIN; NO_ORIGIN: none */
    const char *body_patch;     /* json of the mock's body_patch, NULL "{}" */
    const char *raw_content_type;   /* a raw answer of the mock: its Content-Type, NULL none */
    const char *raw_body;           /* and its body (BIG_BODY: BIG_BODY_LEN bytes) */
    int         status;         /* status expected */
    const char *must_have[4];   /* texts the answer must carry (NULL ends) */
    const char *must_not[3];    /* texts it must not carry (NULL ends) */
    const char *form_has[5];    /* "key=value": the key ONCE in the IdP form, with this decoded value */
    const char *form_not[3];    /* keys the IdP form must not have */
    BOOL        no_idp;         /* the IdP must not be called */
    int         idp_status;     /* status of the mock's answer to /token, 0: 200, IDP_DROPS: none, the connection dropped */
    const char *retry_after;    /* Retry-After of the mock's answer, NULL none */
} step_t;

/***************************************************************************
 *              Data
 ***************************************************************************/
PRIVATE const step_t steps[] = {
    /* 1 */
    {NULL, "/auth/login", NULL,
        "{\"username\": \"user+1@x.com\", \"password\": \"p+a%41&client_id=evil&grant_type=x\"}",
        NULL, NULL, NULL, NULL, 200,
        {"Set-Cookie: access_token=ey", NULL}, {NULL},
        {"grant_type=password", "client_id=test-client", "username=user+1@x.com",
            "password=p+a%41&client_id=evil&grant_type=x", NULL},
        {NULL}, FALSE, 0, NULL},
    /* 2 */
    {NULL, "/auth/refresh", "refresh_token=RT&scope=evil%41+y", NULL,
        NULL, NULL, NULL, NULL, 200,
        {"Set-Cookie: access_token=ey", NULL}, {NULL},
        {"grant_type=refresh_token", "client_id=test-client",
            "refresh_token=RT&scope=evil%41+y", NULL},
        {"scope", NULL}, FALSE, 0, NULL},
    /* 3 */
    {NULL, "/auth/logout", "refresh_token=RT&client_id=evil", NULL,
        NULL, NULL, NULL, NULL, 200,
        {"Set-Cookie: access_token=; Max-Age=0;", "Set-Cookie: refresh_token=; Max-Age=0;", NULL},
        {NULL},
        {"client_id=test-client", "refresh_token=RT&client_id=evil", NULL},
        {NULL}, FALSE, 0, NULL},
    /* 4-8 */
    {NULL, "/auth/logout", "refresh_token=RT1", NULL,
        "http://other.example", NULL, NULL, NULL, 403,
        {"origin_not_allowed", NULL}, {"Set-Cookie", NULL}, {NULL}, {NULL}, TRUE, 0, NULL},
    {NULL, "/auth/logout", "refresh_token=RT1", NULL,
        NO_ORIGIN, NULL, NULL, NULL, 403,
        {"origin_not_allowed", NULL}, {"Set-Cookie", NULL}, {NULL}, {NULL}, TRUE, 0, NULL},
    {NULL, "/auth/logout", "refresh_token=RT1", NULL,
        "null", NULL, NULL, NULL, 403,
        {"origin_not_allowed", NULL}, {"Set-Cookie", NULL}, {NULL}, {NULL}, TRUE, 0, NULL},
    {NULL, "/auth/logout", "refresh_token=RT1", NULL,
        "http://localhost.evil.com", NULL, NULL, NULL, 403,
        {"origin_not_allowed", NULL}, {"Set-Cookie", NULL}, {NULL}, {NULL}, TRUE, 0, NULL},
    {NULL, "/auth/logout", "refresh_token=RT1", NULL,
        "http://localhos", NULL, NULL, NULL, 403,
        {"origin_not_allowed", NULL}, {"Set-Cookie", NULL}, {NULL}, {NULL}, TRUE, 0, NULL},
    /* 9-10 */
    {NULL, "/auth/logout", "access_token=AT1", NULL,
        NO_ORIGIN, NULL, NULL, NULL, 403,
        {"origin_not_allowed", NULL}, {"Set-Cookie", NULL}, {NULL}, {NULL}, TRUE, 0, NULL},
    {NULL, "/auth/logout", "access_token=AT1", NULL,
        "http://localhost:18888", NULL, NULL, NULL, 403,
        {"origin_not_allowed", NULL}, {"Set-Cookie", NULL}, {NULL}, {NULL}, TRUE, 0, NULL},
    /* 11 */
    {NULL, "/auth/login", NULL, NULL,
        NULL, "{\"access_token\": \"eyJ.e30.sig; Domain=evil.com\"}", NULL, NULL, 502,
        {"auth_unexpected_error", "access_token with a byte a cookie cannot carry", NULL},
        {"Set-Cookie", "evil.com", NULL}, {NULL}, {NULL}, FALSE, 0, NULL},
    /* 12 */
    {NULL, "/auth/login", NULL, NULL,
        NULL, "{\"refresh_token\": \"RT\\r\\nSet-Cookie: injected=1\"}", NULL, NULL, 502,
        {"auth_unexpected_error", "refresh_token with a byte a cookie cannot carry", NULL},
        {"Set-Cookie", "injected", NULL}, {NULL}, {NULL}, FALSE, 0, NULL},
    /* 13-16 */
    {NULL, "/auth/refresh", "@rt4083", NULL,
        NULL, NULL, NULL, NULL, 200,
        {"Set-Cookie: access_token=ey", NULL}, {NULL},
        {"grant_type=refresh_token", NULL}, {NULL}, FALSE, 0, NULL},
    {NULL, "/auth/refresh", "@rt4084", NULL,
        NULL, NULL, NULL, NULL, 400,
        {"token_too_large", "refresh_token cookie of 4084 bytes", "none longer than 4083", NULL},
        {"Set-Cookie", NULL}, {NULL}, {NULL}, TRUE, 0, NULL},
    {NULL, "/auth/logout", "@rt4084", NULL,
        NULL, NULL, NULL, NULL, 400,
        {"token_too_large", "refresh_token cookie of 4084 bytes", "none longer than 4083", NULL},
        {"Set-Cookie", NULL}, {NULL}, {NULL}, TRUE, 0, NULL},
    {NULL, "/auth/logout", "@rt4083", NULL,
        NULL, NULL, NULL, NULL, 200,
        {"Set-Cookie: refresh_token=; Max-Age=0;", NULL}, {NULL},
        {"client_id=test-client", NULL}, {NULL}, FALSE, 0, NULL},
    /* 17-18 */
    {NULL, "/auth/token", "@at4084", NULL,
        NULL, NULL, NULL, NULL, 200,
        {"\"access_token\":\"aaaa", NULL}, {NULL}, {NULL}, {NULL}, TRUE, 0, NULL},
    {NULL, "/auth/token", "@at4085", NULL,
        NULL, NULL, NULL, NULL, 400,
        {"token_too_large", "access_token cookie of 4085 bytes", "none longer than 4084", NULL},
        {"\"access_token\":\"aaaa", NULL}, {NULL}, {NULL}, TRUE, 0, NULL},
    /* 19 */
    {NULL, "/auth/login", NULL, NULL,
        NULL, NULL, "application/json", "\"\"", 502,
        {"auth_empty_response", "a body that is a JSON string, not an object", NULL},
        {"Set-Cookie", "an empty body", NULL}, {NULL}, {NULL}, FALSE, 0, NULL},
    /* 20 */
    {NULL, "/auth/login", NULL, NULL,
        NULL, NULL, "application/json", BIG_BODY, 502,
        {"auth_unexpected_error", "a body longer than 1048576 bytes", NULL},
        {"Set-Cookie", NULL}, {NULL}, {NULL}, FALSE, 0, NULL},
    /* 21 */
    {NULL, "/auth/login", NULL, NULL,
        NULL, NULL, NULL, NULL, 200,
        {"Set-Cookie: access_token=ey", NULL}, {NULL},
        {"grant_type=password", NULL}, {NULL}, FALSE, 0, NULL},
    /* 22-25 */
    {NULL, "/auth/refresh", "refresh_token=RT1", NULL,
        NULL, NULL, NULL, NULL, 429,
        {"auth_rate_limited", "\r\nRetry-After: 120\r\n",
            "\r\nAccess-Control-Expose-Headers: Retry-After\r\n", NULL},
        {NULL}, {NULL}, {NULL}, FALSE, 429, "120"},
    {NULL, "/auth/refresh", "refresh_token=RT1", NULL,
        NULL, NULL, NULL, NULL, 429,
        {"auth_rate_limited", "\r\nRetry-After: Wed, 21 Oct 2026 07:28:00 GMT\r\n", NULL},
        {NULL}, {NULL}, {NULL}, FALSE, 429, "Wed, 21 Oct 2026 07:28:00 GMT"},
    {NULL, "/auth/refresh", "refresh_token=RT1", NULL,
        NULL, NULL, NULL, NULL, 429,
        {"auth_rate_limited", NULL},
        {"\r\nRetry-After:", NULL}, {NULL}, {NULL}, FALSE, 429, "soon"},
    {NULL, "/auth/refresh", "refresh_token=RT1", NULL,
        NULL, NULL, NULL, NULL, 429,
        {"auth_rate_limited", NULL},
        {"\r\nRetry-After:", NULL}, {NULL}, {NULL}, FALSE, 429, "-5"},
    /* 26-31 */
    {NULL, "/auth/login", NULL, NULL,
        NULL, "{\"access_token\": \"eyJ.e30.sig\\u00e9\"}", NULL, NULL, 502,
        {"auth_unexpected_error", "an access_token with a byte a cookie cannot carry", NULL},
        {"Set-Cookie", NULL}, {NULL}, {NULL}, FALSE, 0, NULL},   /* 0xC3 */
    {NULL, "/auth/login", NULL, NULL,
        NULL, "{\"access_token\": \"eyJ.e30.sig\\u007f\"}", NULL, NULL, 502,
        {"auth_unexpected_error", "an access_token with a byte a cookie cannot carry", NULL},
        {"Set-Cookie", NULL}, {NULL}, {NULL}, FALSE, 0, NULL},   /* 0x7F */
    {NULL, "/auth/login", NULL, NULL,
        NULL, "{\"access_token\": \"eyJ.e30.s ig\"}", NULL, NULL, 502,
        {"auth_unexpected_error", "an access_token with a byte a cookie cannot carry", NULL},
        {"Set-Cookie", NULL}, {NULL}, {NULL}, FALSE, 0, NULL},   /* 0x20 */
    {NULL, "/auth/login", NULL, NULL,
        NULL, "{\"access_token\": \"eyJ.e30.s\\\"ig\"}", NULL, NULL, 502,
        {"auth_unexpected_error", "an access_token with a byte a cookie cannot carry", NULL},
        {"Set-Cookie", NULL}, {NULL}, {NULL}, FALSE, 0, NULL},   /* 0x22 */
    {NULL, "/auth/login", NULL, NULL,
        NULL, "{\"access_token\": \"eyJ.e30.s,ig\"}", NULL, NULL, 502,
        {"auth_unexpected_error", "an access_token with a byte a cookie cannot carry", NULL},
        {"Set-Cookie", NULL}, {NULL}, {NULL}, FALSE, 0, NULL},   /* 0x2C */
    {NULL, "/auth/login", NULL, NULL,
        NULL, "{\"access_token\": \"eyJ.e30.s\\\\ig\"}", NULL, NULL, 502,
        {"auth_unexpected_error", "an access_token with a byte a cookie cannot carry", NULL},
        {"Set-Cookie", NULL}, {NULL}, {NULL}, FALSE, 0, NULL},   /* 0x5C */
    /* 32-45 */
    {NULL, "/auth/refresh", "refresh_token=RT1", NULL,
        NULL, NULL, NULL, NULL, 429,
        {"auth_rate_limited", NULL},
        {"\r\nRetry-After:", NULL}, {NULL}, {NULL}, FALSE, 429, "Wed, 99 Oct 2026 07:28:00 GMT"},
    {NULL, "/auth/refresh", "refresh_token=RT1", NULL,
        NULL, NULL, NULL, NULL, 429,
        {"auth_rate_limited", NULL},
        {"\r\nRetry-After:", NULL}, {NULL}, {NULL}, FALSE, 429, "Wed, 21 Oct 2026 99:99:99 GMT"},
    {NULL, "/auth/refresh", "refresh_token=RT1", NULL,
        NULL, NULL, NULL, NULL, 429,
        {"auth_rate_limited", NULL},
        {"\r\nRetry-After:", NULL}, {NULL}, {NULL}, FALSE, 429, "Thu, 21 Oct 2026 07:28:00 GMT"},
    {NULL, "/auth/refresh", "refresh_token=RT1", NULL,
        NULL, NULL, NULL, NULL, 429,
        {"auth_rate_limited", NULL},
        {"\r\nRetry-After:", NULL}, {NULL}, {NULL}, FALSE, 429, "Wed, 21 Foo 2026 07:28:00 GMT"},
    {NULL, "/auth/refresh", "refresh_token=RT1", NULL,
        NULL, NULL, NULL, NULL, 429,
        {"auth_rate_limited", NULL},
        {"\r\nRetry-After:", NULL}, {NULL}, {NULL}, FALSE, 429, "Xyz, 21 Oct 2026 07:28:00 GMT"},
    {NULL, "/auth/refresh", "refresh_token=RT1", NULL,
        NULL, NULL, NULL, NULL, 429,
        {"auth_rate_limited", NULL},
        {"\r\nRetry-After:", NULL}, {NULL}, {NULL}, FALSE, 429, "Sun, 29 Feb 2026 07:28:00 GMT"},
    {NULL, "/auth/refresh", "refresh_token=RT1", NULL,
        NULL, NULL, NULL, NULL, 429,
        {"auth_rate_limited", NULL},
        {"\r\nRetry-After:", NULL}, {NULL}, {NULL}, FALSE, 429, "12345678901"},
    {NULL, "/auth/refresh", "refresh_token=RT1", NULL,
        NULL, NULL, NULL, NULL, 429,
        {"auth_rate_limited", "\r\nRetry-After: 1234567890\r\n", NULL},
        {NULL}, {NULL}, {NULL}, FALSE, 429, "1234567890"},
    {NULL, "/auth/refresh", "refresh_token=RT1", NULL,
        NULL, NULL, NULL, NULL, 429,
        {"auth_rate_limited", "\r\nRetry-After: Wed, 21 Oct 2026 07:28:00 GMT\r\n", NULL},
        {NULL}, {NULL}, {NULL}, FALSE, 429, "Wednesday, 21-Oct-26 07:28:00 GMT"},
    {NULL, "/auth/refresh", "refresh_token=RT1", NULL,
        NULL, NULL, NULL, NULL, 429,
        {"auth_rate_limited", "\r\nRetry-After: Wed, 21 Oct 2026 07:28:00 GMT\r\n", NULL},
        {NULL}, {NULL}, {NULL}, FALSE, 429, "Wed Oct 21 07:28:00 2026"},
    {NULL, "/auth/refresh", "refresh_token=RT1", NULL,
        NULL, NULL, NULL, NULL, 429,
        {"auth_rate_limited", "\r\nRetry-After: Thu, 01 Oct 2026 07:28:00 GMT\r\n", NULL},
        {NULL}, {NULL}, {NULL}, FALSE, 429, "Thu Oct  1 07:28:00 2026"},
    {NULL, "/auth/refresh", "refresh_token=RT1", NULL,
        NULL, NULL, NULL, NULL, 429,
        {"auth_rate_limited", "\r\nRetry-After: 120\r\n", NULL},
        {NULL}, {NULL}, {NULL}, FALSE, 429, "120  "},
    {NULL, "/auth/refresh", "refresh_token=RT1", NULL,
        NULL, NULL, NULL, NULL, 429,
        {"auth_rate_limited", NULL},
        {"\r\nRetry-After:", NULL}, {NULL}, {NULL}, FALSE, 429, "120, 60"},
    {NULL, "/auth/refresh", "refresh_token=RT1", NULL,
        NULL, NULL, NULL, NULL, 429,
        {"auth_rate_limited", "\r\nRetry-After: Sat, 31 Dec 2016 23:59:60 GMT\r\n", NULL},
        {NULL}, {NULL}, {NULL}, FALSE, 429, "Sat, 31 Dec 2016 23:59:60 GMT"},
    /* 46-51 */
    {NULL, "/auth/refresh", "refresh_token=RT1", NULL,
        "http://other.example", NULL, NULL, NULL, 403,
        {"origin_not_allowed", "Origin 'http://other.example' not allowed for /auth/refresh: "
            "allowed_origin is '" ALLOWED_ORIGIN "'", NULL},
        {"Set-Cookie", NULL}, {NULL}, {NULL}, TRUE, 0, NULL},
    {NULL, "/auth/refresh", "refresh_token=RT1", NULL,
        NO_ORIGIN, NULL, NULL, NULL, 403,
        {"origin_not_allowed", "Origin none not allowed for /auth/refresh", NULL},
        {"Set-Cookie", NULL}, {NULL}, {NULL}, TRUE, 0, NULL},
    {NULL, "/auth/login", NULL, NULL,
        DEV_ORIGIN, NULL, NULL, NULL, 403,
        {"origin_not_allowed", "Origin '" DEV_ORIGIN "' not allowed for /auth/login: "
            "allowed_origin is '" ALLOWED_ORIGIN "'", NULL},
        {"Set-Cookie", NULL}, {NULL}, {NULL}, TRUE, 0, NULL},
    {NULL, "/auth/callback", NULL,
        "{\"code\": \"C\", \"code_verifier\": \"V\", \"redirect_uri\": \"http://localhost/\"}",
        "http://other.example", NULL, NULL, NULL, 403,
        {"origin_not_allowed", "not allowed for /auth/callback", NULL},
        {"Set-Cookie", NULL}, {NULL}, {NULL}, TRUE, 0, NULL},
    {NULL, "/auth/token", "access_token=AT1", NULL,
        "http://other.example", NULL, NULL, NULL, 403,
        {"origin_not_allowed", "not allowed for /auth/token", NULL},
        {"AT1", NULL}, {NULL}, {NULL}, TRUE, 0, NULL},
    {NULL, "/auth/logout", "refresh_token=RT1", NULL,
        DEV_ORIGIN, NULL, NULL, NULL, 403,
        {"origin_not_allowed", "Origin '" DEV_ORIGIN "' not allowed for /auth/logout: "
            "allowed_origin is '" ALLOWED_ORIGIN "'", NULL},
        {"Set-Cookie", NULL}, {NULL}, {NULL}, TRUE, 0, NULL},
    /* 52 */
    {NULL, "/auth/login", NULL,
        "{\"username\": \"" TEXT_300 "\", \"password\": \"" TEXT_600 "\"}",
        NULL, NULL, NULL, NULL, 200,
        {"Set-Cookie: access_token=ey", NULL}, {NULL},
        {"username=" TEXT_300, "password=" TEXT_600, NULL}, {NULL}, FALSE, 0, NULL},
    /* 53-54 */
    {NULL, "/auth/login", NULL, NULL,
        NULL, NULL, NULL, NULL, 502,
        {"auth_service_unavailable", "closed the connection before answering", NULL},
        {"Set-Cookie", NULL}, {NULL}, {NULL}, FALSE, IDP_DROPS, NULL},
    {NULL, "/auth/login", NULL, NULL,
        NULL, NULL, NULL, NULL, 200,
        {"Set-Cookie: access_token=ey", NULL}, {NULL},
        {"grant_type=password", NULL}, {NULL}, FALSE, 0, NULL},
};
#define NSTEPS ((int)(sizeof(steps)/sizeof(steps[0])))

/*
 *  Step NSTEPS: three requests in one write, their statuses in order
 */
PRIVATE const char *pipelined_requests =
    "POST /auth/login HTTP/1.1\r\n"
    "Host: localhost\r\n"
    "Origin: " ALLOWED_ORIGIN "\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: 48\r\n"
    "\r\n"
    "{\"username\": \"testuser\", \"password\": \"testpass\"}"
    "POST /auth/nowhere HTTP/1.1\r\n"
    "Host: localhost\r\n"
    "Origin: " ALLOWED_ORIGIN "\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: 2\r\n"
    "\r\n"
    "{}"
    "GET /auth/login HTTP/1.1\r\n"
    "Host: localhost\r\n"
    "Origin: " ALLOWED_ORIGIN "\r\n"
    "\r\n";
PRIVATE const int pipelined_statuses[MAX_PIPELINED] = {200, 404, 405};
#define PIPELINED_LATENCY_MS    300

/*
 *  Steps NSTEPS+1 to the last: a login and, behind it, one request more
 *  than the BFF lets wait for their answer (its pending_queue_size,
 *  main_test25), and many more: each on a new connection
 */
PRIVATE const int flood_requests[] = {17, 40, 600};
#define NFLOODS ((int)(sizeof(flood_requests)/sizeof(flood_requests[0])))
#define FLOOD_RECONNECT_MS      200     /* the BFF lets its channel go before the next connection */
PRIVATE const char *flood_request =
    "POST /auth/nowhere HTTP/1.1\r\n"
    "Host: localhost\r\n"
    "Origin: " ALLOWED_ORIGIN "\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: 2\r\n"
    "\r\n"
    "{}";

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int post_step(hgobj gobj);
PRIVATE int read_answers(hgobj gobj, const char **starts, int max);
PRIVATE void check_step(hgobj gobj);
PRIVATE void check_pipelined(hgobj gobj);
PRIVATE void check_flood(hgobj gobj);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type----------name-------------flag----default-description*/
SDATA (DTP_INTEGER,   "poll_ms",       SDF_RD, "50",   "Time between two looks at the socket for the answer of a step"),
SDATA (DTP_INTEGER,   "max_polls",     SDF_RD, "100",  "Looks at the socket before an answer is given up"),
SDATA (DTP_STRING,    "bff_port",      SDF_RD, "",     "Port of the BFF on the loopback"),
SDATA (DTP_POINTER,   "user_data",     0,      0,      "user data"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
    {"messages",    "Trace test driver flow"},
    {0, 0}
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj   timer;
    BOOL    dying;          /* TRUE once a death-timer is armed */
    int     step;           /* step whose answer is awaited; -1 before the first, NSTEPS the pipelined one, then the floods */
    BOOL    reconnecting;   /* the wait between two floods, for a new connection */
    int     fd_browser;     /* the plain socket of the requests */
    int     polls;          /* looks at the socket for the answer in flight */
    BOOL    eof;            /* the BFF closed the connection */
    size_t  got;            /* bytes of the answer in flight */
    char    answer[32*1024];
} PRIVATE_DATA;




            /******************************
             *      Framework Methods
             ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER0, 0, gobj);  // C_TIMER counts in seconds
    priv->fd_browser = -1;
    priv->step = -1;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->fd_browser >= 0) {
        close(priv->fd_browser);
        priv->fd_browser = -1;
    }
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_stop(priv->timer);
    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    set_timeout0(priv->timer, 500);
    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout0(priv->timer);
    return 0;
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *  The mock IdP of __idp_side__
 ***************************************************************************/
PRIVATE hgobj find_mock(hgobj gobj)
{
    hgobj idp_side = gobj_find_service("__idp_side__", FALSE);
    if(!idp_side) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", TEST_NAME ": __idp_side__ service not found",
            NULL
        );
        return NULL;
    }
    json_t *jn_filter = json_pack("{s:s}",
        "__gclass_name__", "C_MOCK_KEYCLOAK"
    );
    json_t *dl = gobj_match_children_tree(idp_side, jn_filter);
    hgobj mock = NULL;
    if(dl && json_array_size(dl) > 0) {
        mock = (hgobj)(size_t)json_integer_value(json_array_get(dl, 0));
    } else {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", TEST_NAME ": no C_MOCK_KEYCLOAK found under __idp_side__",
            NULL
        );
    }
    gobj_free_iter(dl);
    return mock;
}

/***************************************************************************
 *  The answer of the mock for the next request, and its form cleared
 ***************************************************************************/
PRIVATE int set_mock_answer(hgobj gobj, const char *body_patch,
    const char *raw_content_type, const char *raw_body, int latency_ms,
    int idp_status, const char *retry_after)
{
    hgobj mock = find_mock(gobj);
    if(!mock) {
        return -1;  // Error already logged
    }
    const char *patch = body_patch? body_patch : "{}";
    json_t *jn_patch = anystring2json(patch, strlen(patch), FALSE);
    gobj_write_json_attr(mock, "body_patch", jn_patch);
    JSON_DECREF(jn_patch)
    gobj_write_str_attr(mock, "raw_content_type", raw_content_type? raw_content_type : "");
    if(raw_body && strcmp(raw_body, BIG_BODY) == 0) {
        char *big = gbmem_malloc(BIG_BODY_LEN + 1);
        if(!big) {
            return -1;  // Error already logged
        }
        memset(big, 'A', BIG_BODY_LEN);
        big[BIG_BODY_LEN] = '\0';
        gobj_write_str_attr(mock, "raw_body", big);
        GBMEM_FREE(big)
    } else {
        gobj_write_str_attr(mock, "raw_body", raw_body? raw_body : "");
    }
    gobj_write_integer_attr(mock, "latency_ms", latency_ms);
    gobj_write_integer_attr(mock, "return_status", idp_status > 0? idp_status : 200);
    gobj_write_bool_attr(mock, "drop_token_request", idp_status == IDP_DROPS);
    gobj_write_str_attr(mock, "retry_after", retry_after? retry_after : "");
    gobj_write_str_attr(mock, "last_form", "");
    return 0;
}

/***************************************************************************
 *  Open the plain socket of the requests
 ***************************************************************************/
PRIVATE int connect_browser(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)atoi(gobj_read_str_attr(gobj, "bff_port")));

    priv->fd_browser = socket(AF_INET, SOCK_STREAM, 0);
    if(priv->fd_browser < 0 ||
            connect(priv->fd_browser, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", TEST_NAME ": cannot connect to the BFF",
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  The Cookie line of a step
 ***************************************************************************/
PRIVATE void build_cookie_line(const char *cookie, char *line, size_t size)
{
    line[0] = '\0';
    if(!cookie) {
        return;
    }
    if(cookie[0] == '@') {
        const char *name = strncmp(cookie, "@rt", 3) == 0? "refresh_token" : "access_token";
        int len = atoi(cookie + 3);
        int n = snprintf(line, size, "Cookie: %s=", name);
        memset(line + n, 'a', (size_t)len);
        snprintf(line + n + len, size - (size_t)(n + len), "\r\n");
        return;
    }
    snprintf(line, size, "Cookie: %s\r\n", cookie);
}

/***************************************************************************
 *  Send all of a request
 ***************************************************************************/
PRIVATE int send_request(hgobj gobj, const char *request, size_t len)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(send(priv->fd_browser, request, len, 0) != (ssize_t)len) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", TEST_NAME ": cannot post the request",
            "step",         "%d", priv->step + 1,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  The request of the current step, with the mock answer set for it
 ***************************************************************************/
PRIVATE int post_step(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->step == NSTEPS) {
        if(set_mock_answer(gobj, NULL, NULL, NULL, PIPELINED_LATENCY_MS, 0, NULL) < 0) {
            return -1;  // Error already logged
        }
        return send_request(gobj, pipelined_requests, strlen(pipelined_requests));
    }
    if(priv->step > NSTEPS) {
        if(priv->fd_browser < 0 && connect_browser(gobj) < 0) {
            return -1;  // Error already logged
        }
        if(set_mock_answer(gobj, NULL, NULL, NULL, PIPELINED_LATENCY_MS, 0, NULL) < 0) {
            return -1;  // Error already logged
        }
        int flood = flood_requests[priv->step - NSTEPS - 1];
        size_t login_len = strstr(pipelined_requests, "POST /auth/nowhere") - pipelined_requests;
        size_t one = strlen(flood_request);
        size_t len = login_len + (size_t)flood * one;
        char *requests = gbmem_malloc(len + 1);
        if(!requests) {
            return -1;  // Error already logged
        }
        memcpy(requests, pipelined_requests, login_len);
        for(int i = 0; i < flood; i++) {
            memcpy(requests + login_len + i * one, flood_request, one);
        }
        int ret = send_request(gobj, requests, len);
        GBMEM_FREE(requests)
        return ret;
    }

    const step_t *st = &steps[priv->step];
    if(set_mock_answer(gobj, st->body_patch, st->raw_content_type, st->raw_body, 0,
            st->idp_status, st->retry_after) < 0) {
        return -1;  // Error already logged
    }

    const char *body = st->body;
    if(!body) {
        body = strcmp(st->url, "/auth/login") == 0?
            "{\"username\": \"testuser\", \"password\": \"testpass\"}" : "{}";
    }
    char cookie_line[MAX_COOKIE_LEN + 64];
    build_cookie_line(st->cookie, cookie_line, sizeof(cookie_line));
    char origin_line[128] = "";
    const char *origin = st->origin? st->origin : ALLOWED_ORIGIN;
    if(strcmp(origin, NO_ORIGIN) != 0) {
        snprintf(origin_line, sizeof(origin_line), "Origin: %s\r\n", origin);
    }

    char request[MAX_COOKIE_LEN + 1024];
    int len = snprintf(request, sizeof(request),
        "%s %s HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "%s"
        "%s"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "\r\n"
        "%s",
        st->method? st->method : "POST",
        st->url,
        origin_line,
        cookie_line,
        (int)strlen(body), body
    );
    return send_request(gobj, request, (size_t)len);
}

/***************************************************************************
 *  Read what has come, and count the answers that are whole (their
 *  headers and the Content-Length of their body): their starts in
 *  `starts`, at most `max`.
 ***************************************************************************/
PRIVATE int read_answers(hgobj gobj, const char **starts, int max)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    while(priv->got < sizeof(priv->answer) - 1) {
        ssize_t n = recv(priv->fd_browser, priv->answer + priv->got,
            sizeof(priv->answer) - 1 - priv->got, MSG_DONTWAIT);
        if(n == 0) {
            priv->eof = TRUE;
        }
        if(n <= 0) {
            break;
        }
        priv->got += (size_t)n;
    }
    priv->answer[priv->got] = '\0';

    int whole = 0;
    const char *p = priv->answer;
    while(whole < max && *p) {
        const char *end_of_headers = strstr(p, "\r\n\r\n");
        const char *content_length = strcasestr(p, "Content-Length:");
        if(!end_of_headers || !content_length || content_length > end_of_headers) {
            break;
        }
        size_t body_len = (size_t)atoi(content_length + strlen("Content-Length:"));
        const char *next = end_of_headers + 4 + body_len;
        if(next > priv->answer + priv->got) {
            break;
        }
        starts[whole++] = p;
        p = next;
    }
    return whole;
}

/***************************************************************************
 *  A form, application/x-www-form-urlencoded, decoded: a dict of each key
 *  to the list of its values
 ***************************************************************************/
PRIVATE void decode_form_text(const char *text, size_t len, char *out)
{
    size_t o = 0;
    for(size_t i = 0; i < len; i++) {
        if(text[i] == '+') {
            out[o++] = ' ';
        } else if(text[i] == '%' && i + 2 < len) {
            char hex[3] = {text[i+1], text[i+2], 0};
            out[o++] = (char)strtol(hex, NULL, 16);
            i += 2;
        } else {
            out[o++] = text[i];
        }
    }
    out[o] = '\0';
}

PRIVATE json_t *decode_form(const char *form)
{
    json_t *jn_form = json_object();
    const char *p = form;
    while(*p) {
        size_t field_len = strcspn(p, "&");
        const char *eq = memchr(p, '=', field_len);
        size_t key_len = eq? (size_t)(eq - p) : field_len;
        char *key = gbmem_malloc(key_len + 1);
        char *value = gbmem_malloc(field_len + 1);
        if(key && value) {
            decode_form_text(p, key_len, key);
            if(eq) {
                decode_form_text(eq + 1, field_len - key_len - 1, value);
            } else {
                value[0] = '\0';
            }
            json_t *jn_values = json_object_get(jn_form, key);
            if(!jn_values) {
                jn_values = json_array();
                json_object_set_new(jn_form, key, jn_values);
            }
            json_array_append_new(jn_values, json_string(value));
        }
        GBMEM_FREE(key)
        GBMEM_FREE(value)
        p += field_len;
        if(*p == '&') {
            p++;
        }
    }
    return jn_form;
}

/***************************************************************************
 *  The form the IdP got in the current step
 ***************************************************************************/
PRIVATE void check_form(hgobj gobj, const step_t *st)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj mock = find_mock(gobj);
    if(!mock) {
        return;  // Error already logged
    }
    const char *form = gobj_read_str_attr(mock, "last_form");
    if(st->no_idp) {
        if(!empty_string(form)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_APP,
                "msg",          "%s", TEST_NAME ": the IdP was called, and must not be",
                "step",         "%d", priv->step + 1,
                "form",         "%.200s", form,
                NULL
            );
        }
        return;
    }
    if(!st->form_has[0] && !st->form_not[0]) {
        return;
    }

    json_t *jn_form = decode_form(form);
    for(int i = 0; st->form_has[i]; i++) {
        const char *expected = st->form_has[i];
        const char *eq = strchr(expected, '=');
        char key[64];
        snprintf(key, sizeof(key), "%.*s", (int)(eq - expected), expected);
        json_t *jn_values = json_object_get(jn_form, key);
        if(json_array_size(jn_values) != 1 ||
                strcmp(json_string_value(json_array_get(jn_values, 0)), eq + 1) != 0) {
            char *dump = json2uglystr(jn_values);
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_APP,
                "msg",          "%s", TEST_NAME ": the IdP form has not a key once, with its value",
                "step",         "%d", priv->step + 1,
                "expected",     "%s", expected,
                "values",       "%s", dump? dump : "",
                "form",         "%.300s", form,
                NULL
            );
            GBMEM_FREE(dump)
        }
    }
    for(int i = 0; st->form_not[i]; i++) {
        if(json_object_get(jn_form, st->form_not[i])) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_APP,
                "msg",          "%s", TEST_NAME ": the IdP form has a key it must not",
                "step",         "%d", priv->step + 1,
                "key",          "%s", st->form_not[i],
                "form",         "%.300s", form,
                NULL
            );
        }
    }
    JSON_DECREF(jn_form)
}

/***************************************************************************
 *  The answer of the current step
 ***************************************************************************/
PRIVATE void check_step(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const step_t *st = &steps[priv->step];
    const char *answer = priv->answer;

    check_form(gobj, st);

    char status_line[32];
    snprintf(status_line, sizeof(status_line), "HTTP/1.1 %d ", st->status);
    if(strncmp(answer, status_line, strlen(status_line)) != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", TEST_NAME ": an answer has not the status expected",
            "step",         "%d", priv->step + 1,
            "expected",     "%d", st->status,
            "answer",       "%.300s", answer,
            NULL
        );
        return;
    }
    for(int i = 0; st->must_have[i]; i++) {
        if(!strstr(answer, st->must_have[i])) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_APP,
                "msg",          "%s", TEST_NAME ": an answer lacks a text expected",
                "step",         "%d", priv->step + 1,
                "expected",     "%s", st->must_have[i],
                "answer",       "%.600s", answer,
                NULL
            );
        }
    }
    for(int i = 0; st->must_not[i]; i++) {
        if(strstr(answer, st->must_not[i])) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_APP,
                "msg",          "%s", TEST_NAME ": an answer carries a text it must not",
                "step",         "%d", priv->step + 1,
                "unexpected",   "%s", st->must_not[i],
                "answer",       "%.600s", answer,
                NULL
            );
        }
    }
}

/***************************************************************************
 *  The answers of the pipelined requests: in their order
 ***************************************************************************/
PRIVATE void check_pipelined(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *starts[MAX_PIPELINED];
    int whole = read_answers(gobj, starts, MAX_PIPELINED);
    if(whole != MAX_PIPELINED) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", TEST_NAME ": the pipelined requests did not get all their answers",
            "answers",      "%d", whole,
            "answer",       "%.600s", priv->answer,
            NULL
        );
        return;
    }
    for(int i = 0; i < MAX_PIPELINED; i++) {
        char status_line[32];
        snprintf(status_line, sizeof(status_line), "HTTP/1.1 %d ", pipelined_statuses[i]);
        if(strncmp(starts[i], status_line, strlen(status_line)) != 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_APP,
                "msg",          "%s", TEST_NAME ": a pipelined answer is out of its order",
                "position",     "%d", i + 1,
                "expected",     "%d", pipelined_statuses[i],
                "answer",       "%.40s", starts[i],
                NULL
            );
        }
    }
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *  More pipelined requests than the BFF lets wait: the connection dropped,
 *  with no answer
 ***************************************************************************/
PRIVATE void check_flood(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->eof || priv->got > 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", TEST_NAME ": too many pipelined requests did not drop the connection",
            "requests",     "%d", flood_requests[priv->step - NSTEPS - 1],
            "eof",          "%d", priv->eof? 1 : 0,
            "answer",       "%.200s", priv->answer,
            NULL
        );
    }
}

/***************************************************************************
 *  Timer fired: look for the answer of the step in flight (none before the
 *  first) and check it once whole, or when it is given up; then post the
 *  next one, or, after the last, stop and die.
 ***************************************************************************/
PRIVATE int ac_timer(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->dying) {
        JSON_DECREF(kw)
        set_yuno_must_die();
        return 0;
    }

    BOOL failed = FALSE;
    if(priv->reconnecting) {
        priv->reconnecting = FALSE;
    } else if(priv->step < 0) {
        failed = connect_browser(gobj) < 0;
    } else {
        const char *starts[MAX_PIPELINED];
        BOOL flood = priv->step > NSTEPS;
        int expected = priv->step == NSTEPS? MAX_PIPELINED : 1;
        BOOL done;
        if(flood) {
            read_answers(gobj, starts, 1);
            done = priv->eof;
        } else {
            done = read_answers(gobj, starts, expected) >= expected;
        }
        if(!done && ++priv->polls < (int)gobj_read_integer_attr(gobj, "max_polls")) {
            set_timeout0(priv->timer, gobj_read_integer_attr(gobj, "poll_ms"));
            JSON_DECREF(kw)
            return 0;
        }
        if(flood) {
            check_flood(gobj);
        } else if(priv->step == NSTEPS) {
            check_pipelined(gobj);
        } else {
            check_step(gobj);
        }
    }

    if(!failed && priv->step < NSTEPS + NFLOODS) {
        if(priv->step > NSTEPS && priv->fd_browser >= 0) {
            /*  The connection the BFF dropped: the next flood on a new one  */
            close(priv->fd_browser);
            priv->fd_browser = -1;
            priv->eof = FALSE;
            priv->reconnecting = TRUE;
            set_timeout0(priv->timer, FLOOD_RECONNECT_MS);
            JSON_DECREF(kw)
            return 0;
        }
        priv->step++;
        priv->polls = 0;
        priv->got = 0;
        priv->answer[0] = '\0';
        if(post_step(gobj) == 0) {
            set_timeout0(priv->timer, gobj_read_integer_attr(gobj, "poll_ms"));
            JSON_DECREF(kw)
            return 0;
        }
        // Error already logged
    }

    if(priv->fd_browser >= 0) {
        close(priv->fd_browser);
        priv->fd_browser = -1;
    }

    hgobj bff_side = gobj_find_service("__bff_side__", FALSE);
    if(bff_side) {
        gobj_stop_tree(bff_side);
    }
    hgobj idp_side = gobj_find_service("__idp_side__", FALSE);
    if(idp_side) {
        gobj_stop_tree(idp_side);
    }

    priv->dying = TRUE;
    set_timeout0(priv->timer, 100);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Volatil child stopped: destroy it. The C_TIMER0 publishes one at each
 *  cancel, and is not volatil.
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    if(gobj_is_volatil(src)) {
        gobj_destroy(src);
    }
    JSON_DECREF(kw)
    return 0;
}




                    /***************************
                     *      FSM
                     ***************************/




PRIVATE const GMETHODS gmt = {
    .mt_create  = mt_create,
    .mt_destroy = mt_destroy,
    .mt_start   = mt_start,
    .mt_stop    = mt_stop,
    .mt_play    = mt_play,
    .mt_pause   = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST25_UNTRUSTED_INPUT);

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL,
            "msg",      "%s", "GClass ALREADY created",
            "gclass",   "%s", gclass_name,
            NULL
        );
        return -1;
    }

    ev_action_t st_idle[] = {
        {EV_TIMEOUT,        ac_timer,       0},
        {EV_STOPPED,        ac_stopped,     0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,        0},
        {EV_STOPPED,        0},
        {0, 0}
    };

    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,              // lmt
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,              // authz_table
        0,              // command_table
        s_user_trace_level,
        0               // gcflag_t
    );
    if(!__gclass__) {
        return -1;
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_test25_untrusted_input(void)
{
    return create_gclass(C_TEST25_UNTRUSTED_INPUT);
}
