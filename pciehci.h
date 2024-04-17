/*
********************************************************************
* PCI EHCI Device Driver for Irix 6.5                              *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: pciehci.h                                                  *
* Description: Defines and ioctls used by driver and user          *
*              space applications                                  *
********************************************************************
*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      01-09-2012     -Updates to trace functionalities                                        *
* BSDero      11-20-2011     -poll definitions added                                                  *
* BSDero      11-04-2011     -ioctl definitions added                                                 *
* BSDero      10-12-2011     -Initial version                                                         *
*                                                                                                     *
*******************************************************************************************************
*/
/* ioctl user interfaces */

#include "ehcireg.h"
#define EHCI_MAX_NUM_PORTS                            16
/*
#define _IOCTL(x,y)                                (int)(((x << 8) | y) & 0x0000ffff )
#define _IOCTLR( x,y,z)                            ((_IOCTL(x,y))| z << 16)
#define _IOCTLW( x,y,z)                            _IOCTLR(x,y,z)
#define _IOCTLWR( x,y,z)                           _IOCTLR(x,y,z)*/

/***************************************************************************************+

'E' for EHCI 
'U' for UHCI
'O' for OHCI
'X' for XHCI
'B' for USB
'D' for Pseudo device

'M' umass
'K' keyboard'
'P' mouse or pointer
'C' camera
'S' Scanner
'N' Network
'R' Printer
'Z' custom

****************************************************************************************/
#define IOCTL_EHCI_DRIVER_INFO                       _IOR('E', 1, EHCI_info_t)

typedef struct {
    uchar_t                     long_name[80];      /* device driver short name                      */
    uchar_t                     short_name[20];     /* device driver long name                       */
    uchar_t                     version[16];        /* device driver version                         */
    uchar_t                     short_version[8];   /* Short version string                          */
    uchar_t                     seqn[16];           /* Sequence number                               */
    uchar_t                     build_date[16];     /* Build date                                    */
    uchar_t                     reserved[128];      /* Reserved                                      */
} EHCI_info_t;


/****************************************************************************************
get ports status  (1 = device connected, 0 no device)
****************************************************************************************/
#define IOCTL_EHCI_PORTS_STATUS                       _IOR('E', 2, EHCI_ports_status_t)

typedef struct {
    uchar_t                     num_ports;
    uint32_t                    port[EHCI_MAX_NUM_PORTS];      /* port status                      */
} EHCI_ports_status_t;


/****************************************************************************************
TRACE FEATURES
Set debug level 
0 = no debug messages
1 = informational/no recoverable error messages
2 = all error/warning messages
3 = entry/exit of functions and callbacks
4 = debug, all messages showing
****************************************************************************************/
/* Entry points */ 
#define TRC_INIT                                    0x00000001  /* pfxinit and pfxreg */
#define TRC_UNLOAD                                  0x00000002  /* pfxunreg and pfxunload */
#define TRC_ATTACH                                  0x00000004  /* pfxattach */
#define TRC_DETACH                                  0x00000008  /* pfxdetach */
#define TRC_OPEN                                    0x00000010  /* pfxopen */                
#define TRC_CLOSE                                   0x00000020  /* pfxclose */
#define TRC_READ                                    0x00000040  /* pfxread */
#define TRC_WRITE                                   0x00000080  /* pfxwrite */
#define TRC_IOCTL                                   0x00000100  /* pfxioctl */
#define TRC_STRATEGY                                0x00000200  /* pfxstrategy */
#define TRC_POLL                                    0x00000400  /* pfxread */
#define TRC_MAP                                     0x00000800  /* pfxmap and pfxunmap */
#define TRC_INTR                                    0x00001000  /* pfxintr */
#define TRC_ERR                                     0x00002000  /* pfxerr */
#define TRC_HALT                                    0x00004000  /* pfxhalt */

                
/* Help Functionalities */
#define TRC_GC_DMA                                  0x00100000  /* memory lists */


#define TRC_NO_MESSAGES                             0x00000000  /* no trace/debug messages */
#define TRC_ENTRY_POINTS                            0x0000ffff  /* all entry points */
#define TRC_ALL                                     0xffffffff  /* debug all        */



#define IOCTL_EHCI_SET_DEBUG_LEVEL                  _IOW('E', 3, USB_trace_class_t)
#define IOCTL_EHCI_GET_DEBUG_LEVEL                  _IOR('E', 4, USB_trace_class_t)

typedef struct{
    uchar_t     level;
    uint64_t    class; 
}USB_trace_class_t;


/****************************************************************************************
EHCI PCI controller information
****************************************************************************************/
typedef struct {
    uchar_t                   pci_bus;
	uchar_t                   pci_slot;
	int                       pci_function;
    uint32_t                  device_id;
    uchar_t                   device_string[64];
    int                       num_ports;    
} EHCI_controller_info_t;

