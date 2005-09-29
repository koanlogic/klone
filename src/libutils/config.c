#include <sys/types.h>
#include <klone/queue.h>
#include <klone/config.h>
#include <klone/debug.h>
#include <klone/utils.h>
#include <klone/str.h>
#include <klone/io.h>

TAILQ_HEAD(config_list_s, config_s);
typedef struct config_list_s config_list_t;

struct config_s
{
    TAILQ_ENTRY(config_s) np; /* next & prev pointers */
    char *key;                /* config item key name */
    char *value;              /* config item value    */
    config_list_t children;   /* subkeys              */
    config_t *parent;         /* parent config obj    */
};

void config_print(config_t *c, int lev)
{
    config_t *item;
    int i;

    for(i = 0; i < lev; ++i)
        printf("  ");
    printf("%s: %s\n", c->key, c->value);

    ++lev;
    TAILQ_FOREACH(item, &c->children, np)
        config_print(item, lev);
}

/**
 *  \defgroup config_t config_t - Configuration handling
 *  \{
 *      \par
 */


int config_add_child(config_t *c, const char *key, config_t **pc)
{
    config_t *child = NULL;

    dbg_err_if(config_create(&child));

    child->parent = c;
    child->key = u_strdup(key);
    dbg_err_if(child->key == NULL);

    TAILQ_INSERT_TAIL(&c->children, child, np);

    *pc = child;

    return 0;
err:
    return ~0;
}

/* get n-th child item called key */
config_t* config_get_child_n(config_t *c, const char *key, int n)
{
    config_t *item;

    TAILQ_FOREACH(item, &c->children, np)
    {
        if(strcmp(item->key, key) == 0 && n-- == 0)
            return item;  /* found */
    }

    return NULL; /* not found */
}

config_t* config_get_child(config_t *c, const char *key)
{
    return config_get_child_n(c, key, 0);
}

int config_get_subkey_nth(config_t *c,const char *subkey, int n, config_t **pc)
{
    config_t *child = NULL;
    char *first_key = NULL, *p;

    if((p = strchr(subkey, '.')) == NULL)
    {
        if((child = config_get_child_n(c, subkey, n)) != NULL)
        {
            *pc = child;
            return 0;
        } 
    } else {
        if((first_key = u_strndup(subkey, p-subkey)) != NULL)
        {
            child = config_get_child(c, first_key);
            u_free(first_key);
        }
        if(child != NULL)
            return config_get_subkey(child, ++p, pc);
    }
    return ~0; /* not found */

}

int config_get_subkey(config_t *c, const char *subkey, config_t **pc)
{
    return config_get_subkey_nth(c, subkey, 0, pc);
}

static config_t* config_get_root(config_t *c)
{
    while(c->parent)
        c = c->parent;
    return c;
}

static int config_set_value(config_t *c, const char *val)
{
    config_t *root, *ignore;
    const char *varval, *vs, *ve, *p;
    string_t *var = NULL, *value = NULL;

    dbg_err_if(c == NULL);

    /* free() previous value if any */
    if(c->value)
    {
        free(c->value);
        c->value = NULL;
    } 

    if(val)
    {
        dbg_err_if(string_create(NULL, 0, &var));
        dbg_err_if(string_create(NULL, 0, &value));

        root = config_get_root(c);
        dbg_err_if(root == NULL);

        /* search and replace ${variables} */
        vs = ve = val;
        for(; vs && *vs && (p = strstr(vs, "${")) != NULL; vs = ++ve)
        {   /* variable substituion */
            dbg_err_if(string_append(value, vs, p-vs));

            /* skip ${ */
            vs = p+2;               

            /* look for closig bracket */
            ve = strchr(vs, '}');
            dbg_err_if(ve == NULL); /* closing bracket missing */

            /* get the variable name/path */
            dbg_err_if(string_set(var, vs, ve-vs));

            /* first see if the variable can be resolved in the local scope
               otherwise resolve it from the root */
            root = c->parent;
            if(config_get_subkey(root, string_c(var), &ignore))
                root = config_get_root(c);
            dbg_err_if(root == NULL);

            /* append the variable value */
            if((varval = config_get_subkey_value(root, string_c(var)))!= NULL)
                dbg_err_if(string_append(value, varval, strlen(varval)));
        }
        if(ve && *ve)
            dbg_err_if(string_append(value, ve, strlen(ve)));

        string_trim(value); /* remove leading and trailing spaces */

        c->value = strdup(string_c(value));;
        dbg_err_if(c->value == NULL);

        string_free(value);
        string_free(var);
    }

    return 0;
err:
    if(value)
        string_free(value);
    if(var)
        string_free(var);
    return ~0;
}

static int config_do_set_key(config_t *c, const char *key, const char *val, 
    int overwrite)
{
    config_t *child = NULL;
    char *p, *child_key;

    if((p = strchr(key, '.')) == NULL)
    {
        child = config_get_child(c, key);
        if(child == NULL || !overwrite)
        {   /* there's no such child, add a new child */
            dbg_err_if(config_add_child(c, key, &child));
        } 
        dbg_err_if(config_set_value(child, val));
    } else {
        child_key = u_strndup(key, p-key);
        dbg_err_if(child_key == NULL);
        if((child = config_get_child(c, child_key)) == NULL)
            dbg_err_if(config_add_child(c, child_key, &child));
        free(child_key);
        return config_set_key(child, ++p, val);
    }
    return 0;
err:
    return ~0;
}

int config_add_key(config_t *c, const char *key, const char *val)
{
    return config_do_set_key(c, key, val, 0 /* don't overwrite */);
}

