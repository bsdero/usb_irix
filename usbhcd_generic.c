/*
********************************************************************
* USB stack and host controller driver for SGI IRIX 6.5            *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: usbehci.c                                                  *
* Description: EHCI host controller driver                         *
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
USB_trace_class_t                                   global_trace_class = { 0xff, TRC_ALL};


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
*   Supported EHCI devices                                                                            *
*******************************************************************************************************
*/
static struct{
    uint32_t       device_id;
    uchar_t          *controller_description;
}ehci_descriptions[]={
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
    0x31041106, "VIA VT6212 EHCI USB 2.0 controller",
    0 ,NULL
};



/*
*******************************************************************************************************
* EHCI instance                                                                                       *
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
#define usbehci_SST_RX_READY      (0x0001)
#define usbehci_SST_TX_READY      (0x0002)
#define usbehci_SST_ERROR         (0x0004)
#define usbehci_SST_INUSE         (0x8000)
    pciio_intr_t            ps_intr;       /* pciio intr for INTA and INTB */
    pciio_dmamap_t          ps_ctl_dmamap; /* control channel dma mapping */
    pciio_dmamap_t          ps_str_dmamap; /* stream channel dma mapping */
    struct pollhead         *ps_pollhead;  /* for poll() */
    int                     ps_blocks;  /* block dev size in NBPSCTR blocks */
}usbehci_instance_t;

/*
*******************************************************************************************************
* Global variables                                                                                    *
*******************************************************************************************************
*/
int                                                 usbehci_devflag = D_MP;
char                                                *usbehci_mversion = M_VERSION;  /* for loadable modules */
int                                                 usbehci_inuse = 0;     /* number of "usbehci" devices open */
gc_list_t                                           gc_list;
usbehci_instance_t                                  *global_soft = NULL; 
 
 
/*
*******************************************************************************************************
* Callback functions for usbcore                                                                      *
*******************************************************************************************************
*/
int ehci_reset( void *hcd);
int ehci_start( void *hcd);
int ehci_stop( void *hcd);
int ehci_shutdown( void *hcd);
int ehci_suspend( void *hcd);
int ehci_resume( void *hcd);
int ehci_status( void *hcd);
void ehci_free_pipe( void *hcd, struct usb_pipe *pipe);
struct usb_pipe *ehci_alloc_default_control_pipe( void *hcd, struct usb_pipe *pipe);
struct usb_pipe *ehci_alloc_bulk_pipe( void *hcd, struct usb_pipe *pipe, struct usb_endpoint_descriptor *descriptor);
int ehci_send_control( void *hcd, struct usb_pipe *pipe, int dir, void *cmd, int cmdsize, void *data, int datasize);
int ehci_usb_send_bulk( void *hcd, struct usb_pipe *pipe, int dir, void *data, int datasize);
int ehci_alloc_intr_pipe( void *hcd, struct usb_pipe *pipe, struct usb_endpoint_descriptor *descriptor);
int ehci_usb_poll_intr( void *hcd, struct usb_pipe *pipe, void *data);
int ehci_ioctl( void *hcd, int cmd, void *uarg);
int ehci_set_trace_level( void *hcd, void *trace_level);

/*
*******************************************************************************************************
* Methods for usbcore                                                                                 *
*******************************************************************************************************
*/
hcd_methods_t ehci_methods={
	{ USB_EHCI, "EHCI", "Enhanced Host Controller Interface (USB 2.0)", "usbehci.o", 
	USB_DRIVER_IS_HCD, 0, 0, 0, 0, NULL},	
	ehci_reset,
	ehci_start, 
	ehci_stop,
	ehci_shutdown,
	ehci_suspend,
	ehci_resume,
	ehci_status,
	ehci_free_pipe,
	ehci_alloc_default_control_pipe, 
	ehci_alloc_bulk_pipe, 
	ehci_send_control, 
	ehci_usb_send_bulk,
	ehci_alloc_intr_pipe,
	ehci_usb_poll_intr,
	ehci_ioctl,
	ehci_set_trace_level,
	0x00000000
};


