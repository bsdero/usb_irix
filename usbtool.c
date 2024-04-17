/*
********************************************************************
* PCI EHCI Device Driver for Irix 6.5                              *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: usbcoreutil.c                                                 *
* Description: USB stack utility and test program                  *
*                                                                  *
*                                                                  *
********************************************************************
*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      05-07-2012     -Initial version (Parts rippen from previous tools                       *
*                                                                                                     *
*******************************************************************************************************
*/
/*

MAIN_MENU 
    X.-Exit
	M.-Display Menu
	O.-Open
	H.-Help
If device open
    1.-Close
    2.-USBcore tools
	3.-USB HCD tools
	4.-USB Device tools
	5.-Libusb tools
       	


2.-USBcore tools
    R.-Return to previous menu
	M.-Display Menu	

	1.-Polling events test
    2.-ioctl1
    3.-ioctl2
    n.-ioctl..n 
    	
3.-USB HCD tools
    R.-Return to previous menu
	M.-Display Menu	

    1.-EHCI tools
    2.-OHCI tools
    3.-XHCI tools
    4.-UHCI tools

4.-USB Device tools
    R.-Return to previous menu
	M.-Display Menu	

    1.-Keyboard
    2.-Mouse
    3.-UMass
    4.-xxxx

5.-Libusb tools    
    R.-Return to previous menu
	M.-Display Menu	
    
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




typedef int(*func_t)(void *);

typedef struct{
    char                          *caption; /* length must be 19 */
	func_t                        func; 
	char                          key;
}menu_item_t;



int display_menu(void *items1, void *items2);
int display_menu_part( void *menu_items);
int menu_size( menu_item_t *menu);


int exit_function( void *p);
int open_device(void *p);
int close_device(void *p);
int usbcore_dummy(void *p);
int display_main_menu( void *p);
int return_function( void *p);
int display_gen_menu1( void *p);
int usbcore_tools( void *p);

menu_item_t main_menu_top[]={
    "Exit usbutil       ", exit_function     , 'X',
	"Display menu       ", display_main_menu , 'M',
	"Open Device        ", open_device       , 'O',
	"Help               ", usbcore_dummy     , 'H',
	NULL, NULL, 0
};

menu_item_t main_menu_bottom[]={
    "Close device       ", close_device      , 0,
    "USBcore tools      ", usbcore_tools     , 0,
    "USB HCD tools      ", usbcore_dummy     , 0,
    "USB device tools   ", usbcore_dummy     , 0,
    "LibUSB tools       ", usbcore_dummy     , 0,
	NULL, NULL, 0
};


menu_item_t generic_menu_top[]={
    "Return to previous ", return_function   , 'R',
	"Display menu       ", display_gen_menu1 , 'M',
	NULL, NULL, 0
};

menu_item_t usbcore_menu_bottom[]={
    "Polling evetns test", usbcore_dummy     , 0,
    "IOCTL_SET_TRACE    ", usbcore_dummy     , 0,
    "IOCTL_GET_TRACE    ", usbcore_dummy     , 0,
    "IOCTL_GET_X        ", usbcore_dummy     , 0,
    "IOCTL_GET_Y        ", usbcore_dummy     , 0,
	NULL, NULL, 0
};


int device_fd = -1;

int display_menu_part( void *menu_items){
    menu_item_t *item;
    int i;
	
	item = ( menu_item_t *) menu_items;
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

int usbcore_dummy(void *p){ 
	return(0);
}

int display_main_menu( void *p){
    display_menu( main_menu_top, main_menu_bottom);
	return(0);}

int display_gen_menu1( void *p){
    display_menu( generic_menu_top, usbcore_menu_bottom);
	return(0);
}


int display_menu(void *items1, void *items2){
    printf("+-------------------------+-------------------------+-------------------------+\n");
	printf("|                    USBCORE FOR SGI IRIX 6.5 TEST UTILITY                    |\n");
    printf("+-------------------------+-------------------------+-------------------------+\n");
	printf("|                           << GENERAL  COMMANDS >>                           |\n");
    display_menu_part( items1);
	if( device_fd != -1){
        printf("+-------------------------+-------------------------+-------------------------+\n");
	    display_menu_part( items2);
    }
    printf("+-------------------------+-------------------------+-------------------------+\n");
    return(0);	
}

int exit_function( void *p){
   close_device( p);
   printf("Quitting..\n");
   exit( 0);
   
   return(0); /* should not happen */
}

int open_device( void *p){
    device_fd = 1;
	return(0);
}

int close_device( void *p){
    device_fd = -1;
	return(0);
}

int return_function( void *p){
    return( -100);
}

int run_menu(menu_item_t *items1, menu_item_t *items2){
	char s[80];
	int i, opt;
	int rc;
	
	display_menu( items1, items2);	
	do{
        
	    printf("\n Option select ('M' for menu) >> ");
	    gets( s);
	    if( isalpha( s[0])){
	        for( i = 0; items1[i].caption != NULL; i++){
			    if( items1[i].key == toupper(s[0]))
				    rc = items1[i].func( NULL);
	        }
	    }else{
            if( device_fd != -1){		
                if( isdigit(s[0])){
		            opt = atoi( s);
			        if( opt < menu_size( items2))
			            rc = items2[opt].func( NULL);
				}
			}
		}
	    
		if( rc == -100)
		    break;
	}while(1);
	return(0);
}

int usbcore_tools( void *p){
    run_menu( generic_menu_top, usbcore_menu_bottom);
}



int main(){
    run_menu( main_menu_top, main_menu_bottom);
}

