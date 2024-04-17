/*
********************************************************************
* USB stack and host controller driver for SGI IRIX 6.5            *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: usbioctl_funcs.c                                           *
* Description: USB core kernel driver functions                    *
*                                                                  *
********************************************************************
*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      04-23-2012     -Initial version                                                         *
*                                                                                                     *
*******************************************************************************************************
*/

int ioctl_usb_get_driver_info( void *hcd, void *arg);
int ioctl_usb_not_implemented( void *hcd, void *arg);
int ioctl_no_func( void *hcd, void *arg);
int ioctl_set_debug_level( void *hcd, void *arg);
int ioctl_get_debug_level( void *hcd, void *arg);

int ioctl_usb_get_driver_info( void *hcd, void *arg){ 
    USB_driver_info_t *info = ( USB_driver_info_t *) arg;
    
    printf("ioctl_usb_get_driver_info() start\n");
    strcpy( info->long_name,     USBCORE_DRV_LONG_NAME);
    strcpy( info->short_name,    USBCORE_DRV_SHORT_NAME);
    strcpy( info->version,       USBCORE_DRV_VERSION);        	
    strcpy( info->short_version, USBCORE_DRV_SHORT_VERSION);
    strcpy( info->seqn,          USBCORE_DRV_SEQ);
    strcpy( info->build_date,    USBCORE_DRV_BUILD_DATE);        	
    info->type = USB_DRIVER_IS_CORE;
    printf("ioctl_usb_get_driver_info() end\n");    
    return( 0);	
}



int ioctl_usb_not_implemented( void *hcd, void *arg){
	printf("ioctl_usb_not_implemented() start\n");
	printf("Not implemented\n");
	printf("ioctl_usb_not_implemented() end\n");
	return( 0);
}



int ioctl_no_func( void *hcd, void *arg){
	printf("ioctl not exist\n");
	
    return(0);
}



int ioctl_set_debug_level( void *hcd, void *arg){
	usbcore_instance_t *soft = (usbcore_instance_t *) hcd;
	USB_trace_class_t *trace_class = (USB_trace_class_t *) arg;
	void *hcd_soft; 
	
	
	int i;
	
	
	printf("ioctl_set_debug_level() start\n");
	
    global_trace_class.class = trace_class->class;
    global_trace_class.level = trace_class->level;
    printf("propagation to hcd drivers: \n");    
    for( i = 0 ; i < hcd_methods_num; i++){
        hcd_methods_t[i].hcd_set_trace_level( 
            hcd_methods_t[i].module_header.soft,  (void *) &global_trace_class);
    }
    
	printf("ioctl_set_debug_level() end\n");
	return(0);
} 



int ioctl_get_debug_level( void *hcd, void *arg){
	USB_trace_class_t *trace_class = (USB_trace_class_t *) arg;
	printf("ioctl_get_debug_level() start\n");
	
    trace_class->class = global_trace_class.class;
    trace_class->level = global_trace_class.level;
    
	printf("ioctl_get_debug_level() end\n");
	
	return(0);
} 


