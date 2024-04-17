/*
********************************************************************
* PCI EHCI Device Driver for Irix 6.5                              *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: pciehci.c                                                  *
* Description: Entry points for pciehci driver                     *
*                                                                  *
*                                                                  *
********************************************************************
*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      01-29-2012     -EHCI functions and definitions left out in ehci.c                       *
*                                                                                                     *
* BSDero      12-15-2011     -DMA and byte dump utilities reexaminated and tested                     *
*                                                                                                     *
* BSDero      12-04-2011     -Added pfxpoll entry point. Fixes and basic events support               *
*                                                                                                     *
* BSDero      11-20-2011     -Crashes fixed in SGI Fuel                                               *
*                                                                                                     *
* BSDero      11-18-2011     -Added non O2 support                                                    *
*                                                                                                     *
* BSDero      11-01-2011     -Added pfxioctl entry point                                              *
*                                                                                                     *
* BSDero      10-31-2011     -Added trace macro                                                       *
*                                                                                                     *
* BSDero      10-15-2011     -Fixed bug in ehci_interrupt. Now ports connect/disconnect are correctly *
*                             detected                                                                *
*                                                                                                     * 
* BSDero      10-12-2011     -Added support for pfxopen/pfxclose entry points                         *
*                                                                                                     *
* BSDero      10-11-2011     -Added connection/disconnection detect of USB devices                    *
*                            -Added USB device speed detect                                           *
*                            -Identify which ports are changed                                        *
*                                                                                                     *
* BSDero      10-10-2011     -Fixed crash on pciehci_detach()                                         *
*                            -EREAD/EWRITE and friend macros bug fixed.                               *
*                            -PCI Interrupts are working now                                          *
*                            -EHCI devices support added                                              *
*                            -Fixed crash on pciehci_dma_intr                                         *
*                                                                                                     *
* BSDero      10-01-2011     -Added initial support for another USB controller. Taken from NetBSD     * 
*                                                                                                     *
* BSDero      09-24-2011     -Code ripped from FreeBSD for EHCI, PCI EHCI interrupts not working yet  *
*                            -Macros for NetBSD source code compatibility                             *
*                            -Added basic data types                                                  *
*                                                                                                     *
* BSDero      09-17-2011     -Added support for basic ehci PCI map                                    *
*                            -Basic configurations registers read                                     *
*                                                                                                     *
* BSDero      09-10-2011     -Initial version -taken from SGI Driver Developers Guide                 *
*******************************************************************************************************

WARNING!!!

EXPERIMENTAL DRIVER FOR EHCI, NOT USEFUL YET, USE IT UNDER UR OWN RISK!!!
NOR STABLE NOR ENOUGH TESTED!!
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
#include <sys/atomic_ops.h>
#include <sys/PCI/PCI_defs.h>
#include <sys/PCI/pciio.h>

#include <sys/mload.h>
#include <string.h>
#include <ctype.h>


/* included sources */
#include "config.h"
#include "hcicommon.h" /* common for host controllers */
#include "pciehci.h"   /* user space ioctls and defines, used also in driver code */
#include "trace.c"
#include "dumphex.c"   /* dump utility */
#include "list.c"      /* list utility */
#include "gc.c"        /* memory lists utility */
#include "dma.c"       /* dma lists utility */
#include "ehci.c"

typedef int(*func_t)(void *);
typedef struct{
    vertex_hdl_t            conn;
    vertex_hdl_t            masterv;
	vertex_hdl_t            usb_dev1Mode;
	vertex_hdl_t            usb_dev0Mode;
	int                     mode;
	func_t                  func; 
}usbcore_soft_t;
#define	usbcore_soft_get(v)	(usbcore_soft_t *)hwgraph_fastinfo_get(v)

static struct{
    int       cmd;
    char      *str;
}ehci_ioctls[]={
    IOCTL_EHCI_DRIVER_INFO,            "IOCTL_EHCI_DRIVER_INFO",
	IOCTL_EHCI_CONTROLLER_INFO,        "IOCTL_EHCI_CONTROLLER_INFO",
    IOCTL_EHCI_PORTS_STATUS,           "IOCTL_EHCI_PORTS_STATUS",
    IOCTL_EHCI_SET_DEBUG_LEVEL,        "IOCTL_EHCI_SET_DEBUG_LEVEL",
    IOCTL_EHCI_GET_DEBUG_LEVEL,        "IOCTL_EHCI_GET_DEBUG_LEVEL",
    IOCTL_EHCI_SETUP_TRANSFER,         "IOCTL_EHCI_SETUP_TRANSFER",
    IOCTL_EHCI_ENUMERATE,              "IOCTL_EHCI_ENUMERATE",
    IOCTL_EHCI_DUMP_DATA,              "IOCTL_EHCI_DUMP_DATA",
    IOCTL_EHCI_CHECKINFO,              "IOCTL_EHCI_CHECKINFO",
    IOCTL_EHCI_HC_RESET,               "IOCTL_EHCI_HC_RESET",
    IOCTL_EHCI_PORT_SUSPEND,           "IOCTL_EHCI_PORT_SUSPEND",
    IOCTL_EHCI_PORT_RESET,             "IOCTL_EHCI_PORT_RESET",
    IOCTL_EHCI_USBREGS_GET,            "IOCTL_EHCI_USBREGS_GET",
    IOCTL_EHCI_USBREGS_SET,            "IOCTL_EHCI_USBREGS_SET",
    IOCTL_EHCI_HC_SUSPEND,             "IOCTL_EHCI_HC_SUSPEND",
    IOCTL_EHCI_HC_HALT,                "IOCTL_EHCI_HC_HALT",
    IOCTL_EHCI_PORT_ENABLE,            "IOCTL_EHCI_PORT_ENABLE",
    IOCTL_EHCI_PORT_DISABLE,           "IOCTL_EHCI_PORT_DISABLE",
    IOCTL_DUMMY_ENABLE,                "IOCTL_DUMMY_ENABLE",
    IOCTL_DUMMY_DISABLE,               "IOCTL_DUMMY_DISABLE",
    IOCTL_EHCI_USB_COMMAND,            "IOCTL_EHCI_USB_COMMAND",
	-1,                                "UNKNOWN IOCTL",
    0, NULL
};


