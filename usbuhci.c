/*
********************************************************************
* USB stack and host controller driver for SGI IRIX 6.5            *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: usbuhci.c                                                  *
* Description: UHCI host controller driver                         *
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
*   Supported UHCI devices                                                                            *
*******************************************************************************************************
*/

static struct{
    uint32_t       device_id;
    uchar_t          *controller_description;
}uhci_descriptions[]={
    0x26888086, "Intel 631XESB/632XESB/3100 USB controller USB-1",
    0x26898086,"Intel 631XESB/632XESB/3100 USB controller USB-2",
    0x268a8086,"Intel 631XESB/632XESB/3100 USB controller USB-3",
    0x268b8086,"Intel 631XESB/632XESB/3100 USB controller USB-4",
    0x70208086,"Intel 82371SB (PIIX3) USB controller",
    0x71128086,"Intel 82371AB/EB (PIIX4) USB controller",
    0x24128086,"Intel 82801AA (ICH) USB controller",
    0x24228086,"Intel 82801AB (ICH0) USB controller",
    0x24428086,"Intel 82801BA/BAM (ICH2) USB controller USB-A",
    0x24448086,"Intel 82801BA/BAM (ICH2) USB controller USB-B",
    0x24828086,"Intel 82801CA/CAM (ICH3) USB controller USB-A",
    0x24848086,"Intel 82801CA/CAM (ICH3) USB controller USB-B",
    0x24878086,"Intel 82801CA/CAM (ICH3) USB controller USB-C",
    0x24c28086,"Intel 82801DB (ICH4) USB controller USB-A",
    0x24c48086,"Intel 82801DB (ICH4) USB controller USB-B",
    0x24c78086,"Intel 82801DB (ICH4) USB controller USB-C",
    0x24d28086,"Intel 82801EB (ICH5) USB controller USB-A",
    0x24d48086,"Intel 82801EB (ICH5) USB controller USB-B",
    0x24d78086,"Intel 82801EB (ICH5) USB controller USB-C",
    0x24de8086,"Intel 82801EB (ICH5) USB controller USB-D",
    0x26588086,"Intel 82801FB/FR/FW/FRW (ICH6) USB controller USB-A",
    0x26598086,"Intel 82801FB/FR/FW/FRW (ICH6) USB controller USB-B",
    0x265a8086,"Intel 82801FB/FR/FW/FRW (ICH6) USB controller USB-C",
    0x265b8086,"Intel 82801FB/FR/FW/FRW (ICH6) USB controller USB-D",
    0x27c88086,"Intel 82801G (ICH7) USB controller USB-A",
    0x27c98086,"Intel 82801G (ICH7) USB controller USB-B",
    0x27ca8086,"Intel 82801G (ICH7) USB controller USB-C",
    0x27cb8086,"Intel 82801G (ICH7) USB controller USB-D",
    0x28308086,"Intel 82801H (ICH8) USB controller USB-A",
    0x28318086,"Intel 82801H (ICH8) USB controller USB-B",
    0x28328086,"Intel 82801H (ICH8) USB controller USB-C",
    0x28348086,"Intel 82801H (ICH8) USB controller USB-D",
    0x28358086,"Intel 82801H (ICH8) USB controller USB-E",
    0x29348086,"Intel 82801I (ICH9) USB controller",
    0x29358086,"Intel 82801I (ICH9) USB controller",
    0x29368086,"Intel 82801I (ICH9) USB controller",
    0x29378086,"Intel 82801I (ICH9) USB controller",
    0x29388086,"Intel 82801I (ICH9) USB controller",
    0x29398086,"Intel 82801I (ICH9) USB controller",
    0x3a348086,"Intel 82801JI (ICH10) USB controller USB-A",
    0x3a358086,"Intel 82801JI (ICH10) USB controller USB-B",
    0x3a368086,"Intel 82801JI (ICH10) USB controller USB-C",
    0x3a378086,"Intel 82801JI (ICH10) USB controller USB-D",
    0x3a388086,"Intel 82801JI (ICH10) USB controller USB-E",
    0x3a398086,"Intel 82801JI (ICH10) USB controller USB-F",
    0x719a8086,"Intel 82443MX USB controller",
    0x76028086,"Intel 82372FB/82468GX USB controller",
    0x30381106,"VIA VT6212 UHCI USB 1.0 controller",
    0 ,NULL
};


