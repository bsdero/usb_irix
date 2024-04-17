#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>
#include <strings.h>
#include <sys/ioctl.h>
#include "usbio.h"




#define IOCTL_USB1                                  _IOCTL_('0', 32)
#define IOCTL_USB2                                  _IOCTL_R( '1', 15, char)
#define IOCTL_USB3                                  _IOCTL_W( '2', 8, int)
#define IOCTL_USB4                                  _IOCTL_WR( '3', 127, long long int)
#define IOCTL_USB5                                  _IOCTL_('A', 1)

int main(){
    char device_name[]="/hw/usbcore/usb_dev0";
    int device_fd;	
		
    printf("Opening device '%s' \n", device_name);

    device_fd = open( device_name, O_RDWR);
    if( device_fd < 0){
        perror("error opening device");
    }else{
        printf("device '%s' opened, fd=%d \n", device_name, device_fd);
    }    
    
    printf("running ioctl1\n");
    ioctl( device_fd, IOCTL_USB1, NULL);
    printf("running ioctl2\n");

    ioctl( device_fd, IOCTL_USB2, NULL);
    printf("running ioctl3\n");
    
    ioctl( device_fd, IOCTL_USB3, NULL);
    
    printf("running ioctl4\n");
    ioctl( device_fd, IOCTL_USB4, NULL);
    
    printf("running ioctl5\n");
    ioctl( device_fd, IOCTL_USB5, NULL);
    close( device_fd);
    return( 0);
}
    
    
