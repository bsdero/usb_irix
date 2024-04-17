/*
********************************************************************
* USB stack and host controller driver for SGI IRIX 6.5            *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: usbcore.c                                                  *
* Description: USB core kernel driver                              *
*                                                                  *
********************************************************************
*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      03-29-2012     -Initial version                                                         *
*                                                                                                     *
*******************************************************************************************************
*/

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
#include <sys/errno.h>
#include <sys/atomic_ops.h>
#include <sys/PCI/PCI_defs.h>
#include <sys/PCI/pciio.h>

#include <sys/mload.h>
#include <string.h>
#include <ctype.h>



#define MAX_USB_DEVICES                             32




/*
file relationship

     usbio.h
      |
     usbioctl.h   
      |   \ 
      |    hcicommon.h - hci drivers (pciehci.c pciuhci.c)
      |       
      |\
      | usbdevs.h --- user space
      |             \
     usbcore.c     usbdevice drivers
                         \
                          \
                            user headers
                            
*/


/*
*******************************************************************************************************
* Headers                                                                                             *
*******************************************************************************************************
*/
#include "config.h"
#include "usbioctl.h"
#include "usb.h"
#include "usbhc.h"                                  /* Host controller defines */

/*
*******************************************************************************************************
* Global for all included sources                                                                     *
*******************************************************************************************************
*/
USB_trace_class_t                                   global_trace_class = { 12, TRC_ALL};


/*
*******************************************************************************************************
* Included sources                                                                                    *
*******************************************************************************************************
*/
#include "trace.c"
#include "dumphex.c"                                /* dump utility */
#include "list.c"                                   /* list utility */
#include "gc.c"                                     /* memory lists utility */
#include "dma.c"                                    /* dma lists utility */


/*
*******************************************************************************************************
* IOCTL Table                                                                                         *
*******************************************************************************************************
*/
typedef struct{
    int                  cmd;
	char                *str;
	USB_ioctl_func_t    func;
} ioctl_item_t;


/*
*******************************************************************************************************
* Utility function declaration and source code                                                        *
*******************************************************************************************************
*/

int usbcore_register_hcd_driver( void *hcd_arg);
int usbcore_unregister_hcd_driver( void *hcd_arg);
int dump_ioctl( int n);
int usbcore_event( void *arg);
int ioctl_usb_get_driver_info( void *hcd, void *arg);
int ioctl_usb_get_num_modules( void *hcd, void *arg);
int ioctl_usb_not_implemented( void *hcd, void *arg);
int ioctl_no_func( void *hcd, void *arg);
int ioctl_usb_set_debug_level( void *hcd, void *arg);
int ioctl_usb_get_debug_level( void *hcd, void *arg);
int ioctl_usb_get_module_info( void *hcd, void *arg);

