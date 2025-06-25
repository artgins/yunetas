#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <argp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>

const char *argp_program_version = "keycloak_pkey_to_jwks 1.0";
const char *argp_program_bug_address = "<bugs@example.com>";
static char doc[] = "Convert Keycloak base64url RSA modulus to JWKS using issuer as kid";
static char args_doc[] = "";

static struct argp_option options[] = {
    { "iss",  'i', "ISSUER",   0, "Issuer URL (required, also used as kid)", 0 },
    { "pkey", 'p', "MODULUS",  0, "Base64url-encoded RSA modulus (n) (required)", 0 },
    { "exp",  'e', "EXPONENT", 0, "Base64url-encoded exponent (default: AQAB)", 0 },
    { 0 }
};

struct arguments {
    const char *iss;
    const char *n;
    const char *e;
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    struct arguments *args = state->input;

    switch (key) {
        case 'i':
            args->iss = arg;
            break;
        case 'p':
            args->n = arg;
            break;
        case 'e':
            args->e = arg;
            break;
        case ARGP_KEY_END:
            if (!args->iss || !args->n)
                argp_usage(state);
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = {
    .options = options,
    .parser = parse_opt,
    .args_doc = args_doc,
    .doc = doc,
    .children = NULL,
    .help_filter = NULL,
    .argp_domain = NULL
};

void print_jwk(const char *n_b64url, const char *e_b64url, const char *iss) {
    printf("{\n");
    printf("    \"keys\": [\n");
    printf("        {\n");
    printf("            \"kty\": \"RSA\",\n");
    printf("            \"use\": \"sig\",\n");
    printf("            \"alg\": \"RS256\",\n");
    printf("            \"kid\": \"%s\",\n", iss);
    printf("            \"n\": \"%s\",\n", n_b64url);
    printf("            \"e\": \"%s\"\n", e_b64url);
    printf("        }\n");
    printf("    ]\n");
    printf("}\n");
}

int main(int argc, char **argv) {
    struct arguments args = {
        .iss = NULL,
        .n   = NULL,
        .e   = "AQAB"
    };

    argp_parse(&argp, argc, argv, 0, 0, &args);
    print_jwk(args.n, args.e, args.iss);
    return 0;
}
