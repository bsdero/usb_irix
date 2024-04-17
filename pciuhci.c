/*
********************************************************************
* PCI UHCI Device Driver for Irix 6.5                              *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
*                                                                  *
* 2011                                                             *
*                                                                  *
********************************************************************

*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      09-10-2011     Initial version -taken from existing code base                           *
*******************************************************************************************************

WARNING!!!

EXPERIMENTAL DRIVER FOR UHCI, NOT USEFUL YET, USE IT UNDER UR OWN RISK!!!
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
#include "uhcireg.h"   /* user space ioctls and defines, used also in driver code */
#include "trace.c"
#include "dumphex.c"   /* dump utility */
#include "list.c"      /* list utility */
#include "gc.c"        /* memory lists utility */
#include "dma.c"       /* dma lists utility */




/************************************************************************************
 * UHCI SPECIFIC DEFINES AND MACROS                                                 *
 ************************************************************************************/
 /* defined for easier port from NetBSD */
#define UBARR(sc)                                   NULL
#define UREAD1(sc,a)                                pciio_pio_read8(  (uint8_t *)  ((sc)->pci_io_caps) + a )
#define UREAD2(sc,a)                                pciio_pio_read16( (uint16_t *) ((sc)->pci_io_caps) + a )
#define UREAD4(sc,a)                                pciio_pio_read32( (uint32_t *) ((sc)->pci_io_caps) + a )
#define UWRITE1(sc,a,x)                             pciio_pio_write8(  x, (uint8_t *)  ((sc)->pci_io_caps) + a  )
#define UWRITE2(sc,a,x)                             pciio_pio_write16( x, (uint16_t *) ((sc)->pci_io_caps) + a  )
#define UWRITE4(sc,a,x)                             pciio_pio_write32( x, (uint32_t *) ((sc)->pci_io_caps) + a  )
#ifdef _SGIO2_
 #define UREAD1(sc, a)                               pciio_pio_read8(  (uint8_t *)  ((sc)->pci_io_caps) + a )
 #define UREAD2(sc, a)                               pciio_pio_read16( (uint16_t *) ((sc)->pci_io_caps) + (a>>1) )
 #define UREAD4(sc, a)                               pciio_pio_read32( (uint32_t *) ((sc)->pci_io_caps) + (a>>2) )
 #define UWRITE1(sc, a, x)                           pciio_pio_write8(  x, (uint8_t *)  ((sc)->pci_io_caps) + a  )
 #define UWRITE2(sc, a, x)                           pciio_pio_write16( x, (uint16_t *) ((sc)->pci_io_caps) + (a>>1)  )
 #define UWRITE4(sc, a, x)                           pciio_pio_write32( x, (uint32_t *) ((sc)->pci_io_caps) + (a>>2)  )
#else
 #define UREAD1(sc, a)                               ( (uint8_t )   *(  ((sc)->pci_io_caps) + a ))
 #define UREAD2(sc, a)                               ( (uint16_t)   *( (uint16_t *) ((sc)->pci_io_caps) + (a>>1) ))
 #define UREAD4(sc, a)                               ( (uint32_t)   *( (uint32_t *) ((sc)->pci_io_caps) + (a>>2) ))
 #define UWRITE1(sc, a, x)                           ( *(    (uint8_t  *)  ((sc)->pci_io_caps) + a       ) = x)
 #define UWRITE2(sc, a, x)                           ( *(    (uint16_t *)  ((sc)->pci_io_caps) + (a>>1)  ) = x)
 #define UWRITE4(sc, a, x)                           ( *(    (uint32_t *)  ((sc)->pci_io_caps) + (a>>2)  ) = x)
#endif


#define UHCICMD(sc, cmd)                            UWRITE2(sc, UHCI_CMD, cmd)
#define UHCISTS(sc)                                 UREAD2(sc, UHCI_STS)
#define UHCI_RESET_TIMEOUT                          100  /* ms, reset timeout */
#define UHCI_INTR_ENDPT                             1

#define UHCI_NUM_CONF_REGISTERS                     0xc2
#define UHCI_NUM_IO_REGISTERS                       0x14


/***************************************************************************
 *   Specific driver flags building configuration                          *
 ***************************************************************************/
int         pciuhci_devflag = D_MP;
char        *ucon_mversion = M_VERSION;  /* for loadable modules */





