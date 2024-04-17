/*
********************************************************************
* USB stack and host controller driver for SGI IRIX 6.5            *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: usbxxxhci.c                                                  *
* Description: XXXHCI host controller driver                         *
*                                                                  *
********************************************************************
*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      05-03-2012     -Initial version                                                         *
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
*   Supported XXXHCI devices                                                                            *
*******************************************************************************************************
*/
static struct{
    uint32_t       device_id;
    uchar_t          *controller_description;
}xxxhci_descriptions[]={
    0x268c8086, "Intel 63XXESB USB 2.0 controller",
    0x523910b9, "ALi M5239 USB 2.0 controller",
    0x10227463, "AMD 8111 USB 2.0 controller",
    0x20951022, "(AMD CS5536 Geode) USB 2.0 controller",
    0x43451002, "ATI SB200 USB 2.0 controller",
    0x43731002, "ATI SB400 USB 2.0 controller",
    0x25ad8086, "Intel 6300ESB USB 2.0 controller",
    0x24cd8086, "(Intel 82801DB/L/M ICH4) USB 2.0 controller",
    0x24dd8086, "(Intel 82801EB/R ICH5) USB 2.0 controller",
    0x265c8086, "(Intel 82801FB ICH6) USB 2.0 controller",
    0x27cc8086, "(Intel 82801GB/R ICH7) USB 2.0 controller",
    0x28368086, "(Intel 82801H ICH8) USB 2.0 controller USB2-A",
    0x283a8086, "(Intel 82801H ICH8) USB 2.0 controller USB2-B",
    0x293a8086, "(Intel 82801I ICH9) USB 2.0 controller",
    0x293c8086, "(Intel 82801I ICH9) USB 2.0 controller",
    0x3a3a8086, "(Intel 82801JI ICH10) USB 2.0 controller USB-A",
    0x3a3c8086, "(Intel 82801JI ICH10) USB 2.0 controller USB-B",
    0x3b348086, "Intel PCH USB 2.0 controller USB-A",
    0x3b3c8086, "Intel PCH USB 2.0 controller USB-B",
    0x00e01033, "NEC uPD 720100 USB 2.0 controller",
    0x006810de, "NVIDIA nForce2 USB 2.0 controller",
    0x008810de, "NVIDIA nForce2 Ultra 400 USB 2.0 controller",
    0x00d810de, "NVIDIA nForce3 USB 2.0 controller",
    0x00e810de, "NVIDIA nForce3 250 USB 2.0 controller",
    0x005b10de, "NVIDIA nForce4 USB 2.0 controller",
    0x036d10de, "NVIDIA nForce MCP55 USB 2.0 controller",
    0x03f210de, "NVIDIA nForce MCP61 USB 2.0 controller",
    0x0aa610de, "NVIDIA nForce MCP79 USB 2.0 controller",
    0x0aa910de, "NVIDIA nForce MCP79 USB 2.0 controller",
    0x0aaa10de, "NVIDIA nForce MCP79 USB 2.0 controller",
    0x15621131, "Philips ISP156x USB 2.0 controller",
    0x31041106, "VIA VT6212 XXXHCI USB 2.0 controller",
    0 ,NULL
};



/*
*******************************************************************************************************
* XXXHCI instance                                                                                       *
*******************************************************************************************************
*/
typedef struct {
	module_header_t        module_header;
    vertex_hdl_t           ps_conn;        /* connection for pci services  */
    vertex_hdl_t           ps_vhdl;        /* backpointer to device vertex */
    vertex_hdl_t           ps_charv;       /* backpointer to device vertex */
    USB_func_t             ps_event_func;  /* function to USBcore event    */ 
    uchar_t                *ps_cfg;        /* cached ptr to my config regs */
    pciio_piomap_t         ps_cmap;        /* piomap (if any) for ps_cfg */
    pciio_piomap_t         ps_rmap;        /* piomap (if any) for ps_regs */
    unsigned               ps_sst;         /* driver "software state" */
#define usbxxxhci_SST_RX_READY      (0x0001)
#define usbxxxhci_SST_TX_READY      (0x0002)
#define usbxxxhci_SST_ERROR         (0x0004)
#define usbxxxhci_SST_INUSE         (0x8000)
    pciio_intr_t            ps_intr;       /* pciio intr for INTA and INTB */
    pciio_dmamap_t          ps_ctl_dmamap; /* control channel dma mapping */
    pciio_dmamap_t          ps_str_dmamap; /* stream channel dma mapping */
    struct pollhead         *ps_pollhead;  /* for poll() */
    int                     ps_blocks;  /* block dev size in NBPSCTR blocks */
}usbxxxhci_instance_t;