ioctl_item_t usbcore_ioctls[]={
    IOCTL_USB_GET_DRIVER_INFO,      "IOCTL_USB_GET_DRIVER_INFO",     ioctl_usb_get_driver_info,
    IOCTL_USB_GET_NUM_MODULES,      "IOCTL_USB_GET_NUM_MODULES",     ioctl_usb_get_num_modules,
    IOCTL_USB_GET_MODULE_INFO,      "IOCTL_USB_GET_MODULE_INFO",     ioctl_usb_get_module_info,
    IOCTL_USB_GET_ROOT_HUB_INFO,    "IOCTL_USB_GET_ROOT_HUB_INFO",   ioctl_usb_not_implemented, 
    IOCTL_USB_SET_DEBUG_LEVEL,      "IOCTL_USB_SET_DEBUG_LEVEL",     ioctl_usb_set_debug_level,
    IOCTL_USB_GET_DEBUG_LEVEL,      "IOCTL_USB_GET_DEBUG_LEVEL",     ioctl_usb_get_debug_level,
    IOCTL_USB_GET_ROOT_HUB_STATUS,  "IOCTL_USB_GET_ROOT_HUB_STATUS", ioctl_usb_not_implemented,
    IOCTL_USB_GET_DEVICE,           "IOCTL_USB_GET_DEVICE",          ioctl_usb_not_implemented,
    IOCTL_USB_GET_DEVICES_NUM,      "IOCTL_USB_GET_DEVICES_NUM",     ioctl_usb_not_implemented,
    IOCTL_USB_CONTROL,              "IOCTL_USB_CONTROL",             ioctl_usb_not_implemented,
    IOCTL_USB_BULK,                 "IOCTL_USB_BULK",                ioctl_usb_not_implemented,
    IOCTL_USB_RESETEP,              "IOCTL_USB_RESETEP",             ioctl_usb_not_implemented,
    IOCTL_USB_SETINTF,              "IOCTL_USB_SETINTF",             ioctl_usb_not_implemented,
    IOCTL_USB_SETCONFIG,            "IOCTL_USB_SETCONFIG",           ioctl_usb_not_implemented,
    IOCTL_USB_GETDRIVER,            "IOCTL_USB_GETDRIVER",           ioctl_usb_not_implemented,
    IOCTL_USB_SUBMITURB,            "IOCTL_USB_SUBMITURB",           ioctl_usb_not_implemented,
    IOCTL_USB_DISCARDURB,           "IOCTL_USB_DISCARDURB",          ioctl_usb_not_implemented,
    IOCTL_USB_REAPURB,              "IOCTL_USB_REAPURB",             ioctl_usb_not_implemented,
    IOCTL_USB_REAPURBNDELAY,        "IOCTL_USB_REAPURBNDELAY",       ioctl_usb_not_implemented,
    IOCTL_USB_CLAIMINTF,            "IOCTL_USB_CLAIMINTF",           ioctl_usb_not_implemented,
    IOCTL_USB_RELEASEINTF,          "IOCTL_USB_RELEASEINTF",         ioctl_usb_not_implemented,
    IOCTL_USB_CONNECTINFO,          "IOCTL_USB_CONNECTINFO",         ioctl_usb_not_implemented,
    IOCTL_USB_IOCTL,                "IOCTL_USB_IOCTL",               ioctl_usb_not_implemented,
    IOCTL_USB_HUB_PORTINFO,         "IOCTL_USB_HUB_PORTINFO",        ioctl_usb_not_implemented,
    IOCTL_USB_RESET,                "IOCTL_USB_RESET",               ioctl_usb_not_implemented,
    IOCTL_USB_CLEAR_HALT,           "IOCTL_USB_CLEAR_HALT",          ioctl_usb_not_implemented,
    IOCTL_USB_DISCONNECT,           "IOCTL_USB_DISCONNECT",          ioctl_usb_not_implemented,
    IOCTL_USB_CONNECT,              "IOCTL_USB_CONNECT",             ioctl_usb_not_implemented,
	-1,                             "UNKNOWN IOCTL",                 ioctl_no_func,
    0, NULL, NULL
};

module_header_t usbcore_module_header={
	0, "USBCORE", "USBcore device driver", "usbcore.o", 
	USB_DRIVER_IS_CORE, 0, 0, 0, 0, NULL 
};




/*
*******************************************************************************************************
* Loadable modules stuff and global Variables                                                         *
*******************************************************************************************************
*/
char                                                *usbcore_mversion = M_VERSION;
int                                                 usbcore_devflag = D_MP;
unsigned int                                        usbcore_busy = 0;
gc_list_t                                           gc_list;
usbcore_instance_t                                  *global_soft = NULL;
#define MAX_HCD                                     4
hcd_methods_t                                       *hcd_modules[MAX_HCD];
int                                                 hcd_modules_num = 0;
int                                                 all_modules_num = 0;
#define MAX_MODULES                                 32 
module_header_t                                     *modules[MAX_MODULES];

