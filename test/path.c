#include <u/libu.h>
#include <klone/utils.h>

struct path_vec_s
{
    const char *src, *exp;
};

const struct path_vec_s paths[] = {
    { "/",              "/" },
    { "/",              "/" },
    { "//.",            "/" },
    { "///.",           "/" },
    { "//.//",          "/" },
    { "//.//",          "/" },
    { "/.",             "/" },
    { "/..",            "/" },
    { "/../",           "/" },
    { "/a",             "/a" },
    { "/a/",            "/a/" },
    { "/a/..",          "/" },
    { "/a/../",         "/" },
    { "/../a",          "/a" },
    { "/../../../",     "/" },
    { "/a/b/.../c",     "/a/b/c" },
    { "/a/b/./c",       "/a/b/c" },
    { "/a/b/./c/",      "/a/b/c/" },
    { "/a/b/../c",      "/a/c" },
    { "/a/b/../c/",     "/a/c/" },
    { NULL, NULL }
};

static int test_normalize(void)
{
    char buf[512], *src, *exp;
    int i;

    for(i = 0; paths[i].src != NULL; ++i)
    {
        src = paths[i].src;
        exp = paths[i].exp; /* expected result */

        strlcpy(buf, src, sizeof(buf));

        con_err_if(u_path_normalize(buf));

        con_err_if(strcmp(buf, exp));
    }
    return 0;
err:
    if(src && exp && buf)
        con("src: [%s]  exp: [%s]   norm: [%s]: FAILED", src, exp, buf);
    return ~0;
}

U_TEST_MODULE( path )
{

    U_TEST_RUN( test_normalize );

    return 0;                                                
}