/***************************************************************************
 *   Specific driver flags building configuration                          *
 ***************************************************************************/
int         pciehci_devflag = D_MP;
char        *pciehci_mversion = M_VERSION;  /* for loadable modules */



/***************************************************************************
 *   Function table of contents                                            *
 ***************************************************************************/
void                    pciehci_init(void);
int                     pciehci_unload(void);
int                     pciehci_reg(void);
int                     pciehci_unreg(void);
int                     pciehci_attach(vertex_hdl_t conn);
int                     pciehci_detach(vertex_hdl_t conn);
static pciio_iter_f     pciehci_reloadme;
static pciio_iter_f     pciehci_unloadme;
int                     pciehci_open(dev_t *devp, int oflag, int otyp, cred_t *crp);
int                     pciehci_close(dev_t dev, int oflag, int otyp, cred_t *crp);
int                     pciehci_ioctl(dev_t dev, int cmd, void *arg, int mode, cred_t *crp, int *rvalp);
int                     pciehci_read(dev_t dev, uio_t * uiop, cred_t *crp);
int                     pciehci_write(dev_t dev, uio_t * uiop,cred_t *crp);
int                     pciehci_strategy(struct buf *bp);
int                     pciehci_poll(dev_t dev, short events, int anyyet, short *reventsp, struct pollhead **phpp, unsigned int *genp);
int                     pciehci_map(dev_t dev, vhandl_t *vt, off_t off, size_t len, uint_t prot);
int                     pciehci_unmap(dev_t dev, vhandl_t *vt);
void                    pciehci_dma_intr(intr_arg_t arg);
static error_handler_f  pciehci_error_handler;
void                    pciehci_halt(void);
int                     pciehci_size(dev_t dev);
int                     pciehci_print(dev_t dev, char *str);

pciehci_soft_t          global_soft;
/*uint32_t                global_debug_level = PCIEHCI_DEBUG_ALL;*/
uint32_t                global_ports_status[EHCI_MAX_NUM_PORTS]; 
uchar_t                 global_num_ports;
uint32_t                global_debug_level = 0;
int                     global_pciehci_inuse = 0;


/***************************************************************************
 *   Driver specific entry-point functions                                 *
 ***************************************************************************/

int usbcore_register_driver(){
    func_t                  func;
    vertex_hdl_t            vhdl; 
	usbcore_soft_t          *usbcore; 
	int rc; 
	char str[45];
	printf("usbcore_register_hci()\n");
    if (GRAPH_SUCCESS != hwgraph_traverse(GRAPH_VERTEX_NONE, "/usbcore/usb_dev0", &vhdl)){
		printf("Error in hwgraph_traverse()\n");
        return -1;
	}
	USECDELAY( 1000000);	
	usbcore = usbcore_soft_get( vhdl);
	if( usbcore == NULL){
	    printf("could not get information from usbcore_soft_get()\n");
		return( 1);
	}
	USECDELAY( 1000000);
		
	func = usbcore->func; 
	strcpy( str, "PCIEHCI");
	rc = func( (void *) str);
	printf("rc = %d\n", rc);
	
	return( rc);
}

/*
 *    pciehci_init: called once during system startup or
 *      when a loadable driver is loaded.
 */
void pciehci_init(void){
    printf("pciehci_init()\n");
    printf("**********************************************************\n");
    printf("* EHCI USB Driver for Silicon Graphics Irix 6.5          *\n");
    printf("* By bsdero at gmail dot org, 2011                       *\n");
    printf("* Version 0.0.1                                          *\n");
    printf("**********************************************************\n");
    printf("EHCI Driver loaded!!\n");

    /*
     * if we are already registered, note that this is a
     * "reload" and reconnect all the places we attached.
     */
    USECDELAY(100000);
    pciio_iterate("pciehci_", pciehci_reloadme);
}


/*
 *    pciehci_unload: if no "pciehci" is open, put us to bed
 *      and let the driver text get unloaded.
 */
int pciehci_unload(void){
    if (global_pciehci_inuse)
        return EBUSY;
    pciio_iterate("pciehci_", pciehci_unloadme);
    return 0;
}


/*
 *    pciehci_reg: called once during system startup or
 *      when a loadable driver is loaded.
 *    NOTE: a bus provider register routine should always be
 *      called from _reg, rather than from _init. In the case
 *      of a loadable module, the devsw is not hooked up
 *      when the _init routines are called.
 */
