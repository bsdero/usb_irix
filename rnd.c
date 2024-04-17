#include <stdio.h>
#include <stdlib.h>

int main(){

#define USB_DEVICES_PATH                            "usb"
#define USB_DEVICES_FULL_PATH                       "/hw/" USB_DEVICES_PATH

    int seed0 = 123;
    int seed1 = 0;
    int seed2 = 23;  
    int max = 0xff; 
    unsigned int a, b, c; 
    unsigned int ac;

    unsigned int rc; 

    a = 0x0005; 
    b = 0xdeec; 
    c = 0xe66d; 

    ac = 0x0b;

    seed0 = ( (( seed1 * c) >> 16) + (( seed2 * b) >> 16) + (seed0 * c) + (seed1 *b) + (seed2 * a));
    seed1 = ( (( seed2 * c) >> 16) + ( seed1 * c) + ( seed2 * b));
    seed2 = ( seed2 * c) + ac;

    seed1 += seed2 >> 16; 
    seed0 += seed1 >> 16;
    
    seed0 &= 0xffff;
    seed1 &= 0xffff;
    seed2 &= 0xffff;

    rc = (seed0 << 16) | seed1;

    if( max > 0)
       rc = rc % max;
     
    printf("rc = %d, '%s'\n", rc, USB_DEVICES_FULL_PATH);

}
