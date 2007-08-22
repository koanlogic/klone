#include <u/libu.h>
#include <klone/utils.h>

struct htmlenc_vec_s
{
    const char *src, *exp;
    int ssz, esz; /* source and exp size */
};

#define HTMLENC_VEC( src, exp ) { src, exp, (sizeof(src)-1), (sizeof(exp)-1) }

const struct htmlenc_vec_s htmlenc_vec[] = {
    HTMLENC_VEC( "", ""),
    HTMLENC_VEC( "a", "a"),
    HTMLENC_VEC( "ab", "ab"),
    HTMLENC_VEC( "abc", "abc"),
    HTMLENC_VEC( "abcd", "abcd"),
    HTMLENC_VEC( "ab&cd", "ab&amp;cd"),
    HTMLENC_VEC( "ab<cd", "ab&lt;cd"),
    HTMLENC_VEC( "&", "&amp;"),
    HTMLENC_VEC( "<", "&lt;"),
    HTMLENC_VEC( ">", "&gt;"),
    HTMLENC_VEC( "\"", "&quot;"),
    HTMLENC_VEC( "\'", "&#39;"),
    HTMLENC_VEC( "&<>\"\'", "&amp;&lt;&gt;&quot;&#39;"),
    HTMLENC_VEC( "A&<>\"\'Z", "A&amp;&lt;&gt;&quot;&#39;Z"),
    HTMLENC_VEC( "A&B<C>D\"E\'Z", "A&amp;B&lt;C&gt;D&quot;E&#39;Z"),

    HTMLENC_VEC( "\x01", "\x01"), /* not printable chars are not encoded */
    // FIXME: there are other HTML entities?
    { NULL, NULL, 0 }
};

int test_hexencoding(void)
{
    /* +1 is to also encode '\0' */
    unsigned char src[255 + 1], buf0[1024], buf1[1024];
    ssize_t wr;
    int i;
    
    memset(src, '*', sizeof(src));
    memset(buf0, '*', sizeof(buf0));
    memset(buf1, '*', sizeof(buf1));

    for(i = 0; i < 256; ++i)
        src[i] = 255 - i;

    con_err_if((wr = u_hexncpy(buf0, src, sizeof(src), HEXCPY_ENCODE)) < 0);

    con_err_if((wr = u_hexncpy(buf1, buf0, wr, HEXCPY_DECODE)) < 0);

    con_err_if(memcmp(src, buf1, sizeof(src)));

    return 0;
err:
    return ~0;
}

int test_sqlencoding(void)
{
    /* +1 is to also encode '\0' */
    unsigned char src[255 + 1], buf0[1024], buf1[1024];
    ssize_t wr;
    int i;
    
    memset(src, '*', sizeof(src));
    memset(buf0, '*', sizeof(buf0));
    memset(buf1, '*', sizeof(buf1));

    for(i = 0; i < 256; ++i)
        src[i] = 255 - i;

    con_err_if((wr = u_sqlncpy(buf0, src, sizeof(src), SQLCPY_ENCODE)) < 0);

    con_err_if((wr = u_sqlncpy(buf1, buf0, wr, SQLCPY_DECODE)) < 0);

    con_err_if(memcmp(src, buf1, sizeof(src)));

    return 0;
err:
    return ~0;
}

int test_htmlencoding_1(void)
{
    enum { BUFSZ = 1024 };
    const char *src, *exp;
    char buf0[BUFSZ], buf1[BUFSZ];
    ssize_t wr;
    int i, ssz, esz;

    for(i = 0; htmlenc_vec[i].src != NULL; ++i)
    {
        src = htmlenc_vec[i].src;
        exp = htmlenc_vec[i].exp; /* expected result */
        ssz = htmlenc_vec[i].ssz; 
        esz = htmlenc_vec[i].esz; 

        con_err_if((wr = u_htmlncpy(buf0, src, ssz, HTMLCPY_ENCODE)) < 0);

        con_err_if(memcmp(buf0, exp, esz));

        con_err_if((wr = u_htmlncpy(buf1, buf0, wr, HTMLCPY_DECODE)) < 0);

        con_err_ifm(wr != ssz, "%ld  %d", wr, ssz);

        con_err_if(memcmp(src, buf1, ssz));
    }

    return 0;
err:
    return ~0;
}

int test_htmlencoding_0(void)
{
    /* +1 is to also encode '\0' */
    unsigned char src[255 + 1], buf0[1024], buf1[1024];
    ssize_t wr;
    int i;
    
    memset(src, '*', sizeof(src));
    memset(buf0, '*', sizeof(buf0));
    memset(buf1, '*', sizeof(buf1));

    for(i = 0; i < 256; ++i)
        src[i] = 255 - i;

    con_err_if((wr = u_htmlncpy(buf0, src, sizeof(src), HTMLCPY_ENCODE)) < 0);

    con_err_if((wr = u_htmlncpy(buf1, buf0, wr, HTMLCPY_DECODE)) < 0);

    for(i = 0; i < 256; ++i)
        if(src[i] != buf1[i])
            con("%d:  %x   %x", i, src[i], buf1[i]);

    con_err_if(memcmp(src, buf1, sizeof(src)));

    return 0;
err:
    return ~0;
}

int test_urlencoding(void)
{
    /* +1 is to also encode '\0' */
    unsigned char src[255 + 1], buf0[1024], buf1[1024];
    ssize_t wr;
    int i;
    
    memset(src, '*', sizeof(src));
    memset(buf0, '*', sizeof(buf0));
    memset(buf1, '*', sizeof(buf1));

    for(i = 0; i < 256; ++i)
        src[i] = 255 - i;

    con_err_if((wr = u_urlncpy(buf0, src, sizeof(src), URLCPY_ENCODE)) < 0);

    con_err_if((wr = u_urlncpy(buf1, buf0, wr, URLCPY_DECODE)) < 0);

    con_err_if(memcmp(src, buf1, sizeof(src)));

    return 0;
err:
    return ~0;
}

U_TEST_MODULE( misc )
{
    U_TEST_RUN( test_htmlencoding_0 );
    U_TEST_RUN( test_htmlencoding_1 );

    U_TEST_RUN( test_urlencoding );

    U_TEST_RUN( test_hexencoding );

    U_TEST_RUN( test_sqlencoding );

    return 0;                                                
}

