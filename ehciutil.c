/*
********************************************************************
* PCI EHCI Device Driver for Irix 6.5                              *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: ehciutil.c                                                 *
* Description: EHCI utility and test program                       *
*                                                                  *
*                                                                  *
********************************************************************
*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      02-08-2012     -Initial version                                                         *
*                                                                                                     *
*******************************************************************************************************
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>
#include <strings.h>
#include <sys/ioctl.h>
#include "pciehci.h"



typedef int(*func_t)(void *);

typedef struct{
    char                          *caption; /* length must be 19 */
	func_t                        func; 
	char                          key;
}menu_item_t;

int device_fd = -1;


int display_menu(void *ptr);
int menu_size( menu_item_t *menu);
int exit_function( void *p);
int open_device(void *p);
int close_device(void *p);
int driver_info(void *p);
int device_info(void *p);
int get_set_ehci_registers(void *p);
int ehci_reset_controller(void *p);
int ehci_reset_port(void *p);
int ehci_enable_port(void *p);
int ehci_disable_port(void *p);
int ehci_dummy( void *p);
int ehci_enumerate( void *p);
int ehci_dump( void *p);
int ehci_dummy_disable( void *p);
int ehci_dummy_enable( void *p);
int ehci_send_usb_request( void *p);

menu_item_t menu_items1[]={
    "Exit usbutil       ", exit_function, 'X',
	"Menu options       ", display_menu , 'M',
	"Open Device        ", open_device  , 'O',
	NULL, NULL, 0
};

menu_item_t menu_items2[]={
    "Close device       ", close_device            , 0  ,
	"Driver information ", driver_info             , 0  ,
	"EHCI PCI card info ", device_info             , 0  ,
	"Get debug class    ", ehci_dummy              , 0  ,
	"Set debug class    ", ehci_dummy              , 0  ,
	"EHCI regs set/get  ", get_set_ehci_registers  , 0  ,
	"Test events        ", ehci_dummy              , 0  ,
	"EHCI reset         ", ehci_reset_controller   , 0  ,
	"EHCI suspend       ", ehci_dummy              , 0  ,
	"EHCI halt          ", ehci_dummy              , 0  ,
	"EHCI set active    ", ehci_dummy              , 0  ,
	"EHCI port reset    ", ehci_reset_port         , 0  ,
	"EHCI port on/off   ", ehci_enable_port        , 0  ,
	"EHCI enumerate     ", ehci_enumerate          , 0  ,
	"EHCI dump          ", ehci_dump               , 0  ,
	"EHCI dummi enable  ", ehci_dummy_enable       , 0  ,
	"EHCI dummi disable ", ehci_dummy_disable      , 0  ,
	"EHCI send URB      ", ehci_send_usb_request   , 0  ,
	NULL, NULL, 0
};

int ehci_dummy( void *p){
    return(0);
}

void reverse(char *s){           /* reverse string s */
    int i, j, temp;
    for (i=0, j=strlen(s)-1; i<j; i++, j--) {
        temp = s[j];
        s[j] = s[i];
        s[i] = temp;
    } 
}

void itob(unsigned int n, char *s, int bits){ /* convert int to binary string */
    int i=0;
  
    s[i++] = 'b';
    do {
        s[i++] = n % 2 + '0';
    } while ((n /= 2) != 0);
	
	for( ; i<bits; i++)
	    s[i] = '0';
	
	s[i] = '\0';
    reverse(s);
}


int exit_function( void *p){
   close_device( p);
   printf("Quitting..\n");
   exit( 0);
   
   return(0); /* should not happen */
}



int open_device(void *p){
    char device_name[256];
	
	if( device_fd != -1){
	    printf("ERROR, Device already open, close it first\n");
		return(0);
	}
	
    printf("Type the device file:[/hw/module/001c01/Ibrick/xtalk/15/pci/2c/pciehci/char] ");
	gets( device_name);
	if( strlen(device_name) == 0)
	    strcpy( device_name, "/hw/module/001c01/Ibrick/xtalk/15/pci/2c/pciehci/char");
		
    printf("Opening device '%s' \n", device_name);
    device_fd = open( device_name, O_RDWR);
    if( device_fd < 0){
        perror("error opening device");
    }else{
        printf("device '%s' opened, fd=%d \n", device_name, device_fd);
    }    
	return(0);
}


