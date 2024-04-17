/**********************************************************************
*              Copyright (C) 1990-1993, Silicon Graphics, Inc.        *
* These coded instructions, statements, and computer programs contain *
* unpublished proprietary information of Silicon Graphics, Inc., and  *
* are protected by Federal copyright law.  They may not be disclosed  *
* to third parties or copied or duplicated in any form, in whole or   *
* in part, without prior written consent of Silicon Graphics, Inc.    *
**********************************************************************/

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

/***************************************************************************
 *   Specific driver flags building configuration                          *
 ***************************************************************************/
#define VENDOR_ID_NUM    0x00
#define DEVICE_ID_NUM    0x00


#define NEW(ptr)        (ptr = kmem_alloc(sizeof (*(ptr)), KM_SLEEP))
#define DEL(ptr)        (kmem_free(ptr, sizeof (*(ptr))))
/*
 *    usbcore: a generic device driver for a generic PCI device.
 */
int         usbcore_devflag = D_MP;
char        *usbcore_mversion = M_VERSION;  /* for loadable modules */
int         usbcore_inuse = 0;     /* number of "usbcore" devices open */
/*
 *    All registers on the Sample PCIIO Client
 *      device are 32 bits wide.
 */
typedef __uint32_t      usbcore_reg_t;
typedef volatile struct usbcore_regs_s *usbcore_regs_t; /* dev registers */
typedef struct usbcore_soft_s *usbcore_soft_t;         /* software state */
/*
 *    usbcore_regs: layout of device registers
 *      Our device config registers are, of course, at
 *      the base of our assigned CFG space.
 *      Our sample device registers are in the PCI area
 *      decoded by the device's first BASE_ADDR window.
 */
struct usbcore_regs_s {
    usbcore_reg_t             pr_control;
    usbcore_reg_t             pr_status;
};


struct usbcore_soft_s {
    vertex_hdl_t      ps_conn;    /* connection for pci services */
    vertex_hdl_t      ps_vhdl;    /* backpointer to device vertex */
    vertex_hdl_t      ps_blockv;  /* backpointer to block vertex */
    vertex_hdl_t      ps_charv;   /* backpointer to char vertex */
    volatile uchar_t  *ps_cfg;     /* cached ptr to my config regs */
    usbcore_regs_t      ps_regs;    /* cached ptr to my regs */
    pciio_piomap_t    ps_cmap;    /* piomap (if any) for ps_cfg */
    pciio_piomap_t    ps_rmap;    /* piomap (if any) for ps_regs */
    unsigned          ps_sst;     /* driver "software state" */
#define usbcore_SST_RX_READY      (0x0001)
#define usbcore_SST_TX_READY      (0x0002)
#define usbcore_SST_ERROR         (0x0004)
#define usbcore_SST_INUSE         (0x8000)
    pciio_intr_t      ps_intr;       /* pciio intr for INTA and INTB */
    pciio_dmamap_t    ps_ctl_dmamap; /* control channel dma mapping */
    pciio_dmamap_t    ps_str_dmamap; /* stream channel dma mapping */
    struct pollhead   *ps_pollhead;  /* for poll() */
    int               ps_blocks;  /* block dev size in NBPSCTR blocks */
};
#define usbcore_soft_set(v,i)     device_info_set((v),(void *)(i))
#define usbcore_soft_get(v)       ((usbcore_soft_t)device_info_get((v)))
/*=====================================================================
 *          FUNCTION TABLE OF CONTENTS
 */
void                  usbcore_init(void);
int                   usbcore_unload(void);
int                   usbcore_reg(void);
int                   usbcore_unreg(void);
int                   usbcore_attach(vertex_hdl_t conn);
int                   usbcore_detach(vertex_hdl_t conn);
static pciio_iter_f   usbcore_reloadme;
static pciio_iter_f   usbcore_unloadme;
int                   usbcore_open(dev_t *devp, int oflag, int otyp,
                                 cred_t *crp);
int                   usbcore_close(dev_t dev, int oflag, int otyp,
                                  cred_t *crp);
int                   usbcore_ioctl(dev_t dev, int cmd, void *arg,
                                  int mode, cred_t *crp, int *rvalp);
int                   usbcore_read(dev_t dev, uio_t * uiop, cred_t *crp);
int                   usbcore_write(dev_t dev, uio_t * uiop,cred_t *crp);
int                   usbcore_strategy(struct buf *bp);
int                   usbcore_poll(dev_t dev, short events, int anyyet,
                               short *reventsp, struct pollhead **phpp,
                               unsigned int *genp);
int                   usbcore_map(dev_t dev, vhandl_t *vt,
                                off_t off, size_t len, uint_t prot);