int pciehci_reg(void){
    uint16_t device_id, vendor_id;
    int i;

	
    printf("pciehci_reg()\n");

    /* registering EHCI devices */
    printf("  -registering EHCI devices\n");
    for( i = 0; ehci_descriptions[i].device_id != 0; i++){
        device_id = ((ehci_descriptions[i].device_id & 0xffff0000) >> 16);
        vendor_id = (ehci_descriptions[i].device_id & 0x0000ffff);
        pciio_driver_register(vendor_id, device_id, "pciehci_", 0);  /* pciehci_attach and pciehci_detach entry points */
    }

    return 0;
}

/*
 *    pciehci_unreg: called when a loadable driver is unloaded.
 */
int pciehci_unreg(void){
    pciio_driver_unregister("pciehci_");
    return 0;
}


/*
 *    pciehci_attach: called by the pciio infrastructure
 *      once for each vertex representing a crosstalk widget.
 *      In large configurations, it is possible for a
 *      huge number of CPUs to enter this routine all at
 *      nearly the same time, for different specific
 *      instances of the device. Attempting to give your
 *      devices sequence numbers based on the order they
 *      are found in the system is not only futile but may be
 *      dangerous as the order may differ from run to run.
 */
int pciehci_attach(vertex_hdl_t conn){
    vertex_hdl_t            vhdl, blockv, charv;
    volatile uchar_t        *cfg;
    pciehci_soft_t          soft;
    pciehci_regs_t          regs;
    uint16_t                vendor_id;
    uint16_t                device_id;
    uint32_t                ssid;
    uchar_t                 rev_id;
    device_desc_t           pciehci_dev_desc;
    uint32_t                val;
    pciio_piomap_t          rmap = 0;
    pciio_piomap_t          cmap = 0;
    int                     i, retcode;
    uint32_t                device_vendor_id;
	dma_node_t              *dma_node = NULL;
    char                    superehci[]="STRING FROM EHCI DRIVER SPACE 3141516\n";
    int                     superint = 0;
	
	
    printf("pciehci_attach()\n");
    TRACE( 0xffffffff, 0xff, "TEST TRACE", NULL);
    USECDELAY(100000);
    hwgraph_device_add(conn,"pciehci","pciehci_",&vhdl,&blockv,&charv);
    /*
     * Allocate a place to put per-device information for this vertex.
     * Then associate it with the vertex in the most efficient manner.
     */
    NEW(soft);
    ASSERT(soft != NULL);


    hci_soft_set(vhdl, soft);
    hci_soft_set(blockv, soft);
    hci_soft_set(charv, soft);
    soft->ps_conn = conn;
    soft->ps_vhdl = vhdl;
    soft->ps_blockv = blockv;
    soft->ps_charv = charv;

    pciehci_dev_desc = device_desc_dup(vhdl);
    device_desc_intr_name_set(pciehci_dev_desc, "pciehci");
    device_desc_default_set(vhdl, pciehci_dev_desc);
    /*
     * Find our PCI CONFIG registers.
     */
    cfg = (volatile uchar_t *) pciio_pio_addr
        (conn, 0,                     /* device and (override) dev_info */
         PCIIO_SPACE_CFG,             /* select configuration addr space */
         0,                           /* from the start of space, */
         EHCI_NUM_CONF_REGISTERS,     /* ... up to vendor specific stuff */
         &cmap,                       /* in case we needed a piomap */
         0);                          /* flag word */
    soft->ps_cfg = *cfg;              /* save for later */
    soft->ps_cmap = cmap;
    printf("ok 1");
    USECDELAY(10000); 
    vendor_id = (uint16_t) PCI_CFG_GET16(conn, EHCI_CF_VENDOR_ID);
    device_id = (uint16_t) PCI_CFG_GET16(conn, EHCI_CF_DEVICE_ID);
    ssid = (uint32_t) PCI_CFG_GET32( conn, EHCI_CF_SSID);
    rev_id = (uchar_t) PCI_CFG_GET8( conn, EHCI_CF_REVISION_ID);

    printf( "EHCI supported device found\n");
    printf( "Vendor ID: 0x%x\nDevice ID: 0x%x\n", vendor_id, device_id);
    USECDELAY(10000);
    device_vendor_id = device_id << 16 | vendor_id;
    for( i = 0; ehci_descriptions[i].device_id != 0; i++){
        if( ehci_descriptions[i].device_id == device_vendor_id){
            soft->controller_desc.device_type = PCIEHCI_DEV;
            soft->controller_desc.vendor_id = vendor_id;
            soft->controller_desc.device_id = device_id;
            strcpy( soft->controller_desc.description, (char *)ehci_descriptions[i].controller_description);
            printf("Device Description: %s\n", ehci_descriptions[i].controller_description);
            break;
        }
    }
    dumphex( 0xffffffff, 0xff, (unsigned char *)soft->controller_desc.description, strlen( soft->controller_desc.description));
    printf( "SSID: 0x%x\nRevision ID: 0x%x\n", ssid, rev_id);



    /*
     * Get a pointer to our DEVICE registers
     */
    regs = (pciehci_regs_t) pciio_pio_addr
        (conn, 0,                       /* device and (override) dev_info */
         PCIIO_SPACE_WIN(0),            /* in my primary decode window, */
         0, EHCI_NUM_IO_REGISTERS,      /* base and size */
         &rmap,                         /* in case we needed a piomap */
         0);                            /* flag word */
    soft->ps_regs = regs;               /* save for later */
    soft->ps_rmap = rmap;
    cfg = (uchar_t *) regs;


    switch (pci_read_config(conn, PCI_USBREV, 1) & PCI_USB_REV_MASK) {
        case PCI_USB_REV_PRE_1_0:
        case PCI_USB_REV_1_0:
        case PCI_USB_REV_1_1:
            /*
             * NOTE: some EHCI USB controllers have the wrong USB
             * revision number. It appears those controllers are
             * fully compliant so we just ignore this value in
             * some common cases.
             */
            printf( "pre-2.0 USB revision (ignored)\n");
            /* fallthrough */
            break;
        case PCI_USB_REV_2_0:
            printf( "USB 2.0 revision\n");
        break;
        default:
            /* Quirk for Parallels Desktop 4.0 */
            printf("USB revision is unknown. Assuming v2.0.\n");
        break;
    }

    soft->pci_io_caps = (uchar_t *) cfg;
    printf("EHCI preconfiguration registers-dump\n");
    ehci_dump_regs( soft);
    printf("\n");

	

    if ((device_id == 0x3104 || device_id == 0x3038) &&
     (rev_id & 0xf0) == 0x60){

        /* Correct schedule sleep time to 10us */
        val = (uint32_t) pci_read_config(conn, EHCI_DS_MAX_TURN_AROUND_TIME, 1);
        if ( !(val & 0x20))
            pci_write_config(conn, EHCI_DS_MAX_TURN_AROUND_TIME, val, 1);
            printf("VIA-quirk applied\n");
        }

    soft->sc_flags = 0;
    if( vendor_id == 0x1002 || vendor_id == 0x1106){
        soft->sc_flags |= EHCI_SCFLG_LOSTINTRBUG;
        printf( "Dropped interrupts workaround enabled\n");
    }

	
	soft->pciio_info_device = pciio_info_get(conn);
    printf("Starting with gc_list\n");   
    gc_list_init( &soft->gc_list);
    printf("Starting with DMA\n");   
    USECDELAY(100000);
    dma_list_init( &soft->dma_list);
    dma_node = dma_alloc( soft->ps_conn, &soft->dma_list, 2048, 0);
    dma_list_dump( &soft->dma_list);
    printf("got DMA node dma_node=0x%08x\n", dma_node);   
    soft->dma_node = dma_node;

    ehci_init(soft);
/*
    printf("Take controller\n");
    ehci_pci_takecontroller(soft);
    USECDELAY(100000);
    */
    /*
     * Set up our interrupt.
     * We might interrupt on INTA or INTB,
     * but route 'em both to the same function.
     */
     
        
    printf("Creating PCI INTR\n");
    soft->ps_intr = pciio_intr_alloc
        (conn, pciehci_dev_desc,
         PCIIO_INTR_LINE_C,   /* VIA EHCI works with line C only */
         vhdl);

    printf("about to connect..\n");
    global_soft = soft;
    retcode = pciio_intr_connect(soft->ps_intr, pciehci_dma_intr, soft,(void *) 0);

    printf("retcode = %d\n", retcode);

    /*
     * set up our error handler.
     */
    pciio_error_register(conn, pciehci_error_handler, soft);
    /*
     * For pciio clients, *now* is the time to
     * allocate pollhead structures.
     */
    soft->ps_pollhead = phalloc(KM_SLEEP);
    soft->ps_ctl_dmamap = NULL;
    printf("pollhead=0x%x\n", soft->ps_pollhead);

    printf("call usbcore_register_hcd from pciehci \n");   
    USECDELAY(100000);
    superint = usbcore_register_driver();
    printf("csuperint = %d \n", superint);       
    
    return 0;                      /* attach successsful */
}