/***************************************************************************
 *   UHCI Specific structures/data                                         *
 ***************************************************************************/
typedef struct pciuhci_soft_s     *pciuhci_soft_t;         /* software state */

/*
 *    All registers on the Sample PCIIO Client
 *      device are 32 bits wide.
 */
typedef __uint32_t                                             pciuhci_reg_t;
typedef volatile struct pciuhci_regs_s                         *pciuhci_regs_t;  /* dev registers */
typedef struct pciuhci_soft_s                                  *pciuhci_soft_t;         /* software state */
/*
 *    pciuhci_regs: layout of device registers
 *      Our device config registers are, of course, at
 *      the base of our assigned CFG space.
 *      Our sample device registers are in the PCI area
 *      decoded by the device's first BASE_ADDR window.
 */
struct pciuhci_regs_s {
    pciuhci_reg_t             pr_control;
    pciuhci_reg_t             pr_status;
};

struct pciuhci_soft_s {
    HCI_COMMON_DATA
    uchar_t                       *pci_io_caps; /* pointer to mmaped registers */
};
	
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


/***************************************************************************
 *   Function table of contents                                            *
 ***************************************************************************/
void                    pciuhci_init(void);
int                     pciuhci_unload(void);
int                     pciuhci_reg(void);
int                     pciuhci_unreg(void);
int                     pciuhci_attach(vertex_hdl_t conn);
int                     pciuhci_detach(vertex_hdl_t conn);
static pciio_iter_f     pciuhci_reloadme;
static pciio_iter_f     pciuhci_unloadme;
int                     pciuhci_open(dev_t *devp, int oflag, int otyp, cred_t *crp);
int                     pciuhci_close(dev_t dev, int oflag, int otyp, cred_t *crp);
int                     pciuhci_ioctl(dev_t dev, int cmd, void *arg, int mode, cred_t *crp, int *rvalp);
int                     pciuhci_read(dev_t dev, uio_t * uiop, cred_t *crp);
int                     pciuhci_write(dev_t dev, uio_t * uiop,cred_t *crp);
int                     pciuhci_strategy(struct buf *bp);
int                     pciuhci_poll(dev_t dev, short events, int anyyet, short *reventsp, struct pollhead **phpp, unsigned int *genp);
int                     pciuhci_map(dev_t dev, vhandl_t *vt, off_t off, size_t len, uint_t prot);
int                     pciuhci_unmap(dev_t dev, vhandl_t *vt);
void                    pciuhci_dma_intr(intr_arg_t arg);
static error_handler_f  pciuhci_error_handler;
void                    pciuhci_halt(void);
int                     pciuhci_size(dev_t dev);
int                     pciuhci_print(dev_t dev, char *str);

int         pciuhci_inuse = 0;     /* number of "pciuhci" devices open */

/***************************************************************************
 *   UHCI functions                                                        *
 ***************************************************************************/
int uhci_restart(pciuhci_soft_t sc){
    printf("uhci_restart()\n");

  	if (UREAD2(sc, UHCI_CMD) & UHCI_CMD_RS) {
		printf("UHCI Already started\n");
		return (0);
	}


	/*
	 * Assume 64 byte packets at frame end and start HC controller:
	 */
	UHCICMD(sc, (UHCI_CMD_MAXP | UHCI_CMD_RS));

	/* wait 10 milliseconds */
    USECDELAY(100);


	/* check that controller has started */

	if (UREAD2(sc, UHCI_STS) & UHCI_STS_HCH) {
		printf( "uhci restart failed\n");
		return (1);
	}
	return (0);
}
 
void uhci_reset(pciuhci_soft_t sc){
	uint16_t n;


	printf("uhci_reset()\n");

	/* disable interrupts */
	UWRITE2(sc, UHCI_INTR, 0);

	/* global reset */
	UHCICMD(sc, UHCI_CMD_GRESET);

	/* wait */
	USECDELAY(1000);
	
	/* terminate all transfers */

	UHCICMD(sc, UHCI_CMD_HCRESET);

	/* the reset bit goes low when the controller is done */
	n = UHCI_RESET_TIMEOUT;
	while (n--) {
		/* wait one millisecond */

		USECDELAY(10);

		if (!(UREAD2(sc, UHCI_CMD) & UHCI_CMD_HCRESET)) {
			goto done_1;
		}
	}

	printf( "controller did not reset\n");

done_1:

	n = 10;
	while (n--) {
		/* wait one millisecond */

		USECDELAY(10000);

		/* check if HC is stopped */
		if (UREAD2(sc, UHCI_STS) & UHCI_STS_HCH) {
			goto done_2;
		}
	}

	printf("controller did not stop\n");

done_2:

	/* reload the configuration */
	UWRITE2(sc, UHCI_FRNUM, sc->sc_saved_frnum);
	UWRITE1(sc, UHCI_SOF, sc->sc_saved_sof);
}