#define UHCI_CF_VENDOR_ID                           0x00
#define UHCI_CF_DEVICE_ID                           0x02
#define UHCI_CF_COMMAND                             0x04
#define UHCI_CF_STATUS                              0x06
#define UHCI_CF_REVISION_ID                         0x08
#define UHCI_CF_CLASS_CODE                          0x09
#define UHCI_CF_CACHE_LINE_SIZE                     0x0c
#define UHCI_CF_LATENCY_TIME                        0x0d
#define UHCI_CF_HEADER_TYPE                         0x0e
#define UHCI_CF_BIST                                0x0f
#define UHCI_CF_MMAP_IO_BASE_ADDR                   0x10
#define UHCI_CF_CIS_BASE_ADDR                       0x14
#define UHCI_CF_CARDBUS_CIS_PTR                     0x28
#define UHCI_CF_SSID                                0x2c
#define UHCI_CF_PWR_MGMT_CAPS                       0x34
#define UHCI_CF_INTERRUPT_LINE                      0x3c
#define UHCI_CF_INTERRUPT_PIN                       0x3d
#define UHCI_NUM_CONF_REGISTERS                     0xc2
#define UHCI_NUM_IO_REGISTERS                       0x14


/*
*******************************************************************************************************
* UHCI instance                                                                                       *
*******************************************************************************************************
*/
typedef struct {
	module_header_t        module_header;
    vertex_hdl_t           ps_conn;        /* connection for pci services  */
    vertex_hdl_t           ps_vhdl;        /* backpointer to device vertex */
    vertex_hdl_t           ps_charv;       /* backpointer to device vertex */
    USB_func_t             ps_event_func;  /* function to USBcore event    */ 
    uchar_t                *ps_cfg;        /* cached ptr to my config regs */
    uchar_t                *ps_regs;        /* cached ptr to my regs */
    uchar_t                *pci_io_caps;
    pciio_piomap_t         ps_cmap;        /* piomap (if any) for ps_cfg */
    pciio_piomap_t         ps_rmap;        /* piomap (if any) for ps_regs */
    unsigned               ps_sst;         /* driver "software state" */
#define usbuhci_SST_RX_READY      (0x0001)
#define usbuhci_SST_TX_READY      (0x0002)
#define usbuhci_SST_ERROR         (0x0004)
#define usbuhci_SST_INUSE         (0x8000)
    pciio_intr_t            ps_intr;       /* pciio intr for INTA and INTB */
    pciio_dmamap_t          ps_ctl_dmamap; /* control channel dma mapping */
    pciio_dmamap_t          ps_str_dmamap; /* stream channel dma mapping */
    struct pollhead         *ps_pollhead;  /* for poll() */
    int                     ps_blocks;  /* block dev size in NBPSCTR blocks */
}usbuhci_instance_t;

