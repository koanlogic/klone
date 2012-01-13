/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: atom.c,v 1.12 2006/01/09 12:38:38 tat Exp $
 */

#include "klone_conf.h"
#include <u/libu.h>
#include <klone/utils.h>
#include <klone/atom.h>

int atom_create(const char *id, const char *data, size_t size, void *arg, 
    atom_t **patom)
{
    atom_t *atom = NULL;

    dbg_err_if (id == NULL);
    dbg_err_if (data == NULL);
    dbg_err_if (patom == NULL);

    atom = u_zalloc(sizeof(atom_t));
    dbg_err_if(atom == NULL);

    atom->id = u_strdup(id);
    dbg_err_if(atom->id == NULL);

    atom->data = u_memdup(data, size);
    dbg_err_if(atom->data == NULL);

    atom->size = size;
    atom->arg = arg;

    *patom = atom;

    return 0;
err:
    if(atom)
        atom_free(atom);
    return ~0;
}

int atom_free(atom_t *atom)
{
    if (atom)
    {
        U_FREE(atom->id);
        U_FREE(atom->data);
        U_FREE(atom);
    }

    return 0;
}

/* sum of atoms size field */
size_t atoms_size(atoms_t *as)
{
    dbg_err_if (as == NULL);

    return as->size;
err:
    return -1;
}

/* # of atoms */
size_t atoms_count(atoms_t *as)
{
    dbg_err_if (as == NULL);

    return as->count;
err:
    return -1;
}

/* return the n-th atom */
int atoms_getn(atoms_t *as, size_t n, atom_t **patom)
{
    atom_t *atom;
    size_t i = 0;

    dbg_err_if (as == NULL);
    dbg_err_if (n >= as->count);
    dbg_err_if (patom == NULL);

    LIST_FOREACH(atom, &as->list, np)
    {
        if(i++ == n)
        {
            *patom = atom;
            return 0;
        }
    }

err:
    return ~0; /* out of bounds */
}

/* return the atom whose ID is id */
int atoms_get(atoms_t *as, const char *id, atom_t **patom)
{
    atom_t *atom;

    dbg_err_if (as == NULL);
    dbg_err_if (id == NULL);
    dbg_err_if (patom == NULL);

    LIST_FOREACH(atom, &as->list, np)
    {
        if(strcmp(id, atom->id) == 0)
        {
            *patom = atom;
            return 0;
        }
    }

err:
    return ~0; /* not found */
}

/* add an atom to the list */
int atoms_add(atoms_t *as, atom_t *atom)
{
    dbg_err_if (as == NULL);
    dbg_err_if (atom == NULL);

    LIST_INSERT_HEAD(&as->list, atom, np);

    as->count++;
    as->size += atom->size;

    return 0;
err:
    return ~0;
}

/* add an atom to the list */
int atoms_remove(atoms_t *as, atom_t *atom)
{
    dbg_err_if (as == NULL);
    dbg_err_if (atom == NULL);

    LIST_REMOVE(atom, np);

    as->count--;
    as->size -= atom->size;

    return 0;
err:
    return ~0;

}

/* create an atom list */
int atoms_create(atoms_t **pas)
{
    atoms_t *as = NULL;

    dbg_err_if (pas == NULL);

    as = u_zalloc(sizeof(atoms_t));
    dbg_err_if(as == NULL);

    LIST_INIT(&as->list);

    *pas = as;

    return 0;
err:
    U_FREE(as);
    return ~0;
}

/* free an atom list */
int atoms_free(atoms_t *as)
{
    atom_t *atom;

    if (as)
    {
        while(atoms_count(as))
        {
            dbg_err_if(atoms_getn(as, 0, &atom));
            dbg_err_if(atoms_remove(as, atom));
        }

        U_FREE(as);
    }

    return 0;
err:
    return ~0;
}