int close_device(void *p){
	if( device_fd == -1){
		return(0);
	}
    close( device_fd);
	device_fd = -1;
	printf("Device closed successfully\n");
	return(0);
}

int driver_info(void *p){
    EHCI_info_t info;
	int rc; 
	
    rc = ioctl( device_fd, IOCTL_EHCI_DRIVER_INFO, &info);
	
	printf("ioctl IOCTL_EHCI_DRIVER_INFO executed, rc=%d\n", rc);
	
	if( rc == 0){
	    printf("Driver Name: %s\n", info.short_name);
	    printf("Driver Description: %s\n", info.long_name);
	    printf("Driver Version: %s\n", info.version);
	    printf("Driver Short Version: %s\n", info.short_version);
	    printf("Driver Seq Number: %s\n", info.seqn);
	    printf("Driver Build Date: %s\n", info.build_date);	
    }
	
	return(0);
}    


int device_info(void *p){
    EHCI_controller_info_t info;
	int rc; 
	
    rc = ioctl( device_fd, IOCTL_EHCI_CONTROLLER_INFO, &info);
	
	printf("ioctl IOCTL_EHCI_CONTROLLER_INFO executed, rc=%d\n", rc);
	if( rc == 0){
	    printf("Device string: %s\n", info.device_string);
	    printf("Device PCI ID: 0x%x\n", info.device_id);
	    printf("Device ports number: %d\n", info.num_ports);
	    printf("Device PCI bus: %d\n", info.pci_bus);
	    printf("Device PCI slot: %d\n", info.pci_slot);
	    printf("Device PCI function: %d\n", info.pci_function);
    }
	return(0);	
}    

int get_set_ehci_registers(void *p){
    EHCI_regs_t EHCI_regs;
	char str[80];
	int rc;
	unsigned int i, j;
	
    printf("\nEHCI REGISTERS\n");
	rc = ioctl( device_fd, IOCTL_EHCI_USBREGS_GET, &EHCI_regs);
	printf("ioctl IOCTL_EHCI_USBREGS_GET executed, rc=%d\n", rc);
	if( rc != 0){
	    return(rc);
	}

    
    i = EHCI_regs.usb_cmd;				
	itob( i, str, 32);
    printf("USBCMD = 0x%08x, %s\n", i, str);
    if (i & EHCI_CMD_ITC_1)
        printf("  EHCI_CMD_ITC_1\n");
    if (i & EHCI_CMD_ITC_2)
        printf("  EHCI_CMD_ITC_2\n");
    if (i & EHCI_CMD_ITC_4)
        printf("  EHCI_CMD_ITC_4\n");
    if (i & EHCI_CMD_ITC_8)
        printf("  EHCI_CMD_ITC_8\n");
    if (i & EHCI_CMD_ITC_16)
        printf("  EHCI_CMD_ITC_16\n");
    if (i & EHCI_CMD_ITC_32)
        printf("  EHCI_CMD_ITC_32\n");
    if (i & EHCI_CMD_ITC_64)
        printf("  EHCI_CMD_ITC_64\n");
    if (i & EHCI_CMD_ASPME)
        printf("  EHCI_CMD_ASPME\n");
    if (i & EHCI_CMD_ASPMC)
        printf("  EHCI_CMD_ASPMC\n");
    if (i & EHCI_CMD_LHCR)
        printf("  EHCI_CMD_LHCR\n");
    if (i & EHCI_CMD_IAAD)
        printf("  EHCI_CMD_IAAD\n");
    if (i & EHCI_CMD_ASE)
        printf("  EHCI_CMD_ASE\n");
    if (i & EHCI_CMD_PSE)
        printf("  EHCI_CMD_PSE\n");
    if (i & EHCI_CMD_FLS_M)
        printf("  EHCI_CMD_FLS_M\n");
    if (i & EHCI_CMD_HCRESET)
        printf("  EHCI_CMD_HCRESET\n");
    if (i & EHCI_CMD_RS)
        printf("  EHCI_CMD_RS\n");
		
    i = EHCI_regs.usb_sts;				
	itob( i, str, 32);
    printf("USBSTS = 0x%08x, %s\n", i, str);
	 if (i & EHCI_STS_ASS)
        printf("  EHCI_STS_ASS\n");
    if (i & EHCI_STS_PSS)
        printf("  EHCI_STS_PSS\n");
    if (i & EHCI_STS_REC)
        printf("  EHCI_STS_REC\n");
    if (i & EHCI_STS_HCH)
        printf("  EHCI_STS_HCH\n");
    if (i & EHCI_STS_IAA)
        printf("  EHCI_STS_IAA\n");
    if (i & EHCI_STS_HSE)
        printf("  EHCI_STS_HSE\n");
    if (i & EHCI_STS_FLR)
        printf("  EHCI_STS_FLR\n");
    if (i & EHCI_STS_PCD)
        printf("  EHCI_STS_PCD\n");
    if (i & EHCI_STS_ERRINT)
        printf("  EHCI_STS_ERRINT\n");
    if (i & EHCI_STS_INT)
        printf("  EHCI_STS_INT\n");

    i = EHCI_regs.usb_intr;				
	itob( i, str, 32);
    printf("USBINTR = 0x%08x, %s\n", i, str);

    i = EHCI_regs.frindex;				
	itob( i, str, 32);
    printf("USBFRINDEX = 0x%08x, %s\n", i, str);

    i = EHCI_regs.ctrldssegment;				
	itob( i, str, 32);
    printf("USBCTRLDSSEGMENT = 0x%08x, %s\n", i, str);

    i = EHCI_regs.periodiclistbase;				
	itob( i, str, 32);
    printf("USBPERIODICLISTBASE = 0x%08x, %s\n", i, str);

    i = EHCI_regs.asynclistaddr;				
	itob( i, str, 32);
    printf("USBASYNCLISTADDR = 0x%08x, %s\n", i, str);
	
    i = EHCI_regs.configured;				
	itob( i, str, 32);
    printf("EHCICONFIGFLAG = 0x%08x, %s\n", i, str);

	printf("USB Num ports = %d\n", EHCI_regs.num_ports);
	
	for( i = 0; i < EHCI_regs.num_ports; i++){
	    j = EHCI_regs.portsc[i];
		itob( j, str, 32);
	    printf("   Port %d: 0x%08x, %s\n", i, j, str);
		if( j & EHCI_PS_PP)
		    printf("        Port power\n");
		if( j & EHCI_PS_PO)
		    printf("        Port owner\n");
		if( j & EHCI_PS_SUSP)
		    printf("        Port suspend\n");
		if( j & EHCI_PS_PEC)
		    printf("        Port enable/disable change\n");
		if( j & EHCI_PS_PE)
		    printf("        Port enabled/disabled\n");
		if( j & EHCI_PS_CSC)
		    printf("        Port connect status change\n");
		if( j & EHCI_PS_CS)
		    printf("        Port connect status (device connected)\n");
     		
	}
	return(0);
}