#define IOCTL_EHCI_CONTROLLER_INFO                  _IOR('E', 5, EHCI_controller_info_t)


#define IOCTL_EHCI_SETUP_TRANSFER                   _IO('E', 6)
#define IOCTL_EHCI_ENUMERATE                        _IO('E', 7)
#define IOCTL_EHCI_DUMP_DATA                        _IO('E', 9)
#define IOCTL_DUMMY_ENABLE                          _IO('E', 10)
#define IOCTL_DUMMY_DISABLE                         _IO('E', 11)
#define IOCTL_EHCI_USB_COMMAND                      _IO('E', 12)
typedef struct{
	uchar_t device_address;
	uchar_t control;
	uchar_t endpoint; 
	uchar_t speed;
	uint16_t hub_address;
	uint16_t port_number;
}EHCI_USB_pipe_t;/* 64 bytes length */


typedef struct{
        /* keep in sync with usbdevice_fs.h:usbdevfs_ctrltransfer */
        uchar_t  bRequestType;
        uchar_t  bRequest;
        uint16_t wValue;
        uint16_t wIndex;
        uint16_t wLength;
        uint32_t timeout;      /* in milliseconds */
        
        /* pointer to data */
        uchar_t *data;
        uint16_t dir;    /* usb command direction */
        uint32_t command_size; 
        uint32_t data_size;
}EHCI_USB_command_t;  /*252 bytes length */

typedef struct{
    EHCI_USB_pipe_t       pipe;
    EHCI_USB_command_t    usb_command;
	size_t                max_data;
	size_t                remaining_data;
	int                   rc; 
}EHCI_USB_packet_t; 




/****************************************************************************************
Poll messages and events 
PCIEHCI_PORTEVENT        
PCIEHCI_PORTCONNECT
PCIEHCI_PORTDISCONNECT

****************************************************************************************/
#define EHCI_EVENT_PORT_ACTIVITY                    0x0001
#define EHCI_EVENT_PORT_CONNECT                     0x0002
#define EHCI_EVENT_PORT_DISCONNECT                  0x0004
#define EHCI_EVENT_RX_READY                         0x0008
#define EHCI_EVENT_WX_READY                         0x0010
#define EHCI_EVENT_ERROR                            0x0020

#define IOCTL_EHCI_CHECKINFO                        _IO('E', 8)


#define IOCTL_EHCI_HC_RESET                         _IO('D', 1)

typedef uchar_t           EHCI_portnum_t;
#define IOCTL_EHCI_PORT_SUSPEND                     _IOW('D', 2, EHCI_portnum_t)
#define IOCTL_EHCI_PORT_RESET                       _IOW('D', 3, EHCI_portnum_t)

typedef struct{
    uint32_t              usb_cmd;
    uint32_t              usb_sts;
    uint32_t              usb_intr;
    uint32_t              frindex;
    uint32_t              ctrldssegment;    
	uint32_t              periodiclistbase;
	uint32_t              asynclistaddr;
	uint32_t              configured;
	uint32_t              portsc[EHCI_MAX_NUM_PORTS];
	uint32_t              num_ports;
}EHCI_regs_t; 

#define IOCTL_EHCI_USBREGS_GET                     _IOR('D', 4, EHCI_regs_t)
#define IOCTL_EHCI_USBREGS_SET                     _IOW('D', 5, EHCI_regs_t)
#define IOCTL_EHCI_HC_SUSPEND                      _IO('D', 6)
#define IOCTL_EHCI_HC_HALT                         _IO('D', 7)
#define IOCTL_EHCI_PORT_ENABLE                     _IOW('D', 8, EHCI_portnum_t)
#define IOCTL_EHCI_PORT_DISABLE                    _IOW('D', 9, EHCI_portnum_t)


/* proposed IOCTLs */
/*
  IOCTL_EHCI_DRIVER_INFO
  IOCTL_EHCI_ROOT_HUB_INFO
  IOCTL_EHCI_SET_DEBUG_CLASS
  IOCTL_EHCI_GET_DEBUG_CLASS
  IOCTL_EHCI_GET_EVENT  
  IOCTL_EHCI_SEND_RAW_URB
  IOCTL_EHCI_RECV_RAW_URB
  IOCTL_EHCI_HC_RESET
  IOCTL_EHCI_HC_SUSPEND
  IOCTL_EHCI_HC_HALT
  IOCTL_EHCI_PORT_RESET
  IOCTL_EHCI_PORT_SUSPEND
  IOCTL_EHCI_PORT_ENABLE
  IOCTL_EHCI_SETUP_ASYNC_TRANSFER
  IOCTL_EHCI_SET_CONFIGURATION

*/