void uhci_start(pciuhci_soft_t sc){


	printf( "uhci_start()\n");

	/* enable interrupts */

	UWRITE2(sc, UHCI_INTR,
	    (UHCI_INTR_TOCRCIE |
	    UHCI_INTR_RIE |
	    UHCI_INTR_IOCE |
	    UHCI_INTR_SPIE));

	if (uhci_restart(sc)) {
		   printf("cannot start UHCI controller\n");
	}


}

int uhci_init(pciuhci_soft_t sc){
	uint16_t bit;
	uint16_t x;
	uint16_t y;

	printf("uhci_init()\n");

	/* reset the controller */
	uhci_reset(sc);

	/* start the controller */
	uhci_start(sc);

	return (0);
}


void uhci_interrupt(pciuhci_soft_t sc){
	uint32_t status;


	printf( "uhci_interrupt\n");

	status = UREAD2(sc, UHCI_STS) & UHCI_STS_ALLINTRS;
	if (status == 0) {
		/* the interrupt was not for us */
		goto done;
	}
	
	if (status & (UHCI_STS_RD | UHCI_STS_HSE |
	    UHCI_STS_HCPE | UHCI_STS_HCH)) {

		if (status & UHCI_STS_RD) {
			printf("resume detect\n");
		}
		if (status & UHCI_STS_HSE) {
			printf("host system error\n");
		}
		if (status & UHCI_STS_HCPE) {
			printf("host controller process error\n");
			    
		}
		if (status & UHCI_STS_HCH) {
			/* no acknowledge needed */
			DPRINTF("%host controller halted\n");
			
#ifdef USB_DEBUG
			if (uhcidebug > 0) {
				uhci_dump_all(sc);
			}
#endif
		}
	}
	/* get acknowledge bits */
	status &= (UHCI_STS_USBINT |
	    UHCI_STS_USBEI |
	    UHCI_STS_RD |
	    UHCI_STS_HSE |
	    UHCI_STS_HCPE);

	if (status == 0) {
		/* nothing to acknowledge */
		goto done;
	}
	/* acknowledge interrupts */
	UWRITE2(sc, UHCI_STS, status);



done:

}


void uhci_dumpregs(pciuhci_soft_t sc)
{
	printf(" regs: cmd=%04x, sts=%04x, intr=%04x, frnum=%04x, "
	    "flbase=%08x, sof=%04x, portsc1=%04x, portsc2=%04x\n",
	    UREAD2(sc, UHCI_CMD),
	    UREAD2(sc, UHCI_STS),
	    UREAD2(sc, UHCI_INTR),
	    UREAD2(sc, UHCI_FRNUM),
	    UREAD4(sc, UHCI_FLBASEADDR),
	    UREAD1(sc, UHCI_SOF),
	    UREAD2(sc, UHCI_PORTSC1),
	    UREAD2(sc, UHCI_PORTSC2));
}


/***************************************************************************
 *   Driver specific entry-point functions                                 *
 ***************************************************************************/

/*
 *    pciuhci_init: called once during system startup or
 *      when a loadable driver is loaded.
 */
void pciuhci_init(void){
    printf("pciuhci_init()\n");
    printf("*************************************************\n");
    printf("* UHCI Driver for Irix 6.5                      *\n");
    printf("* By bsdero at gmail dot org, 2011              *\n");
    printf("* Version 0.0.1                                 *\n");
    printf("*************************************************\n");
    printf("Driver loaded\n");

    /*
     * if we are already registered, note that this is a
     * "reload" and reconnect all the places we attached.
     */
    pciio_iterate("pciuhci_", pciuhci_reloadme);
}


