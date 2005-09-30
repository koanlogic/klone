#include <errno.h>
#include <klone/klone.h>
#include <klone/str.h>
#include <klone/utils.h>
#include <klone/debug.h>

/**
 *  \defgroup string_t string_t - string object
 *  \{
 *      \par
 */


/* null strings will be bound to the null char* */
static char null_cstr[1] = { 0 };
static char* null = &null_cstr;

/* internal string struct */
struct string_s
{
    char *data;
    size_t data_sz, data_len, shift_cnt;
};

/**
 * \brief  Return the string length
 *
 * Return the length of the given string.
 *
 * \param s     string object
 *
 * \return the string length
 */
inline size_t string_len(string_t *s)
{
    return s->data_len;
}

/**
 * \brief  Return the string value
 *
 * Return the const char* value of the given string object. Such const char*
 * value cannot be modified, realloc'd or free'd.
 *
 * \param s     string object
 *
 * \return the string value or NULL if the string is empty
 */
inline const char *string_c(string_t *s)
{
    return s->data;
}

int string_sql_encode(string_t *s)
{
    char *buf = NULL;
    size_t bufsz = 0;

    dbg_err_if(s->data_len == 0);

    bufsz = (string_len(s) * 2) + 1;

    buf = u_malloc(bufsz);
    dbg_err_if(buf == NULL);

    dbg_err_if(u_sqlncpy(buf, string_c(s), string_len(s), SQLCPY_ENCODE) <= 0);

    dbg_err_if(string_set(s, buf, strlen(buf)));

    u_free(buf);

    return 0;
err:
    if(buf)
        u_free(buf);
    return ~0;
}

int string_sql_decode(string_t *s)
{
    char *buf = NULL;
    size_t bufsz = 0;

    dbg_err_if(s->data_len == 0);

    bufsz = string_len(s) + 1;

    buf = u_malloc(bufsz);
    dbg_err_if(buf == NULL);

    dbg_err_if(u_sqlncpy(buf, string_c(s), string_len(s), SQLCPY_DECODE) <= 0);

    dbg_err_if(string_set(s, buf, strlen(buf)));

    u_free(buf);

    return 0;
err:
    if(buf)
        u_free(buf);
    return ~0;
}

/**
 * \brief  Remove leading and trailing blanks
 *
 * Remove leading and trailing blanks from the given string
 *
 * \param s     string object
 *
 * \return \c 0 on success, not zero on failure
 */
int string_trim(string_t *s)
{
    if(s->data_len)
    {
        u_trim(s->data);

        s->data_len = strlen(s->data);
    }

    return 0;
}

/**
 * \brief  Translate unsafe characters to HTML entities
 *
 * Translate ", ', < and > to &quot;, &#39;, &lt;, $gt;
 *
 * \param s     string object
 *
 * \return \c 0 on success, not zero on failure
 */
int string_html_encode(string_t *s)
{
    char *buf = NULL;
    size_t bufsz = 0;

    dbg_err_if(s->data_len == 0);

    bufsz = (string_len(s) * 6) + 1;

    buf = u_malloc(bufsz);
    dbg_err_if(buf == NULL);

    dbg_err_if(u_htmlncpy(buf, string_c(s), string_len(s), HTMLCPY_ENCODE) <=0);

    dbg_err_if(string_set(s, buf, strlen(buf)));

    u_free(buf);

    return 0;
err:
    if(buf)
        u_free(buf);
    return ~0;
}

/**
 * \brief  Translate HTML entities to the char they represent
 *
 * Translate  &quot;, &#39;, &lt;, $gt; to ", ', < and > 
 *
 * \param s     string object
 *
 * \return \c 0 on success, not zero on failure
 */
int string_html_decode(string_t *s)
{
    char *buf = NULL;
    size_t bufsz = 0;

    dbg_err_if(s->data_len == 0);

    bufsz = string_len(s) + 1;

    buf = u_malloc(bufsz);
    dbg_err_if(buf == NULL);

    dbg_err_if(u_htmlncpy(buf, string_c(s), string_len(s), HTMLCPY_DECODE) <=0);

    dbg_err_if(string_set(s, buf, strlen(buf)));

    u_free(buf);

    return 0;
err:
    if(buf)
        u_free(buf);
    return ~0;
}

/**
 * \brief  URL-encode the given string
 *
 * Replace any non-alphanumeric character except "-_." with a '%' followed by
 * the character ASCII integer value in hex notation.
 *
 * \param s     string object
 *
 * \return \c 0 on success, not zero on failure
 */
int string_url_encode(string_t *s)
{
    char *buf = NULL;
    size_t bufsz = 0;

    dbg_err_if(s->data_len == 0);

    bufsz = (string_len(s) * 3) + 1;

    buf = u_malloc(bufsz);
    dbg_err_if(buf == NULL);

    dbg_err_if(u_urlncpy(buf, string_c(s), string_len(s), URLCPY_ENCODE) <= 0);

    dbg_err_if(string_set(s, buf, strlen(buf)));

    u_free(buf);

    return 0;
err:
    if(buf)
        u_free(buf);
    return ~0;
}