int config_set_key(config_t *c, const char *key, const char *val)
{
    return config_do_set_key(c, key, val, 1 /* overwrite */);
}

int config_load(config_t *c, io_t *io, int overwrite)
{
    enum { MAX_NEST_LEV = 20 };
    string_t *line = NULL, *key = NULL, *lastkey = NULL, 
             *sticky = NULL, *value = NULL;
    const char *ln, *p, *pv;
    size_t len;
    int level = 0, lineno = 1;
    short sticky_len[MAX_NEST_LEV];

    dbg_err_if(string_create(NULL, 0, &line));
    dbg_err_if(string_create(NULL, 0, &key));
    dbg_err_if(string_create(NULL, 0, &value));
    dbg_err_if(string_create(NULL, 0, &lastkey));
    dbg_err_if(string_create(NULL, 0, &sticky));

    for(; u_getline(io, line) == 0; string_clear(line), ++lineno)
    {
        /* remove comments if any */
        if((p = strchr(string_c(line), '#')) != NULL)
            dbg_err_if(string_set_length(line, p - string_c(line)));

        /* remove leading and trailing blanks */
        dbg_err_if(string_trim(line));

        ln = string_c(line);
        len = string_len(line);

        /* remove trailing nl(s) */
        while(len && u_isnl(ln[len-1]))
            string_set_length(line, --len);

        if(len == 0)
            continue; /* empty line */

        /* eat leading blanks */
        for(; u_isblank(*ln); ++ln);

        if(ln[0] == '{')
        {   /* group config values */
            if(string_len(lastkey) == 0)
                warn_err("config error [line %d]: { not after a no-value key", 
                         lineno);
            warn_err_ifm(++level == MAX_NEST_LEV, 
                "config error: too much nesting levels");
            sticky_len[level] = string_len(lastkey);
            if(string_len(sticky))
                dbg_err_if(string_append(sticky, ".", 1));
            dbg_err_if(string_append(sticky, string_c(lastkey), 
                string_len(lastkey)));
            dbg_err_if(string_clear(lastkey));
            if(!u_isblank_str(++ln))
                warn_err("config error [line %d]: { or } must be the "
                         "only not-blank char in a line", lineno);
            continue;       /* EOL */
        } else if(ln[0] == '}') {
            warn_err_ifm(level == 0,"config error: unmatched '}'");
            dbg_err_if(string_set_length(sticky, 
                string_len(sticky)-sticky_len[level]));
            /* remove the dot if not empty */
            if(string_len(sticky))
                dbg_err_if(string_set_length(sticky, string_len(sticky)-1));
            level--;
            if(!u_isblank_str(++ln))
                warn_err("config error [line %d]: { or } must be the "
                         "only not-blank char in a line", lineno);
            continue;
        }

        /* find the end of the key string */
        for(p = ln; *p && !u_isblank(*p); ++p);

        /* set the key */
        if(string_len(sticky))
        {
            dbg_err_if(string_set(key, string_c(sticky),string_len(sticky)));
            dbg_err_if(string_append(key, ".", 1));
        } else
            dbg_err_if(string_clear(key));
        dbg_err_if(string_append(key, ln, p-ln));

        /* set the value */
        dbg_err_if(string_set(value, p, strlen(p)));
        dbg_err_if(string_trim(value));

        /* if the valus is empty an open bracket will follow, save the key */
        if(string_len(value) == 0)
            dbg_err_if(string_set(lastkey, ln, p-ln));

        /* add to the var list */
        dbg_err_if(config_do_set_key(c, 
                        string_c(key), 
                        string_len(value) ? string_c(value) : NULL, 
                        overwrite));
    }
    
    warn_err_ifm(string_len(sticky), 
        "config error: missing '{'");

    string_free(sticky);
    string_free(lastkey);
    string_free(value);
    string_free(key);
    string_free(line);

    return 0;
err:
    if(sticky)
        string_free(sticky);
    if(lastkey)
        string_free(lastkey);
    if(key)
        string_free(key);
    if(value)
        string_free(value);
    if(line)
        string_free(line);
    return ~0;
}

int config_create(config_t **pc)
{
    config_t *c = NULL;

    c = u_calloc(sizeof(config_t));
    dbg_err_if(c == NULL);

    TAILQ_INIT(&c->children);

    *pc = c;

    return 0;
err:
    if(c)
        config_free(c);
    return ~0;
}

void config_del_key(config_t *c, config_t *child)
{
    TAILQ_REMOVE(&c->children, child, np);
}

int config_free(config_t *c)
{
    config_t *child = NULL;
    if(c)
    {
        /* free all children */
        while((child = TAILQ_FIRST(&c->children)) != NULL)
        {
            config_del_key(c, child);
            dbg_err_if(config_free(child));
        }
        /* free parent */
        if(c->key)
            u_free(c->key);
        if(c->value)
            u_free(c->value);
        u_free(c);
    }
    return 0;
err:
    return ~0;
}

const char* config_get_key(config_t *c)
{
    dbg_err_if(!c);

    return c->key;
err:
    return NULL;
}

const char* config_get_value(config_t *c)
{
    dbg_err_if(!c);
    
    return c->value;
err:
    return NULL;
}

const char* config_get_subkey_value(config_t *c, const char *subkey)
{
    config_t *skey;

    dbg_err_if(config_get_subkey(c, subkey, &skey));

    return config_get_value(skey);
err:
    dbg("subkey: %s", subkey);
    return NULL;
}

/**
 *  \}
 */