/*
 *    pciehci_detach: called by the pciio infrastructure
 *      once for each vertex representing a crosstalk
 *      widget when unregistering the driver.
 *
 *      In large configurations, it is possible for a
 *      huge number of CPUs to enter this routine all at
 *      nearly the same time, for different specific
 *      instances of the device. Attempting to give your
 *      devices sequence numbers based on the order they
 *      are found in the system is not only futile but may be
 *      dangerous as the order may differ from run to run.
 */
int pciehci_detach(vertex_hdl_t conn)
{
    vertex_hdl_t            vhdl, blockv, charv;
    pciehci_soft_t            soft;
    printf("pciehci_detach()\n");

    if (GRAPH_SUCCESS !=
        hwgraph_traverse(conn, "pciehci", &vhdl))
        return -1;
    soft = (pciehci_soft_t)device_info_get(vhdl);
    printf(" ok ");
    pciio_error_register(conn, 0, 0);

    printf(" 1");
    pciio_intr_disconnect(soft->ps_intr);

    printf(" 2");
    pciio_intr_free(soft->ps_intr);

    printf(" 3");
    phfree(soft->ps_pollhead);
    printf(" 4");
	EOWRITE4(soft, EHCI_USBINTR, 0);
    ehci_hcreset( soft);	
    USECDELAY(100000); 
	printf(" 5");
    gc_list_destroy( &soft->gc_list);
    dma_list_dump( &soft->dma_list);
    dma_list_destroy( &soft->dma_list);
    
/*    if (soft->ps_ctl_dmamap)
        pciio_dmamap_free(soft->ps_ctl_dmamap);
        
    if (soft->ps_str_dmamap)
        pciio_dmamap_free(soft->ps_str_dmamap);


    if (soft->ps_cmap)
        pciio_piomap_free(soft->ps_cmap);
*/
        
    printf(" 6");
    if (soft->ps_rmap)
        pciio_piomap_free(soft->ps_rmap);
    printf(" 7");
    hwgraph_edge_remove(conn, "pciehci", &vhdl);
    /*
     * we really need "hwgraph_dev_remove" ...
     */
    printf(" 8");
    if (GRAPH_SUCCESS ==
        hwgraph_edge_remove(vhdl, EDGE_LBL_BLOCK, &blockv)) {
        hci_soft_set(blockv, 0);
        hwgraph_vertex_destroy(blockv);
    }
    printf(" 9");
    if (GRAPH_SUCCESS ==
        hwgraph_edge_remove(vhdl, EDGE_LBL_CHAR, &charv)) {
        hci_soft_set(charv, 0);
        hwgraph_vertex_destroy(charv);
    }
    hci_soft_set(vhdl, 0);
    hwgraph_vertex_destroy(vhdl);
    DEL(soft);
    return 0;
}