int                   usbcore_unmap(dev_t dev, vhandl_t *vt);
void                  usbcore_dma_intr(intr_arg_t arg);
static error_handler_f usbcore_error_handler;
void                  usbcore_halt(void);
int                   usbcore_size(dev_t dev);
int                   usbcore_print(dev_t dev, char *str);
 
/*=====================================================================
 *                  Driver Initialization
 */
/*
 *    usbcore_init: called once during system startup or
 *      when a loadable driver is loaded.
 */
void
usbcore_init(void)
{
    printf("usbcore_init()\n");
    /*
     * if we are already registered, note that this is a
     * "reload" and reconnect all the places we attached.
     */
    pciio_iterate("usbcore_", usbcore_reloadme);
}
/*
 *    usbcore_unload: if no "usbcore" is open, put us to bed
 *      and let the driver text get unloaded.
 */
int
usbcore_unload(void)
{
    if (usbcore_inuse)
        return EBUSY;
    pciio_iterate("usbcore_", usbcore_unloadme);
    return 0;
}
/*
 *    usbcore_reg: called once during system startup or
 *      when a loadable driver is loaded.
 *    NOTE: a bus provider register routine should always be
 *      called from _reg, rather than from _init. In the case
 *      of a loadable module, the devsw is not hooked up
 *      when the _init routines are called.
 */
int
usbcore_reg(void)
{
    printf("usbcore_reg()\n");
    pciio_driver_register(VENDOR_ID_NUM,
                          DEVICE_ID_NUM,
                          "usbcore_",
                          0);
    return 0;
}
/*
 *    usbcore_unreg: called when a loadable driver is unloaded.
 */
int
usbcore_unreg(void)
{
    pciio_driver_unregister("usbcore_");
    return 0;
}
/*
 *    usbcore_attach: called by the pciio infrastructure
 *      once for each vertex representing a crosstalk widget.
 *      In large configurations, it is possible for a
 *      huge number of CPUs to enter this routine all at
 *      nearly the same time, for different specific
 *      instances of the device. Attempting to give your
 *      devices sequence numbers based on the order they
 *      are found in the system is not only futile but may be
 *      dangerous as the order may differ from run to run.
 */
int
usbcore_attach(vertex_hdl_t conn)
{
    vertex_hdl_t            vhdl, blockv, charv;
    volatile uchar_t       *cfg;
    usbcore_regs_t            regs;
    usbcore_soft_t            soft;
    pciio_piomap_t          cmap = 0;
    pciio_piomap_t          rmap = 0;
    printf("usbcore_attach()\n");
    hwgraph_device_add(conn,"usbcore","usbcore_",&vhdl,&blockv,&charv);
    /*
     * Allocate a place to put per-device information for this vertex.
     * Then associate it with the vertex in the most efficient manner.
     */
    NEW(soft);
    ASSERT(soft != NULL);
    usbcore_soft_set(vhdl, soft);
    usbcore_soft_set(blockv, soft);
    usbcore_soft_set(charv, soft);
    soft->ps_conn = conn;
    soft->ps_vhdl = vhdl;
    soft->ps_blockv = blockv;
    soft->ps_charv = charv;
    /*
     * Find our PCI CONFIG registers.
     */
    cfg = (volatile uchar_t *) pciio_pio_addr
        (conn, 0,               /* device and (override) dev_info */
         PCIIO_SPACE_CFG,       /* select configuration addr space */
         0,                     /* from the start of space, */
         PCI_CFG_VEND_SPECIFIC, /* ... up to vendor specific stuff */
         &cmap,                 /* in case we needed a piomap */
         0);                    /* flag word */
    soft->ps_cfg = cfg;         /* save for later */
    soft->ps_cmap = cmap;
    printf("usbcore_attach: I can see my CFG regs at 0x%x\n", cfg);
    /*
     * Get a pointer to our DEVICE registers
     */
    regs = (usbcore_regs_t) pciio_pio_addr
        (conn, 0,               /* device and (override) dev_info */
         PCIIO_SPACE_WIN(0),    /* in my primary decode window, */
         0, sizeof(*regs),      /* base and size */
         &rmap,                 /* in case we needed a piomap */
         0);                    /* flag word */
    soft->ps_regs = regs;       /* save for later */
    soft->ps_rmap = rmap;
    printf("usbcore_attach: I can see my device regs at 0x%x\n", regs);
    /*
     * Set up our interrupt.
     * We might interrupt on INTA or INTB,
     * but route 'em both to the same function.
     */
    soft->ps_intr = pciio_intr_alloc
        (conn, 0,
         PCIIO_INTR_LINE_A |
         PCIIO_INTR_LINE_B,
         vhdl);
    pciio_intr_connect(soft->ps_intr,
                       usbcore_dma_intr, soft,(void *) 0);
    /*
     * set up our error handler.
     */
    pciio_error_register(conn, usbcore_error_handler, soft);
    /*
     * For pciio clients, *now* is the time to
     * allocate pollhead structures.
     */
    soft->ps_pollhead = phalloc(0);
    return 0;                      /* attach successsful */
}
/*
 *    usbcore_detach: called by the pciio infrastructure
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
int
usbcore_detach(vertex_hdl_t conn)
{
    vertex_hdl_t            vhdl, blockv, charv;
    usbcore_soft_t            soft;
    printf("usbcore_detach()\n");
    if (GRAPH_SUCCESS !=
        hwgraph_traverse(conn, "usbcore", &vhdl))
        return -1;
    soft = usbcore_soft_get(vhdl);
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
        pciio_piomap_free(soft->ps_rmap);
    hwgraph_edge_remove(conn, "usbcore", &vhdl);
    /*
     * we really need "hwgraph_dev_remove" ...
     */
    if (GRAPH_SUCCESS ==
        hwgraph_edge_remove(vhdl, EDGE_LBL_BLOCK, &blockv)) {
        usbcore_soft_set(blockv, 0);
        hwgraph_vertex_destroy(blockv);
    }
    if (GRAPH_SUCCESS ==
        hwgraph_edge_remove(vhdl, EDGE_LBL_CHAR, &charv)) {
        usbcore_soft_set(charv, 0);
        hwgraph_vertex_destroy(charv);
    }
    usbcore_soft_set(vhdl, 0);
    hwgraph_vertex_destroy(vhdl);
    DEL(soft);
    return 0;
}
/*
 *    usbcore_reloadme: utility function used indirectly
 *      by usbcore_init, via pciio_iterate, to "reconnect"
 *      each connection point when the driver has been
 *      reloaded.
 */
