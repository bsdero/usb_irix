#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <stropts.h>
#include <poll.h>
#include "pciehci.h"

int main(){

    int fd;

	
    fd = open( "/hw/module/001c01/Ibrick/xtalk/15/pci/2c/pciehci/char", O_RDWR);
    if( fd < 0){
          perror("error..\n");
                  return(1);
    }

    ioctl( fd, IOCTL_EHCI_SETUP_TRANSFER, NULL);
        
    close(fd);



    return(0);
}