/*
*******************************************************************************************************
* Entry points for USBcore                                                                            *
*******************************************************************************************************
*/
void usbcore_init(void);
int usbcore_unload(void);
int usbcore_reg(void);
int usbcore_unreg(void);
int usbcore_attach(vertex_hdl_t conn);
int usbcore_detach(vertex_hdl_t conn);
int usbcore_open(dev_t *devp, int flag, int otyp, struct cred *crp);
int usbcore_close(dev_t dev);
int usbcore_read(dev_t dev, uio_t * uiop, cred_t *crp);
int usbcore_write();
int usbcore_ioctl(dev_t dev, int cmd, void *uarg, int mode, cred_t *crp, int *rvalp);
int usbcore_map(dev_t dev, vhandl_t *vt, off_t off, size_t len, uint_t prot);
int usbcore_unmap(dev_t dev, vhandl_t *vt);



/*
*******************************************************************************************************
* Utility functions source code                                                                       *
*******************************************************************************************************
*/
int ioctl_usb_get_driver_info( void *hcd, void *arg){ 
    USB_driver_info_t *info = ( USB_driver_info_t *) arg;
    uint64_t class = TRC_USB_CORE | TRC_IOCTL;

    TRACE( class, 6, "start", "");
    strcpy( info->long_name,     USBCORE_DRV_LONG_NAME);
    strcpy( info->short_name,    USBCORE_DRV_SHORT_NAME);
    strcpy( info->version,       USBCORE_DRV_VERSION);        	
    strcpy( info->short_version, USBCORE_DRV_SHORT_VERSION);
    strcpy( info->seqn,          USBCORE_DRV_SEQ);
    strcpy( info->build_date,    USBCORE_DRV_BUILD_DATE);        	
    info->type = USB_DRIVER_IS_CORE;
    TRACE( class, 6, "end", "");    
    return( 0);	
}



int ioctl_usb_not_implemented( void *hcd, void *arg){
    uint64_t class = TRC_USB_CORE | TRC_IOCTL;

    TRACE( class, 6, "start", "");
    TRACE( class, 6, "Not implemented", "");
    TRACE( class, 6, "end", "");    
	return( 0);
}



int ioctl_no_func( void *hcd, void *arg){
    uint64_t class = TRC_USB_CORE | TRC_IOCTL;

    TRACE( class, 6, "start", "");
	TRACE( class, 6, "ioctl not exist", "");
    TRACE( class, 6, "end", "");    
	
    return(0);
}



int ioctl_usb_set_debug_level( void *hcd, void *arg){
	usbcore_instance_t *soft = (usbcore_instance_t *) hcd;
	USB_trace_class_t *trace_class = (USB_trace_class_t *) arg;
	int i;
    uint64_t class = TRC_USB_CORE | TRC_IOCTL;

    TRACE( class, 6, "start", "");
	
	
    global_trace_class.class = trace_class->class;
    global_trace_class.level = trace_class->level;

    for( i = 0 ; i < hcd_modules_num; i++){
        hcd_modules[i]->hcd_set_trace_level( 
            hcd_modules[i]->module_header.soft,  (void *) &global_trace_class);
    }
    
    TRACE( class, 6, "end", "");
	return(0);
} 



int ioctl_usb_get_debug_level( void *hcd, void *arg){
	USB_trace_class_t *trace_class = (USB_trace_class_t *) arg;
    uint64_t class = TRC_USB_CORE | TRC_IOCTL;

    TRACE( class, 6, "start", "");
	
    trace_class->class = global_trace_class.class;
    trace_class->level = global_trace_class.level;
    
    TRACE( class, 6, "end", "");
	return(0);
} 



int ioctl_usb_get_num_modules( void *hcd, void *arg){
	int *num_modules = (int *) arg;
    uint64_t class = TRC_USB_CORE | TRC_IOCTL;

    TRACE( class, 6, "start", "");
	
	*num_modules = all_modules_num+1;

    TRACE( class, 6, "end", "");
    return(0);
}



