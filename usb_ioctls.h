/*
********************************************************************
* PCI EHCI Device Driver for Irix 6.5                              *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: usb.h                                                      *
* Description: Defines and ioctls used by driver and user          *
*              space applications                                  *
********************************************************************
*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      03-07-2012     -Initial version                                                         *
*                                                                                                     *
*******************************************************************************************************
*/

/*
*******************************************************************************************************
* USB OCTL SPECS                                                                                      *
* -Arguments bigger than 256 bytes should be passed through a pointer                                 *
* -Letter specifications for ioctls:                                                                  *
*    * For USB core                                                                                   *
*      'E' for EHCI passthrough                                                                       *
*      'O' for OHCI passthrough                                                                       *
*      'U' for UHCI passthrough                                                                       *
*      'X' for XHCI passthrough                                                                       *
*      'E' for EHCI passthrough                                                                       *
*      'B' for USB driver                                                                             *
*      'D' for Pseudo device                                                                          *
*                                                                                                     *
*                                                                                                     *
*    * For USB devices                                                                                *
*      'M' for USB Massive Storage                                                                    *
*      'K' for USB Keyboard                                                                           *
*      'P' for USB Mouse or pointing device                                                           *
*      'C' for USB Camera                                                                             *
*      'S' for USB Scanners                                                                           *
*      'N' for USB Network interfaces                                                                 *
*      'R' for USB Printers                                                                           *
*      'G' for USB Game controllers                                                                   *
*      'Z' for USB Custom devices                                                                     *
*                                                                                                     *
*******************************************************************************************************
*/




/*
*******************************************************************************************************
*  IOCTL: IOCTL_USB_GET_DRIVER_INFO                                                                   *
*  Returns USB driver information                                                                     *
*******************************************************************************************************
*/
#define IOCTL_USB_GET_DRIVER_INFO                   _IOCTL_R('U', 1, USB_driver_info_t)

typedef struct {
    uchar_t                     long_name[32];      /* device driver long name                       */
    uchar_t                     short_name[16];     /* device driver short name                      */
    uchar_t                     version[16];        /* device driver version                         */
    uchar_t                     short_version[8];   /* Short version string                          */
    uchar_t                     seqn[16];           /* Sequence number                               */
    uchar_t                     build_date[16];     /* Build date                                    */
	uchar_t                     type;               /* driver type                                   */
#define USB_DRIVER_IS_HCD                           1
#define USB_DRIVER_IS_CORE                          2
#define USB_DRIVER_IS_DEVICE                        3 
    uchar_t                     reserved[23];       /* Reserved                                      */
} USB_driver_info_t;                                /* size: 128 bytes */


/*
*******************************************************************************************************
*  IOCTL: IOCTL_USB_GET_MODULES                                                                       *
*  Returns USB attached drivers information                                                           *
*******************************************************************************************************
*/
#define IOCTL_USB_GET_MODULES                       _IOCTL_R('U', 2, USB_driver_modules_info_t)
#define USB_MAX_DRIVERS_INFO_TABLE                  256

typedef struct {
    int                         num_drivers;        /* number of loaded drivers                      */
    USB_driver_info_t           *drivers_info;      /* drivers description                           */ 
} USB_driver_modules_info_t;


/*
*******************************************************************************************************
*  IOCTL: IOCTL_USB_GET_ROOT_HUB_INFO                                                                 *
*  Returns USB ROOT HUB information                                                                   *
*******************************************************************************************************
*/

#define IOCTL_USB_GET_ROOT_HUB_INFO                 _IOCTL_R('U', 3, USB_root_hub_info_t)  
  
typedef struct{
    int                         hub_index;          /* index of hub                                  */
	int                         hub_parent;         /* index of hub parent                           */
	uchar_t                     hcd_drivers;        /* HCD supported driver by this HUB              */
#define USB_UHCI   	                                0x01
#define USB_EHCI   	                                0x02
#define USB_OHCI   	                                0x04
#define USB_XHCI   	                                0x08
    uchar_t                     pci_bus;
	uchar_t                     pci_slot;
	int                         pci_function;
    uint16_t                    product_id;
	uint16_t                    vendor_id;
    uchar_t                     device_string[64];
    int                         num_ports; 
    uchar_t                     serial_number;	
	uint16_t                    address;
} USB_root_hub_info_t;