/*
*******************************************************************************************************
* Entry points                                                                                        *
*******************************************************************************************************
*/
void                  usbehci_init(void);
int                   usbehci_unload(void);
int                   usbehci_reg(void);
int                   usbehci_unreg(void);
int                   usbehci_attach(vertex_hdl_t conn);
int                   usbehci_detach(vertex_hdl_t conn);
static pciio_iter_f   usbehci_reloadme;
static pciio_iter_f   usbehci_unloadme;
int                   usbehci_open(dev_t *devp, int oflag, int otyp, cred_t *crp);
int                   usbehci_close(dev_t dev, int oflag, int otyp, cred_t *crp);
int                   usbehci_ioctl(dev_t dev, int cmd, void *arg, int mode, cred_t *crp, int *rvalp);
int                   usbehci_read(dev_t dev, uio_t * uiop, cred_t *crp);
int                   usbehci_write(dev_t dev, uio_t * uiop,cred_t *crp);
int                   usbehci_strategy(struct buf *bp);
int                   usbehci_poll(dev_t dev, short events, int anyyet, 
                               short *reventsp, struct pollhead **phpp, unsigned int *genp);
int                   usbehci_map(dev_t dev, vhandl_t *vt, off_t off, size_t len, uint_t prot);
int                   usbehci_unmap(dev_t dev, vhandl_t *vt);
void                  usbehci_dma_intr(intr_arg_t arg);
static error_handler_f usbehci_error_handler;
void                  usbehci_halt(void);
int                   usbehci_size(dev_t dev);
int                   usbehci_print(dev_t dev, char *str);
 
/*
*******************************************************************************************************
* Host controller callback implementation for USBcore                                                 *
*******************************************************************************************************
*/
int ehci_reset( void *hcd){
}

int ehci_start( void *hcd){
}

int ehci_stop( void *hcd){
}

int ehci_shutdown( void *hcd){
}

int ehci_suspend( void *hcd){
}

int ehci_resume( void *hcd){
}

int ehci_status( void *hcd){ /* just a demo */
    usbehci_instance_t *soft = ( usbehci_instance_t *) hcd;
    uint64_t class = TRC_USB_HCD | TRC_HELPER | TRC_HCD_SET( USB_EHCI);
    
    
    TRACE( class, 10, "start", "");
    TRACE( class, 12, "hcd driver = '%s'", 
        soft->module_header.short_description);
    TRACE( class, 10, "end", "");
    return( 1234);
}

void ehci_free_pipe( void *hcd, struct usb_pipe *pipe){
}

struct usb_pipe *ehci_alloc_default_control_pipe( void *hcd, struct usb_pipe *pipe){
}

struct usb_pipe *ehci_alloc_bulk_pipe( void *hcd, struct usb_pipe *pipe, struct usb_endpoint_descriptor *descriptor){
}

int ehci_send_control( void *hcd, struct usb_pipe *pipe, int dir, void *cmd, int cmdsize, void *data, int datasize){
}

int ehci_usb_send_bulk( void *hcd, struct usb_pipe *pipe, int dir, void *data, int datasize){
}

int ehci_alloc_intr_pipe( void *hcd, struct usb_pipe *pipe, struct usb_endpoint_descriptor *descriptor){
}

int ehci_usb_poll_intr( void *hcd, struct usb_pipe *pipe, void *data){
}

int ehci_ioctl( void *hcd, int cmd, void *uarg){
    	
}



int ehci_set_trace_level( void *hcd, void *arg){
	USB_trace_class_t *trace_arg = (USB_trace_class_t *) arg;
    uint64_t class = TRC_USB_HCD | TRC_HELPER | TRC_HCD_SET( USB_EHCI);
    	
	TRACE( class, 10, "start", "");

	global_trace_class.class = trace_arg->class;
	global_trace_class.level = trace_arg->level;
	
    TRACE( class, 12, "trace class settled; class=0x%x, level=%d", 
       global_trace_class.class, global_trace_class.level);
	TRACE( class, 10, "end", "");	
}



