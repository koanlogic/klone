/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: atom.h,v 1.8 2006/01/09 12:38:37 tat Exp $
 */

#ifndef _KLONE_ATOM_H_
#define _KLONE_ATOM_H_

#include <stdlib.h>
#include <u/libu.h>

#ifdef __cplusplus
extern "C" {
#endif 

/* global server-maintaned atom list */
typedef struct atom_s
{
    LIST_ENTRY(atom_s) np;  /* next & prev pointers         */
    char *id;               /* atom identifier              */
    char *data;             /* atom data block              */
    size_t size;            /* atom data block size         */
    void *arg;              /* opaque data                  */
} atom_t;

/* define atom list */
LIST_HEAD(atom_list_s, atom_s);
typedef struct atom_list_s atom_list_t;

struct atoms_s
{
    atom_list_t list;
    size_t size, count;
};

/* create an atom */
int atom_create(const char *id, const char *data, size_t size, void* arg, 
    atom_t**pa);
/* free an atom */
int atom_free(atom_t* atom);

/* atom_t list */
struct atoms_s;
typedef struct atoms_s atoms_t; 

/* create an atom list */
int atoms_create(atoms_t **);

/* free an atom list */
int atoms_free(atoms_t *);

/* sum of atoms size field */
size_t atoms_size(atoms_t *);

/* # of atoms */
size_t atoms_count(atoms_t *);

/* return the n-th atom */
int atoms_getn(atoms_t *, size_t n, atom_t**);

/* return the atom whose ID is id */
int atoms_get(atoms_t *, const char *id, atom_t**);

/* add an atom to the list */
int atoms_add(atoms_t *, atom_t*);

/* add an atom to the list */
int atoms_remove(atoms_t *, atom_t*);

#ifdef __cplusplus
}
#endif 

#endif
