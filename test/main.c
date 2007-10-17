#include <u/libu.h>

int facility = LOG_LOCAL0;

/* run ./runtests -h for help */

int main(int argc, char**argv)
{
    U_TEST_MODULE_USE( misc );
    U_TEST_MODULE_USE( path );

    return u_test_run(argc, argv);
}