int register_ehci_with_usbcore(void *hcd){
    USB_func_t                  func;
    vertex_hdl_t                vhdl; 
	usbcore_instance_t          *usbcore;
	usbehci_instance_t          *soft;
	int rc; 
    uint64_t class = TRC_USB_HCD | TRC_HELPER | TRC_HCD_SET( USB_EHCI);
	char str[45];
	
	soft = (usbehci_instance_t *) hcd;
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
		
	
	ehci_methods.module_header.soft = (void *) soft;
    bcopy( (void *) &ehci_methods.module_header, (void *) &soft->module_header,
        sizeof( module_header_t));
	
	func = usbcore->register_hcd_driver; 
    rc = func( (void *) &ehci_methods);
	TRACE( class, 6,"usbcore:register_hcd_driver() rc = %d", rc);
	
	soft->ps_event_func = usbcore->event_func;
	
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
* usbehci_init: called once during system startup or                                                  *
* when a loadable driver is loaded.                                                                   *
*******************************************************************************************************
*/
void usbehci_init(void){
    uint64_t class = TRC_USB_HCD | TRC_INIT | TRC_HCD_SET( USB_EHCI);

	
  	TRACE( class, 2, "start", "");
  	TRACE( class, 0, "**********************************************************", "");
  	TRACE( class, 0, "* EHCI USB Driver for Silicon Graphics Irix 6.5          *", "");
  	TRACE( class, 0, "* By bsdero at gmail dot org, 2011                       *", "");
  	TRACE( class, 0, "* Version %s                                        *", USBCORE_DRV_VERSION);
  	TRACE( class, 0, "* Sequence %s                                *", USBCORE_DRV_SEQ);
  	TRACE( class, 0, "**********************************************************", "");
    TRACE( class, 0, "EHCI Driver loaded!", "");

    /*
     * if we are already registered, note that this is a
     * "reload" and reconnect all the places we attached.
     */

    pciio_iterate("usbehci_", usbehci_reloadme);	
    gc_list_init( &gc_list);
	TRACE( class, 2, "end", "");
}


/*
*******************************************************************************************************
*    usbehci_unload: if no "usbehci" is open, put us to bed                                           *
*      and let the driver text get unloaded.                                                          *
*******************************************************************************************************
*/
int usbehci_unload(void){
    uint64_t class = TRC_USB_HCD | TRC_UNLOAD | TRC_HCD_SET( USB_EHCI);
	TRACE( class, 2, "start", "");
    if (usbehci_inuse)
        return EBUSY;

    pciio_iterate("usbehci_", usbehci_unloadme);
    gc_list_destroy( &gc_list);      
    TRACE( class, 0, "EHCI Driver unloaded", "");
	TRACE( class, 2, "end", "");
    
    return 0;
}



/*
*******************************************************************************************************
*    usbehci_reg: called once during system startup or                                                *
*      when a loadable driver is loaded.                                                              *
*    NOTE: a bus provider register routine should always be                                           *
*      called from _reg, rather than from _init. In the case                                          *
*      of a loadable module, the devsw is not hooked up                                               *
*      when the _init routines are called.                                                            *
*******************************************************************************************************
 */
int usbehci_reg(void){
	int i;
	uint16_t device_id, vendor_id;
    uint64_t class = TRC_USB_HCD | TRC_INIT | TRC_HCD_SET( USB_EHCI);
	
	TRACE( class, 2, "start", "");
	
    /* registering EHCI devices */
    for( i = 0; ehci_descriptions[i].device_id != 0; i++){
        device_id = ((ehci_descriptions[i].device_id & 0xffff0000) >> 16);
        vendor_id = (ehci_descriptions[i].device_id & 0x0000ffff);
        pciio_driver_register(vendor_id, device_id, "usbehci_", 0);  /* usbehci_attach and usbehci_detach entry points */
    }

	TRACE( class, 2, "end", "");
    return 0;
}



/*
*******************************************************************************************************
* usbehci_unreg: called when a loadable driver is unloaded.                                           *
*******************************************************************************************************
*/
int usbehci_unreg(void){
	TRACE( TRC_USB_HCD| TRC_UNLOAD|TRC_HCD_SET(USB_EHCI), 2, "start", "");
    pciio_driver_unregister("usbehci_");
	TRACE( TRC_USB_HCD| TRC_UNLOAD|TRC_HCD_SET(USB_EHCI), 2, "end", "");
    return 0;
}


/*
*******************************************************************************************************
*    usbehci_attach: called by the pciio infrastructure                                               *
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
int usbehci_attach(vertex_hdl_t conn){
    vertex_hdl_t            vhdl = GRAPH_VERTEX_NONE;    
    vertex_hdl_t            charv = GRAPH_VERTEX_NONE;       
    volatile uchar_t        *cfg;
    usbehci_instance_t      *soft;
    pciio_piomap_t          cmap = 0;
    pciio_piomap_t          rmap = 0;
    graph_error_t           ret = (graph_error_t) 0;
    int rc;
    uint64_t                class = TRC_USB_HCD| TRC_ATTACH|TRC_HCD_SET(USB_EHCI);
    
	TRACE( class, 2, "start", "");
	TRACE( class, 0, "EHCI Host Controller Device Detected!", "");
    
    if (GRAPH_SUCCESS != hwgraph_traverse(GRAPH_VERTEX_NONE, "/usb", &vhdl)){
		TRACERR( class, "error in hwgraph_traverse(), quitting..", ""); 
        return( -1);
    }
    

    ret = hwgraph_edge_get( vhdl, "usbehci", &charv);
    TRACE( class, 8, "rc of hwgraph_edge_get() = %d, added usbehci edge", ret);

    ret = hwgraph_char_device_add( vhdl, "usbehci", "usbehci_", &charv); 
    TRACE( class, 8, 
        "rc of hwgraph_char_device_add() = %d, added usbehci edge (char device)", ret);    


    /*
     * Allocate a place to put per-device information for this vertex.
     * Then associate it with the vertex in the most efficient manner.
     */
    soft = (usbehci_instance_t *) gc_malloc( &gc_list, sizeof( usbehci_instance_t));
    
    ASSERT(soft != NULL);
    device_info_set(vhdl, (void *)soft);
    soft->ps_conn = conn;
    soft->ps_vhdl = vhdl;
    soft->ps_charv = charv;


    /* if all is ok, register host controller driver here */
    rc = register_ehci_with_usbcore((void *) soft);
    TRACE( class, 4, "register_ehci_with_usbcore() rc = %d", rc);

	TRACE( class, 2, "end", "");

    return 0;                      /* attach successsful */
}