/*
*******************************************************************************************************
* Global variables                                                                                    *
*******************************************************************************************************
*/
int                                                 usbxxxhci_devflag = D_MP;
char                                                *usbxxxhci_mversion = M_VERSION;  /* for loadable modules */
int                                                 usbxxxhci_inuse = 0;     /* number of "usbxxxhci" devices open */
gc_list_t                                           gc_list;
usbxxxhci_instance_t                                  *global_soft = NULL; 
 
 
/*
*******************************************************************************************************
* Callback functions for usbcore                                                                      *
*******************************************************************************************************
*/
int xxxhci_reset( void *hcd);
int xxxhci_start( void *hcd);
int xxxhci_init( void *hcd);
int xxxhci_stop( void *hcd);
int xxxhci_shutdown( void *hcd);
int xxxhci_suspend( void *hcd);
int xxxhci_resume( void *hcd);
int xxxhci_status( void *hcd);
void xxxhci_free_pipe( void *hcd, struct usb_pipe *pipe);
struct usb_pipe *xxxhci_alloc_default_control_pipe( void *hcd, struct usb_pipe *pipe);
struct usb_pipe *xxxhci_alloc_bulk_pipe( void *hcd, struct usb_pipe *pipe, struct usb_endpoint_descriptor *descriptor);
int xxxhci_send_control( void *hcd, struct usb_pipe *pipe, int dir, void *cmd, int cmdsize, void *data, int datasize);
int xxxhci_usb_send_bulk( void *hcd, struct usb_pipe *pipe, int dir, void *data, int datasize);
int xxxhci_alloc_intr_pipe( void *hcd, struct usb_pipe *pipe, struct usb_endpoint_descriptor *descriptor);
int xxxhci_usb_poll_intr( void *hcd, struct usb_pipe *pipe, void *data);
int xxxhci_ioctl( void *hcd, int cmd, void *uarg);
int xxxhci_set_trace_level( void *hcd, void *trace_level);

/*
*******************************************************************************************************
* Methods for usbcore                                                                                 *
*******************************************************************************************************
*/
hcd_methods_t xxxhci_methods={
	{ 1, "XXXHCI", "Enhanced Host Controller Interface (USB 2.0)", "usbxxxhci.o", 
	USB_DRIVER_IS_HCD, 0, 0, 0, 0, NULL},	
	xxxhci_reset,
	xxxhci_start,
	xxxhci_init, 
	xxxhci_stop,
	xxxhci_shutdown,
	xxxhci_suspend,
	xxxhci_resume,
	xxxhci_status,
	xxxhci_free_pipe,
	xxxhci_alloc_default_control_pipe, 
	xxxhci_alloc_bulk_pipe, 
	xxxhci_send_control, 
	xxxhci_usb_send_bulk,
	xxxhci_alloc_intr_pipe,
	xxxhci_usb_poll_intr,
	xxxhci_ioctl,
	xxxhci_set_trace_level,
	0x00000000
};


