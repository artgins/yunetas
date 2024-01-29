#include <criterion/criterion.h>
#include <stdlib.h>

//#include "foo.h"

Test(gobj, empty)
{
    cr_assert_eq(0, 0);
}

Test(gobj, simple)
{
    cr_assert_eq(0, 0);
}
