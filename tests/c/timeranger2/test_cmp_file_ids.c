/****************************************************************************
 *          test_cmp_file_ids.c
 *
 *  The order of two md2 file ids is the order of their NAMES, as the load
 *  sorts the md2 files of a key: strcmp("<a>.md2", "<b>.md2"). The cache
 *  cells of a key are kept in that order (find_cache_cell), and a global
 *  rowid is a position in it, so the comparison must give the same sign as
 *  the names for every pair of ids.
 *
 *  cmp_file_ids() compares in place, without building the two names (it
 *  runs on every append). This test checks it against the names built with
 *  snprintf(): a list of cases where the suffix decides (one id is the
 *  start of the other, the next char is below or above '.', ids made of
 *  the suffix's own chars, bytes above 127), then pseudo-random pairs from
 *  a small alphabet around '.'.
 *
 *  A unit test: it #includes timeranger2.c to reach the PRIVATE function.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "timeranger2.c"

/***************************************************************************
 *  The reference: the two names, compared whole
 ***************************************************************************/
PRIVATE int cmp_names(const char *a, const char *b)
{
    char a_[2*NAME_MAX];
    char b_[2*NAME_MAX];
    snprintf(a_, sizeof(a_), "%s.md2", a);
    snprintf(b_, sizeof(b_), "%s.md2", b);
    return strcmp(a_, b_);
}

PRIVATE int sign(int x)
{
    return (x > 0) - (x < 0);
}

PRIVATE int check_pair(const char *a, const char *b)
{
    int got = sign(cmp_file_ids(a, b));
    int expected = sign(cmp_names(a, b));
    if(got != expected) {
        printf("%sFAILED%s cmp_file_ids(\"%s\", \"%s\") = %d, the names say %d\n",
            On_Red BWhite, Color_Off, a, b, got, expected
        );
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  Deterministic pseudo-random numbers: the same pairs on every run
 ***************************************************************************/
PRIVATE uint32_t lcg_state = 12345;

PRIVATE uint32_t lcg_next(void)
{
    lcg_state = lcg_state * 1103515245u + 12345u;
    return lcg_state >> 8;
}

/***************************************************************************
 *
 ***************************************************************************/
int main(int argc, char *argv[])
{
    int errors = 0;

    /*
     *  Cases where the suffix decides, and plain ones
     */
    const char *cases[][2] = {
        {"", ""},
        {"", "a"},
        {"a", "a"},
        {"a", "b"},
        {"a", "a-b"},           // '-' < '.': "a-b.md2" < "a.md2"
        {"a", "a/"},            // '/' > '.'
        {"a", "a0"},
        {"a", "a."},
        {"a", "a.m"},
        {"a", "a.md"},
        {"a", "a.md2"},
        {"a", "a.md2x"},
        {"a", "a.md3"},
        {"a", "a.mc"},
        {".md2", ""},
        {".md", "."},
        {"2024-01-01", "2024-01-01"},
        {"2024-01-01", "2024-01-02"},
        {"2024-1", "2024-1-5"},
        {"2024-1", "2024-10"},
        {"2024-1-1", "2024-1-15"},
        {"x\x7f", "x"},
        {"x\x80", "x"},         // bytes above 127 compare unsigned, as strcmp()
        {"x\xff", "x.md2"},
        {"\xff", ""},
    };
    int n_cases = (int)(sizeof(cases)/sizeof(cases[0]));
    for(int i = 0; i < n_cases; i++) {
        if(check_pair(cases[i][0], cases[i][1]) < 0) {
            errors++;
        }
        if(check_pair(cases[i][1], cases[i][0]) < 0) {
            errors++;
        }
    }

    /*
     *  Pseudo-random pairs over an alphabet around '.', half of them with
     *  a common start (where the suffix is reached)
     */
    static const char alphabet[] = "ab.-2md0~\x80\xff";
    const int alphabet_len = (int)sizeof(alphabet) - 1;
    const int n_pairs = 2000000;
    char a[16];
    char b[16];
    for(int n = 0; n < n_pairs && errors < 10; n++) {
        int la = (int)(lcg_next() % 7);
        for(int i = 0; i < la; i++) {
            a[i] = alphabet[lcg_next() % (uint32_t)alphabet_len];
        }
        a[la] = 0;
        if(lcg_next() % 2) {
            int lb = (int)(lcg_next() % (uint32_t)(la + 1));   // a start of a, then more
            memcpy(b, a, (size_t)lb);
            int extra = (int)(lcg_next() % 5);
            for(int i = 0; i < extra; i++) {
                b[lb + i] = alphabet[lcg_next() % (uint32_t)alphabet_len];
            }
            b[lb + extra] = 0;
        } else {
            int lb = (int)(lcg_next() % 7);
            for(int i = 0; i < lb; i++) {
                b[i] = alphabet[lcg_next() % (uint32_t)alphabet_len];
            }
            b[lb] = 0;
        }
        if(check_pair(a, b) < 0) {
            errors++;
        }
    }

    if(errors) {
        printf("%sFAILED%s test_cmp_file_ids: %d mismatches\n", On_Red BWhite, Color_Off, errors);
        return -1;
    }
    printf("test_cmp_file_ids: %d cases and %d pairs agree with the names\n", 2*n_cases, n_pairs);
    return 0;
}