/*
*******************************************************************************************************
* Entry points                                                                                        *
*******************************************************************************************************
*/
void                  usbxxxhci_init(void);
int                   usbxxxhci_unload(void);
int                   usbxxxhci_reg(void);
int                   usbxxxhci_unreg(void);
int                   usbxxxhci_attach(vertex_hdl_t conn);
int                   usbxxxhci_detach(vertex_hdl_t conn);
static pciio_iter_f   usbxxxhci_reloadme;
static pciio_iter_f   usbxxxhci_unloadme;
int                   usbxxxhci_open(dev_t *devp, int oflag, int otyp, cred_t *crp);
int                   usbxxxhci_close(dev_t dev, int oflag, int otyp, cred_t *crp);
int                   usbxxxhci_ioctl(dev_t dev, int cmd, void *arg, int mode, cred_t *crp, int *rvalp);
int                   usbxxxhci_read(dev_t dev, uio_t * uiop, cred_t *crp);
int                   usbxxxhci_write(dev_t dev, uio_t * uiop,cred_t *crp);
int                   usbxxxhci_strategy(struct buf *bp);
int                   usbxxxhci_poll(dev_t dev, short events, int anyyet, 
                               short *reventsp, struct pollhead **phpp, unsigned int *genp);
int                   usbxxxhci_map(dev_t dev, vhandl_t *vt, off_t off, size_t len, uint_t prot);
int                   usbxxxhci_unmap(dev_t dev, vhandl_t *vt);
void                  usbxxxhci_dma_intr(intr_arg_t arg);
static error_handler_f usbxxxhci_error_handler;
void                  usbxxxhci_halt(void);
int                   usbxxxhci_size(dev_t dev);
int                   usbxxxhci_print(dev_t dev, char *str);


/*
*******************************************************************************************************
* Host controller specific hardware functions                                                         *
*******************************************************************************************************
*/
/*
 PUT YOUR FUNCTIONS HERE
 * 
 * 
 * 
 * 
 * 
 * 
 */


 
/*
*******************************************************************************************************
* Host controller callback implementation for USBcore                                                 *
*******************************************************************************************************
*/
int xxxhci_reset( void *hcd){
}

int xxxhci_start( void *hcd){
}

int xxxhci_init( void *hcd){
}

int xxxhci_stop( void *hcd){
}

int xxxhci_shutdown( void *hcd){
}

int xxxhci_suspend( void *hcd){
}

int xxxhci_resume( void *hcd){
}

int xxxhci_status( void *hcd){ /* just a demo */
    usbxxxhci_instance_t *soft = ( usbxxxhci_instance_t *) hcd;
    uint64_t class = TRC_USB_HCD | TRC_HELPER | TRC_HCD_SET( USB_XXXHCI);
    
    
    TRACE( class, 10, "start", "");
    TRACE( class, 12, "hcd driver = '%s'", 
        soft->module_header.short_description);
    TRACE( class, 10, "end", "");
    return( 1234);
}

void xxxhci_free_pipe( void *hcd, struct usb_pipe *pipe){
}

struct usb_pipe *xxxhci_alloc_default_control_pipe( void *hcd, struct usb_pipe *pipe){
}

struct usb_pipe *xxxhci_alloc_bulk_pipe( void *hcd, struct usb_pipe *pipe, struct usb_endpoint_descriptor *descriptor){
}

int xxxhci_send_control( void *hcd, struct usb_pipe *pipe, int dir, void *cmd, int cmdsize, void *data, int datasize){
}

int xxxhci_usb_send_bulk( void *hcd, struct usb_pipe *pipe, int dir, void *data, int datasize){
}

int xxxhci_alloc_intr_pipe( void *hcd, struct usb_pipe *pipe, struct usb_endpoint_descriptor *descriptor){
}

int xxxhci_usb_poll_intr( void *hcd, struct usb_pipe *pipe, void *data){
}

int xxxhci_ioctl( void *hcd, int cmd, void *uarg){
    	
}



int xxxhci_set_trace_level( void *hcd, void *arg){
	USB_trace_class_t *trace_arg = (USB_trace_class_t *) arg;
    uint64_t class = TRC_USB_HCD | TRC_HELPER | TRC_HCD_SET( USB_XXXHCI);
    	
	TRACE( class, 10, "start", "");

	global_trace_class.class = trace_arg->class;
	global_trace_class.level = trace_arg->level;
	
    TRACE( class, 12, "trace class settled; class=0x%x, level=%d", 
       global_trace_class.class, global_trace_class.level);
	TRACE( class, 10, "end", "");	
}