/*
 *    pciehci_reloadme: utility function used indirectly
 *      by pciehci_init, via pciio_iterate, to "reconnect"
 *      each connection point when the driver has been
 *      reloaded.
 */
static void
pciehci_reloadme(vertex_hdl_t conn)
{
    vertex_hdl_t            vhdl;
    pciehci_soft_t            soft;
    if (GRAPH_SUCCESS !=
        hwgraph_traverse(conn, "pciehci", &vhdl))
        return;
    soft = (pciehci_soft_t)device_info_get(vhdl);
    /*
     * Reconnect our error and interrupt handlers
     */
    pciio_error_register(conn, pciehci_error_handler, soft);
    pciio_intr_connect(soft->ps_intr, pciehci_dma_intr, soft, 0);
}
/*
 *    pciehci_unloadme: utility function used indirectly by
 *      pciehci_unload, via pciio_iterate, to "disconnect" each
 *      connection point before the driver becomes unloaded.
 */
static void
pciehci_unloadme(vertex_hdl_t pconn)
{
    vertex_hdl_t            vhdl;
    pciehci_soft_t            soft;
    if (GRAPH_SUCCESS !=
        hwgraph_traverse(pconn, "pciehci", &vhdl))
        return;
    soft = (pciehci_soft_t)device_info_get(vhdl);
    /*
     * Disconnect our error and interrupt handlers
     */
    pciio_error_register(pconn, 0, 0);
    pciio_intr_disconnect(soft->ps_intr);
}
/* ====================================================================
 *          DRIVER OPEN/CLOSE
 */
/*
 *    pciehci_open: called when a device special file is
 *      opened or when a block device is mounted.
 */
/* ARGSUSED */
int pciehci_open(dev_t *devp, int oflag, int otyp, cred_t *crp){
    vertex_hdl_t              vhdl;
    pciehci_soft_t            soft;
    pciehci_regs_t            regs;
    
    printf("pciehci_open()\n");
    if( *devp == NULL){
        return( EIO); 
    }
    
    vhdl = dev_to_vhdl(*devp);
    soft = (pciehci_soft_t)device_info_get(vhdl);
    
    if( soft == NULL){
        return( EIO);
    }
    
    regs = soft->ps_regs;
    printf("pciehci_open() regs=%x\n", regs);

    /*
     * BLOCK DEVICES: now would be a good time to
     * calculate the size of the device and stash it
     * away for use by pciehci_size.
     */
    /*
     * USER ABI (64-bit): chances are, you are being
     * compiled for use in a 64-bit IRIX kernel; if
     * you use the _ioctl or _poll entry points, now
     * would be a good time to test and save the
     * user process' model so you know how to
     * interpret the user ioctl and poll requests.
     */
    if (!(DEVICE_SST_INUSE & atomicSetUint(&soft->ps_sst, DEVICE_SST_INUSE))){
        atomicAddInt(&global_pciehci_inuse, 1);
    return 0;
    }
    
    return EBUSY;
}


/*
 *    pciehci_close: called when a device special file
 *      is closed by a process and no other processes
 *      still have it open ("last close").
 */
/* ARGSUSED */
int pciehci_close(dev_t dev, int oflag, int otyp, cred_t *crp){
    vertex_hdl_t              vhdl;
    pciehci_soft_t            soft;
    pciehci_regs_t            regs;
    
    printf("pciehci_close()\n");
    
    vhdl = dev_to_vhdl(dev);
    soft = (pciehci_soft_t)device_info_get(vhdl);
    
    if( soft == NULL){
        return( EIO);
    }
    
    regs = soft->ps_regs;
    printf("pciehci_close() regs=%x\n", regs);
    atomicClearUint(&soft->ps_sst, DEVICE_SST_INUSE);
    atomicAddInt(&global_pciehci_inuse, -1);
    return 0;
}


void ehci_setup_usb_transfer(dev_t dev){
      vertex_hdl_t vhdl = dev_to_vhdl(dev);
      pciehci_soft_t          soft = (pciehci_soft_t)device_info_get(vhdl);
      
      
      soft->ps_ctl_dmamap = pciio_dmamap_alloc(vhdl, device_desc_default_get(vhdl), 
              sizeof (int), PCIIO_DMA_DATA |  PCIIO_FIXED);
              
      printf("map = 0x%x\n", soft->ps_ctl_dmamap);
      
}




char *get_ioctl_str( int cmd){
    int i;


    for( i = 0; ehci_ioctls[i].str != NULL; i++){
	    if( cmd == ehci_ioctls[i].cmd)
		    return( (char *) ehci_ioctls[i].str);
    }

    if( i > 0) 	
	    return( (char *) ehci_ioctls[i - 1].str);
		
    return(NULL);
}