int ioctl_usb_get_module_info( void *instance, void *arg){ 
	USB_driver_module_info_t *info = (USB_driver_module_info_t *) arg;
    uint64_t class = TRC_USB_CORE | TRC_IOCTL;

    TRACE( class, 6, "start", "");
    

	if( info->num_module < 0){
        return( EINVAL);		
	}
	
	if( info->num_module >= all_modules_num){
		return( EINVAL);
    }
    
    info->module_id = modules[info->num_module]->module_id;
    info->type = modules[info->num_module]->type;
    strcpy( info->short_description, modules[info->num_module]->short_description);
    strcpy( info->long_description, modules[info->num_module]->long_description);
    strcpy( info->module_name, modules[info->num_module]->module_name);
    
    TRACE( class, 6, "end", "");
    return( 0);	
}


/*
*******************************************************************************************************
* FUNCTION:  get_ioctl_item                                                                           *
* BEHAVIOR:  Returns a pointer to the ioctl item table with the description of the ioctl passed as    *
*            argument. It will return NULL if ioctl table has no ioctl values at all. If the ioctl    *
*			 was not found, it'll return a pointer to a ioctl item descripting a Unknown IOCTL        *
* ARGUMENTS: A int value with the ioctl                                                               *
* RETURNS:   A pointer to a string descripting the ioctl passed as argument,                          *
*            "UNKNOWN IOCTL" if the ioctl passed as argument was not found                            *
*******************************************************************************************************
*/
ioctl_item_t *get_ioctl_item( int cmd){
    int i;


    for( i = 0; usbcore_ioctls[i].str != NULL; i++){
	    if( cmd == usbcore_ioctls[i].cmd)
		    return( &usbcore_ioctls[i] );
    }

    return( &usbcore_ioctls[i - 1]);
}