/**
 * \brief  URL-decode the given string
 *
 * Replace '%' sign followed by two hex digit with the corresponding ASCII value
 *
 * \param s     string object
 *
 * \return \c 0 on success, not zero on failure
 */
int string_url_decode(string_t *s)
{
    char *buf = NULL;
    size_t bufsz = 0;

    dbg_err_if(s->data_len == 0);

    bufsz = string_len(s) + 1;

    buf = u_malloc(bufsz);
    dbg_err_if(buf == NULL);

    dbg_err_if(u_urlncpy(buf, string_c(s), string_len(s), URLCPY_DECODE));

    dbg_err_if(string_set(s, buf, strlen(buf)));

    u_free(buf);

    return 0;
err:
    if(buf)
        u_free(buf);
    return ~0;
}

/**
 * \brief  Set the length of a string (shortening it)
 * 
 *
 * \param s     string object
 * \param len   on success \a s will be \a len chars long
 *
 * \return \c 0 on success, not zero on failure
 */
int string_set_length(string_t *s, size_t len)
{
    dbg_err_if(len > s->data_len);

    if(len < s->data_len)
    {
        s->data_len = len;
        s->data[len] = 0;
    }

    return 0;
err:
    return ~0;
}


/**
 * \brief  Copy the value of a string to another
 *
 * Copy \a src string to \a dst string. 
 *
 * \param dst   destination string
 * \param src   source string
 *
 * \return \c 0 on success, not zero on failure
 */
inline int string_copy(string_t *dst, string_t *src)
{
    string_clear(dst);
    return string_append(dst, src->data, src->data_len);
}

/**
 * \brief  Clear a string
 *
 * Totally erase the content of the given string.
 *
 * \param s     string object
 *
 * \return \c 0 on success, not zero on failure
 */
int string_clear(string_t *s)
{
    /* clear the string but not deallocate the buffer */
    if(s->data_sz)
    {
        s->data[0] = 0;
        s->data_len = 0;
    }

    return 0;
}

/**
 * \brief  Create a new string
 *
 * Create a new string object and save its pointer to \a *ps.
 *
 * If \a buf is not NULL (and \a len > 0) the string will be initialized with
 * the content of \a buf.
 *
 * \param buf   initial string value
 * \param len   length of \a buf
 * \param ps    on success will get the new string object
 *
 * \return \c 0 on success, not zero on failure
 */
int string_create(const char *buf, size_t len, string_t **ps)
{
    string_t *s = NULL;

    s = u_calloc(sizeof(string_t));
    dbg_err_if(s == NULL);

    s->data = null;

    if(buf)
        dbg_err_if(string_append(s, buf, len));

    *ps = s;

    return 0;
err:
    dbg_strerror(errno);
    return ~0;
}


/**
 * \brief  Free a string
 *
 * Release all resources and free the given string object.
 *
 * \param s     string object
 *
 * \return \c 0 on success, not zero on failure
 */
int string_free(string_t *s)
{
    if(s)
    {
        if(s->data_sz)
            u_free(s->data);
        u_free(s);
    }
    return 0;
}


/**
 * \brief  Set the value of a string
 *
 * Set the value of \a s to \a buf.
 *
 * \param s     string object
 * \param buf   the value that will be copied to \a s
 * \param len   length of \a buf
 *
 * \return \c 0 on success, not zero on failure
 */
int string_set(string_t *s, const char *buf, size_t len)
{
    string_clear(s);
    return string_append(s, buf, len);
}

/**
 * \brief  Append a char* to a string
 *
 * Append a char* value to the given string. 
 *
 * \param s     string object
 * \param buf   the value that will be appended to \a s
 * \param len   length of \a buf
 *
 * \return \c 0 on success, not zero on failure
 */
int string_append(string_t *s, const char *buf, size_t len)
{
    char *ndata;
    size_t nsz, min;

    if(!len)
        return 0; /* nothing to do */

    /* if there's not enough space on pc->data alloc a bigger buffer */
    if(s->data_len + len + 1 > s->data_sz)
    {
        min = s->data_len + len + 1; /* min required buffer length */
        nsz = s->data_sz;
        while(nsz <= min)
            nsz += (BLOCK_SIZE << s->shift_cnt++);
        if(s->data == null)
            s->data = NULL;
        ndata = (char*)u_realloc(s->data, nsz);
        dbg_err_if(ndata == NULL);
        s->data = ndata;
        s->data_sz = nsz;
    }

    /* append this chunk to the data buffer */
    strncpy(s->data + s->data_len, buf, len);
    s->data_len += len;
    s->data[s->data_len] = 0;
    
    return 0;
err:
    return ~0;
}


/**
 *  \}
 */


