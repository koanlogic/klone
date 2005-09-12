#include <stdlib.h>
#include <string.h>
#include <klone/uri.h>
#include <klone/utils.h>

/* split a string separated by 'c' in two substrings */
static int split(const char *s, size_t len, char c, char **left, char **right)
{
    char *buf = 0;
    const char *p;
    char *l = 0, *r = 0;
    
    buf = u_strndup(s, len);
    if(!buf)
        goto err;

    if((p = strchr(buf, c)) != NULL)
    {
        l = u_strndup(s, p - buf);
        r = u_strndup(1 + p, len - (p - buf) - 1);
        if(!l || !r)
            goto err;
    } else {
        r = NULL;
        if((l = u_strndup(buf, len)) == NULL)
            goto err;
    }

    /* return result strings */
    *left = l;
    *right = r;

    u_free(buf);

    return 0;
err:
    if(buf)
        u_free(buf);
    if(l)
        u_free(l);
    if(r)
        u_free(r);
    return ~0;
}

static int parse_userinfo(const char *s, size_t len, uri_t *uri)
{
    return split(s, len, ':', &uri->user, &uri->pwd);
}

static int parse_hostinfo(const char *s, size_t len, uri_t *uri)
{
    char *port = 0;

    if(split(s, len, ':', &uri->host, &port))
        return ~0;

    if(port)
    {
        uri->port = atoi(port);
        u_free(port);
    }
    return 0;
}


static int parse_middle(const char *s, size_t len, uri_t *uri)
{
    const char *p;

    if( (p = strchr(s, '@')) == NULL)
        return parse_hostinfo(s, len, uri);
    else
        return parse_userinfo(s, p-s,uri) + parse_hostinfo(1+p, s+len-p-1, uri);
}

/**
 *  \ingroup utils 
 *  \{
 *      \par
 */

void free_uri(uri_t *uri)
{
    if(uri->scheme)
        u_free(uri->scheme);
    if(uri->user)
        u_free(uri->user);
    if(uri->pwd)
        u_free(uri->pwd);
    if(uri->host)
        u_free(uri->host);
    if(uri->path)
        u_free(uri->path);
    u_free(uri);
}

int uri_parse(const char *s, uri_t **pu)
{
    const char *p, *p0;
    int i;
    uri_t *uri;

    if( (uri = (uri_t*)u_calloc(sizeof(uri_t))) == NULL)
        return ~0;

    if((p = strchr(s, ':')) == NULL)
        goto err; /* malformed */

    /* save the schema string */
    if((uri->scheme = u_strndup(s, p - s)) == NULL)
        goto err;

    /* skip ':' */
    p++; 

    /* skip "//" */
    for(i = 0; i < 2; ++i, ++p)
        if(!p || *p == 0 || *p != '/')
            goto err; /* malformed */

    /* save p */
    p0 = p; 

    /* find the first path char ('/') or the end of the string */
    while(*p && *p != '/')
        ++p;

    /* parse userinfo and hostinfo */
    if(p - p0 && parse_middle(p0, p - p0, uri))
        goto err;

    /* save path */
    if(*p && (uri->path = u_strdup(p)) == NULL)
        goto err;

    *pu = uri;

    return 0;
err:
    if(uri)
        free_uri(uri);
    return ~0;
}


/**
 *  \}
 */