/*
*******************************************************************************************************
* Global variables                                                                                    *
*******************************************************************************************************
*/
int                                                 usbuhci_devflag = D_MP;
char                                                *usbuhci_mversion = M_VERSION;  /* for loadable modules */
int                                                 usbuhci_inuse = 0;     /* number of "usbuhci" devices open */
gc_list_t                                           gc_list;
usbuhci_instance_t                                  *global_soft = NULL; 
 
 
/*
*******************************************************************************************************
* Callback functions for usbcore                                                                      *
*******************************************************************************************************
*/
int uhci_reset( void *hcd);
int uhci_start( void *hcd);
int uhci_init( void *hcd);
int uhci_stop( void *hcd);
int uhci_shutdown( void *hcd);
int uhci_suspend( void *hcd);
int uhci_resume( void *hcd);
int uhci_status( void *hcd);
void uhci_free_pipe( void *hcd, struct usb_pipe *pipe);
struct usb_pipe *uhci_alloc_default_control_pipe( void *hcd, struct usb_pipe *pipe);
struct usb_pipe *uhci_alloc_bulk_pipe( void *hcd, struct usb_pipe *pipe, struct usb_endpoint_descriptor *descriptor);
int uhci_send_control( void *hcd, struct usb_pipe *pipe, int dir, void *cmd, int cmdsize, void *data, int datasize);
int uhci_usb_send_bulk( void *hcd, struct usb_pipe *pipe, int dir, void *data, int datasize);
int uhci_alloc_intr_pipe( void *hcd, struct usb_pipe *pipe, struct usb_endpoint_descriptor *descriptor);
int uhci_usb_poll_intr( void *hcd, struct usb_pipe *pipe, void *data);
int uhci_ioctl( void *hcd, int cmd, void *uarg);
int uhci_set_trace_level( void *hcd, void *trace_level);

/*
*******************************************************************************************************
* Methods for usbcore                                                                                 *
*******************************************************************************************************
*/
hcd_methods_t uhci_methods={
	{ 2, "UHCI", "Enhanced Host Controller Interface (USB 2.0)", "usbuhci.o", 
	USB_DRIVER_IS_HCD, "", 0, 0, 0, 0, 0, NULL},	
	uhci_reset,
	uhci_start,
	uhci_init, 
	uhci_stop,
	uhci_shutdown,
	uhci_suspend,
	uhci_resume,
	uhci_status,
	uhci_free_pipe,
	uhci_alloc_default_control_pipe, 
	uhci_alloc_bulk_pipe, 
	uhci_send_control, 
	uhci_usb_send_bulk,
	uhci_alloc_intr_pipe,
	uhci_usb_poll_intr,
	uhci_ioctl,
	uhci_set_trace_level,
	0x00000000
};


/*
*******************************************************************************************************
* Entry points                                                                                        *
*******************************************************************************************************
*/
void                  usbuhci_init(void);
int                   usbuhci_unload(void);
int                   usbuhci_reg(void);
int                   usbuhci_unreg(void);
int                   usbuhci_attach(vertex_hdl_t conn);
int                   usbuhci_detach(vertex_hdl_t conn);
static pciio_iter_f   usbuhci_reloadme;
static pciio_iter_f   usbuhci_unloadme;
int                   usbuhci_open(dev_t *devp, int oflag, int otyp, cred_t *crp);
int                   usbuhci_close(dev_t dev, int oflag, int otyp, cred_t *crp);
int                   usbuhci_ioctl(dev_t dev, int cmd, void *arg, int mode, cred_t *crp, int *rvalp);
int                   usbuhci_read(dev_t dev, uio_t * uiop, cred_t *crp);
int                   usbuhci_write(dev_t dev, uio_t * uiop,cred_t *crp);
int                   usbuhci_strategy(struct buf *bp);
int                   usbuhci_poll(dev_t dev, short events, int anyyet, 
                               short *reventsp, struct pollhead **phpp, unsigned int *genp);
int                   usbuhci_map(dev_t dev, vhandl_t *vt, off_t off, size_t len, uint_t prot);
int                   usbuhci_unmap(dev_t dev, vhandl_t *vt);
void                  usbuhci_dma_intr(intr_arg_t arg);
static error_handler_f usbuhci_error_handler;
void                  usbuhci_halt(void);
int                   usbuhci_size(dev_t dev);
int                   usbuhci_print(dev_t dev, char *str);


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
int uhci_reset( void *hcd){
}

int uhci_start( void *hcd){
}

int uhci_init( void *hcd){
}

int uhci_stop( void *hcd){
}

int uhci_shutdown( void *hcd){
}

int uhci_suspend( void *hcd){
}

int uhci_resume( void *hcd){
}