int ehci_reset_controller(void *p){
	int rc; 
		
    rc = ioctl( device_fd, IOCTL_EHCI_HC_RESET, NULL);
	printf("ioctl IOCTL_EHCI_HC_RESET executed, rc=%d\n", rc);
	return(rc);
}

int ehci_enumerate(void *p){
	int rc; 
		
    rc = ioctl( device_fd, IOCTL_EHCI_ENUMERATE, NULL);
	printf("ioctl IOCTL_EHCI_ENUMERATE executed, rc=%d\n", rc);
	return(rc);
}

int ehci_dump(void *p){
	int rc; 
		
    rc = ioctl( device_fd, IOCTL_EHCI_DUMP_DATA, NULL);
	printf("ioctl IOCTL_EHCI_DUMP_DATA executed, rc=%d\n", rc);
	return(rc);
}

int ehci_reset_port(void *p){
    EHCI_portnum_t portnum;
	int i;
	int rc; 
	char str[80];

	printf("Enter port to reset: ");
	gets( str);
	i = atoi( str);
	portnum = (uchar_t) i;
    rc = ioctl( device_fd, IOCTL_EHCI_PORT_RESET, &portnum);	
	printf("ioctl IOCTL_EHCI_PORT_RESET executed, rc=%d\n", rc);
	return(rc);
}

int ehci_enable_port(void *p){
    EHCI_portnum_t portnum;
	int i;
	int rc; 
	char str[80];

	printf("Enter port to enable/disable: ");
	gets( str);
	i = atoi( str);
	portnum = (uchar_t) i;
	printf("Enable/Disable? (e/n) :");
	gets( str);
	if( tolower(str[0]) == 'e'){
        rc = ioctl( device_fd, IOCTL_EHCI_PORT_ENABLE, &portnum);	
	    printf("ioctl IOCTL_EHCI_PORT_ENABLE executed, rc=%d\n", rc);
	}else{
        rc = ioctl( device_fd, IOCTL_EHCI_PORT_DISABLE, &portnum);	  
	    printf("ioctl IOCTL_EHCI_PORT_DISABLE executed, rc=%d\n", rc);	
	}
	return(rc);	
}

