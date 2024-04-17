/*
********************************************************************
* USB stack and host controller driver for SGI IRIX 6.5            *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: usbhc.h                                                    *
* Description: USB data structures used by host controller         *
*              drivers, device drivers and user apps               *
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

#ifndef _USBHC_H
#define _USBHC_H_

#include <sys/sema.h>
#include "usb.h"
#include "usbioctl.h"


/* globals for all modules */
#define USECDELAY(ms)                               delay(drv_usectohz(ms))


/* Information on a USB end point. */
struct usb_pipe {
    struct usb_s *cntl;
    uint64_t path;
    uint8_t type;
    uint8_t ep;
    uint8_t devaddr;
    uint8_t speed;
    uint16_t maxpacket;
    uint8_t tt_devaddr;
    uint8_t tt_port;
};
typedef struct usb_pipe                             usb_pipe_t;

/* Common information for usb controllers. */
struct usb_s {
    struct usb_pipe *defaultpipe;
    mutex_t resetlock;
    int busid;
    uint16_t bdf;
    uint8_t type;
    uint8_t maxaddr;
};
typedef struct usb_s                                usb_s_t;

/* Information for enumerating USB hubs */
struct usbhub_s {
    struct usbhub_op_s *op;
    struct usb_pipe *pipe;
    struct usb_s *cntl;
    mutex_t lock;
    uint32_t powerwait;
    uint32_t port;
    uint32_t threads;
    uint32_t portcount;
    uint32_t devcount;
};
typedef struct usbhub_s                             usbhub_t;

/* Hub callback (32bit) info*/
struct usbhub_op_s {
    int (*detect)(struct usbhub_s *hub, uint32_t port);
    int (*reset)(struct usbhub_s *hub, uint32_t port);
    void (*disconnect)(struct usbhub_s *hub, uint32_t port);
};


#ifdef _SGIO2_
 #define EREAD1(sc, a)                               pciio_pio_read8(  (uint8_t *)  ((sc)->pci_io_caps) + a )
 #define EREAD2(sc, a)                               pciio_pio_read16( (uint16_t *) ((sc)->pci_io_caps) + (a>>1) )
 #define EREAD4(sc, a)                               pciio_pio_read32( (uint32_t *) ((sc)->pci_io_caps) + (a>>2) )
 #define EWRITE1(sc, a, x)                           pciio_pio_write8(  x, (uint8_t *)  ((sc)->pci_io_caps) + a  )
 #define EWRITE2(sc, a, x)                           pciio_pio_write16( x, (uint16_t *) ((sc)->pci_io_caps) + (a>>1)  )
 #define EWRITE4(sc, a, x)                           pciio_pio_write32( x, (uint32_t *) ((sc)->pci_io_caps) + (a>>2)  )
 #define EOREAD1(sc, a)                              pciio_pio_read8(   (uint8_t *)  ((sc)->pci_io_caps) + a + (sc)->sc_offs )
 #define EOREAD2(sc, a)                              pciio_pio_read16( (uint16_t *)  ((sc)->pci_io_caps) + ((a + (sc)->sc_offs) >> 1) )
 #define EOREAD4(sc, a)                              pciio_pio_read32( (uint32_t *)  ((sc)->pci_io_caps) + ((a + (sc)->sc_offs) >> 2) )
 #define EOWRITE1(sc, a, x)                          pciio_pio_write8(  x, (uint8_t *)  ((sc)->pci_io_caps) + a + (sc)->sc_offs )
 #define EOWRITE2(sc, a, x)                          pciio_pio_write16( x, (uint16_t *) ((sc)->pci_io_caps) + ((a + (sc)->sc_offs)>>1) )
 #define EOWRITE4(sc, a, x)                          pciio_pio_write32( x, (uint32_t *) ((sc)->pci_io_caps) + ((a + (sc)->sc_offs)>>2) )