/*
*******************************************************************************************************
* USBcore calls functions                                                                             *
*******************************************************************************************************
*/
int register_xxxhci_with_usbcore(void *hcd){
    USB_func_t                  func;
    vertex_hdl_t                vhdl; 
	usbcore_instance_t          *usbcore;
	usbxxxhci_instance_t          *soft;
	int rc; 
    uint64_t class = TRC_USB_HCD | TRC_HELPER | TRC_HCD_SET( USB_XXXHCI);
	char str[45];
	
	soft = (usbxxxhci_instance_t *) hcd;
	TRACE( class, 2, "start", "");


    if (GRAPH_SUCCESS != hwgraph_traverse(GRAPH_VERTEX_NONE, "/usb/usbcore", &vhdl)){
 	    TRACERR( class,  
 	        "ERROR: hwgraph_traverse() could not follow /usb/usbcore path\n", "");
        return( EINVAL);
	}

	usbcore = (usbcore_instance_t *) device_info_get( vhdl);
	if( usbcore == NULL){
	    TRACERR( class,  
	        "ERROR: could not get usbcore information from device_info_get()\n", "");
	    return( EFAULT);
	}
		
	
	rc = usbcore->register_hcd_driver( (void *) &xxxhci_methods);
	TRACE( class, 6,"rc = %d", rc);
    soft->ps_event_func = usbcore->event_func;
	
	TRACE( class, 2, "end", "");
	
	return( rc);
}


int unregister_xxxhci_with_usbcore(void *hcd){
    USB_func_t                  func;
    vertex_hdl_t                vhdl; 
	usbcore_instance_t          *usbcore;
	usbxxxhci_instance_t          *soft;
	int rc; 
    uint64_t class = TRC_USB_HCD | TRC_HELPER | TRC_HCD_SET( USB_XXXHCI);
	char str[45];
	
	soft = (usbxxxhci_instance_t *) hcd;
	TRACE( class, 2, "start", "");


    if (GRAPH_SUCCESS != hwgraph_traverse(GRAPH_VERTEX_NONE, "/usb/usbcore", &vhdl)){
 	    TRACERR( class,  
 	        "ERROR: hwgraph_traverse() could not follow /usb/usbcore path\n", "");
        return( EINVAL);
	}

	usbcore = (usbcore_instance_t *) device_info_get( vhdl);
	if( usbcore == NULL){
	    TRACERR( class,  
	        "ERROR: could not get usbcore information from device_info_get()\n", "");
	    return( EFAULT);
	}

	func = usbcore->unregister_hcd_driver; 
    rc = func( (void *) &xxxhci_methods);
	TRACE( class, 6,"rc = %d", rc);
	TRACE( class, 2, "end", "");
	
	return( rc);
}



/*
*******************************************************************************************************
* Entry points                                                                                        *
*******************************************************************************************************
*/
    
/*
*******************************************************************************************************
* usbxxxhci_init: called once during system startup or                                                  *
* when a loadable driver is loaded.                                                                   *
*******************************************************************************************************
*/
void usbxxxhci_init(void){
    uint64_t class = TRC_USB_HCD | TRC_INIT | TRC_HCD_SET( USB_XXXHCI);

	
  	TRACE( class, 2, "start", "");
  	TRACE( class, 0, "**********************************************************", "");
  	TRACE( class, 0, "* XXXHCI USB Driver for Silicon Graphics Irix 6.5          *", "");
  	TRACE( class, 0, "* By bsdero at gmail dot org, 2011                       *", "");
  	TRACE( class, 0, "* Version %s                                        *", USBCORE_DRV_VERSION);
  	TRACE( class, 0, "* Sequence %s                                *", USBCORE_DRV_SEQ);
  	TRACE( class, 0, "**********************************************************", "");
    TRACE( class, 0, "XXXHCI Driver loaded!", "");

    /*
     * if we are already registered, note that this is a
     * "reload" and reconnect all the places we attached.
     */

    pciio_iterate("usbxxxhci_", usbxxxhci_reloadme);	
    gc_list_init( &gc_list);
	TRACE( class, 2, "end", "");
}


