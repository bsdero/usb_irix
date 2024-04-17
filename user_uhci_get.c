/*
 * Use pciba to dump the registers found 
 * using base address register 0.
 *
 * See pciba(7m).
 */
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/fcntl.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <stdio.h>
/*
 * Path assumes O2000/Onyx2 PCI shoebox installed
 * in first CPU module.
 */
#define MEMPATH "/hw/unknown/pci/vendor/0x1106/device/0x3038/instance/85/base/4"
#define MEMSIZE (128)
extern int errno;
main(int argc, char *argv[])
{
        volatile u_int *word_addr;
        int     fd;
        char    *path;
        int     size, newline = 0;
        path = MEMPATH;
        size = MEMSIZE;
        fd = open(path, O_RDWR);
        if (fd < 0 ) {
                perror("open ../base/0 ");
                return errno;
        } else {
                printf("Successfully opened %s fd: %d\n", path, fd);
                printf("Trying mmap\n");
                word_addr = (unsigned int *)
                     mmap(0,size,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
                if (word_addr == (unsigned int *)-1) {
                        perror("mmap");
                } else {
                        int     i;
                        volatile int    x;
                        printf("Dumping registers \n");
                        for (i = 0; i < 32; i++){
                                x = *(volatile int *)(word_addr + i) ;
                                if (newline == 0) {
                                        printf("0x%2.2x:", i*4);
                                }
                                printf(" 0x%8.8x", x);
                                if ((++newline%4) == 0){
                                        newline = 0;
                                        printf("\n");
                                }
                        }
                }
                close (fd);
        }
        exit(0);
} 
