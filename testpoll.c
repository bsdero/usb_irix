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
    EHCI_info_t info;
	EHCI_ports_status_t ports;
	int i;
    struct pollfd fds;
	
    fd = open( "/hw/module/001c01/Ibrick/xtalk/15/pci/2c/pciehci/char", O_RDWR);
    if( fd < 0){
          perror("error..\n");
                  return(1);
    }
/*
    ioctl( fd, IOCTL_EHCI_DRIVER_INFO, &info);
	printf("Driver Name: %s\n", info.short_name);
	printf("Driver Description: %s\n", info.long_name);
	printf("Driver Version: %s\n", info.version);
	printf("Driver Short Version: %s\n", info.short_version);
	printf("Driver Seq Number: %s\n", info.seqn);
	printf("Driver Build Date: %s\n", info.build_date);
	

    ioctl( fd, IOCTL_EHCI_PORTS_INFO, &ports);
    printf("Number of ports=%d, values:\n", ports.num_ports);	
    for( i = 0; i < ports.num_ports; i++)
         printf(" -Port %d, status = %d\n", i + 1, ports.port[i]);
	
  */  
    fds.fd = fd;
    fds.events = POLLIN;
    
    do{
      i = poll( &fds, 1, -1);
      printf( "something happened!! = %i %i\n", i, fds.revents);
      if( fds.revents & EHCI_EVENT_PORT_ACTIVITY)
          printf("Port activity\n");
      if( fds.revents & EHCI_EVENT_PORT_CONNECT)
          printf("Port connected\n");
      if( fds.revents & EHCI_EVENT_PORT_DISCONNECT)
          printf("Port disconnected\n");

      printf("\n"); 
      /*ioctl( fd, IOCTL_EHCI_EVENT_RECEIVED, NULL);    */
      fflush( stdout);    
    }while(1);        
    close(fd);



    return(0);
}