/*
*******************************************************************************************************
*    usbxxxhci_unload: if no "usbxxxhci" is open, put us to bed                                           *
*      and let the driver text get unloaded.                                                          *
*******************************************************************************************************
*/
int usbxxxhci_unload(void){
    uint64_t class = TRC_USB_HCD | TRC_UNLOAD | TRC_HCD_SET( USB_XXXHCI);
	TRACE( class, 2, "start", "");
    if (usbxxxhci_inuse)
        return EBUSY;

    pciio_iterate("usbxxxhci_", usbxxxhci_unloadme);
    gc_list_destroy( &gc_list);      
    TRACE( class, 0, "XXXHCI Driver unloaded", "");
	TRACE( class, 2, "end", "");
    
    return 0;
}



/*
*******************************************************************************************************
*    usbxxxhci_reg: called once during system startup or                                                *
*      when a loadable driver is loaded.                                                              *
*    NOTE: a bus provider register routine should always be                                           *
*      called from _reg, rather than from _init. In the case                                          *
*      of a loadable module, the devsw is not hooked up                                               *
*      when the _init routines are called.                                                            *
*******************************************************************************************************
 */
int usbxxxhci_reg(void){
	int i;
	uint16_t device_id, vendor_id;
    uint64_t class = TRC_USB_HCD | TRC_INIT | TRC_HCD_SET( USB_XXXHCI);
	
	TRACE( class, 2, "start", "");
	
    /* registering XXXHCI devices */
    for( i = 0; xxxhci_descriptions[i].device_id != 0; i++){
        device_id = ((xxxhci_descriptions[i].device_id & 0xffff0000) >> 16);
        vendor_id = (xxxhci_descriptions[i].device_id & 0x0000ffff);
        pciio_driver_register(vendor_id, device_id, "usbxxxhci_", 0);  /* usbxxxhci_attach and usbxxxhci_detach entry points */
    }

	TRACE( class, 2, "end", "");
    return 0;
}



/*
*******************************************************************************************************
* usbxxxhci_unreg: called when a loadable driver is unloaded.                                           *
*******************************************************************************************************
*/
int usbxxxhci_unreg(void){
	TRACE( TRC_USB_HCD| TRC_UNLOAD|TRC_HCD_SET(USB_XXXHCI), 2, "start", "");
    pciio_driver_unregister("usbxxxhci_");
	TRACE( TRC_USB_HCD| TRC_UNLOAD|TRC_HCD_SET(USB_XXXHCI), 2, "end", "");
    return 0;
}