/* ====================================================================
 *          CONTROL ENTRY POINT
 */
/*
 *    pciehci_ioctl: a user has made an ioctl request
 *      for an open character device.
 *      Arguments cmd and arg are as specified by the user;
 *      arg is probably a pointer to something in the user's
 *      address space, so you need to use copyin() to
 *      read through it and copyout() to write through it.
 */
/* ARGSUSED */
int pciehci_ioctl(dev_t dev, int cmd, void *arg, int mode, cred_t *crp, int *rvalp){
    vertex_hdl_t vhdl = dev_to_vhdl(dev);
    EHCI_info_t EHCI_info;
    EHCI_ports_status_t EHCI_ports_status;
    EHCI_controller_info_t EHCI_controller_info;
	EHCI_regs_t EHCI_regs;
	EHCI_portnum_t EHCI_portnum;
	EHCI_USB_packet_t EHCI_USB_packet;
	EHCI_USB_command_t EHCI_USB_command;
	uint32_t v;
    int i, j, pr, rc;

    
    pciehci_soft_t          soft = (pciehci_soft_t)device_info_get(vhdl);
    pciehci_regs_t          regs = soft->ps_regs;
    printf("pciehci_ioctl() enter regs=%x\n", regs);
    printf("cmd = 0x%x, %s\n", cmd, get_ioctl_str( cmd));    

    printf("cmd = 0x%x, 0x%x\n", cmd, IOCTL_EHCI_USB_COMMAND);    
    switch( cmd){
        case IOCTL_EHCI_DRIVER_INFO:{
            strcpy( (char *) EHCI_info.long_name, USBIRIX_DRV_LONG_NAME);
            strcpy( (char *) EHCI_info.short_name, USBIRIX_DRV_SHORT_NAME);
            strcpy( (char *) EHCI_info.version, USBIRIX_DRV_VERSION);
            strcpy( (char *) EHCI_info.short_version, USBIRIX_DRV_SHORT_VERSION);
            strcpy( (char *) EHCI_info.seqn, USBIRIX_DRV_SEQ);
            strcpy( (char *) EHCI_info.build_date, USBIRIX_BUILD_DATE);
            copyout( (caddr_t) &EHCI_info, arg, sizeof( EHCI_info_t));
            rc = 0;
            break;
        }
        case IOCTL_EHCI_PORTS_STATUS:{
            
            EHCI_ports_status.num_ports = global_num_ports;
            for( i = 0; i < global_num_ports; i++)
                EHCI_ports_status.port[i] = global_ports_status[i];
        
            copyout( (caddr_t) &EHCI_ports_status, arg, sizeof( EHCI_ports_status_t));
			rc = 0;
            break;
        }
        case IOCTL_EHCI_CONTROLLER_INFO:{
            EHCI_controller_info.pci_bus = (uchar_t) pciio_info_bus_get( soft->pciio_info_device);
			EHCI_controller_info.pci_slot = (uchar_t) pciio_info_slot_get( soft->pciio_info_device);
			EHCI_controller_info.pci_function = (int) pciio_info_function_get( soft->pciio_info_device);
            strcpy( (char *) EHCI_controller_info.device_string, soft->controller_desc.description);
            EHCI_controller_info.device_id = soft->controller_desc.vendor_id | (soft->controller_desc.device_id << 16);
            EHCI_controller_info.num_ports = soft->sc_noport;
            copyout( (caddr_t) &EHCI_controller_info, arg, sizeof( EHCI_controller_info_t));
			rc = 0;
            break;
        }
        case IOCTL_EHCI_USBREGS_GET:{
		    
		    EHCI_regs.usb_cmd = EOREAD4(soft, EHCI_USBCMD);
            EHCI_regs.usb_sts = EOREAD4(soft, EHCI_USBSTS);
			EHCI_regs.usb_intr = EOREAD4(soft, EHCI_USBINTR);
			EHCI_regs.frindex = EOREAD4(soft, EHCI_FRINDEX);
			EHCI_regs.ctrldssegment = EOREAD4(soft, EHCI_CTRLDSSEGMENT);
			EHCI_regs.periodiclistbase = EOREAD4(soft, EHCI_PERIODICLISTBASE);
			EHCI_regs.asynclistaddr = EOREAD4(soft, EHCI_ASYNCLISTADDR);
			EHCI_regs.configured = EOREAD4(soft, EHCI_CONFIGFLAG);
			EHCI_regs.num_ports = soft->sc_noport;
			for( i = 1; i <= soft->sc_noport; i++)
			    EHCI_regs.portsc[i-1] = EOREAD4(soft, EHCI_PORTSC(i));
				
			copyout( (caddr_t) &EHCI_regs, arg, sizeof( EHCI_regs_t));
			rc = 0;
			break;
		}
		case IOCTL_EHCI_HC_RESET:{
		    rc = ehci_hcreset( soft);
			break;
		}
		case IOCTL_EHCI_PORT_RESET:{
		    copyin( arg, (caddr_t) &EHCI_portnum, sizeof( EHCI_portnum_t));
			printf("portnum=%d\n", EHCI_portnum);
			i = EHCI_portnum + 1;
						/* reset port */
		    rc = -1;
            EOWRITE4( soft, EHCI_PORTSC(i),  EOREAD4(soft, EHCI_PORTSC(i)) | EHCI_PS_PR);
            for (j = 0; j < 100; i++) {
                USECDELAY(1000);
                pr = EOREAD4(soft, EHCI_PORTSC(i)) & EHCI_PS_PR;
                if (!pr){
				    rc = 0;
                    break;
				}    
            }
			
			break;
		}
        case IOCTL_EHCI_PORT_ENABLE:{
		    copyin( arg, (caddr_t) &EHCI_portnum, sizeof( EHCI_portnum_t));
			printf("portnum=%d\n", EHCI_portnum);
			i = EHCI_portnum + 1;
			v = EOREAD4(soft, EHCI_PORTSC(i)) & ~EHCI_PS_CLEAR;
			EOWRITE4( soft, EHCI_PORTSC(i), v | EHCI_PS_PE);
			rc = 0;
			break;
		}
        case IOCTL_EHCI_PORT_DISABLE:{
		    copyin( arg, (caddr_t) &EHCI_portnum, sizeof( EHCI_portnum_t));
			printf("portnum=%d\n", EHCI_portnum);
			i = EHCI_portnum + 1;
			v = EOREAD4(soft, EHCI_PORTSC(i)) & ~EHCI_PS_CLEAR;
			EOWRITE4( soft, EHCI_PORTSC(i), v & ~EHCI_PS_PE);
			rc = 0;
			break;
		}
		case IOCTL_EHCI_ENUMERATE:{
		    ehci_enumerate(soft);
		    rc = 0;
		    break;
		}
		case IOCTL_EHCI_DUMP_DATA:{
		    ehci_dump_data( soft);
		    rc = 0;
			break;
		}
		case IOCTL_DUMMY_DISABLE:{ 
			rc = ehci_dummy_disable(soft);
			break;
	    }
		case IOCTL_DUMMY_ENABLE:{ 
			rc = ehci_dummy_enable(soft);
			break;
	    }
	    case IOCTL_EHCI_USB_COMMAND:{
                printf("running urb\n");
			/*copyin( arg, (caddr_t) &EHCI_USB_packet.usb_command, sizeof( EHCI_USB_command_t));
			*/
			EHCI_USB_packet.usb_command.command_size = 8;
            EHCI_USB_packet.usb_command.bRequestType = 0x80;
            EHCI_USB_packet.usb_command.bRequest = 0x6;
            EHCI_USB_packet.usb_command.wValue = 0x300;
            EHCI_USB_packet.usb_command.wIndex = 0x400;
            EHCI_USB_packet.usb_command.timeout = 0x380;
            EHCI_USB_packet.usb_command.wLength = 0xff;
            EHCI_USB_packet.pipe.control = 1;
            EHCI_USB_packet.pipe.device_address = 0; 
            EHCI_USB_packet.pipe.endpoint = 0;
            EHCI_USB_packet.pipe.speed = 2;
            EHCI_USB_packet.pipe.hub_address = 0;
            EHCI_USB_packet.pipe.port_number = 0;
		    rc = ehci_transfer_control( soft, &EHCI_USB_packet);
		    /*
		    copyout( (caddr_t) &EHCI_USB_packet, arg, sizeof( EHCI_USB_packet_t));*/
		    break;	
		}
        default:
                printf("UNKNOWN IOCTL\n");
				break;
    }    
    
    
    *rvalp = rc;
	printf("pciehci_ioctl() enter rc=%d\n", rc);
    return 0;          /* TeleType® is a registered trademark */
}
/* ====================================================================
 *          DATA TRANSFER ENTRY POINTS
 *      Since I'm trying to provide an example for both
 *      character and block devices, I'm routing read
 *      and write back through strategy as described in
 *      the IRIX Device Driver Programming Guide.
 *      This limits our character driver to reading and
 *      writing in multiples of the standard sector length.
 */
