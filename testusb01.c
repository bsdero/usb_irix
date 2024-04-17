#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>
#include <strings.h>
#include <sys/ioctl.h>
#include "usbioctl.h"

int main(){

    int fd;
    USB_driver_info_t info;
    int num;
    USB_trace_class_t class;
    int rc;

    printf("sizeof(int)=%d, sizeof(int*)=%d\n", sizeof(int), sizeof(&fd));
	
    fd = open( "/hw/usb/usbcore", O_RDWR);
    if( fd < 0){
          perror("error..\n");
                  return(1);
    }

    
    rc = ioctl( fd, IOCTL_USB_GET_DRIVER_INFO, &info);
    printf("rc = %d\n", rc);
    printf("Driver Name: %s\n", info.short_name);
    printf("Driver Description: %s\n", info.long_name);
    printf("Driver Version: %s\n", info.version);
    printf("Driver Short Version: %s\n", info.short_version);
    printf("Driver Seq Number: %s\n", info.seqn);
    printf("Driver Build Date: %s\n", info.build_date);
    printf("Driver type: %d\n", info.type);
    rc = ioctl( fd, IOCTL_USB_GET_NUM_MODULES, &num);
    printf("rc = %d\n", rc);
    class.class = 11;
    class.level = 12;    
    
    rc = ioctl( fd, IOCTL_USB_SET_DEBUG_LEVEL, &class);
    printf("rc = %d\n", rc);
    class.class = 0;
    class.level = 0;    

    rc = ioctl( fd, IOCTL_USB_GET_DEBUG_LEVEL, &class);
    printf("class=%x, level=%x\n", class.class, class.level);
    printf("rc = %d\n", rc);
     
    rc = ioctl( fd, 44, NULL);
    printf("rc = %d\n", rc);
    close(fd);



    return(0);
}