/*
*******************************************************************************************************
*    usbxxxhci_attach: called by the pciio infrastructure                                               *
*      once for each vertex representing a crosstalk widget.                                          *
*      In large configurations, it is possible for a                                                  *
*      huge number of CPUs to enter this routine all at                                               *
*      nearly the same time, for different specific                                                   *
*      instances of the device. Attempting to give your                                               *
*      devices sequence numbers based on the order they                                               *
*      are found in the system is not only futile but may be                                          *
*      dangerous as the order may differ from run to run.                                             *
*******************************************************************************************************
*/
int usbxxxhci_attach(vertex_hdl_t conn){
    vertex_hdl_t            vhdl = GRAPH_VERTEX_NONE;    
    vertex_hdl_t            charv = GRAPH_VERTEX_NONE;       
    volatile uchar_t        *cfg;
    usbxxxhci_instance_t      *soft;
    pciio_piomap_t          cmap = 0;
    pciio_piomap_t          rmap = 0;
    graph_error_t           ret = (graph_error_t) 0;
    int rc;
    uint64_t                class = TRC_USB_HCD| TRC_ATTACH|TRC_HCD_SET(USB_XXXHCI);
    
	TRACE( class, 2, "start", "");
	TRACE( class, 0, "XXXHCI Host Controller Device Detected!", "");
    
    if (GRAPH_SUCCESS != hwgraph_traverse(GRAPH_VERTEX_NONE, "/usb", &vhdl)){
		TRACERR( class, "error in hwgraph_traverse(), quitting..", ""); 
        return( -1);
    }
    

    ret = hwgraph_edge_get( vhdl, "usbxxxhci", &charv);
    TRACE( class, 8, "rc of hwgraph_edge_get() = %d, added usbxxxhci edge", ret);

    ret = hwgraph_char_device_add( vhdl, "usbxxxhci", "usbxxxhci_", &charv); 
    TRACE( class, 8, 
        "rc of hwgraph_char_device_add() = %d, added usbxxxhci edge (char device)", ret);    


    /*
     * Allocate a place to put per-device information for this vertex.
     * Then associate it with the vertex in the most efficient manner.
     */
    soft = (usbxxxhci_instance_t *) gc_malloc( &gc_list, sizeof( usbxxxhci_instance_t));
    
    ASSERT(soft != NULL);
    device_info_set(vhdl, (void *)soft);
    soft->ps_conn = conn;
    soft->ps_vhdl = vhdl;
    soft->ps_charv = charv;
	xxxhci_methods.module_header.soft = (void *) soft;
    bcopy( (void *) &xxxhci_methods.module_header, (void *) &soft->module_header,
        sizeof( module_header_t));


    /* if all is ok, register host controller driver here */
    rc = register_xxxhci_with_usbcore((void *) soft);
    TRACE( class, 4, "rc = %d", rc);

	TRACE( class, 2, "end", "");

    return 0;                      /* attach successsful */
}




/*
*******************************************************************************************************
*    usbxxxhci_detach: called by the pciio infrastructure                                               *
*      once for each vertex representing a crosstalk                                                  *
*      widget when unregistering the driver.                                                          *
*                                                                                                     *
*      In large configurations, it is possible for a                                                  *
*      huge number of CPUs to enter this routine all at                                               *
*      nearly the same time, for different specific                                                   *
*      instances of the device. Attempting to give your                                               *
*      devices sequence numbers based on the order they                                               *
*      are found in the system is not only futile but may be                                          *
*      dangerous as the order may differ from run to run.                                             *
*******************************************************************************************************
*/
int usbxxxhci_detach(vertex_hdl_t conn){
    vertex_hdl_t                  vhdl, blockv, charv;
    usbxxxhci_instance_t            *soft;
    uint64_t                      class = TRC_USB_HCD| TRC_DETACH|TRC_HCD_SET(USB_XXXHCI);
    int rc;
    
	TRACE( class, 2, "start", "");
    if (GRAPH_SUCCESS != hwgraph_traverse(GRAPH_VERTEX_NONE, "/usb", &vhdl))
        return( -1);
        
    soft = (usbxxxhci_instance_t *) device_info_get(vhdl);
    rc = unregister_xxxhci_with_usbcore((void *) soft);
    TRACE( class, 4, "rc = %d", rc);
/*    
    pciio_error_register(conn, 0, 0);
    pciio_intr_disconnect(soft->ps_intr);
    pciio_intr_free(soft->ps_intr);
    phfree(soft->ps_pollhead);
    if (soft->ps_ctl_dmamap)
        pciio_dmamap_free(soft->ps_ctl_dmamap);
    if (soft->ps_str_dmamap)
        pciio_dmamap_free(soft->ps_str_dmamap);
    if (soft->ps_cmap)
        pciio_piomap_free(soft->ps_cmap);
    if (soft->ps_rmap)
        pciio_piomap_free(soft->ps_rmap);*/
    hwgraph_edge_remove(vhdl, "usbxxxhci", &charv);
    /*
     * we really need "hwgraph_dev_remove" ...
     */
     
    if (GRAPH_SUCCESS == hwgraph_edge_remove(vhdl, EDGE_LBL_CHAR, &charv)) {
        device_info_set(charv, NULL);
        hwgraph_vertex_destroy(charv);
    }
    
    device_info_set(vhdl, NULL);
    hwgraph_vertex_destroy(vhdl);
    gc_list_destroy( &gc_list);      
	TRACE( class, 2, "end", "");
    
    return 0;
}