int ehci_dummy_disable( void *p){
	int rc; 
		
    rc = ioctl( device_fd, IOCTL_DUMMY_DISABLE, NULL);
	printf("ioctl IOCTL_DUMMY_DISABLE executed, rc=%d\n", rc);
	return(rc);

}

int ehci_dummy_enable( void *p){
	int rc; 
    rc = ioctl( device_fd, IOCTL_DUMMY_ENABLE, NULL);
	printf("ioctl IOCTL_DUMMY_ENABLE executed, rc=%d\n", rc);
    return(0);
}

int ehci_send_usb_request( void *p){
    int rc; 	
    EHCI_USB_packet_t packet;
	char str[80];
	char buffer[512];

	printf("Get bRequestType: ");
	gets( str);
	packet.usb_command.bRequestType = atoi( str);
    
	printf("Get bRequest: ");
	gets( str);
	packet.usb_command.bRequest = atoi( str);
    
	printf("Get wValue: ");
	gets( str);
	packet.usb_command.wValue = atoi( str);
    
	printf("Get wIndex: ");
	gets( str);
	packet.usb_command.wIndex = atoi( str);
    
	printf("Get wLength: ");
	gets( str);
	packet.usb_command.wLength = atoi( str);

	printf("Get timeout: ");
	gets( str);
	packet.usb_command.timeout = atoi( str);

    rc = ioctl( device_fd, IOCTL_EHCI_USB_COMMAND, &packet);
    
    
	printf("ioctl IOCTL_EHCI_USB_COMMAND executed, rc=%d\n", rc);
    return(0);
    
}

int display_menu_part( menu_item_t *menu_items){
    menu_item_t *item;
    int i;
	
	item = menu_items;
	i = 0;
	
	while( item[i].caption != NULL){
	    if( item[i].key == 0)
	        printf("| [%02d]%s  ", i, item[i].caption);
		else
		    printf("| [%c] %s  ", item[i].key, item[i].caption);
		i++;
		
		if( item[i].caption == NULL){
		    printf("                                                   |\n");
			break;
		}else{
  	        if( item[i].key == 0)
		        printf(" [%02d]%s  ", i, item[i].caption);
 		    else
		        printf(" [%c] %s  ", item[i].key, item[i].caption);
		}
		i++;
		
		if( item[i].caption == NULL){
		    printf("                         |\n");
			break;
		}else{
		    if( item[i].key == 0)
		        printf(" [%02d]%s |\n", i, item[i].caption);
			else
			    printf(" [%c] %s |\n", item[i].key, item[i].caption);
		}
		i++;
		
	}
	
    return(0);
}

int menu_size( menu_item_t *menu){
    int i = 0;
	menu_item_t *m = menu;
	
	for( ; m[i].caption != NULL; i++);
	return(i);
}

int display_menu(void *ptr){
    printf("+-------------------------+-------------------------+-------------------------+\n");
	printf("|              PCIEHCI DRIVER TEST UTILITY FOR SGI IRIX 6.5.30                |\n");
    printf("+-------------------------+-------------------------+-------------------------+\n");
	printf("|                           << GENERAL  COMMANDS >>                           |\n");
    display_menu_part( menu_items1);
	if( device_fd != -1){
        printf("+-------------------------+-------------------------+-------------------------+\n");
	    display_menu_part( menu_items2);
    }
    printf("+-------------------------+-------------------------+-------------------------+\n");
    return(0);	
}

int run_main_menu(){
	char s[80];
	int i, opt;
	
	display_menu( (void *) NULL);	
	do{
        
	    printf("\n Option select ('M' for menu) >> ");
	    gets( s);
	    if( isalpha( s[0])){
	        for( i = 0; menu_items1[i].caption != NULL; i++){
			    if( menu_items1[i].key == toupper(s[0]))
				    menu_items1[i].func( NULL);
	        }
	    }else{
            if( device_fd != -1){		
                if( isdigit(s[0])){
		            opt = atoi( s);
			        if( opt < menu_size( menu_items2))
			            menu_items2[opt].func( NULL);
				}
			}
		}
	   
	}while(1);
	return(0);
}

int main(){
    run_main_menu();
}