/*
 *    pciuhci_unload: if no "pciuhci" is open, put us to bed
 *      and let the driver text get unloaded.
 */
int pciuhci_unload(void){
    if (pciuhci_inuse)
        return EBUSY;
    pciio_iterate("pciuhci_", pciuhci_unloadme);
    return 0;
}


/*
 *    pciuhci_reg: called once during system startup or
 *      when a loadable driver is loaded.
 *    NOTE: a bus provider register routine should always be
 *      called from _reg, rather than from _init. In the case
 *      of a loadable module, the devsw is not hooked up
 *      when the _init routines are called.
 */
int pciuhci_reg(void){
    uint16_t device_id, vendor_id;
    uchar_t *device_description;
    int i;

    printf("pciuhci_reg()\n");

    /*We'll detect these devices */
    /* registering UHCI devices */
    printf("  -registering UHCI devices\n");
    for( i = 0; uhci_descriptions[i].device_id != 0; i++){
        device_id = ((uhci_descriptions[i].device_id & 0xffff0000) >> 16);
        vendor_id = (uhci_descriptions[i].device_id & 0x0000ffff);
        pciio_driver_register(vendor_id, device_id, "pciuhci_", 0);  /* pciuhci_attach and pciuhci_detach entry points */
    }

    return 0;
}

/*
 *    pciuhci_unreg: called when a loadable driver is unloaded.
 */
int pciuhci_unreg(void){
    pciio_driver_unregister("pciuhci_");
    return 0;
}



/*
 *    pciuhci_attach: called by the pciio infrastructure
 *      once for each vertex representing a crosstalk widget.
 *      In large configurations, it is possible for a
 *      huge number of CPUs to enter this routine all at
 *      nearly the same time, for different specific
 *      instances of the device. Attempting to give your
 *      devices sequence numbers based on the order they
 *      are found in the system is not only futile but may be
 *      dangerous as the order may differ from run to run.
 */
int pciuhci_attach(vertex_hdl_t conn){
    vertex_hdl_t            vhdl, blockv, charv;
    volatile uchar_t        *cfg;
    pciehci_soft_t          soft;
	pciehci_regs_t          regs;
    __uint16_t              vendor_id;
    __uint16_t              device_id;
    __uint32_t              ssid;
    uchar_t                 rev_id;
    device_desc_t           pciuhci_dev_desc;
    __uint32_t              val;
    pciio_piomap_t          rmap = 0;
    pciio_piomap_t          cmap = 0;
    int                     i, retcode;
    uint32_t                device_vendor_id;
    int rid;


    printf("pciuhci_attach()\n");

    hwgraph_device_add(conn,"pciuhci","pciuhci_",&vhdl,&blockv,&charv);
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

    pciuhci_dev_desc = device_desc_dup(vhdl);
    device_desc_intr_name_set(pciuhci_dev_desc, "pciuhci");
    device_desc_default_set(vhdl, pciuhci_dev_desc);
	
    /*
     * Find our PCI UHCI CONFIG registers.
     */
    cfg = (volatile uchar_t *) pciio_pio_addr
        (conn, 0,               /* device and (override) dev_info */
         PCIIO_SPACE_CFG,       /* select configuration addr space */
         0,                     /* from the start of space, */
         UHCI_NUM_CONF_REGISTERS,     /* ... up to vendor specific stuff */
         &cmap,                 /* in case we needed a piomap */
         0);                    /* flag word */
    soft->ps_cfg = *cfg;         /* save for later */
    soft->ps_cmap = cmap;

    vendor_id = PCI_CFG_GET16(conn, uhci_CF_VENDOR_ID);
    device_id = PCI_CFG_GET16(conn, uhci_CF_DEVICE_ID);
	
    printf( "EHCI supported device found\n");
    printf( "Vendor ID: 0x%x\nDevice ID: 0x%x\n", vendor_id, device_id);
    device_vendor_id = device_id << 16 | vendor_id;
    for( i = 0; uhci_descriptions[i].device_id != 0; i++){
        if( uhci_descriptions[i].device_id == device_id){
		    soft->device_type = PCIUHCI_DEV;
			soft->vendor_id = vendor_id;
			soft->device_id = device_id;
			strcpy( soft->description, uhci_descriptions[i].controller_description);
            printf("Device Description: %s\n", uhci_descriptions[i].controller_description);
            break;
        }
    }

    /*
     * Get a pointer to our DEVICE registers
     */
	 
    printf("getting piomap to IO registers\n");
    regs = (pciuhci_regs_t) pciio_pio_addr
        (conn, 0,               /* device and (override) dev_info */
         PCIIO_SPACE_WIN(0),    /* in my primary decode window, */
         0, UHCI_NUM_IO_REGISTERS,      /* base and size */
         &rmap,                 /* in case we needed a piomap */
         0);                    /* flag word */
		 
    soft->ps_regs = regs;       /* save for later */
    soft->ps_rmap = rmap;
/*    printf("pciuhci_attach: I can see my device regs at 0x%x\n", regs);
        cfg = (uchar_t *) regs;
        printf("registers: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
             cfg[0], cfg[1], cfg[2], cfg[3], cfg[4], cfg[5], cfg[6], cfg[7]);
*/


    soft->pci_io_caps = (uchar_t *) regs;
	/* disable interrupts */
	
	printf("Disabling interrupts\n");
	UWRITE2(soft, UHCI_INTR, 0);

	
	pci_write_config(soft, PCI_LEGSUP, PCI_LEGSUP_USBPIRQDEN, 2);

	uhci_init();
    soft->ps_intr = pciio_intr_alloc
        (conn, 0,
         PCIIO_INTR_LINE_A |
         PCIIO_INTR_LINE_B,
         vhdl);
    pciio_intr_connect(soft->ps_intr,
                       pciuhci_dma_intr, soft,(void *) 0);
    /*
     * set up our error handler.
     */
    pciio_error_register(conn, pciuhci_error_handler, soft);
    /*
     * For pciio clients, *now* is the time to
     * allocate pollhead structures.
     */
    soft->ps_pollhead = phalloc(0);	
	
    return 0;                      /* attach successsful */
}

