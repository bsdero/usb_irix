
#define htohc32(sc,val)                        (val)
#define MAX_QUEUE_HEADS                        32

typedef struct{
    list_t              queue_heads_list;  
	ehci_qh_t           *queue_heads;	
}ehci_transfer_chain_t;

int ehci_transfer_chain_init( vertex_hdl_t conn){
    list_init( conn->ehci_transfer_chain->queue_heads_list );
    conn->ehci_transfer_chain->queue_heads = NULL;	
    return( 0);
}

int ehci_transfer_chain_alloc( vertex_hdl_t conn){
    int i;
    ehci_qh_t *qh;
	dma_node_t *dma_node;
	
    dma_node = dma_alloc( conn, conn->dma_list, sizeof(ehci_qh_t) * MAX_QUEUE_HEADS, 0);
    if( dma_node == NULL){
	    printf( "ehci_transfer_chain_alloc(): error in dma_alloc, could not reserve dma_mem for queue heads\n");
	    return(-1);
	}

	conn->ehci_transfer_chain->queue_heads = (ehci_qh_t *) dma_node->mem;
    bzero( (void *)dma_node->mem, sizeof(ehci_qh_t) * MAX_QUEUE_HEADS);
 	
	/* queue heads set */
	for( i = 0; i < MAX_QUEUE_HEADS; i++){
	    qh = conn->ehci_transfer_chain->queue_heads[i];
		qh->qh_self =
		    htohc32(sc, kaddr_2_daddr(dma_node->daddr, dma_node->kaddr, (uint32_t) qh) |
		    htohc32(sc, EHCI_LINK_QH);

		qh->qh_endp =
		    htohc32(sc, EHCI_QH_SET_EPS(EHCI_QH_SPEED_HIGH));
		qh->qh_endphub =
		    htohc32(sc, EHCI_QH_SET_MULT(1));
		qh->qh_curqtd = 0;

		qh->qh_qtd.qtd_next =
		    htohc32(sc, EHCI_LINK_TERMINATE);
		qh->qh_qtd.qtd_altnext =
		    htohc32(sc, EHCI_LINK_TERMINATE);
		qh->qh_qtd.qtd_status =
		    htohc32(sc, EHCI_QTD_HALTED);
	
	}
	return(0);
}



static void ehci_dump_link( uint32_t link, int type){
	link = hc32toh(sc, link);
	printf("0x%08x", link);
	if (link & EHCI_LINK_TERMINATE)
		printf("<T>");
	else {
		printf("<");
		if (type) {
			switch (EHCI_LINK_TYPE(link)) {
			case EHCI_LINK_ITD:
				printf("ITD");
				break;
			case EHCI_LINK_QH:
				printf("QH");
				break;
			case EHCI_LINK_SITD:
				printf("SITD");
				break;
			case EHCI_LINK_FSTN:
				printf("FSTN");
				break;
			}
		}
		printf(">");
	}
}

static void ehci_dump_qtd(ehci_softc_t *sc, ehci_qtd_t *qtd){
	uint32_t s;

	printf("  next=");
	ehci_dump_link(sc, qtd->qtd_next, 0);
	printf(" altnext=");
	ehci_dump_link(sc, qtd->qtd_altnext, 0);
	printf("\n");
	s = hc32toh(sc, qtd->qtd_status);
	printf("  status=0x%08x: toggle=%d bytes=0x%x ioc=%d c_page=0x%x\n",
	    s, EHCI_QTD_GET_TOGGLE(s), EHCI_QTD_GET_BYTES(s),
	    EHCI_QTD_GET_IOC(s), EHCI_QTD_GET_C_PAGE(s));
	printf("    cerr=%d pid=%d stat=%s%s%s%s%s%s%s%s\n",
	    EHCI_QTD_GET_CERR(s), EHCI_QTD_GET_PID(s),
	    (s & EHCI_QTD_ACTIVE) ? "ACTIVE" : "NOT_ACTIVE",
	    (s & EHCI_QTD_HALTED) ? "-HALTED" : "",
	    (s & EHCI_QTD_BUFERR) ? "-BUFERR" : "",
	    (s & EHCI_QTD_BABBLE) ? "-BABBLE" : "",
	    (s & EHCI_QTD_XACTERR) ? "-XACTERR" : "",
	    (s & EHCI_QTD_MISSEDMICRO) ? "-MISSED" : "",
	    (s & EHCI_QTD_SPLITXSTATE) ? "-SPLIT" : "",
	    (s & EHCI_QTD_PINGSTATE) ? "-PING" : "");

	for (s = 0; s < 5; s++) {
		printf("  buffer[%d]=0x%08x\n", s,
		    hc32toh(sc, qtd->qtd_buffer[s]));
	}
	for (s = 0; s < 5; s++) {
		printf("  buffer_hi[%d]=0x%08x\n", s,
		    hc32toh(sc, qtd->qtd_buffer_hi[s]));
	}
}

static void ehci_dump_sqh(ehci_qh_t *qh){
	uint32_t endp;
	uint32_t endphub;

	printf("QH(%p) at 0x%08x:\n", qh, hc32toh(sc, qh->qh_self) & ~0x1F);
	printf("  link=");
	ehci_dump_link(sc, qh->qh_link, 1);
	printf("\n");
	endp = hc32toh(sc, qh->qh_endp);
	printf("  endp=0x%08x\n", endp);
	printf("    addr=0x%02x inact=%d endpt=%d eps=%d dtc=%d hrecl=%d\n",
	    EHCI_QH_GET_ADDR(endp), EHCI_QH_GET_INACT(endp),
	    EHCI_QH_GET_ENDPT(endp), EHCI_QH_GET_EPS(endp),
	    EHCI_QH_GET_DTC(endp), EHCI_QH_GET_HRECL(endp));
	printf("    mpl=0x%x ctl=%d nrl=%d\n",
	    EHCI_QH_GET_MPL(endp), EHCI_QH_GET_CTL(endp),
	    EHCI_QH_GET_NRL(endp));
	endphub = hc32toh(sc, qh->qh_endphub);
	printf("  endphub=0x%08x\n", endphub);
	printf("    smask=0x%02x cmask=0x%02x huba=0x%02x port=%d mult=%d\n",
	    EHCI_QH_GET_SMASK(endphub), EHCI_QH_GET_CMASK(endphub),
	    EHCI_QH_GET_HUBA(endphub), EHCI_QH_GET_PORT(endphub),
	    EHCI_QH_GET_MULT(endphub));
	printf("  curqtd=");
	ehci_dump_link(sc, qh->qh_curqtd, 0);
	printf("\n");
	printf("Overlay qTD:\n");
	ehci_dump_qtd(sc, (void *)&qh->qh_qtd);
}

/* initial creation of Queue Heads for USB transfers */
int ehci_transfer_chain_init( vertex_hdl_t conn, async_transfer_chain_t *atc, dma_list_t *dma_list, gc_list_t *gc_list){
    dma_node_t *dma_node;
	
    atc->qhs = NULL;
	list_init( &atc->qh_list_start );
	atc->dma_qhs = NULL;
	
	dma_qhs = dma_alloc( conn, dma_list, sizeof(ehci_qh_t) * MAX_QUEUE_HEADS, 0);
	dma_node = (dma_node_t *) dma_qhs->mem;
	bzero( (void *)dma_node, sizeof(ehci_qh_t) * MAX_QUEUE_HEADS);
		qh->qh_self =
		    htohc32(sc, buf_res.physaddr) |
		    htohc32(sc, EHCI_LINK_QH);

		qh->qh_endp =
		    htohc32(sc, EHCI_QH_SET_EPS(EHCI_QH_SPEED_HIGH));
		qh->qh_endphub =
		    htohc32(sc, EHCI_QH_SET_MULT(1));
		qh->qh_curqtd = 0;

		qh->qh_qtd.qtd_next =
		    htohc32(sc, EHCI_LINK_TERMINATE);
		qh->qh_qtd.qtd_altnext =
		    htohc32(sc, EHCI_LINK_TERMINATE);
		qh->qh_qtd.qtd_status =
		    htohc32(sc, EHCI_QTD_HALTED);

	return(0);
}

int async_transfer_chain_free( async_transfer_chain_t *atc){
    atc->qhs = NULL;
	list_init( &atc->qh_list_start );
	dma_qhs = NULL;
}


int 