/*
*******************************************************************************************************
* Entry points for USBcore                                                                            *
*******************************************************************************************************
*/
void usbcore_init(void){
    vertex_hdl_t masterv = GRAPH_VERTEX_NONE;
    vertex_hdl_t usbcore_device_vtx = GRAPH_VERTEX_NONE;
    vertex_hdl_t usbdaemon_device_vtx = GRAPH_VERTEX_NONE;
    vertex_hdl_t connv = GRAPH_VERTEX_NONE;
    graph_error_t ret = (graph_error_t) 0;
    usbcore_instance_t *soft, *soft_usbcore_device, *soft_usbdaemon_device; 
    soft = soft_usbcore_device = soft_usbdaemon_device = NULL;
    uint64_t class = TRC_USB_CORE | TRC_INIT;

  	TRACE( class, 2, "start", "");
  	TRACE( class, 0, "**********************************************************", "");
  	TRACE( class, 0, "* USBCORE Driver and stack for Silicon Graphics Irix 6.5 *", "");
  	TRACE( class, 0, "* By bsdero at gmail dot org, 2011                       *", "");
  	TRACE( class, 0, "* Version %s                                        *", USBCORE_DRV_VERSION);
  	TRACE( class, 0, "* Sequence %s                                *", USBCORE_DRV_SEQ);
  	TRACE( class, 0, "**********************************************************", "");
    TRACE( class, 0, "USBCORE Driver loaded!", "");


    TRACE( class, 2, "start", "");
    
    /* initialize garbage collector list */
    gc_list_init( &gc_list);

    /* initialize this instance */
    soft = ( usbcore_instance_t *) gc_malloc( &gc_list, sizeof( usbcore_instance_t));
    if( soft == NULL){
		TRACERR( class, "gc_malloc() returned NULL, quitting", "");
        ret = ( graph_error_t) ENOMEM;
        goto done;
    }
    
    bzero( soft, sizeof( usbcore_instance_t));
    
    
    /* add /hw/usb path */
    ret = hwgraph_path_add( 
                GRAPH_VERTEX_NONE,      /* start at /hw */
                "/usb",                   /* this is the path */
                &masterv);              /* put vertex there */    
    TRACE( class, 6, "rc of hwgraph_path_add() = %d, master vertex", ret);

    /* couldn't add, goto done */
    if (ret != GRAPH_SUCCESS) 
        goto done;

    /* add char device /hw/usb/usbcore */
    ret = hwgraph_edge_get( masterv, "usbcore", &usbcore_device_vtx);
    TRACE( class, 6, "rc of hwgraph_edge_get() = %d, added usbcore edge", ret);
    ret = hwgraph_char_device_add( masterv, "usbcore", "usbcore_", &usbcore_device_vtx); 
    TRACE( class, 6, "rc of hwgraph_char_device_add() = %d, added usbcore edge (char device)", ret);

    /* add char device /hw/usb/usbdaemon */
    ret = hwgraph_edge_get( masterv, "usbdaemon", &usbdaemon_device_vtx);
    TRACE( class, 6, "rc of hwgraph_edge_get() = %d, added usbdaemon edge", ret);
    ret = hwgraph_char_device_add( masterv, "usbdaemon", "usbcore_", &usbdaemon_device_vtx); 
    TRACE( class, 6, "rc of hwgraph_char_device_add() = %d, added usbdaemon edge (char device)", ret);

    /* initialize usbcore instance */
    soft->masterv = masterv; 
    soft->conn = connv;
    soft->usbdaemon = usbdaemon_device_vtx; /* note which vertex is "usbdaemon" */
    soft->usbcore = usbcore_device_vtx; /* note which vertex is "usbcore" */
    soft->register_hcd_driver = usbcore_register_hcd_driver;
    soft->unregister_hcd_driver = usbcore_unregister_hcd_driver;
    soft->event_func = usbcore_event;
    soft->mode = 0xff;
    device_info_set( masterv, soft);

    /* add usbcore instance of device */
    soft_usbcore_device = ( usbcore_instance_t *) gc_malloc( &gc_list, sizeof( usbcore_instance_t));
    if( soft_usbcore_device == NULL){
        ret = ( graph_error_t) ENOMEM;
        goto done;
    }
    
    bcopy( soft, soft_usbcore_device, sizeof( usbcore_instance_t));
    soft_usbcore_device->mode = 0x00;
    device_info_set( usbcore_device_vtx, soft_usbcore_device);
    

    /* add daemon instance of device */
    soft_usbdaemon_device = ( usbcore_instance_t *) gc_malloc( &gc_list, sizeof( usbcore_instance_t));
    if( soft_usbdaemon_device == NULL){
        ret = ( graph_error_t) ENOMEM;
        goto done;
    }
    bcopy( soft, soft_usbdaemon_device, sizeof( usbcore_instance_t));
    soft_usbdaemon_device->mode = 0x01;    
    device_info_set( usbdaemon_device_vtx, soft_usbdaemon_device);

    /* clear list of host controller methods */    
    bzero( (void *) hcd_modules, sizeof( hcd_methods_t *) * MAX_HCD);
    hcd_modules_num = 0;   
    
    
    
   
    /* clear array of pointers to module descriptions */
    bzero( (void *) modules, sizeof( module_header_t *) * MAX_MODULES);
    
    /* the first module should be usbcore module */
    modules[0] = ( usbcore_instance_t *) gc_malloc( &gc_list, sizeof( usbcore_instance_t));
    bcopy( (void *) &usbcore_module_header, (void *) modules[0], 
        sizeof( module_header_t));
    all_modules_num = 1;

        
    
done: /* If any error, undo all partial work */
    if (ret != 0)   {
        if ( usbdaemon_device_vtx != GRAPH_VERTEX_NONE) 
            hwgraph_vertex_destroy( usbdaemon_device_vtx);
            
        if ( usbcore_device_vtx != GRAPH_VERTEX_NONE) 
            hwgraph_vertex_destroy( usbcore_device_vtx);
            
        if ( soft){ 
            gc_list_destroy( &gc_list);  
        }
      
    }

    global_soft = soft;
    TRACE( class, 2, "end", "");
       
}

