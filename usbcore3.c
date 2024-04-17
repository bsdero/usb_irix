/**************************************************************************
 *									  *
 *		 Copyright (C) 1990-1993, Silicon Graphics, Inc		  *
 *									  *
 *  These coded instructions, statements, and computer programs	 contain  *
 *  unpublished	 proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may	not be disclosed  *
 *  to	third  parties	or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/cred.h>
#include <ksys/ddmap.h>
#include <sys/poll.h>
#include <sys/invent.h>
#include <sys/debug.h>
#include <sys/sbd.h>
#include <sys/kmem.h>
#include <sys/edt.h>
#include <sys/dmamap.h>
#include <sys/hwgraph.h>
#include <sys/iobus.h>
#include <sys/iograph.h>
#include <sys/param.h>
#include <sys/pio.h>
#include <sys/sema.h>
#include <sys/ddi.h>
#include <sys/atomic_ops.h>
#include <sys/PCI/PCI_defs.h>
#include <sys/PCI/pciio.h>

#include <sys/mload.h>
#include <string.h>
#include <ctype.h>
#define USECDELAY(ms)                               delay(drv_usectohz(ms))

/* included sources */
#include "config.h"
/* #include "hcicommon.h" */ /* common for host controllers */
/* #include "uhcireg.h"   */ /* user space ioctls and defines, used also in driver code */
#include "usbcore.h"
#include "trace.c"
#include "dumphex.c"   /* dump utility */
#include "list.c"      /* list utility */
#include "gc.c"        /* memory lists utility */
#include "dma.c"       /* dma lists utility */




#define IOCTL_1                   _IO('0', 1)
#define IOCTL_2                   _IOR('1', 2, int)
#define IOCTL_3                   _IOW('2', 3, char)
#define IOCTL_4                   _IOWR('3', 4, long)

char                   *usbcore_mversion = M_VERSION;
int                     usbcore_devflag = D_MP;

/* busy counts the reasons why we can not
 * currently unload this driver.
 */
unsigned		usbcore_busy = 0;

typedef struct{
    vertex_hdl_t            conn;
    vertex_hdl_t            masterv;
	vertex_hdl_t            usb_dev1Mode;
	vertex_hdl_t            usb_dev0Mode;
	int                     mode;
}usbcore_soft_t;

gc_list_t      gc_list;
usbcore_soft_t *global_soft = NULL; 


#define	usbcore_soft_get(v)	(usbcore_soft_t *)hwgraph_fastinfo_get(v)



void usbcore_init(void){
	vertex_hdl_t masterv = GRAPH_VERTEX_NONE;
    vertex_hdl_t usb_dev0v = GRAPH_VERTEX_NONE;
    vertex_hdl_t usb_dev1v = GRAPH_VERTEX_NONE;
	vertex_hdl_t connv = GRAPH_VERTEX_NONE;
    graph_error_t ret = (graph_error_t) 0;
    usbcore_soft_t *pfd, *pfd_dev0, *pfd_dev1; 
	pfd = pfd_dev0 = pfd_dev1 = NULL;

	
	printf("usbcore_init()\n");
	
	gc_list_init( &gc_list);

    pfd = ( usbcore_soft_t *) gc_malloc( &gc_list, sizeof( usbcore_soft_t));
	if( pfd == NULL){
	    ret = ( graph_error_t) ENOMEM;
	    goto done;
	}
	
    bzero( pfd, sizeof( usbcore_soft_t));
    ret = hwgraph_path_add(
                GRAPH_VERTEX_NONE,      /* start at /hw */
                "/usbcore",                   /* this is the path */
                &masterv);              /* put vertex there */    
                
	printf("rc of hwgraph_path_add() = %d, master vertex\n", ret);
	USECDELAY( 1000000);
    if (ret != GRAPH_SUCCESS) 
	    goto done;

    ret = hwgraph_edge_get( masterv, "usb_dev0", &usb_dev0v);
    if (ret != GRAPH_SUCCESS) { /* it does not. create it. */
	
	    printf("rc of hwgraph_edge_get() = %d, added usbcore edge\n", ret);
	    USECDELAY( 1000000);	

		
        ret = hwgraph_char_device_add( masterv, "usb_dev0", "usbcore_", &usb_dev0v); 
	    printf("rc of hwgraph_char_device_add() = %d, added usb_dev0 edge (char device)\n", ret);
	    USECDELAY( 1000000);	
        if (ret != GRAPH_SUCCESS) 
	        goto done;
    }

    ret = hwgraph_edge_get( masterv, "usb_dev1", &usb_dev1v);
    if (ret != GRAPH_SUCCESS) { /* it does not. create it. */
	
	    printf("rc of hwgraph_edge_get() = %d, added usbcore edge\n", ret);
	    USECDELAY( 1000000);	
		
        ret = hwgraph_char_device_add( masterv, "usb_dev1", "usbcore_", &usb_dev1v); 
	    printf("rc of hwgraph_char_device_add() = %d, added usb_dev1 edge (char device)\n", ret);
	    USECDELAY( 1000000);	
        if (ret != GRAPH_SUCCESS) 
	        goto done;
    }


	pfd->masterv = masterv; 
	pfd->conn = connv;
    pfd->usb_dev1Mode = usb_dev1v; /* note which vertex is "usb_dev1" */
    pfd->usb_dev0Mode = usb_dev0v; /* note which vertex is "usb_dev0" */

    pfd->mode = 0xff;
	device_info_set( masterv, pfd);

    pfd_dev0 = ( usbcore_soft_t *) gc_malloc( &gc_list, sizeof( usbcore_soft_t));
	if( pfd_dev0 == NULL){
	    ret = ( graph_error_t) ENOMEM;
	    goto done;
	}
    
	bcopy( pfd, pfd_dev0, sizeof( usbcore_soft_t));
    pfd_dev0->mode = 0x00;
    device_info_set( usb_dev0v, pfd_dev0);
	
    pfd_dev1 = ( usbcore_soft_t *) gc_malloc( &gc_list, sizeof( usbcore_soft_t));
	if( pfd_dev1 == NULL){
	    ret = ( graph_error_t) ENOMEM;
	    goto done;
	}
	bcopy( pfd, pfd_dev1, sizeof( usbcore_soft_t));
    pfd_dev1->mode = 0x01;	
    device_info_set( usb_dev1v, pfd_dev1);
	
	
done: /* If any error, undo all partial work */
    if (ret != 0)   {
        if ( usb_dev1v != GRAPH_VERTEX_NONE) 
	        hwgraph_vertex_destroy( usb_dev1v);
			
        if ( usb_dev0v != GRAPH_VERTEX_NONE) 
		    hwgraph_vertex_destroy( usb_dev0v);
			
        if ( pfd){ 
	      gc_list_destroy( &gc_list);  
	    }
	  
    }

	global_soft = pfd;
   	
}