/*
*******************************************************************************************************
*    usbxxxhci_reloadme: utility function used indirectly                                               *
*      by usbxxxhci_init, via pciio_iterate, to "reconnect"                                             *
*      each connection point when the driver has been                                                 *
*      reloaded.                                                                                      *
*******************************************************************************************************
*/
static void usbxxxhci_reloadme(vertex_hdl_t conn){
    vertex_hdl_t            vhdl;
    usbxxxhci_instance_t      *soft;
    uint64_t                class = TRC_USB_HCD | TRC_INIT | TRC_HCD_SET(USB_XXXHCI);

	TRACE( class, 2, "start", "");
    if (GRAPH_SUCCESS != hwgraph_traverse(conn, "usbxxxhci", &vhdl))
        return;
        
    soft = (usbxxxhci_instance_t *) device_info_get(vhdl);
    /*
     * Reconnect our error and interrupt handlers
     */
    pciio_error_register(conn, usbxxxhci_error_handler, soft);
    pciio_intr_connect(soft->ps_intr, usbxxxhci_dma_intr, soft, 0);
	TRACE( class, 2, "end", "");
}





/*
*******************************************************************************************************
 *    usbxxxhci_unloadme: utility function used indirectly by
 *      usbxxxhci_unload, via pciio_iterate, to "disconnect" each
 *      connection point before the driver becomes unloaded.
 */
static void usbxxxhci_unloadme(vertex_hdl_t pconn){
    vertex_hdl_t            vhdl;
    usbxxxhci_instance_t      *soft;
    uint64_t                class = TRC_USB_HCD | TRC_UNLOAD | TRC_HCD_SET(USB_XXXHCI);
    
	TRACE( class, 2, "start", "");
    if (GRAPH_SUCCESS != hwgraph_traverse(pconn, "usbxxxhci", &vhdl))
        return;
        
    soft = (usbxxxhci_instance_t *) device_info_get(vhdl);
    /*
     * Disconnect our error and interrupt handlers
     */
    pciio_error_register(pconn, 0, 0);
    pciio_intr_disconnect(soft->ps_intr);
	TRACE( class, 2, "end", "");
}



/*
*******************************************************************************************************
*    pcixxxhci_open: called when a device special file is                                               * 
*      opened or when a block device is mounted.                                                      *
*******************************************************************************************************
*/ 
int usbxxxhci_open(dev_t *devp, int oflag, int otyp, cred_t *crp){
    /* user should not open this module, it should be used through usbcore module */        
    return( EINVAL);
}




/*
*******************************************************************************************************
*    usbxxxhci_close: called when a device special file
*      is closed by a process and no other processes
*      still have it open ("last close").
*******************************************************************************************************
*/ 
int usbxxxhci_close(dev_t dev, int oflag, int otyp, cred_t *crp){
    /* user should not open this module, it should be used through usbcore module */        
    return 0;
}


/*
*******************************************************************************************************
*    usbxxxhci_ioctl: a user has made an ioctl request                                                  *
*      for an open character device.                                                                  *
*      Arguments cmd and arg are as specified by the user;                                            *
*      arg is probably a pointer to something in the user's                                           *
*      address space, so you need to use copyin() to                                                  *
*      read through it and copyout() to write through it.                                             *
*******************************************************************************************************
*/ 
int usbxxxhci_ioctl(dev_t dev, int cmd, void *arg, int mode, cred_t *crp, int *rvalp){
    /* user should not open this module, it should be used through usbcore module */        
    *rvalp = -1;
    return ENOTTY;          /* TeleType® is a registered trademark */
}