/* ARGSUSED */
int
pciehci_read(dev_t dev, uio_t * uiop, cred_t *crp)
{
    return physiock(pciehci_strategy,
                    0,          /* alocate temp buffer & buf_t */
                    dev,        /* dev_t arg for strategy */
                    B_READ,     /* direction flag for buf_t */
                    pciehci_size(dev),
                    uiop);
}
/* ARGSUSED */
int
pciehci_write(dev_t dev, uio_t * uiop, cred_t *crp)
{
    return physiock(pciehci_strategy,
                    0,          /* alocate temp buffer & buf_t */
                    dev,        /* dev_t arg for strategy */
                    B_WRITE,    /* direction flag for buf_t */
                    pciehci_size(dev),
                    uiop);
}
/* ARGSUSED */
int
pciehci_strategy(struct buf *bp)
{
    /*
     * XXX - create strategy code here.
     */
    return 0;
}
/* ====================================================================
 *          POLL ENTRY POINT
 */
int pciehci_poll(dev_t dev, short events, int anyyet,
           short *reventsp, struct pollhead **phpp, unsigned int *genp){

#define OUR_EVENTS (POLLIN|POLLOUT|POLLRDNORM)

    vertex_hdl_t            vhdl = dev_to_vhdl(dev);
    pciehci_soft_t          soft = (pciehci_soft_t)device_info_get(vhdl);
    pciehci_regs_t          regs = soft->ps_regs;
    short                   happened = 0;
    unsigned int            gen;
    short wanted = events & OUR_EVENTS;
               
    printf("pciehci_poll() regs=%x\n", regs);
    /*
     * Need to snapshot the pollhead generation number before we check
     * device state.  In many drivers a lock is used to interlock the
     * "high" and "low" portions of the driver.  In those cases we can
     * wait to do this snapshot till we're in the critical region.
     * Snapshotting it early isn't a problem since that makes the
     * snapshotted generation number a more conservative estimate of
     * what generation of  pollhead our event state report indicates.
     */
    /* 
    gen = POLLGEN(soft->ps_pollhead);
    if (events & (POLLIN | POLLRDNORM))
        if (soft->ps_sst & DEVICE_SST_RX_READY)
            happened |= POLLIN | POLLRDNORM;
    if (events & POLLOUT)
        if (soft->ps_sst & DEVICE_SST_TX_READY)
            happened |= POLLOUT;
    if (soft->ps_sst & DEVICE_SST_ERROR)
        happened |= POLLERR;
    *reventsp = happened;
    if (!happened && anyyet) {
        *phpp = soft->ps_pollhead;
        *genp = gen;
    }
    */

    gen = POLLGEN(soft->ps_pollhead);
    
    if (events & (POLLIN))
        if (soft->ps_event != 0){
           /*happened |= POLLIN | POLLRDNORM;*/
           happened = soft->ps_event;
           soft->ps_event = 0;
        }
    *reventsp = happened;
    if (!happened) {
        *phpp = soft->ps_pollhead;
        *genp = gen;
    }
    
    

    return 0;
}
/* ====================================================================
 *          MEMORY MAP ENTRY POINTS
 */