int usbcore_unload(void){
    vertex_hdl_t conn = GRAPH_VERTEX_NONE;
    vertex_hdl_t masterv = GRAPH_VERTEX_NONE;
    vertex_hdl_t usbcore_device_vtx = GRAPH_VERTEX_NONE;
    vertex_hdl_t usbdaemon_device_vtx = GRAPH_VERTEX_NONE;
    usbcore_instance_t *soft; 
    uint64_t class = TRC_USB_CORE | TRC_UNLOAD;

    TRACE( class, 2, "start", "");


    soft = global_soft;    
    masterv = soft->masterv;
    usbcore_device_vtx = soft->usbcore;
    usbdaemon_device_vtx = soft->usbdaemon;
    
    if (GRAPH_SUCCESS != hwgraph_traverse(conn, "/usb", &masterv))
        return -1;    
    
    hwgraph_edge_remove(masterv, "usbcore", &usbcore_device_vtx);    
    hwgraph_edge_remove(masterv, "usbdaemon", &usbdaemon_device_vtx);    
    
    hwgraph_vertex_destroy( usbcore_device_vtx);
    hwgraph_vertex_destroy( usbdaemon_device_vtx);

    hwgraph_edge_remove(conn, "/usb", &masterv);    
    hwgraph_vertex_destroy( masterv); 
    
    
    gc_list_destroy( &gc_list);      
    TRACE( class, 0, "USBCORE Driver unloaded!", "");
    TRACE( class, 2, "end", "");
    
    return 0;                /* always ok to unload */
}


int usbcore_reg(void){
    return 0;
}

int usbcore_unreg(void){
    if (usbcore_busy)
        return -1;

    return 0;
}

int usbcore_attach(vertex_hdl_t conn){
    return 0;
}

int usbcore_detach(vertex_hdl_t conn){
    return 0;

}

/*ARGSUSED */
int usbcore_open(dev_t *devp, int flag, int otyp, struct cred *crp){
    vertex_hdl_t                  vhdl;
    usbcore_instance_t            *soft;
    uint64_t class = TRC_USB_CORE | TRC_OPEN;

    TRACE( class, 4, "start", "");
    
    vhdl = dev_to_vhdl(*devp);
    soft = (usbcore_instance_t *) device_info_get(vhdl);
    TRACE( class, 4, "end", "");

    return 0;
}

/*ARGSUSED */
int usbcore_close(dev_t dev){
    uint64_t class = TRC_USB_CORE | TRC_CLOSE;
    TRACE( class, 4, "start", "");
	
    TRACE( class, 4, "end", "");
    return 0;
}

/*ARGSUSED */
int usbcore_read(dev_t dev, uio_t * uiop, cred_t *crp){
    return EOPNOTSUPP;
}

/*ARGSUSED */
int usbcore_write(){
    return EOPNOTSUPP;
}


int dump_ioctl( int n){
    int i = n;
    uint64_t class = TRC_USB_CORE | TRC_HELPER;

    TRACE( class, 6, "start", "");
    TRACE( class, 6, "ioctl value %x", i);
    
    if(( i & _IOCTL_RMASK) != 0)
        TRACE( class, 6, " -read", "");
    if(( i & _IOCTL_WMASK) != 0)
        TRACE( class, 6, " -write", "");


    TRACE( class, 6, " -char = %c\n", _IOCTL_GETC(i));
    TRACE( class, 6, " -num = %d\n", _IOCTL_GETN( i));
    TRACE( class, 6, " -size = %d\n", _IOCTL_GETS( i));

    TRACE( class, 6, "end", "");
    return(0);
}



