int main(){
    list_t list, *tmp, *i, *node;
	int *m, *n, *o, *p;  
	int x;
	 
    gc_list_init( &list);
	gc_list_dump( &list);

	for( x = 0;x < 5; x++){
	     m = (int *)gc_malloc( &list, sizeof(int));
	    *m = x;
	}
	

	n = (int *)gc_malloc( &list, sizeof(int));
	*n = 5;


	for( x = 6;x < 10; x++){
	     m = (int *)gc_malloc( &list, sizeof(int));
	    *m = x;
	}
	
	gc_list_dump( &list);
	
	list_for_each_safe( node, tmp, &list){
        p = gc_get_data( (gc_node_t *) node);
		printf("node: %i\r\n", *p);
	}
	
	gc_mark( m);
	gc_mark( n);
	gc_list_dump( &list);

    gc_sweep( &list);
	gc_list_dump( &list);
    gc_list_destroy( &list);
	gc_list_dump( &list);	
	
	printf( "str=%s\n", gc_strdup( &list, "holas12345"));
	gc_list_dump( &list);
    gc_list_destroy( &list);
	gc_list_dump( &list);	
	return(0);
}