#else
 #define EREAD1(sc, a)                               ( (uint8_t )   *(  ((sc)->pci_io_caps) + a ))
 #define EREAD2(sc, a)                               ( (uint16_t)   *( (uint16_t *) ((sc)->pci_io_caps) + (a>>1) ))
 #define EREAD4(sc, a)                               ( (uint32_t)   *( (uint32_t *) ((sc)->pci_io_caps) + (a>>2) ))
 #define EWRITE1(sc, a, x)                           ( *(    (uint8_t  *)  ((sc)->pci_io_caps) + a       ) = x)
 #define EWRITE2(sc, a, x)                           ( *(    (uint16_t *)  ((sc)->pci_io_caps) + (a>>1)  ) = x)
 #define EWRITE4(sc, a, x)                           ( *(    (uint32_t *)  ((sc)->pci_io_caps) + (a>>2)  ) = x)
 #define EOREAD1(sc, a)                              ( (uint8_t )   *(  ((sc)->pci_io_caps) + a + (sc)->sc_offs ))
 #define EOREAD2(sc, a)                              ( (uint16_t )  *( (uint16_t *) ((sc)->pci_io_caps) + ((a + (sc)->sc_offs)>>1) ))   
 #define EOREAD4(sc, a)                              ( (uint32_t)   *( (uint32_t *) ((sc)->pci_io_caps) + ((a + (sc)->sc_offs)>>2) ))   
 #define EOWRITE1(sc, a, x)                          ( *(    (uint8_t  *)  ((sc)->pci_io_caps) + a + (sc)->sc_offs ) = x) 
 #define EOWRITE2(sc, a, x)                          ( *(    (uint16_t *)  ((sc)->pci_io_caps) + ((a + (sc)->sc_offs)>>1) ) = x)
 #define EOWRITE4(sc, a, x)                          ( *(    (uint32_t *)  ((sc)->pci_io_caps) + ((a + (sc)->sc_offs)>>2) ) = x)
#endif

#define PCI_CFG_GET(c,b,o,t)                        pciio_config_get(c,o,sizeof(t))
#define PCI_CFG_SET(c,b,o,t,v)                      pciio_config_set(c,o,sizeof(t),v)


#define PCI_CFG_GET8(c,o)                           PCI_CFG_GET(c,0, o, uchar_t )
#define PCI_CFG_GET16(c,o)                          PCI_CFG_GET(c,0, o, uint16_t)
#define PCI_CFG_GET32(c,o)                          PCI_CFG_GET(c,0, o, uint32_t)
#define PCI_CFG_SET8(c,o,v)                         PCI_CFG_SET(c,0, o, uchar_t, v)
#define PCI_CFG_SET16(c,o,v)                        PCI_CFG_SET(c,0, o, uint16_t, v)
#define PCI_CFG_SET32(c,o,v)                        PCI_CFG_SET(c,0, o, uint32_t, v)

/*
*******************************************************************************************************
* USB module header type                                                                              *
*******************************************************************************************************
*/
typedef struct{
    uint32_t                    module_id;
    char                        short_description[12];
    char                        long_description[64];
    char                        module_name[32];
    uint32_t                    type;
    char                        hardware_description[64];
    uint32_t                    device_id;
    uint32_t                    vendor_id;
    uint32_t                    class_id;
    uint32_t                    interface_id;
    uint32_t                    info_size;
    void                        *soft;              /* reserved                                     */
}module_header_t;


typedef struct{
	module_header_t                                   module_header;
    int (*hcd_reset)                                  (void *);
    int (*hcd_start)                                  (void *);    
    int (*hcd_init)                                   (void *);    
    int (*hcd_stop)                                   (void *);    
    int (*hcd_shutdown)                               (void *);    
    int (*hcd_suspend)                                (void *);    
    int (*hcd_resume)                                 (void *);
    int (*hcd_status)                                 (void *); 
	void (*hcd_free_pipe)                             (void *, struct usb_pipe *);
    struct usb_pipe *(*hcd_alloc_default_control_pipe)(void *, struct usb_pipe *);
    struct usb_pipe *(*hcd_alloc_bulk_pipe)           (void *, struct usb_pipe *, struct usb_endpoint_descriptor *);
    int (*hcd_send_control)                           (void *, struct usb_pipe *, int, void *, int, void *, int);
    int (*hcd_usb_send_bulk)                          (void *, struct usb_pipe *, int, void *, int);
    int (*hcd_alloc_intr_pipe)                        (void *, struct usb_pipe *, struct usb_endpoint_descriptor *);
    int (*hcd_usb_poll_intr)                          (void *, struct usb_pipe *, void *);
    int (*hcd_ioctl)                                  (void *, int, void *);
    int (*hcd_set_trace_level)                        (void *, void *);
    uint32_t                                          flags; 
}hcd_methods_t;
/*
*******************************************************************************************************
* Host Controller ioctls                                                                              *
*******************************************************************************************************
*/


/*
*******************************************************************************************************
* USBcore Instance Type                                                                               *
*******************************************************************************************************
*/
typedef int( *USB_func_t)( void *);
typedef int( *USB_ioctl_func_t)(void *, void*);
typedef int( *USBCORE_event_func_t)(void *, int, void *);
typedef struct{
	module_header_t         module_header;
    vertex_hdl_t            conn;
    vertex_hdl_t            masterv;
    vertex_hdl_t            usbdaemon;
    vertex_hdl_t            usbcore;
    int                     mode;
    USB_func_t              register_hcd_driver; 
    USB_func_t              unregister_hcd_driver; 
    USBCORE_event_func_t    event_func;
	hcd_methods_t           hcd_methods[4];
}usbcore_instance_t;

#endif