/*ARGSUSED*/
int usbcore_ioctl(dev_t dev, int cmd, void *uarg, int mode, cred_t *crp, int *rvalp){
    vertex_hdl_t               vhdl;
    usbcore_instance_t        *soft;
    void                      *ubuf;
    size_t                     size;
    int                          rc;
    ioctl_item_t              *item;
    uint64_t class = TRC_USB_CORE | TRC_IOCTL;

    TRACE( class, 4, "start", "");

    vhdl = dev_to_vhdl(dev);
 
    soft = (usbcore_instance_t *) device_info_get(vhdl);
    TRACE( class, 4, "soft->mode = %x", soft->mode);
	dump_ioctl( cmd);
    
    item = get_ioctl_item( cmd);
    TRACE( class, 4, "cmd = %x, '%s', cmd_table=%d", cmd, item->str, item->cmd);
    
    if( item->cmd <= 0){
        rc = EINVAL;
        goto done;
    }
        
	/* get size defined in ioctl */
    size = _IOCTL_GETS( cmd);
    if( size > 0){ 
		if( uarg == NULL){ /* something is weird */
		    rc = EINVAL;
		    goto done; 	
		}
		
	    /* reserve kernel memory */
		ubuf = gc_malloc( &gc_list, size);
		if( ubuf == NULL){
		    TRACERR( class, "Error in gc_malloc(), exit..", "");	
			rc = ENOMEM;
			goto done;
		}

        /* if we're writing data, copy from user space */
	    if( (cmd & _IOCTL_WMASK) != 0){	
	        copyin( uarg, ubuf, size);
        }
	}
	
    /* run ioctl function now */
    TRACE( class, 10, "Running ioctl", "");
	rc = item->func( (void *) soft, ubuf);

    if( size > 0){ 
	    if( (cmd & _IOCTL_RMASK) != 0){	
	        copyout( ubuf, uarg, size);
        }
		gc_mark( ubuf);
	}



/*    dump_ioctl( cmd);*/
    printf("ioctl() end\n");
    
done:        
    TRACE( class, 6, "exiting ioctl() entry point, rc=%d", rc);
    TRACE( class, 4, "end", "");

    return *rvalp = rc;
    
}

/*ARGSUSED*/
int usbcore_map(dev_t dev, vhandl_t *vt, off_t off, size_t len, uint_t prot){
    return 0;
}

/*ARGSUSED*/
int usbcore_unmap(dev_t dev, vhandl_t *vt){
    return (0);
}


/* HCD drivers call this function when something happens, so it should perform a pollwakeup() or
   take actions */
int usbcore_event( void *arg){
    uint64_t class = TRC_USB_CORE | TRC_POLL;

    TRACE( class, 6, "start", "");
	
    int i =  *((int *)arg);
	/* wakeup */
    TRACE( class, 6, "end", "");
    return(0);
}

int usbcore_register_hcd_driver( void *hcd_arg){
    hcd_methods_t *hcd_m = (hcd_methods_t *) hcd_arg;
    int i, already_on_list = 0;
    int index = -1;
    uint64_t class = TRC_USB_CORE | TRC_HELPER;

    TRACE( class, 6, "start", "");


    TRACE( class, 2, "Detected host controller='%s'", hcd_m->module_header.short_description);
    TRACE( class, 6, "Registering HCD driver '%s' with ID='0x%x'", 
        hcd_m->module_header.short_description, hcd_m->module_header.module_id);
    

    
    /* look if this HCD already has been added to hcd methods list */
    for( i = 0; i < hcd_modules_num; i++){ 
	    if( hcd_modules[i]->module_header.module_id == hcd_m->module_header.module_id){
	        already_on_list = 1;
	        index = i;
	        break;
	    }
	}
    
    /* if it's already on list */   
    if( already_on_list == 0 && hcd_modules_num < MAX_HCD){
	    TRACE( class, 10, "HCD methods not on list, adding..", "");
		
		hcd_modules[hcd_modules_num] = ( hcd_methods_t *) gc_malloc( &gc_list, sizeof( hcd_methods_t));
        bcopy( (void *) hcd_arg, (void *) hcd_modules[hcd_modules_num], sizeof( hcd_methods_t) );
        hcd_modules_num++;
    }else{ 
	    TRACE( class, 10, "HCD methods already on list, passing out..", "");
	    return(0);
    }
    

     
 
    
    /* now look if this HCD module info is already on modules list */
    already_on_list = 0;
    for( i = 0; i < all_modules_num; i++){
        if( modules[i]->module_id == hcd_m->module_header.module_id){
	        already_on_list = 1;
	        break;
   	    }    
        
    }    


    if( already_on_list == 0 && all_modules_num < MAX_MODULES){
	    TRACE( class, 10, "module info not on list, adding..", "");
        modules[all_modules_num] = (module_header_t *) gc_malloc( 
            &gc_list, sizeof( module_header_t));
        bcopy( (void *) &hcd_m->module_header, modules[all_modules_num], 
             sizeof( module_header_t));   
        all_modules_num++;
    }else{ 
	    TRACE( class, 10, "module info already on list, passing out..", "");
    }
    
  
    for( i = 0; i < hcd_modules_num; i++)
        TRACE( class, 8, "module hcd list %d = %x", i, hcd_modules[i]->module_header.module_id);


    for( i = 0; i < all_modules_num; i++)
        TRACE( class, 8, "module list %d = %x", i, modules[i]->module_id);

     

	TRACE( class, 8, "Setting trace_level in this hcd", "");
    hcd_modules[hcd_modules_num-1]->hcd_set_trace_level(
            hcd_modules[hcd_modules_num-1]->module_header.soft,  (void *) &global_trace_class);       
    

    TRACE( class, 6, "end", "");
    return( 0);
    
}