static void
usbcore_reloadme(vertex_hdl_t conn)
{
    vertex_hdl_t            vhdl;
    usbcore_soft_t            soft;
    if (GRAPH_SUCCESS !=
        hwgraph_traverse(conn, "usbcore", &vhdl))
        return;
    soft = usbcore_soft_get(vhdl);
    /*
     * Reconnect our error and interrupt handlers
     */
    pciio_error_register(conn, usbcore_error_handler, soft);
    pciio_intr_connect(soft->ps_intr, usbcore_dma_intr, soft, 0);
}
/*
 *    usbcore_unloadme: utility function used indirectly by
 *      usbcore_unload, via pciio_iterate, to "disconnect" each
 *      connection point before the driver becomes unloaded.
 */
static void
usbcore_unloadme(vertex_hdl_t pconn)
{
    vertex_hdl_t            vhdl;
    usbcore_soft_t            soft;
    if (GRAPH_SUCCESS !=
        hwgraph_traverse(pconn, "usbcore", &vhdl))
        return;
    soft = usbcore_soft_get(vhdl);
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
 *    usbcore_open: called when a device special file is
 *      opened or when a block device is mounted.
 */
/* ARGSUSED */
int
usbcore_open(dev_t *devp, int oflag, int otyp, cred_t *crp)
{
    vertex_hdl_t            vhdl = dev_to_vhdl(*devp);
    usbcore_soft_t            soft = usbcore_soft_get(vhdl);
    usbcore_regs_t            regs = soft->ps_regs;
    printf("usbcore_open() regs=%x\n", regs);
    /*
     * BLOCK DEVICES: now would be a good time to
     * calculate the size of the device and stash it
     * away for use by usbcore_size.
     */
    /*
     * USER ABI (64-bit): chances are, you are being
     * compiled for use in a 64-bit IRIX kernel; if
     * you use the _ioctl or _poll entry points, now
     * would be a good time to test and save the
     * user process' model so you know how to
     * interpret the user ioctl and poll requests.
     */
    if (!(usbcore_SST_INUSE & atomicSetUint(&soft->ps_sst, usbcore_SST_INUSE)))
        atomicAddInt(&usbcore_inuse, 1);
    return 0;
}
/*
 *    usbcore_close: called when a device special file
 *      is closed by a process and no other processes
 *      still have it open ("last close").
 */
/* ARGSUSED */
int
usbcore_close(dev_t dev, int oflag, int otyp, cred_t *crp)
{
    vertex_hdl_t            vhdl = dev_to_vhdl(dev);
    usbcore_soft_t            soft = usbcore_soft_get(vhdl);
    usbcore_regs_t            regs = soft->ps_regs;
    printf("usbcore_close() regs=%x\n", regs);
    atomicClearUint(&soft->ps_sst, usbcore_SST_INUSE);
    atomicAddInt(&usbcore_inuse, -1);
    return 0;
}
/* ====================================================================
 *          CONTROL ENTRY POINT
 */
/*
 *    usbcore_ioctl: a user has made an ioctl request
 *      for an open character device.
 *      Arguments cmd and arg are as specified by the user;
 *      arg is probably a pointer to something in the user's
 *      address space, so you need to use copyin() to
 *      read through it and copyout() to write through it.
 */
/* ARGSUSED */
int
usbcore_ioctl(dev_t dev, int cmd, void *arg,
            int mode, cred_t *crp, int *rvalp)
{
    vertex_hdl_t            vhdl = dev_to_vhdl(dev);
    usbcore_soft_t            soft = usbcore_soft_get(vhdl);
    usbcore_regs_t            regs = soft->ps_regs;
    printf("usbcore_ioctl() regs=%x\n", regs);
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
usbcore_read(dev_t dev, uio_t * uiop, cred_t *crp)
{
    return physiock(usbcore_strategy,
                    0,          /* alocate temp buffer & buf_t */
                    dev,        /* dev_t arg for strategy */
                    B_READ,     /* direction flag for buf_t */
                    usbcore_size(dev),
                    uiop);
}
/* ARGSUSED */
int
usbcore_write(dev_t dev, uio_t * uiop, cred_t *crp)
{
    return physiock(usbcore_strategy,
                    0,          /* alocate temp buffer & buf_t */
                    dev,        /* dev_t arg for strategy */
                    B_WRITE,    /* direction flag for buf_t */
                    usbcore_size(dev),
                    uiop);
}
/* ARGSUSED */
int
usbcore_strategy(struct buf *bp)
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
usbcore_poll(dev_t dev, short events, int anyyet,
           short *reventsp, struct pollhead **phpp, unsigned int *genp)
{
    vertex_hdl_t            vhdl = dev_to_vhdl(dev);
    usbcore_soft_t            soft = usbcore_soft_get(vhdl);
    usbcore_regs_t            regs = soft->ps_regs;
    short                   happened = 0;
    unsigned int            gen;
    printf("usbcore_poll() regs=%x\n", regs);
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
        if (soft->ps_sst & usbcore_SST_RX_READY)
            happened |= POLLIN | POLLRDNORM;
    if (events & POLLOUT)
        if (soft->ps_sst & usbcore_SST_TX_READY)
            happened |= POLLOUT;
    if (soft->ps_sst & usbcore_SST_ERROR)
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
usbcore_map(dev_t dev, vhandl_t *vt,
          off_t off, size_t len, uint_t prot)
{
    vertex_hdl_t            vhdl = dev_to_vhdl(dev);
    usbcore_soft_t            soft = usbcore_soft_get(vhdl);
    vertex_hdl_t            conn = soft->ps_conn;
    usbcore_regs_t            regs = soft->ps_regs;
    pciio_piomap_t          amap = 0;
    caddr_t                 kaddr;
    printf("usbcore_map() regs=%x\n", regs);
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
usbcore_unmap(dev_t dev, vhandl_t *vt)
{
    /*
     * XXX - need to find "amap" that we used in usbcore_map() above,
     * and if (amap) pciio_piomap_free(amap);
     */
    return (0);
}
/* ====================================================================
 *          INTERRUPT ENTRY POINTS
 *  We avoid using the standard name, since our prototype has changed.
 */
void
usbcore_dma_intr(intr_arg_t arg)
{
    usbcore_soft_t            soft = (usbcore_soft_t) arg;
    vertex_hdl_t            vhdl = soft->ps_vhdl;
    usbcore_regs_t            regs = soft->ps_regs;
    cmn_err(CE_CONT, "usbcore %v: dma done, regs at 0x%X\n", vhdl, regs);
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
usbcore_error_handler(void *einfo,
                    int error_code,
                    ioerror_mode_t mode,
                    ioerror_t *ioerror)
{
    usbcore_soft_t            soft = (usbcore_soft_t) einfo;
    vertex_hdl_t            vhdl = soft->ps_vhdl;
#if DEBUG && ERROR_DEBUG
    cmn_err(CE_CONT, "%v: usbcore_error_handler\n", vhdl);
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
 *    usbcore_halt: called during orderly system
 *      shutdown; no other device driver call will be
 *      made after this one.
 */
void
usbcore_halt(void)
{
    printf("usbcore_halt()\n");
}
/*
 *    usbcore_size: return the size of the device in
 *      "sector" units (multiples of NBPSCTR).
 */
int
usbcore_size(dev_t dev)
{
    vertex_hdl_t            vhdl = dev_to_vhdl(dev);
    usbcore_soft_t            soft = usbcore_soft_get(vhdl);
    return soft->ps_blocks;
}
/*
 *    usbcore_print: used by the kernel to report an
 *      error detected on a block device.
 */
int
usbcore_print(dev_t dev, char *str)
{
    cmn_err(CE_NOTE, "%V: %s\n", dev, str);
    return 0;
}
