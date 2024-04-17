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
	{ USB_XXXHCI, "XXXHCI", "Enhanced Host Controller Interface (USB 2.0)", "usbxxxhci.o", 
	USB_DRIVER_IS_HCD, 0, 0, 0, 0, NULL},	
	xxxhci_reset,
	xxxhci_start, 
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