/*
 *    pciuhci_detach: called by the pciio infrastructure
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
int pciuhci_detach(vertex_hdl_t conn){
    vertex_hdl_t            vhdl, blockv, charv;
    pciuhci_soft_t            soft;
    printf("pciuhci_detach()\n");
    if (GRAPH_SUCCESS !=
        hwgraph_traverse(conn, "pciuhci", &vhdl))
        return -1;
    soft = (pciuhci_soft_t)device_info_get(vhdl);
    pciio_error_register(conn, 0, 0);
    pciio_intr_disconnect(soft->ps_intr);
    pciio_intr_free(soft->ps_intr);
    phfree(soft->ps_pollhead);
/*    if (soft->ps_ctl_dmamap)
        pciio_dmamap_free(soft->ps_ctl_dmamap);
    if (soft->ps_str_dmamap)
        pciio_dmamap_free(soft->ps_str_dmamap);
*/
    if (soft->ps_cmap)
        pciio_piomap_free(soft->ps_cmap);
    if (soft->ps_rmap)
        pciio_piomap_free(soft->ps_rmap);
    hwgraph_edge_remove(conn, "pciuhci", &vhdl);
    /*
     * we really need "hwgraph_dev_remove" ...
     */
    if (GRAPH_SUCCESS ==
        hwgraph_edge_remove(vhdl, EDGE_LBL_BLOCK, &blockv)) {
        pciuhci_soft_set(blockv, 0);
        hwgraph_vertex_destroy(blockv);
    }
    if (GRAPH_SUCCESS ==
        hwgraph_edge_remove(vhdl, EDGE_LBL_CHAR, &charv)) {
        pciuhci_soft_set(charv, 0);
        hwgraph_vertex_destroy(charv);
    }
    pciuhci_soft_set(vhdl, 0);
    hwgraph_vertex_destroy(vhdl);
    DEL(soft);
    return 0;
}


/*
 *    pciuhci_reloadme: utility function used indirectly
 *      by pciuhci_init, via pciio_iterate, to "reconnect"
 *      each connection point when the driver has been
 *      reloaded.
 */