/*
*******************************************************************************************************
*    usbehci_detach: called by the pciio infrastructure                                               *
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
int usbehci_detach(vertex_hdl_t conn){
    vertex_hdl_t                  vhdl, blockv, charv;
    usbehci_instance_t            *soft;
    uint64_t                      class = TRC_USB_HCD| TRC_DETACH|TRC_HCD_SET(USB_EHCI);
    
    
	TRACE( class, 2, "start", "");
    if (GRAPH_SUCCESS != hwgraph_traverse(GRAPH_VERTEX_NONE, "/usb", &vhdl))
        return( -1);
        
    soft = (usbehci_instance_t *) device_info_get(vhdl);
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
    hwgraph_edge_remove(vhdl, "usbehci", &charv);
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
*    usbehci_reloadme: utility function used indirectly                                               *
*      by usbehci_init, via pciio_iterate, to "reconnect"                                             *
*      each connection point when the driver has been                                                 *
*      reloaded.                                                                                      *
*******************************************************************************************************
*/
static void usbehci_reloadme(vertex_hdl_t conn){
    vertex_hdl_t            vhdl;
    usbehci_instance_t      *soft;
    uint64_t                class = TRC_USB_HCD | TRC_INIT | TRC_HCD_SET(USB_EHCI);

	TRACE( class, 2, "start", "");
    if (GRAPH_SUCCESS != hwgraph_traverse(conn, "usbehci", &vhdl))
        return;
        
    soft = (usbehci_instance_t *) device_info_get(vhdl);
    /*
     * Reconnect our error and interrupt handlers
     */
    pciio_error_register(conn, usbehci_error_handler, soft);
    pciio_intr_connect(soft->ps_intr, usbehci_dma_intr, soft, 0);
	TRACE( class, 2, "end", "");
}





/*
*******************************************************************************************************
 *    usbehci_unloadme: utility function used indirectly by
 *      usbehci_unload, via pciio_iterate, to "disconnect" each
 *      connection point before the driver becomes unloaded.
 */
static void usbehci_unloadme(vertex_hdl_t pconn){
    vertex_hdl_t            vhdl;
    usbehci_instance_t      *soft;
    uint64_t                class = TRC_USB_HCD | TRC_UNLOAD | TRC_HCD_SET(USB_EHCI);
    
	TRACE( class, 2, "start", "");
    if (GRAPH_SUCCESS != hwgraph_traverse(pconn, "usbehci", &vhdl))
        return;
        
    soft = (usbehci_instance_t *) device_info_get(vhdl);
    /*
     * Disconnect our error and interrupt handlers
     */
    pciio_error_register(pconn, 0, 0);
    pciio_intr_disconnect(soft->ps_intr);
	TRACE( class, 2, "end", "");
}



