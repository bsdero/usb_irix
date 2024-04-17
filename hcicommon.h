/*
********************************************************************
* PCI EHCI Device Driver for Irix 6.5                              *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: hcicommon.h                                                *
* Description: Common definitions and values if another USB        *
*              controller is developed                             *
*                                                                  *
********************************************************************
*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      09-24-2011     Fixes for extensible xHCI standards                                      *
* BSDero      09-24-2011     Code ripped from FreeBSD for EHCI, PCI EHCI interrupts not working yet   *
* BSDero      09-17-2011     Added support for basic ehci map                                         *
* BSDero      10-04-2011     Initial version -taken from existing code base                           *
*******************************************************************************************************

*/

/************************************************************************************
 * IRIX/SGI SPECIFIC DEFINES AND MACROS                                             *
 ************************************************************************************/

/* Irix 6.5.x specific */
#define PCI_CFG_BASE(c)                             NULL
#define PCI_CFG_GET(c,b,o,t)                        pciio_config_get(c,o,sizeof(t))
#define PCI_CFG_SET(c,b,o,t,v)                      pciio_config_set(c,o,sizeof(t),v)


#define PCI_CFG_GET8(c,o)                           PCI_CFG_GET(c,0, o, uchar_t )
#define PCI_CFG_GET16(c,o)                          PCI_CFG_GET(c,0, o, __uint16_t)
#define PCI_CFG_GET32(c,o)                          PCI_CFG_GET(c,0, o, __uint32_t)
#define PCI_CFG_SET8(c,o,v)                         PCI_CFG_SET(c,0, o, uchar_t, v)
#define PCI_CFG_SET16(c,o,v)                        PCI_CFG_SET(c,0, o, __uint16_t, v)
#define PCI_CFG_SET32(c,o,v)                        PCI_CFG_SET(c,0, o, __uint32_t, v)

/* for easier port from FreeBSD/NetBSD */
#define USECDELAY(ms)                               delay(drv_usectohz(ms))
#define pci_read_config(dev,reg,nbytes)             pciio_config_get( dev, reg, nbytes)
#define pci_write_config(dev,reg,value,nbytes)      pciio_config_set( dev, reg, nbytes, value)

/* easier mem alloc and free */
#define NEW(ptr)                                   (ptr = kmem_alloc(sizeof (*(ptr)), KM_SLEEP))
#define DEL(ptr)                                   (kmem_free(ptr, sizeof (*(ptr))))

#ifndef SN
        typedef unsigned short  __uint16_t;
#endif

#define hci_soft_set(v,i)                          device_info_set((v),(void *)(i))
#define hci_soft_get(v)                            ((pciusb_soft_t)device_info_get((v)))



/***************************************************************************
 *  Device control status defines                                          *
 ***************************************************************************/
#define DEVICE_SST_RX_READY                        (0x0100)
#define DEVICE_SST_TX_READY                        (0x0200)
#define DEVICE_SST_ERROR                           (0x0400)
#define DEVICE_SST_INUSE                           (0x0800)


/***************************************************************************
 *  Device common controller description                                   *
 ***************************************************************************/
typedef struct{
    uint16_t         device_type;
#define PCIEHCI_DEV                                 0x0001
#define PCIUHCI_DEV                                 0x0002
    uint16_t         vendor_id;
    uint16_t         device_id;
    char             description[64];
}controller_desc_t;


/***************************************************************************
 *  Device common device                                                   *
 ***************************************************************************/
 #define HCI_COMMON_DATA \
    controller_desc_t             controller_desc; \
    vertex_hdl_t                  ps_conn; \
    vertex_hdl_t                  ps_vhdl; \
    vertex_hdl_t                  ps_blockv; \
    vertex_hdl_t                  ps_charv; \
    volatile uchar_t              ps_cfg; \
    pciehci_regs_t                ps_regs; \
    pciio_piomap_t                ps_cmap; \
    pciio_piomap_t                ps_rmap; \
    uint32_t                      ps_sst; \
    uint32_t                      ps_event; \
    pciio_intr_t                  ps_intr; \
    pciio_dmamap_t                ps_ctl_dmamap; \
    pciio_dmamap_t                ps_str_dmamap; \
    struct pollhead               *ps_pollhead; \
    int                           ps_blocks; \
    gc_list_t                     gc_list; \
    dma_list_t                    dma_list; \
    dma_node_t                    *dma_node;