/* ARGSUSED */
int
pciehci_map(dev_t dev, vhandl_t *vt,
          off_t off, size_t len, uint_t prot)
{
    vertex_hdl_t            vhdl = dev_to_vhdl(dev);
    pciehci_soft_t          soft = (pciehci_soft_t)device_info_get(vhdl);
    vertex_hdl_t            conn = soft->ps_conn;
    pciehci_regs_t            regs = soft->ps_regs;
    pciio_piomap_t          amap = 0;
    caddr_t                 kaddr;
    printf("pciehci_map() regs=%x\n", regs);
    /*
     * Stuff we want users to mmap is in our second BASE_ADDR window.
     */
    kaddr = (caddr_t) pciio_pio_addr
        (conn, 0,
         PCIIO_SPACE_WIN(1),
         off, len, &amap, 0);
    if (kaddr == NULL)
        return EINVAL;
    /*
     * XXX - must stash amap somewhere so we can pciio_piomap_free it
     * when the mapping goes away.
     */
    v_mapphys(vt, kaddr, len);
    return 0;
}
/* ARGSUSED2 */
int
pciehci_unmap(dev_t dev, vhandl_t *vt)
{
    /*
     * XXX - need to find "amap" that we used in pciehci_map() above,
     * and if (amap) pciio_piomap_free(amap);
     */
    return (0);
}
/* ====================================================================
 *          INTERRUPT ENTRY POINTS
 *  We avoid using the standard name, since our prototype has changed.
 */
void
pciehci_dma_intr(intr_arg_t arg)
{
    pciehci_soft_t            soft = (pciehci_soft_t) arg;
    printf("pciehci_dma_intr()\n");
    printf("soft=0x%x arg=0x%x\n", soft, arg);
    if( soft == NULL){
        soft = global_soft;
    }
    
/*
    vertex_hdl_t              vhdl = soft->ps_vhdl;
    pciehci_regs_t            regs = soft->ps_regs;

    cmn_err(CE_CONT, "pciehci soft=%x\n", soft);
*/
    printf("interrupt!! \n");
    USECDELAY(1000);
    ehci_interrupt( soft);
        /*pollwakeup( soft->ps_pollhead, POLLIN); */
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
/* ====================================================================
 *          ERROR HANDLING ENTRY POINTS
 */
static int
pciehci_error_handler(void *einfo,
                    int error_code,
                    ioerror_mode_t mode,
                    ioerror_t *ioerror)
{
    pciehci_soft_t            soft = (pciehci_soft_t) einfo;
    vertex_hdl_t            vhdl = soft->ps_vhdl;
#if DEBUG && ERROR_DEBUG
    cmn_err(CE_CONT, "%v: pciehci_error_handler\n", vhdl);
#else
    vhdl = vhdl;
#endif
    /*
     * XXX- there is probably a lot more to do
     * to recover from an error on a real device;
     * experts on this are encouraged to add common
     * things that need to be done into this function.
     */
    ioerror_dump("sample_pciio", error_code, mode, ioerror);
    return IOERROR_HANDLED;
}
/* ====================================================================
 *          SUPPORT ENTRY POINTS
 */
/*
 *    pciehci_halt: called during orderly system
 *      shutdown; no other device driver call will be
 *      made after this one.
 */
void
pciehci_halt(void)
{
    printf("pciehci_halt()\n");
}
/*
 *    pciehci_size: return the size of the device in
 *      "sector" units (multiples of NBPSCTR).
 */
int
pciehci_size(dev_t dev)
{
    vertex_hdl_t            vhdl = dev_to_vhdl(dev);
    pciehci_soft_t            soft = (pciehci_soft_t)device_info_get(vhdl);
    return soft->ps_blocks;
}
/*
 *    pciehci_print: used by the kernel to report an
 *      error detected on a block device.
 */
int
pciehci_print(dev_t dev, char *str)
{
    cmn_err(CE_NOTE, "%V: %s\n", dev, str);
    return 0;
}