int usbcore_unload(void){
	vertex_hdl_t masterv = GRAPH_VERTEX_NONE;
    vertex_hdl_t usb_dev0v = GRAPH_VERTEX_NONE;
    vertex_hdl_t usb_dev1v = GRAPH_VERTEX_NONE;
    graph_error_t ret = (graph_error_t) 0;
    usbcore_soft_t *pfd, *pfd_dev0, *pfd_dev1; 
	pfd = pfd_dev0 = pfd_dev1 = NULL;

	printf("usbcore_unload()\n");
	
	printf("getting pfd\n");
	USECDELAY(1000000);
    pfd = global_soft;	
	masterv = pfd->masterv;
    usb_dev0v = pfd->usb_dev0Mode;
    usb_dev1v = pfd->usb_dev1Mode;
	
	printf("call hwgraph_vertex_destroy()\n");
	USECDELAY(1000000);
	hwgraph_vertex_destroy( usb_dev0v);
	hwgraph_vertex_destroy( usb_dev1v);

	printf("call hwgraph_edge_remove()\n");
	USECDELAY(1000000);	
    hwgraph_vertex_destroy( masterv); 
	
	printf("call gc_list_destroy()\n");
	USECDELAY(1000000);	
    gc_list_destroy( &gc_list);  	
    return 0;				/* always ok to unload */
}

int
usbcore_reg(void)
{

    return 0;
}

int
usbcore_unreg(void)
{
    if (usbcore_busy)
	return -1;



    return 0;
}

int
usbcore_attach(vertex_hdl_t conn)
{

    return 0;
}

int
usbcore_detach(vertex_hdl_t conn)
{
    return 0;

}

/*ARGSUSED */
int usbcore_open(dev_t *devp, int flag, int otyp, struct cred *crp){
    vertex_hdl_t              vhdl;
    usbcore_soft_t            *soft;

	printf("open()\n");
	USECDELAY(1000000);	
	
    vhdl = dev_to_vhdl(*devp);
	printf("calling device_info_get()\n");
	USECDELAY(1000000);	
	
    soft = (usbcore_soft_t *) device_info_get(vhdl);
	printf("getting mode()\n");
	USECDELAY(1000000);	
	printf("soft->mode = %x\n", soft->mode);


    return 0;
}

/*ARGSUSED */
int
usbcore_close(dev_t dev)
{
    return 0;
}

/*ARGSUSED */
int
usbcore_read(dev_t dev, uio_t * uiop, cred_t *crp)
{
    return EOPNOTSUPP;
}

/*ARGSUSED */
int
usbcore_write()
{
    return EOPNOTSUPP;
}

/*ARGSUSED*/
int
usbcore_ioctl(dev_t dev, int cmd, void *uarg, int mode, cred_t *crp, int *rvalp)
{
    vertex_hdl_t              vhdl;
    usbcore_soft_t            *soft;

	printf("open()\n");
	USECDELAY(1000000);	
	
    vhdl = dev_to_vhdl(dev);
	printf("calling device_info_get()\n");
	USECDELAY(1000000);	
	
    soft = (usbcore_soft_t *) device_info_get(vhdl);
	printf("getting mode()\n");
	USECDELAY(1000000);	
	printf("soft->mode = %x\n", soft->mode);

	printf("cmd = %d   %x\n", cmd, cmd);
	switch( cmd){
	    case IOCTL_1: printf("a 1"); break;
	    case IOCTL_2: printf("b 2"); break;
	    case IOCTL_3: printf("c 3"); break;
	    case IOCTL_4: printf("d 4"); break;
	    default: 
		     printf("unknown");
	}
	printf("\n");
    return *rvalp = ENOTTY;
}

/*ARGSUSED*/
int
usbcore_map(dev_t dev, vhandl_t *vt,
	  off_t off, size_t len, uint_t prot)
{

    return 0;
}

/*ARGSUSED*/
int
usbcore_unmap(dev_t dev, vhandl_t *vt)
{
    return (0);
}