int uhci_status( void *hcd){ /* just a demo */
    usbuhci_instance_t *soft = ( usbuhci_instance_t *) hcd;
    uint64_t class = TRC_USB_HCD | TRC_HELPER | TRC_HCD_SET( USB_UHCI);
    
    
    TRACE( class, 10, "start", "");
    TRACE( class, 12, "hcd driver = '%s'", 
        soft->module_header.short_description);
    TRACE( class, 10, "end", "");
    return( 1234);
}

void uhci_free_pipe( void *hcd, struct usb_pipe *pipe){
}

struct usb_pipe *uhci_alloc_default_control_pipe( void *hcd, struct usb_pipe *pipe){
}

struct usb_pipe *uhci_alloc_bulk_pipe( void *hcd, struct usb_pipe *pipe, struct usb_endpoint_descriptor *descriptor){
}

int uhci_send_control( void *hcd, struct usb_pipe *pipe, int dir, void *cmd, int cmdsize, void *data, int datasize){
}

int uhci_usb_send_bulk( void *hcd, struct usb_pipe *pipe, int dir, void *data, int datasize){
}

int uhci_alloc_intr_pipe( void *hcd, struct usb_pipe *pipe, struct usb_endpoint_descriptor *descriptor){
}

int uhci_usb_poll_intr( void *hcd, struct usb_pipe *pipe, void *data){
}

int uhci_ioctl( void *hcd, int cmd, void *uarg){
    	
}