/*
*******************************************************************************************************
*    pciehci_open: called when a device special file is                                               * 
*      opened or when a block device is mounted.                                                      *
*******************************************************************************************************
*/ 
int usbehci_open(dev_t *devp, int oflag, int otyp, cred_t *crp){
    /* user should not open this module, it should be used through usbcore module */        
    return( EINVAL);
}




/*
*******************************************************************************************************
*    usbehci_close: called when a device special file
*      is closed by a process and no other processes
*      still have it open ("last close").
*******************************************************************************************************
*/ 
int usbehci_close(dev_t dev, int oflag, int otyp, cred_t *crp){
    /* user should not open this module, it should be used through usbcore module */        
    return 0;
}


/*
*******************************************************************************************************
*    usbehci_ioctl: a user has made an ioctl request                                                  *
*      for an open character device.                                                                  *
*      Arguments cmd and arg are as specified by the user;                                            *
*      arg is probably a pointer to something in the user's                                           *
*      address space, so you need to use copyin() to                                                  *
*      read through it and copyout() to write through it.                                             *
*******************************************************************************************************
*/ 
int usbehci_ioctl(dev_t dev, int cmd, void *arg, int mode, cred_t *crp, int *rvalp){
    /* user should not open this module, it should be used through usbcore module */        
    *rvalp = -1;
    return ENOTTY;          /* TeleTypeŽ is a registered trademark */
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
int usbehci_read(dev_t dev, uio_t * uiop, cred_t *crp){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
}

int usbehci_write(dev_t dev, uio_t * uiop, cred_t *crp){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
}

int usbehci_strategy(struct buf *bp){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
    return 0;
}

/*
*******************************************************************************************************
* Poll Entry points                                                                                   *
*******************************************************************************************************
*/
int usbehci_poll(dev_t dev, short events, int anyyet,
           short *reventsp, struct pollhead **phpp, unsigned int *genp){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
}


/*
*******************************************************************************************************
* Memory map entry points                                                                             *
*******************************************************************************************************
*/
int usbehci_map(dev_t dev, vhandl_t *vt, off_t off, size_t len, uint_t prot){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
}

int usbehci_unmap(dev_t dev, vhandl_t *vt){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
}


/*
*******************************************************************************************************
* Interrupt entry point                                                                               *
* We avoid using the standard name, since our prototype has changed.                                  *
*******************************************************************************************************
*/
void usbehci_dma_intr(intr_arg_t arg){

    usbehci_instance_t        *soft = (usbehci_instance_t *) arg;
    
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
static int usbehci_error_handler(void *einfo,
                    int error_code,
                    ioerror_mode_t mode,
                    ioerror_t *ioerror){

/* not used right now */


/*    usbehci_instance_t            *soft = (usbehci_instance_t *) einfo;
    vertex_hdl_t                  vhdl = soft->ps_vhdl;
#if DEBUG && ERROR_DEBUG
    cmn_err(CE_CONT, "%v: usbehci_error_handler\n", vhdl);
#else
    vhdl = vhdl;
#endif
    ioerror_dump("usbehci", error_code, mode, ioerror);
*/



    return IOERROR_HANDLED;
}



/*
*******************************************************************************************************
* Support entry points                                                                                *
*******************************************************************************************************
*    usbehci_halt: called during orderly system                                                       *
*      shutdown; no other device driver call will be                                                  *
*      made after this one.                                                                           *
*******************************************************************************************************
*/
void usbehci_halt(void){
   	TRACE( TRC_USB_HCD| TRC_HELPER|TRC_HCD_SET(USB_EHCI), 2, "start", "");
	TRACE( TRC_USB_HCD| TRC_HELPER|TRC_HCD_SET(USB_EHCI), 2, "end", "");

}



/*
*******************************************************************************************************
*    usbehci_size: return the size of the device in                                                   *
*      "sector" units (multiples of NBPSCTR).                                                         *
*******************************************************************************************************
*/
int usbehci_size(dev_t dev){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
}


/*
*******************************************************************************************************
*    usbehci_print: used by the kernel to report an                                                   *
*      error detected on a block device.                                                              *
*******************************************************************************************************
*/
int usbehci_print(dev_t dev, char *str){
	/* not used */
    return 0;
}