/*
*******************************************************************************************************
*          DATA TRANSFER ENTRY POINTS                                                                 *
*      Since I'm trying to provide an example for both                                                *
*      character and block devices, I'm routing read                                                  *
*      and write back through strategy as described in                                                *
*      the IRIX Device Driver Programming Guide.                                                      *
*      This limits our character driver to reading and                                                *
*      writing in multiples of the standard sector length.                                            *
*******************************************************************************************************
*/ 
int usbxxxhci_read(dev_t dev, uio_t * uiop, cred_t *crp){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
}

int usbxxxhci_write(dev_t dev, uio_t * uiop, cred_t *crp){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
}

int usbxxxhci_strategy(struct buf *bp){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
    return 0;
}

/*
*******************************************************************************************************
* Poll Entry points                                                                                   *
*******************************************************************************************************
*/
int usbxxxhci_poll(dev_t dev, short events, int anyyet,
           short *reventsp, struct pollhead **phpp, unsigned int *genp){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
}


/*
*******************************************************************************************************
* Memory map entry points                                                                             *
*******************************************************************************************************
*/
int usbxxxhci_map(dev_t dev, vhandl_t *vt, off_t off, size_t len, uint_t prot){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
}

int usbxxxhci_unmap(dev_t dev, vhandl_t *vt){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
}


/*
*******************************************************************************************************
* Interrupt entry point                                                                               *
* We avoid using the standard name, since our prototype has changed.                                  *
*******************************************************************************************************
*/
void usbxxxhci_dma_intr(intr_arg_t arg){

    usbxxxhci_instance_t        *soft = (usbxxxhci_instance_t *) arg;
    
    vertex_hdl_t            vhdl = soft->ps_vhdl;
    /*
     * for each buf our hardware has processed,
     *      set buf->b_resid,
     *      call pciio_dmamap_done,
     *      call bioerror() or biodone().
     *
     * XXX - would it be better for buf->b_iodone
     * to be used to get to pciio_dmamap_done?
     */
    /*
     * may want to call pollwakeup.
     */
}



/*
*******************************************************************************************************
* Error handling entry point                                                                          *
*******************************************************************************************************
*/
static int usbxxxhci_error_handler(void *einfo,
                    int error_code,
                    ioerror_mode_t mode,
                    ioerror_t *ioerror){

/* not used right now */


/*    usbxxxhci_instance_t            *soft = (usbxxxhci_instance_t *) einfo;
    vertex_hdl_t                  vhdl = soft->ps_vhdl;
#if DEBUG && ERROR_DEBUG
    cmn_err(CE_CONT, "%v: usbxxxhci_error_handler\n", vhdl);
#else
    vhdl = vhdl;
#endif
    ioerror_dump("usbxxxhci", error_code, mode, ioerror);
*/



    return IOERROR_HANDLED;
}



/*
*******************************************************************************************************
* Support entry points                                                                                *
*******************************************************************************************************
*    usbxxxhci_halt: called during orderly system                                                       *
*      shutdown; no other device driver call will be                                                  *
*      made after this one.                                                                           *
*******************************************************************************************************
*/
void usbxxxhci_halt(void){
   	TRACE( TRC_USB_HCD| TRC_HELPER|TRC_HCD_SET(USB_XXXHCI), 2, "start", "");
	TRACE( TRC_USB_HCD| TRC_HELPER|TRC_HCD_SET(USB_XXXHCI), 2, "end", "");

}



/*
*******************************************************************************************************
*    usbxxxhci_size: return the size of the device in                                                   *
*      "sector" units (multiples of NBPSCTR).                                                         *
*******************************************************************************************************
*/
int usbxxxhci_size(dev_t dev){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
}


/*
*******************************************************************************************************
*    usbxxxhci_print: used by the kernel to report an                                                   *
*      error detected on a block device.                                                              *
*******************************************************************************************************
*/
int usbxxxhci_print(dev_t dev, char *str){
	/* not used */
    return 0;
}