int usbcore_unregister_hcd_driver( void *hcd_arg){
    hcd_methods_t *hcd_m = (hcd_methods_t *) hcd_arg;
    int i, already_on_list = 0;
    int delete_index;
    int rc;
    uint64_t class = TRC_USB_CORE | TRC_HELPER;

    TRACE( class, 6, "start", "");
    TRACE( class, 6, "Unregistering HCD driver '%s' with ID='0x%x'", 
        hcd_m->module_header.short_description, hcd_m->module_header.module_id);

    TRACE( class, 6, "hcd_modules_num=%d, all_modules_num=%d", 
        hcd_modules_num, all_modules_num);
        
    if( hcd_modules_num == 0)
        goto drop_module_info;
        
             
    /* look if this HCD already has been added to hcd methods list */
    for( i = 0; i < hcd_modules_num; i++){ 
	    if( hcd_modules[i]->module_header.module_id == hcd_m->module_header.module_id){
	        already_on_list = 1;
	        delete_index = i;
	        break;
	    }
	}

    /* no, it's not on the list */   
    if( already_on_list == 0 ){
        goto drop_module_info;
    }

    if( hcd_modules_num == 1) 
        delete_index = 0;

    gc_mark( (void *) hcd_modules[delete_index]); 
    
    for( i = delete_index; i < (hcd_modules_num-1); i++)
        hcd_modules[i] = hcd_modules[i+1];
        
    hcd_modules[i] = NULL;    
    
    if( hcd_modules_num > 0)
        hcd_modules_num--;

drop_module_info:
    if( all_modules_num == 0){
        rc = 0;
        goto exit;    
    }
    
    /* now look if this module info is already on modules list */
    already_on_list = 0;
    for( i = 0; i < all_modules_num; i++){
        if( modules[i]->module_id == hcd_m->module_header.module_id){
	        already_on_list = 1;
	        delete_index = i;
	        break;
   	    }    
    }    

    /* no, it's not on the list */   
    if( already_on_list == 0 ){
		rc = 0;
        goto exit;
    }

    if( all_modules_num == 1) 
        delete_index = 0;


    gc_mark( (void *) modules[delete_index]);
    for( i = delete_index; i < (all_modules_num-1); i++)
        modules[i] = modules[i+1];
        
    modules[i] = NULL;   
    if( all_modules_num > 0) 
        all_modules_num--;

    rc = 0;
exit:    

    TRACE( class, 6, "OK hcd_modules_num=%d, all_modules_num=%d", 
        hcd_modules_num, all_modules_num);

    for( i = 0; i < hcd_modules_num; i++)
        TRACE( class, 8, "module hcd list %d = %x", i, hcd_modules[i]->module_header.module_id);


    for( i = 0; i < all_modules_num; i++)
        TRACE( class, 8, "module list %d = %x", i, modules[i]->module_id);
    
    TRACE( class, 6, "end", "");
    return( rc);
    
}
