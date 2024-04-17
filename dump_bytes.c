#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void dump_uint32( uint32_t *u32, size_t size){
    int i, j;
    unsigned char *p;
    
    if( global_trace_class.level < level) 
	return(0);
	
    if((class & global_trace_class.class) == 0)
	return(0);
   
    size /= 4;
    printf("--------  ------------  ----\n");
    for( i = 0; i < size; i++){
	    
        printf("[0x%04x]  [0x%08x]  ", i, u32[i]); 
	p = (unsigned char *) &u32[i];
        for( j = 0; j < 4; j++ ){
	    if( isprint( *p)) 
	        printf("%c", *p);
	    else
                printf(".");		
        
            p++;
			
	}
        printf("\n");
    }
}

void dumphex( uint64_t class, uchar_t level, unsigned char *p, size_t size){
    unsigned int i, j, l, m;
    char *q;
	
    if( global_trace_class.level < level) 
	return(0);
	
    if((class & global_trace_class.class) == 0)
	return(0);
            	
    printf("     _0 _1 _2 _3 _4 _5 _6 _7  _8 _9 _A _B _C _D _E _F    01234567 89ABCDEF\n");
    printf("     -----------------------  -----------------------    -------- --------\n");
	
    m = l = 0;
    do{
	    
        printf("%02x   ", l);
	for( i = 0; i < 8; i++, l++){
	    if( l < size)
	        printf("%02x ", p[i]);
	    else
		printf("   ");
	}
		
	printf(" ");
	for( ; i < 16; i++, l++){
	    if( l < size)
	        printf("%02x ", p[i]);
	    else
		printf("   ");
	}
		
	printf("   ");
	for( i = 0; i < 8; i++, m++){
	    if( m < size)
		if( isprint( p[i] ))
		    printf("%c", p[i]);
		else
		    printf(".");
	    else
	        printf(" ");
	}
		
	printf(" ");
	for( ; i < 16; i++, m++){
	    if( m < size)
	        if( isprint( p[i] ))
		    printf("%c", p[i]);
		else
		    printf(".");
	    else
		printf(" ");
	}
		
	p = p + 16;
	printf("\n");
		
		
    }while( l < size);
    printf("\n");
}


int main(){
    char str[]="tres tristes tigres tragaban trigo sentados en un trigal ::!! 0123456789abcdef\n";
	char str2[256];
	int i;
	int n;
	
	for( i = 0; i < 256; i++)
	    str2[i] = i;
	dump_bytes( str, strlen( str));
	
	dump_bytes( str2, 256);
	
	dump_uint32( (uint32_t *)  str, strlen( str));
	dump_uint32( (uint32_t *)  str2, 256);
	printf("octl = %x\n", IOCTL_TEST);
	n = IOCTL_TEST;
	dump_bytes( (unsigned char *) &n, sizeof( int));
	printf("sizeof( int) = %d\n", sizeof(int));
	return(0);
}