static void
pciuhci_reloadme(vertex_hdl_t conn)
{
    vertex_hdl_t            vhdl;
    pciuhci_soft_t            soft;
    if (GRAPH_SUCCESS !=
        hwgraph_traverse(conn, "pciuhci", &vhdl))
        return;
    soft = (pciuhci_soft_t)device_info_get(vhdl);
    /*
     * Reconnect our error and interrupt handlers
     */
    pciio_error_register(conn, pciuhci_error_handler, soft);
    pciio_intr_connect(soft->ps_intr, pciuhci_dma_intr, soft, 0);
}
/*
 *    pciuhci_unloadme: utility function used indirectly by
 *      pciuhci_unload, via pciio_iterate, to "disconnect" each
 *      connection point before the driver becomes unloaded.
 */
static void
pciuhci_unloadme(vertex_hdl_t pconn)
{
    vertex_hdl_t            vhdl;
    pciuhci_soft_t            soft;
    if (GRAPH_SUCCESS !=
        hwgraph_traverse(pconn, "pciuhci", &vhdl))
        return;
    soft = (pciuhci_soft_t)device_info_get(vhdl);
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
 *    pciuhci_open: called when a device special file is
 *      opened or when a block device is mounted.
 */
/* ARGSUSED */
int
pciuhci_open(dev_t *devp, int oflag, int otyp, cred_t *crp)
{
    vertex_hdl_t            vhdl = dev_to_vhdl(*devp);
    pciuhci_soft_t            soft = (pciuhci_soft_t)device_info_get(vhdl);
    pciuhci_regs_t            regs = soft->ps_regs;
    printf("pciuhci_open() regs=%x\n", regs);
    /*
     * BLOCK DEVICES: now would be a good time to
     * calculate the size of the device and stash it
     * away for use by pciuhci_size.
     */
    /*
     * USER ABI (64-bit): chances are, you are being
     * compiled for use in a 64-bit IRIX kernel; if
     * you use the _ioctl or _poll entry points, now
     * would be a good time to test and save the
     * user process' model so you know how to
     * interpret the user ioctl and poll requests.
     */
    if (!(pciuhci_SST_INUSE & atomicSetUint(&soft->ps_sst, pciuhci_SST_INUSE)))
        atomicAddInt(&pciuhci_inuse, 1);
    return 0;
}
/*
 *    pciuhci_close: called when a device special file
 *      is closed by a process and no other processes
 *      still have it open ("last close").
 */
/* ARGSUSED */
int
pciuhci_close(dev_t dev, int oflag, int otyp, cred_t *crp)
{
    vertex_hdl_t            vhdl = dev_to_vhdl(dev);
    pciuhci_soft_t            soft = (pciuhci_soft_t)device_info_get(vhdl);
    pciuhci_regs_t            regs = soft->ps_regs;
    printf("pciuhci_close() regs=%x\n", regs);
    atomicClearUint(&soft->ps_sst, pciuhci_SST_INUSE);
    atomicAddInt(&pciuhci_inuse, -1);
    return 0;
}
/* ====================================================================
 *          CONTROL ENTRY POINT
 */
/*
 *    pciuhci_ioctl: a user has made an ioctl request
 *      for an open character device.
 *      Arguments cmd and arg are as specified by the user;
 *      arg is probably a pointer to something in the user's
 *      address space, so you need to use copyin() to
 *      read through it and copyout() to write through it.
 */
/* ARGSUSED */
int
pciuhci_ioctl(dev_t dev, int cmd, void *arg,
            int mode, cred_t *crp, int *rvalp)
{
    vertex_hdl_t            vhdl = dev_to_vhdl(dev);
    pciuhci_soft_t            soft = (pciuhci_soft_t)device_info_get(vhdl);
    pciuhci_regs_t            regs = soft->ps_regs;
    printf("pciuhci_ioctl() regs=%x\n", regs);
    *rvalp = -1;
    return ENOTTY;          /* TeleType® is a registered trademark */
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
pciuhci_read(dev_t dev, uio_t * uiop, cred_t *crp)
{
    return physiock(pciuhci_strategy,
                    0,          /* alocate temp buffer & buf_t */
                    dev,        /* dev_t arg for strategy */
                    B_READ,     /* direction flag for buf_t */
                    pciuhci_size(dev),
                    uiop);
}
/* ARGSUSED */
int
pciuhci_write(dev_t dev, uio_t * uiop, cred_t *crp)
{
    return physiock(pciuhci_strategy,
                    0,          /* alocate temp buffer & buf_t */
                    dev,        /* dev_t arg for strategy */
                    B_WRITE,    /* direction flag for buf_t */
                    pciuhci_size(dev),
                    uiop);
}
/* ARGSUSED */
int
pciuhci_strategy(struct buf *bp)
{
    /*
     * XXX - create strategy code here.
     */
    return 0;
}
/* ====================================================================
 *          POLL ENTRY POINT
 */
int
pciuhci_poll(dev_t dev, short events, int anyyet,
           short *reventsp, struct pollhead **phpp, unsigned int *genp)
{
    vertex_hdl_t            vhdl = dev_to_vhdl(dev);
    pciuhci_soft_t            soft = (pciuhci_soft_t)device_info_get(vhdl);
    pciuhci_regs_t            regs = soft->ps_regs;
    short                   happened = 0;
    unsigned int            gen;
    printf("pciuhci_poll() regs=%x\n", regs);
    /*
     * Need to snapshot the pollhead generation number before we check
     * device state.  In many drivers a lock is used to interlock the
     * "high" and "low" portions of the driver.  In those cases we can
     * wait to do this snapshot till we're in the critical region.
     * Snapshotting it early isn't a problem since that makes the
     * snapshotted generation number a more conservative estimate of
     * what generation of  pollhead our event state report indicates.
     */
    gen = POLLGEN(soft->ps_pollhead);
    if (events & (POLLIN | POLLRDNORM))
        if (soft->ps_sst & pciuhci_SST_RX_READY)
            happened |= POLLIN | POLLRDNORM;
    if (events & POLLOUT)
        if (soft->ps_sst & pciuhci_SST_TX_READY)
            happened |= POLLOUT;
    if (soft->ps_sst & pciuhci_SST_ERROR)
        happened |= POLLERR;
    *reventsp = happened;
    if (!happened && anyyet) {
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
pciuhci_map(dev_t dev, vhandl_t *vt,
          off_t off, size_t len, uint_t prot)
{
    vertex_hdl_t            vhdl = dev_to_vhdl(dev);
    pciuhci_soft_t            soft = (pciuhci_soft_t)device_info_get(vhdl);
    vertex_hdl_t            conn = soft->ps_conn;
    pciuhci_regs_t            regs = soft->ps_regs;
    pciio_piomap_t          amap = 0;
    caddr_t                 kaddr;
    printf("pciuhci_map() regs=%x\n", regs);
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
pciuhci_unmap(dev_t dev, vhandl_t *vt)
{
    /*
     * XXX - need to find "amap" that we used in pciuhci_map() above,
     * and if (amap) pciio_piomap_free(amap);
     */
    return (0);
}
/* ====================================================================
 *          INTERRUPT ENTRY POINTS
 *  We avoid using the standard name, since our prototype has changed.
 */
void
pciuhci_dma_intr(intr_arg_t arg)
{
    /*pciuhci_soft_t            soft = (pciuhci_soft_t) arg;
    vertex_hdl_t            vhdl = soft->ps_vhdl;
    pciuhci_regs_t            regs = soft->ps_regs; */
    cmn_err(CE_CONT, "pciuhci  dma done\n");
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
pciuhci_error_handler(void *einfo,
                    int error_code,
                    ioerror_mode_t mode,
                    ioerror_t *ioerror)
{
    pciuhci_soft_t            soft = (pciuhci_soft_t) einfo;
    vertex_hdl_t            vhdl = soft->ps_vhdl;
#if DEBUG && ERROR_DEBUG
    cmn_err(CE_CONT, "%v: pciuhci_error_handler\n", vhdl);
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
 *    pciuhci_halt: called during orderly system
 *      shutdown; no other device driver call will be
 *      made after this one.
 */
void
pciuhci_halt(void)
{
    printf("pciuhci_halt()\n");
}
/*
 *    pciuhci_size: return the size of the device in
 *      "sector" units (multiples of NBPSCTR).
 */
int
pciuhci_size(dev_t dev)
{
    vertex_hdl_t            vhdl = dev_to_vhdl(dev);
    pciuhci_soft_t            soft = (pciuhci_soft_t)device_info_get(vhdl);
    return soft->ps_blocks;
}
/*
 *    pciuhci_print: used by the kernel to report an
 *      error detected on a block device.
 */
int
pciuhci_print(dev_t dev, char *str)
{
    cmn_err(CE_NOTE, "%V: %s\n", dev, str);
    return 0;
}