/*
*******************************************************************************************************
*  IOCTLS: IOCTL_USB_SET_DEBUG_LEVEL and IOCTL_USB_GET_DEBUG_LEVEL                                    *
*  Returns and set debug messages trace level and class                                               *
*  Set debug level                                                                                    *
*  0 = no debug messages                                                                              *
*  1 = informational/no recoverable error messages                                                    *
*  2 = all error/warning messages                                                                     *
*  3 = entry/exit of functions and callbacks                                                          *
*  4 = debug, all messages showing                                                                    *
*******************************************************************************************************
*/
/* Trace classes */
#define TRC_INIT                                    0x00000001  /* pfxinit and pfxreg                */
#define TRC_UNLOAD                                  0x00000002  /* pfxunreg and pfxunload            */
#define TRC_ATTACH                                  0x00000004  /* pfxattach                         */
#define TRC_DETACH                                  0x00000008  /* pfxdetach                         */
#define TRC_OPEN                                    0x00000010  /* pfxopen                           */                
#define TRC_CLOSE                                   0x00000020  /* pfxclose                          */
#define TRC_READ                                    0x00000040  /* pfxread                           */
#define TRC_WRITE                                   0x00000080  /* pfxwrite                          */
#define TRC_IOCTL                                   0x00000100  /* pfxioctl                          */
#define TRC_STRATEGY                                0x00000200  /* pfxstrategy                       */
#define TRC_POLL                                    0x00000400  /* pfxread                           */
#define TRC_MAP                                     0x00000800  /* pfxmap and pfxunmap               */
#define TRC_INTR                                    0x00001000  /* pfxintr                           */
#define TRC_ERR                                     0x00002000  /* pfxerr                            */
#define TRC_HALT                                    0x00004000  /* pfxhalt                           */
                
/* Help Functionalities */
#define TRC_GC                                      0x00100000  /* memory lists                      */
#define TRC_DMA                                     0x00200000  /* dma lists                         */

/* HCD drivers */
#define TRC_HCD_SET(x)                              (x << 24)
#define TRC_HCD_GET(x)                              ((x >> 24) & 0x0000000f)

#define TRC_NO_MESSAGES                             0x00000000  /* no trace/debug messages           */
#define TRC_ENTRY_POINTS                            0x0000ffff  /* all entry points                  */
#define TRC_ALL                                     0xffffffff  /* debug all                         */


/* ioctl for set and get debug class tracing */
#define IOCTL_USB_SET_DEBUG_LEVEL                   _IOCTL_W('U', 4, USB_trace_class_t)
#define IOCTL_USB_GET_DEBUG_LEVEL                   _IOCTL_R('U', 5, USB_trace_class_t)

typedef struct{
    uchar_t                     level;
    uint64_t                    class; 
}USB_trace_class_t;

/*
*******************************************************************************************************
*  IOCTL: IOCTL_USB_GET_ROOT_HUB_STATUS                                                               *
*  Returns USB ROOT HUB status                                                                        *
*******************************************************************************************************
*/
#define IOCTL_USB_GET_ROOT_HUB_STATUS               _IOCTL_R('U', 6, USB_root_hub_status_t)

typedef struct{
#define USB_MAX_ROOT_HUB_PORTS                      8
    uint16_t                    port_status[USB_MAX_ROOT_HUB_PORTS];
#define USB_PORT_CONNECTED                          0x0001
#define USB_PORT_ENABLED                            0x0002
#define USB_PORT_SUSPENDED                          0x0004

#define USB_FULL_SPEED                              0
#define USB_LOW_SPEED                               1
#define USB_HIGH_SPEED                              2
#define USB_GET_SPEED( x)                           ((x >> 4) & 0x0007)
#define USB_GET_HCD_OWNER(x)                        ((x >> 8) & 0x000F)


#define USB_STATUS_OK                               0
#define USB_STATUS_ERROR                            1
#define USB_IN_USE                                  2

	uint32_t                    hc_condition; 	    
#define USB_HC_OK                                   0x00
#define USB_HC_ERROR(x,y)                           (x & y)

}USB_root_hub_status_t;                             


/*
*******************************************************************************************************
*  IOCTL: IOCTL_USB_GET_DEVICE                                                                        *
*  Returns USB DEVICE, passing an ID to the current device                                            *
*  A value of 0 should be passed to get the first device, being the first one the root HUB            *
*  This should be used to create a connected devices map. For USB descriptors of each device,         *
*  IOCTL_USB_GET_DEVICE_DESCRIPTOR ioctl should be used instead                                       *          
*******************************************************************************************************
*/
#define IOCTL_USB_GET_DEVICE                        _IOCTL_WR('U', 7, USB_device_t)
typedef struct{
    uint16_t                    device_index;       /* [IN/OUT] device index                         */
	uint16_t                    hub_index;          /* [OUT] connected to this hub index             */
    uchar_t                     hco;                /* [OUT] host controller owner                   */
	uchar_t                     dco;                /* [OUT] device controller owner                 */
	uchar_t                     status;             /* [OUT] device status                           */
	
	uint16_t                    type;               /* next fields are OUT                           */
	uint16_t                    subtype; 
	uint16_t                    subtype2; 
	uchar_t                     id_string[48];      
	uchar_t                     serial_number;
	uint16_t                    vendor_id[48];
	uint16_t                    product_id[48];
	uint16_t                    address; 
}USB_device_t; 


IOCTL_USB_GET_DEVICES_NUM
IOCTL_USB_GET_INTERFACES_NUM
IOCTL_USB_CLAIM_INTERFACE
IOCTL_USB_RELEASE_INTERFACE
IOCTL_USB_SET_CONFIGURATION
IOCTL_USB_CLEAR_HALT
IOCTL_USB_RESET_EP
IOCTL_USB_RESET
IOCTL_USB_SUBMIT_URB




