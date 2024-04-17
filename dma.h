/*
********************************************************************
* PCI EHCI Device Driver for Irix 6.5                              *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* DMA utils                                                        *
* 2011                                                             *
*                                                                  *
********************************************************************

*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      01-05-2012     -Initial version                                                         *
*                                                                                                     *
*******************************************************************************************************
*/

#include "dumphex.h"
#include "list.h"
#include "gc.h"

#define dma_list_init(ptr)             gc_list_init(ptr)
#define dma_list_empty(l)              gc_list_empty(l)


typedef gc_list_t           dma_list_t;



typedef struct{
    iopaddr_t               paddr;	/* starting phys addr */
    caddr_t                 kaddr;	/* starting kern addr */
    pciio_dmamap_t          map;	/* mapping resources (ugh!) */
    pciaddr_t               daddr;	/* starting pci addr */
    size_t                  pages;	/* size of block in pages */
    size_t                  bytes;	/* size of block in bytes */
    __psunsigned_t          handle;	/* mapping handle */
	caddr_t                 kaddr_aligned; /* address aligned */
}dma_node_t;

extern int		pci_user_dma_max_pages;


dma_node_t *dma_alloc( vertex_hdl_t vhdl, dma_list_t *dma_list, size_t size, unsigned flags);
void dma_drain( dma_node_t *dma_node);
void dma_done( dma_node_t *dma_node);
void dma_mark( dma_node_t *dma_node);
void dma_free( dma_node_t *dma_node);
void dma_list_sweep( dma_list_t *dma_list);
void dma_list_destroy( dma_list_t *dma_list);

dma_node_t *dma_alloc( vertex_hdl_t vhdl, dma_list_t *dma_list, size_t size, unsigned flags){
    dma_node_t *dma_node;
    void                   *kaddr = 0;
    iopaddr_t               paddr;
    pciba_dma_h             dmah;
    pciba_dma_t             dmap = 0;
    pciio_dmamap_t          dmamap = 0;
    size_t                  bytes = size;
    int                     pages;
    pciaddr_t               daddr;
	int                     err = 0;

    dma_node = (dma_node_t *) gc_malloc( dma_list, sizeof(int));
	
	if( dma_node == NULL){
	    printf("dma_alloc: no dma_node allocated\n");
	    return( NULL);
	}	 
	
    
	printf("dma_alloc: user wants 0x%x bytes of DMA, flags 0x%x\n", bytes, flags);
    bytes += 32; /* we need DMA 32bit aligned */
    flags |= PCIIO_WORD_VALUES;
	/* round up the requested size to the next highest page */
	pages = (bytes + NBPP - 1) / NBPP;

	/* make sure the requested size is something reasonable */
	if (pages > pci_user_dma_max_pages) {
		    printf("dma_alloc: request for too much buffer space\n");
		    err = 1;
		    goto dma_alloc_exit;	/* "request for too much buffer space" */
	}
	/* "correct" number of bytes */
	bytes = pages * NBPP;

	/* allocate the space */
	/* XXX- force to same node as the device? */
	/* XXX- someday, we want to handle user buffers,
	 *    and noncontiguous pages, but this will
	 *      require either fancy mapping or handing
	 *      a list of blocks back to the user. For
	 *      now, just tell users to allocate a lot of
	 *      individual single-pages and manage their
	 *      scatter-gather manually.
	 */
	kaddr = kvpalloc(pages, VM_DIRECT | KM_NOSLEEP, 0);
    if (kaddr == 0) {
		printf("dma_alloc: unable to get %d contiguous pages\n", pages);
	    err = 1; /* "insufficient resources, try again later" */
	    goto dma_alloc_exit;
	}

	printf("pciba: kaddr is 0x%08x\n", kaddr);
	paddr = kvtophys(kaddr);
	daddr = pciio_dmatrans_addr( vhdl, 0, paddr, bytes, flags);

	if (daddr == 0) {	/* "no direct path available" */
	    printf("dma_alloc: dmatrans failed, trying dmamap\n");
	    dmamap = pciio_dmamap_alloc	(soft->comm->conn, 0, bytes, flags);
	    if (dmamap == 0) {
			printf("dma_alloc: unable to allocate dmamap\n");
			err = 1;
			goto dma_alloc_exit;	/* "out of mapping resources" */
		}
		
		daddr = pciio_dmamap_addr(dmamap, paddr, bytes);
		if (daddr == 0) {
			printf("dma_alloc: dmamap_addr failed\n");
			err = 1;
			goto dma_alloc_exit; /* "can't get there from here" */
		}
    }
	printf("dma_alloc: daddr is 0x%08x\n", daddr);
    dma_node->bytes = bytes;
	dma_node->pages = pages;
	dma_node->paddr = paddr;
	dma_node->kaddr = kaddr;
	dma_node->map = dmamap;
	dma_node->daddr = daddr;
	dma_node->handle = 0;

	printf("pciba: dmap 0x%x contains va 0x%x bytes 0x%x pa 0x%x pages 0x%x daddr 0x%x\n",
		       dmap, kaddr, bytes, paddr, pages, daddr);
     
	dma_alloc_exit:
	if( err == 0){
		gc_mark( (void *) dma_node);
		dma_node = NULL;
	}
	
	return( dma_node);
}


void dma_drain( dma_node_t *dma_node){
/*   pciio_dmamap_drain(  pciio_dmamap_t *map) */
    pciio_dmamap_drain( &dma_node->dmamap);
}

void dma_done( dma_node_t *dma_node){
/*      pciio_dmamap_done(pciio_dmamap_t map)*/ 
    pciio_dmamap_done( dma_node->dmamap);
}

void dma_mark( dma_node_t *dma_node){
    dma_drain( dma_node);
	dma_done( dma_node);
	gc_mark( (void *) dma_node);
}

void dma_free( dma_node_t *dma_node){
 /*void      pciio_dmamap_free(pciio_dmamap_t map) */
    dma_drain( dma_node);
	dma_done( dma_node);
	pciio_dmamap_free( dma_node->dmamap);
	gc_free( (void *) dma_node);
}

void dma_list_sweep( dma_list_t *dma_list){
    gc_list_sweep( (gc_list_t *) dma_list);
}



void dma_list_destroy( dma_list_t *dma_list){
    list_t *l, *i, *tmp;
    dma_node_t *dma_node;   
	   
	l = (list_t *) dma_list;
	
	list_for_each_safe( i, tmp, l){
	    dma_node = gc_get_data(  (void *)i);
        dma_free( dma_node); 		
	}   

    dma_list_init( dma_list);	

}


