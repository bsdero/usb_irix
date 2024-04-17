/*
********************************************************************
* PCI EHCI Device Driver for Irix 6.5                              *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* Dynamic memory list/garbage collector utilities                  *
* 2011                                                             *
*                                                                  *
********************************************************************

*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      01-04-2012     -Initial version                                                         *
*                                                                                                     *
*******************************************************************************************************
*/

#ifndef _GC_
#define _GC_

#include "list.h"

#define DMALLOC(b)                          kmem_alloc( b, KM_SLEEP)
#define DFREE(p,b)                          kmem_free(p, b)
#define INIT_GC_LIST(m)	                    INIT_LIST_HEAD(m)
#define gc_list_init(l)                     INIT_GC_LIST(l)
#define gc_list_empty(l)                    list_empty(l)
typedef struct{
    list_t            list;
	unsigned char     mark;
	size_t            size;
}gc_node_t;

typedef list_t                    gc_list_t;


gc_node_t *gc_get_node_ptr( void *data);
void *gc_get_data( gc_node_t *node);
void *gc_malloc( gc_list_t *list, size_t size);
void gc_free( void *ptr);
void gc_mark( void *data);
void gc_list_sweep( gc_list_t *gc_list);
void gc_list_destroy( gc_list_t *gc_list);
void gc_list_dump( gc_list_t *gc_list);
char *gc_strdup( gc_list_t *gc_list, char *p);
void gc_dump( void *data);

#endif