int uhci_set_trace_level( void *hcd, void *arg){
	USB_trace_class_t *trace_arg = (USB_trace_class_t *) arg;
    uint64_t class = TRC_USB_HCD | TRC_HELPER | TRC_HCD_SET( USB_UHCI);
    	
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
int register_uhci_with_usbcore(void *hcd){
    USB_func_t                  func;
    vertex_hdl_t                vhdl; 
	usbcore_instance_t          *usbcore;
	usbuhci_instance_t          *soft;
	int rc; 
    uint64_t class = TRC_USB_HCD | TRC_HELPER | TRC_HCD_SET( USB_UHCI);
	char str[45];
	
	soft = (usbuhci_instance_t *) hcd;
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
		
	
	rc = usbcore->register_hcd_driver( (void *) &uhci_methods);
	TRACE( class, 6,"rc = %d", rc);
    soft->ps_event_func = usbcore->event_func;
	
	TRACE( class, 2, "end", "");
	
	return( rc);
}


int unregister_uhci_with_usbcore(void *hcd){
    USB_func_t                  func;
    vertex_hdl_t                vhdl; 
	usbcore_instance_t          *usbcore;
	usbuhci_instance_t          *soft;
	int rc; 
    uint64_t class = TRC_USB_HCD | TRC_HELPER | TRC_HCD_SET( USB_UHCI);
	char str[45];
	
	soft = (usbuhci_instance_t *) hcd;
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
    rc = func( (void *) &uhci_methods);
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
* usbuhci_init: called once during system startup or                                                  *
* when a loadable driver is loaded.                                                                   *
*******************************************************************************************************
*/
void usbuhci_init(void){
    uint64_t class = TRC_USB_HCD | TRC_INIT | TRC_HCD_SET( USB_UHCI);

	
  	TRACE( class, 2, "start", "");
  	TRACE( class, 0, "**********************************************************", "");
  	TRACE( class, 0, "* UHCI USB Driver for Silicon Graphics Irix 6.5          *", "");
  	TRACE( class, 0, "* By bsdero at gmail dot org, 2011                       *", "");
  	TRACE( class, 0, "* Version %s                                        *", USBCORE_DRV_VERSION);
  	TRACE( class, 0, "* Sequence %s                                *", USBCORE_DRV_SEQ);
  	TRACE( class, 0, "**********************************************************", "");
    TRACE( class, 0, "UHCI Driver loaded!", "");

    /*
     * if we are already registered, note that this is a
     * "reload" and reconnect all the places we attached.
     */

    pciio_iterate("usbuhci_", usbuhci_reloadme);	
    gc_list_init( &gc_list);
	TRACE( class, 2, "end", "");
}


/*
*******************************************************************************************************
*    usbuhci_unload: if no "usbuhci" is open, put us to bed                                           *
*      and let the driver text get unloaded.                                                          *
*******************************************************************************************************
*/
int usbuhci_unload(void){
    uint64_t class = TRC_USB_HCD | TRC_UNLOAD | TRC_HCD_SET( USB_UHCI);
	TRACE( class, 2, "start", "");
    if (usbuhci_inuse)
        return EBUSY;

    pciio_iterate("usbuhci_", usbuhci_unloadme);
    gc_list_destroy( &gc_list);      
    TRACE( class, 0, "UHCI Driver unloaded", "");
	TRACE( class, 2, "end", "");
    
    return 0;
}



/*
*******************************************************************************************************
*    usbuhci_reg: called once during system startup or                                                *
*      when a loadable driver is loaded.                                                              *
*    NOTE: a bus provider register routine should always be                                           *
*      called from _reg, rather than from _init. In the case                                          *
*      of a loadable module, the devsw is not hooked up                                               *
*      when the _init routines are called.                                                            *
*******************************************************************************************************
 */
int usbuhci_reg(void){
	int i;
	uint16_t device_id, vendor_id;
    uint64_t class = TRC_USB_HCD | TRC_INIT | TRC_HCD_SET( USB_UHCI);
	
	TRACE( class, 2, "start", "");
	
    /* registering UHCI devices */
    for( i = 0; uhci_descriptions[i].device_id != 0; i++){
        device_id = ((uhci_descriptions[i].device_id & 0xffff0000) >> 16);
        vendor_id = (uhci_descriptions[i].device_id & 0x0000ffff);
        pciio_driver_register(vendor_id, device_id, "usbuhci_", 0);  /* usbuhci_attach and usbuhci_detach entry points */
    }

	TRACE( class, 2, "end", "");
    return 0;
}



/*
*******************************************************************************************************
* usbuhci_unreg: called when a loadable driver is unloaded.                                           *
*******************************************************************************************************
*/
int usbuhci_unreg(void){
	TRACE( TRC_USB_HCD| TRC_UNLOAD|TRC_HCD_SET(USB_UHCI), 2, "start", "");
    pciio_driver_unregister("usbuhci_");
	TRACE( TRC_USB_HCD| TRC_UNLOAD|TRC_HCD_SET(USB_UHCI), 2, "end", "");
    return 0;
}


/*
*******************************************************************************************************
*    usbuhci_attach: called by the pciio infrastructure                                               *
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
int usbuhci_attach(vertex_hdl_t conn){
    vertex_hdl_t            vhdl = GRAPH_VERTEX_NONE;    
    vertex_hdl_t            charv = GRAPH_VERTEX_NONE;       
    uchar_t                 *cfg;
    usbuhci_instance_t      *soft;
    uchar_t                 *regs;    
    pciio_piomap_t          cmap = 0;
    pciio_piomap_t          rmap = 0;
    graph_error_t           ret = (graph_error_t) 0;
    uint16_t                vendor_id;
    uint16_t                device_id;
    uint32_t                ssid;
    uchar_t                 rev_id;
    uchar_t                 val;    
    int                     i;
    uint32_t                device_vendor_id;    
    int                     rc;
    uint64_t                class = TRC_USB_HCD| TRC_ATTACH|TRC_HCD_SET(USB_UHCI);
    
	TRACE( class, 2, "start", "");
	TRACE( class, 0, "UHCI Host Controller Device Detected!", "");
    
    if (GRAPH_SUCCESS != hwgraph_traverse(GRAPH_VERTEX_NONE, "/usb", &vhdl)){
		TRACERR( class, "error in hwgraph_traverse(), quitting..", ""); 
        return( -1);
    }
    

    ret = hwgraph_edge_get( vhdl, "usbuhci", &charv);
    TRACE( class, 8, "rc of hwgraph_edge_get() = %d, added usbuhci edge", ret);

    ret = hwgraph_char_device_add( vhdl, "usbuhci", "usbuhci_", &charv); 
    TRACE( class, 8, 
        "rc of hwgraph_char_device_add() = %d, added usbuhci edge (char device)", ret);    


    /*
     * Allocate a place to put per-device information for this vertex.
     * Then associate it with the vertex in the most efficient manner.
     */
    soft = (usbuhci_instance_t *) gc_malloc( &gc_list, sizeof( usbuhci_instance_t));
    
    ASSERT(soft != NULL);
    device_info_set(vhdl, (void *)soft);
    soft->ps_conn = conn;
    soft->ps_vhdl = vhdl;
    soft->ps_charv = charv;
    
    
    cfg = (uchar_t *) pciio_pio_addr
        (conn, 0,                     /* device and (override) dev_info */
         PCIIO_SPACE_CFG,             /* select configuration addr space */
         0,                           /* from the start of space, */
         UHCI_NUM_CONF_REGISTERS,     /* ... up to vendor specific stuff */
         &cmap,                       /* in case we needed a piomap */
         0);                          /* flag word */
    soft->ps_cfg = cfg;               /* save for later */
    soft->ps_cmap = cmap;
    vendor_id = (uint16_t) PCI_CFG_GET16(conn, UHCI_CF_VENDOR_ID);
    device_id = (uint16_t) PCI_CFG_GET16(conn, UHCI_CF_DEVICE_ID);
    ssid = (uint32_t) PCI_CFG_GET32( conn, UHCI_CF_SSID);
    rev_id = (uchar_t) PCI_CFG_GET8( conn, UHCI_CF_REVISION_ID);
    
    TRACE( class, 0, "UHCI supported device found", "");
    TRACE( class, 0, "Vendor ID: 0x%x, Device ID: 0x%x", vendor_id, device_id);
    TRACE( class, 0, "SSID: 0x%x, Rev ID: 0x%x",ssid, rev_id);
    
    
    device_vendor_id = device_id << 16 | vendor_id;
    for( i = 0; uhci_descriptions[i].device_id != 0; i++){
        if( uhci_descriptions[i].device_id == device_vendor_id){
            uhci_methods.module_header.vendor_id = vendor_id;
            uhci_methods.module_header.device_id = device_id;
            strcpy( uhci_methods.module_header.hardware_description, 
                  (char *)uhci_descriptions[i].controller_description);
            TRACE( class, 0, "Device Description: %s", 
                uhci_descriptions[i].controller_description);
            break;
        }
    }
    
    /*
     * Get a pointer to our DEVICE registers
     */
     
    TRACE( class, 8, "Trying PCIIO_SPACE_WIN", regs);
    regs = (uchar_t *) pciio_pio_addr
        (conn, 0,                       /* device and (override) dev_info */
         PCIIO_SPACE_WIN(1),            /* in my primary decode window, */
         0, UHCI_NUM_IO_REGISTERS,      /* base and size */
         &rmap,                         /* in case we needed a piomap */
         0);                            /* flag word */

    TRACE( class, 8, "regs = 0x%x", regs);
    if( regs == NULL){
		TRACE( class, 8, "failed", ""); 
    }else
        goto solved;

    TRACE( class, 8, "Trying PCIIO_PIOMAP_WIN", regs);
    regs = (uchar_t *) pciio_pio_addr
        (conn, 0,                       /* device and (override) dev_info */
         PCIIO_PIOMAP_WIN(1),            /* in my primary decode window, */
         0, UHCI_NUM_IO_REGISTERS,      /* base and size */
         &rmap,                         /* in case we needed a piomap */
         0);                            /* flag word */

    TRACE( class, 8, "regs = 0x%x", regs);
    if( regs == NULL){
		TRACE( class, 8, "failed", ""); 
    }else
        goto solved;



    TRACE( class, 8, "Trying PCIIO_SPACE_MEM", regs);
    regs = (uchar_t *) pciio_pio_addr
        (conn, 0,                       /* device and (override) dev_info */
         PCIIO_SPACE_MEM,            /* in my primary decode window, */
         0, UHCI_NUM_IO_REGISTERS,      /* base and size */
         &rmap,                         /* in case we needed a piomap */
         0);                            /* flag word */

    TRACE( class, 8, "regs = 0x%x", regs);
    if( regs == NULL){
		TRACE( class, 8, "failed", ""); 
    }else
        goto solved;

solved:
    soft->pci_io_caps = regs;
    soft->ps_regs = regs;               /* save for later */
    soft->ps_rmap = rmap;

    for( i = 0; i < 12; i++){
        val = EREAD1( soft, i);
        TRACE( class, 8, " pci regs %d = 0x%x", i, val);
    }
    
	uhci_methods.module_header.soft = (void *) soft;
    bcopy( (void *) &uhci_methods.module_header, (void *) &soft->module_header,
        sizeof( module_header_t));


    /* if all is ok, register host controller driver here */
    rc = register_uhci_with_usbcore((void *) soft);
    TRACE( class, 4, "rc = %d", rc);

	TRACE( class, 2, "end", "");

    return 0;                      /* attach successsful */
}




/*
*******************************************************************************************************
*    usbuhci_detach: called by the pciio infrastructure                                               *
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
int usbuhci_detach(vertex_hdl_t conn){
    vertex_hdl_t                  vhdl, blockv, charv;
    usbuhci_instance_t            *soft;
    uint64_t                      class = TRC_USB_HCD| TRC_DETACH|TRC_HCD_SET(USB_UHCI);
    int rc;
    
	TRACE( class, 2, "start", "");
    if (GRAPH_SUCCESS != hwgraph_traverse(GRAPH_VERTEX_NONE, "/usb", &vhdl))
        return( -1);
        
    soft = (usbuhci_instance_t *) device_info_get(vhdl);
    rc = unregister_uhci_with_usbcore((void *) soft);
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
    hwgraph_edge_remove(vhdl, "usbuhci", &charv);
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
*    usbuhci_reloadme: utility function used indirectly                                               *
*      by usbuhci_init, via pciio_iterate, to "reconnect"                                             *
*      each connection point when the driver has been                                                 *
*      reloaded.                                                                                      *
*******************************************************************************************************
*/
static void usbuhci_reloadme(vertex_hdl_t conn){
    vertex_hdl_t            vhdl;
    usbuhci_instance_t      *soft;
    uint64_t                class = TRC_USB_HCD | TRC_INIT | TRC_HCD_SET(USB_UHCI);

	TRACE( class, 2, "start", "");
    if (GRAPH_SUCCESS != hwgraph_traverse(conn, "usbuhci", &vhdl))
        return;
        
    soft = (usbuhci_instance_t *) device_info_get(vhdl);
    /*
     * Reconnect our error and interrupt handlers
     */
    pciio_error_register(conn, usbuhci_error_handler, soft);
    pciio_intr_connect(soft->ps_intr, usbuhci_dma_intr, soft, 0);
	TRACE( class, 2, "end", "");
}





/*
*******************************************************************************************************
 *    usbuhci_unloadme: utility function used indirectly by
 *      usbuhci_unload, via pciio_iterate, to "disconnect" each
 *      connection point before the driver becomes unloaded.
 */
static void usbuhci_unloadme(vertex_hdl_t pconn){
    vertex_hdl_t            vhdl;
    usbuhci_instance_t      *soft;
    uint64_t                class = TRC_USB_HCD | TRC_UNLOAD | TRC_HCD_SET(USB_UHCI);
    
	TRACE( class, 2, "start", "");
    if (GRAPH_SUCCESS != hwgraph_traverse(pconn, "usbuhci", &vhdl))
        return;
        
    soft = (usbuhci_instance_t *) device_info_get(vhdl);
    /*
     * Disconnect our error and interrupt handlers
     */
    pciio_error_register(pconn, 0, 0);
    pciio_intr_disconnect(soft->ps_intr);
	TRACE( class, 2, "end", "");
}



/*
*******************************************************************************************************
*    pciuhci_open: called when a device special file is                                               * 
*      opened or when a block device is mounted.                                                      *
*******************************************************************************************************
*/ 
int usbuhci_open(dev_t *devp, int oflag, int otyp, cred_t *crp){
    /* user should not open this module, it should be used through usbcore module */        
    return( EINVAL);
}




/*
*******************************************************************************************************
*    usbuhci_close: called when a device special file
*      is closed by a process and no other processes
*      still have it open ("last close").
*******************************************************************************************************
*/ 
int usbuhci_close(dev_t dev, int oflag, int otyp, cred_t *crp){
    /* user should not open this module, it should be used through usbcore module */        
    return 0;
}


/*
*******************************************************************************************************
*    usbuhci_ioctl: a user has made an ioctl request                                                  *
*      for an open character device.                                                                  *
*      Arguments cmd and arg are as specified by the user;                                            *
*      arg is probably a pointer to something in the user's                                           *
*      address space, so you need to use copyin() to                                                  *
*      read through it and copyout() to write through it.                                             *
*******************************************************************************************************
*/ 
int usbuhci_ioctl(dev_t dev, int cmd, void *arg, int mode, cred_t *crp, int *rvalp){
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
int usbuhci_read(dev_t dev, uio_t * uiop, cred_t *crp){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
}

int usbuhci_write(dev_t dev, uio_t * uiop, cred_t *crp){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
}

int usbuhci_strategy(struct buf *bp){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
    return 0;
}

/*
*******************************************************************************************************
* Poll Entry points                                                                                   *
*******************************************************************************************************
*/
int usbuhci_poll(dev_t dev, short events, int anyyet,
           short *reventsp, struct pollhead **phpp, unsigned int *genp){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
}


/*
*******************************************************************************************************
* Memory map entry points                                                                             *
*******************************************************************************************************
*/
int usbuhci_map(dev_t dev, vhandl_t *vt, off_t off, size_t len, uint_t prot){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
}

int usbuhci_unmap(dev_t dev, vhandl_t *vt){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
}


/*
*******************************************************************************************************
* Interrupt entry point                                                                               *
* We avoid using the standard name, since our prototype has changed.                                  *
*******************************************************************************************************
*/
void usbuhci_dma_intr(intr_arg_t arg){

    usbuhci_instance_t        *soft = (usbuhci_instance_t *) arg;
    
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
static int usbuhci_error_handler(void *einfo,
                    int error_code,
                    ioerror_mode_t mode,
                    ioerror_t *ioerror){

/* not used right now */


/*    usbuhci_instance_t            *soft = (usbuhci_instance_t *) einfo;
    vertex_hdl_t                  vhdl = soft->ps_vhdl;
#if DEBUG && ERROR_DEBUG
    cmn_err(CE_CONT, "%v: usbuhci_error_handler\n", vhdl);
#else
    vhdl = vhdl;
#endif
    ioerror_dump("usbuhci", error_code, mode, ioerror);
*/



    return IOERROR_HANDLED;
}



/*
*******************************************************************************************************
* Support entry points                                                                                *
*******************************************************************************************************
*    usbuhci_halt: called during orderly system                                                       *
*      shutdown; no other device driver call will be                                                  *
*      made after this one.                                                                           *
*******************************************************************************************************
*/
void usbuhci_halt(void){
   	TRACE( TRC_USB_HCD| TRC_HELPER|TRC_HCD_SET(USB_UHCI), 2, "start", "");
	TRACE( TRC_USB_HCD| TRC_HELPER|TRC_HCD_SET(USB_UHCI), 2, "end", "");

}



/*
*******************************************************************************************************
*    usbuhci_size: return the size of the device in                                                   *
*      "sector" units (multiples of NBPSCTR).                                                         *
*******************************************************************************************************
*/
int usbuhci_size(dev_t dev){
    /* user should not open this module, it should be used through usbcore module */        
    return(0);
}


/*
*******************************************************************************************************
*    usbuhci_print: used by the kernel to report an                                                   *
*      error detected on a block device.                                                              *
*******************************************************************************************************
*/
int usbuhci_print(dev_t dev, char *str){
	/* not used */
    return 0;
}


