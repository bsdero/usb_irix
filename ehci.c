/*
********************************************************************
* PCI EHCI Device Driver for Irix 6.5                              *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: ehci.c                                                     *
* Description: ehci defines, typedefs and functions                *
*                                                                  *
*                                                                  *
********************************************************************
*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      01-29-2012     -EHCI functions and definitions left out in ehci.c                       *
*                                                                                                     *
*******************************************************************************************************

WARNING!!!

EXPERIMENTAL DRIVER FOR EHCI, NOT USEFUL YET, USE IT UNDER UR OWN RISK!!!
NOR STABLE NOR ENOUGH TESTED!!
*/


#include "ehcireg.h"
#include "ehci.h"

#define htohc32(sc,val)                        (val)
#define MAX_QUEUE_HEADS                        32

/************************************************************************************
 * EHCI SPECIFIC DEFINES AND MACROS                                                 *
 ************************************************************************************/

 /* PCI configuration offset */
#define EHCI_CF_VENDOR_ID                           0x00
#define EHCI_CF_DEVICE_ID                           0x02
#define EHCI_CF_COMMAND                             0x04
#define EHCI_CF_STATUS                              0x06
#define EHCI_CF_REVISION_ID                         0x08
#define EHCI_CF_CLASS_CODE                          0x09
#define EHCI_CF_CACHE_LINE_SIZE                     0x0c
#define EHCI_CF_LATENCY_TIME                        0x0d
#define EHCI_CF_HEADER_TYPE                         0x0e
#define EHCI_CF_BIST                                0x0f
#define EHCI_CF_MMAP_IO_BASE_ADDR                   0x10
#define EHCI_CF_CIS_BASE_ADDR                       0x14
#define EHCI_CF_CARDBUS_CIS_PTR                     0x28
#define EHCI_CF_SSID                                0x2c
#define EHCI_CF_PWR_MGMT_CAPS                       0x34
#define EHCI_CF_INTERRUPT_LINE                      0x3c
#define EHCI_CF_INTERRUPT_PIN                       0x3d

/* PCI device specifics offset */
#define EHCI_DS_MISC_CTL_1                          0x40
#define EHCI_DS_MISC_CTL_2                          0x41
#define EHCI_DS_MISC_CTL_3                          0x42
#define EHCI_DS_MISC_CTL_4                          0x48
#define EHCI_DS_MISC_CTL_5                          0x49
#define EHCI_DS_MAC_INTER_TRANS_DELAY               0x4a
#define EHCI_DS_MAX_TURN_AROUND_TIME                0x4b
#define EHCI_DS_USB2_TIMEOUT_RX                     0x51
#define EHCI_DS_HI_SPEED_PPT                        0x5a
#define EHCI_DS_SERIAL_BUS_RELEASE_NUM              0x60
#define EHCI_DS_FRAME_LEN_ADJUST                    0x61
#define EHCI_DS_PORT_WAKE_CAPS                      0x62
#define EHCI_DS_USB_LEGACY_SUP_EXTD                 0x68
#define EHCI_DS_USB_LEGACY_SUP_CTL                  0x6c
#define EHCI_DS_SRAM_DIRECT_ACESS_ADDR              0x70
#define EHCI_DS_DIRECT_ACCESS_CTL                   0x73
#define EHCI_DS_DIRECT_ACCESS_DATA                  0x74
#define EHCI_DS_POWER_MGMT_CAPS                     0x80
#define EHCI_DS_POWER_MGMT_CAPS_STATS               0x84



/* defined for easier port from NetBSD/FreeBSD */

#ifdef _SGIO2_
 #define EREAD1(sc, a)                               pciio_pio_read8(  (uint8_t *)  ((sc)->pci_io_caps) + a )
 #define EREAD2(sc, a)                               pciio_pio_read16( (uint16_t *) ((sc)->pci_io_caps) + (a>>1) )
 #define EREAD4(sc, a)                               pciio_pio_read32( (uint32_t *) ((sc)->pci_io_caps) + (a>>2) )
 #define EWRITE1(sc, a, x)                           pciio_pio_write8(  x, (uint8_t *)  ((sc)->pci_io_caps) + a  )
 #define EWRITE2(sc, a, x)                           pciio_pio_write16( x, (uint16_t *) ((sc)->pci_io_caps) + (a>>1)  )
 #define EWRITE4(sc, a, x)                           pciio_pio_write32( x, (uint32_t *) ((sc)->pci_io_caps) + (a>>2)  )
 #define EOREAD1(sc, a)                              pciio_pio_read8(   (uint8_t *)  ((sc)->pci_io_caps) + a + (sc)->sc_offs )
 #define EOREAD2(sc, a)                              pciio_pio_read16( (uint16_t *)  ((sc)->pci_io_caps) + ((a + (sc)->sc_offs) >> 1) )
 #define EOREAD4(sc, a)                              pciio_pio_read32( (uint32_t *)  ((sc)->pci_io_caps) + ((a + (sc)->sc_offs) >> 2) )
 #define EOWRITE1(sc, a, x)                          pciio_pio_write8(  x, (uint8_t *)  ((sc)->pci_io_caps) + a + (sc)->sc_offs )
 #define EOWRITE2(sc, a, x)                          pciio_pio_write16( x, (uint16_t *) ((sc)->pci_io_caps) + ((a + (sc)->sc_offs)>>1) )
 #define EOWRITE4(sc, a, x)                          pciio_pio_write32( x, (uint32_t *) ((sc)->pci_io_caps) + ((a + (sc)->sc_offs)>>2) )
#else
 #define EREAD1(sc, a)                               ( (uint8_t )   *(  ((sc)->pci_io_caps) + a ))
 #define EREAD2(sc, a)                               ( (uint16_t)   *( (uint16_t *) ((sc)->pci_io_caps) + (a>>1) ))
 #define EREAD4(sc, a)                               ( (uint32_t)   *( (uint32_t *) ((sc)->pci_io_caps) + (a>>2) ))
 #define EWRITE1(sc, a, x)                           ( *(    (uint8_t  *)  ((sc)->pci_io_caps) + a       ) = x)
 #define EWRITE2(sc, a, x)                           ( *(    (uint16_t *)  ((sc)->pci_io_caps) + (a>>1)  ) = x)
 #define EWRITE4(sc, a, x)                           ( *(    (uint32_t *)  ((sc)->pci_io_caps) + (a>>2)  ) = x)
 #define EOREAD1(sc, a)                              ( (uint8_t )   *(  ((sc)->pci_io_caps) + a + (sc)->sc_offs ))
 #define EOREAD2(sc, a)                              ( (uint16_t )  *( (uint16_t *) ((sc)->pci_io_caps) + ((a + (sc)->sc_offs)>>1) ))   
 #define EOREAD4(sc, a)                              ( (uint32_t)   *( (uint32_t *) ((sc)->pci_io_caps) + ((a + (sc)->sc_offs)>>2) ))   
 #define EOWRITE1(sc, a, x)                          ( *(    (uint8_t  *)  ((sc)->pci_io_caps) + a + (sc)->sc_offs ) = x) 
 #define EOWRITE2(sc, a, x)                          ( *(    (uint16_t *)  ((sc)->pci_io_caps) + ((a + (sc)->sc_offs)>>1) ) = x)
 #define EOWRITE4(sc, a, x)                          ( *(    (uint32_t *)  ((sc)->pci_io_caps) + ((a + (sc)->sc_offs)>>2) ) = x)
#endif
#define EHCI_NUM_CONF_REGISTERS                     0x88
#define EHCI_NUM_IO_REGISTERS                       0xc0



/***************************************************************************
 *   EHCI Specific structures/data                                         *
 ***************************************************************************/


typedef uint32_t                                                pciehci_reg_t;
typedef volatile struct pciehci_regs_s                         *pciehci_regs_t;  /* dev registers */
typedef struct pciehci_soft_s                                  *pciehci_soft_t;         /* software state */


/*
 *    pciehci_regs: layout of device registers
 *      Our device config registers are, of course, at
 *      the base of our assigned CFG space.
 *      Our device registers are in the PCI area
 *      decoded by the device's first BASE_ADDR window.
 */
struct pciehci_regs_s {
    pciehci_reg_t             pr_control;
    pciehci_reg_t             pr_status;
};

typedef struct{
    list_t              queue_heads_list;  
	ehci_qh_t           *queue_heads;
	dma_node_t          *queue_heads_dma;
}ehci_transfer_chain_t;


typedef struct{
    ehci_qtd_t          qts[4];
	uint32_t            buf[64];
}ehci_transfer_control_data_t;

struct pciehci_soft_s {
    HCI_COMMON_DATA
    uchar_t                       *pci_io_caps; /* pointer to mmaped registers */
	struct                        ehci_qh *sc_async_p_last;
	struct                        ehci_qh *sc_intr_p_last[EHCI_VIRTUAL_FRAMELIST_COUNT];
	struct                        ehci_sitd *sc_isoc_fs_p_last[EHCI_VIRTUAL_FRAMELIST_COUNT];
	struct                        ehci_itd *sc_isoc_hs_p_last[EHCI_VIRTUAL_FRAMELIST_COUNT];

    uint32_t                      sc_terminate_self;/* TD short packet termination pointer */
    uint32_t                      sc_eintrs;
    uint32_t                      sc_cmd;/* shadow of cmd register during suspend */
    uint16_t                      sc_intr_stat[8];
    uint16_t                      sc_flags;/* chip specific flags */
    #define EHCI_SCFLG_SETMODE      0x0001/* set bridge mode again after init */
    #define EHCI_SCFLG_FORCESPEED   0x0002/* force speed */
    #define EHCI_SCFLG_NORESTERM    0x0004/* don't terminate reset sequence */
    #define EHCI_SCFLG_BIGEDESC     0x0008/* big-endian byte order descriptors */
    #define EHCI_SCFLG_BIGEMMIO     0x0010/* big-endian byte order MMIO */
    #define EHCI_SCFLG_TT           0x0020/* transaction translator present */
    #define EHCI_SCFLG_LOSTINTRBUG  0x0040/* workaround for VIA / ATI chipsets */
    #define EHCI_SCFLG_IAADBUG      0x0080/* workaround for nVidia chipsets */

    uint8_t                       sc_offs; /* offset to operational registers */
    uint8_t                       sc_doorbell_disable;/* set on doorbell failure */
    uint8_t                       sc_noport;
    uint8_t                       sc_addr;/* device address */
    uint8_t                       sc_conf;/* device configuration */
    uint8_t                       sc_isreset;
    uint8_t                       sc_hub_idata[8];
    toid_t                        timeout_value;
	pciio_info_t                  pciio_info_device;
    ehci_transfer_chain_t         ehci_transfer_chain;
};


static struct{
    uint32_t       device_id;
    uchar_t          *controller_description;
}ehci_descriptions[]={
    0x268c8086, "Intel 63XXESB USB 2.0 controller",
    0x523910b9, "ALi M5239 USB 2.0 controller",
    0x10227463, "AMD 8111 USB 2.0 controller",
    0x20951022, "(AMD CS5536 Geode) USB 2.0 controller",
    0x43451002, "ATI SB200 USB 2.0 controller",
    0x43731002, "ATI SB400 USB 2.0 controller",
    0x25ad8086, "Intel 6300ESB USB 2.0 controller",
    0x24cd8086, "(Intel 82801DB/L/M ICH4) USB 2.0 controller",
    0x24dd8086, "(Intel 82801EB/R ICH5) USB 2.0 controller",
    0x265c8086, "(Intel 82801FB ICH6) USB 2.0 controller",
    0x27cc8086, "(Intel 82801GB/R ICH7) USB 2.0 controller",
    0x28368086, "(Intel 82801H ICH8) USB 2.0 controller USB2-A",
    0x283a8086, "(Intel 82801H ICH8) USB 2.0 controller USB2-B",
    0x293a8086, "(Intel 82801I ICH9) USB 2.0 controller",
    0x293c8086, "(Intel 82801I ICH9) USB 2.0 controller",
    0x3a3a8086, "(Intel 82801JI ICH10) USB 2.0 controller USB-A",
    0x3a3c8086, "(Intel 82801JI ICH10) USB 2.0 controller USB-B",
    0x3b348086, "Intel PCH USB 2.0 controller USB-A",
    0x3b3c8086, "Intel PCH USB 2.0 controller USB-B",
    0x00e01033, "NEC uPD 720100 USB 2.0 controller",
    0x006810de, "NVIDIA nForce2 USB 2.0 controller",
    0x008810de, "NVIDIA nForce2 Ultra 400 USB 2.0 controller",
    0x00d810de, "NVIDIA nForce3 USB 2.0 controller",
    0x00e810de, "NVIDIA nForce3 250 USB 2.0 controller",
    0x005b10de, "NVIDIA nForce4 USB 2.0 controller",
    0x036d10de, "NVIDIA nForce MCP55 USB 2.0 controller",
    0x03f210de, "NVIDIA nForce MCP61 USB 2.0 controller",
    0x0aa610de, "NVIDIA nForce MCP79 USB 2.0 controller",
    0x0aa910de, "NVIDIA nForce MCP79 USB 2.0 controller",
    0x0aaa10de, "NVIDIA nForce MCP79 USB 2.0 controller",
    0x15621131, "Philips ISP156x USB 2.0 controller",
    0x31041106, "VIA VT6212 EHCI USB 2.0 controller",
    0 ,NULL
};



extern pciehci_soft_t          global_soft;
/*uint32_t                global_debug_level = PCIEHCI_DEBUG_ALL;*/
extern uint32_t                global_ports_status[EHCI_MAX_NUM_PORTS]; 
extern uchar_t                 global_num_ports;
extern uint32_t                global_debug_level;
extern int                     global_pciehci_inuse;

#define offset_of(st, m) \
     ((size_t) ( (char *)&((st *)(0))->m - (char *)0 ))
	 
#define kaddr_2_daddr( daddr,kaddr,ofs)               (uint32_t) (daddr + ((uint64_t)ofs - (uint64_t)kaddr))






/***************************************************************************
 *   EHCI functions declarations                                           *
 ***************************************************************************/
int ehci_reset(pciehci_soft_t sc);
int ehci_hcreset(pciehci_soft_t sc);
void ehci_init(pciehci_soft_t sc);
void ehci_detach(pciehci_soft_t sc);
int ehci_suspend(pciehci_soft_t sc);
int ehci_resume(pciehci_soft_t sc);
void ehci_shutdown(pciehci_soft_t sc);
void ehci_dump_regs(pciehci_soft_t sc);
void ehci_enumerate(pciehci_soft_t sc);
void ehci_interrupt(pciehci_soft_t sc);
void ehci_take_controller(pciehci_soft_t sc);
void ehci_dump_data(pciehci_soft_t sc);
int ehci_dummy_enable( pciehci_soft_t sc);
int ehci_dummy_disable( pciehci_soft_t sc);


/***************************************************************************
 *   EHCI functions                                                        *
 ***************************************************************************/
 
 
 /* initialize queue heads */
 int ehci_queue_heads_init( pciehci_soft_t sc){
	printf("ehci_queue_heads_init() start\n");
    list_init( &sc->ehci_transfer_chain.queue_heads_list );
    sc->ehci_transfer_chain.queue_heads = NULL;	
    sc->ehci_transfer_chain.queue_heads_dma = NULL;
    printf("ehci_queue_heads_init() end\n");
    return( 0);
}

/* alloc and fill queue heads */
int ehci_queue_heads_alloc( pciehci_soft_t sc){
    int i;
    ehci_qh_t *qh;
	dma_node_t *dma_node;
	
	printf("ehci_queue_heads_alloc() start\n");
    dma_node = dma_alloc( sc->ps_conn, &sc->dma_list, sizeof(ehci_qh_t) * MAX_QUEUE_HEADS, 0);
    if( dma_node == NULL){
	    printf( "ehci_chain_alloc(): error in dma_alloc, could not reserve dma_mem for queue heads\n");
	    return(-1);
	}
    
    sc->ehci_transfer_chain.queue_heads_dma = dma_node;
	sc->ehci_transfer_chain.queue_heads = (ehci_qh_t *) dma_node->mem;
    bzero( (void *)dma_node->mem, sizeof(ehci_qh_t) * MAX_QUEUE_HEADS);
 	
	/* queue heads set */
	for( i = 0; i < MAX_QUEUE_HEADS; i++){
	    /* all of our queue heads are dummies, so right now they're not functional. */
	    qh = &sc->ehci_transfer_chain.queue_heads[i];
		qh->qh_link = htohc32(sc, kaddr_2_daddr(dma_node->daddr, dma_node->kaddr, qh) |htohc32(sc, EHCI_LINK_QH));
		qh->qh_endp = htohc32(sc, EHCI_QH_SET_EPS(EHCI_QH_SPEED_HIGH));
		qh->qh_endphub = htohc32(sc, EHCI_QH_SET_MULT(1));
		qh->qh_curqtd = 0;
		qh->qh_qtd.qtd_next = htohc32(sc, EHCI_LINK_TERMINATE);
		qh->qh_qtd.qtd_altnext = htohc32(sc, EHCI_LINK_TERMINATE);
		qh->qh_qtd.qtd_status = htohc32(sc, EHCI_QTD_HALTED);
		qh->status = 0;
	}
	printf("ehci_queue_heads_alloc() end\n");
	return(0);
}

/* destroy queue heads */
int ehci_queue_heads_destroy( pciehci_soft_t sc){
	printf("ehci_queue_heads_destroy() start\n");
	printf("marking queue heads \n");
	USECDELAY( 1000000);
    dma_mark( sc->ehci_transfer_chain.queue_heads_dma);
    list_init( &sc->ehci_transfer_chain.queue_heads_list );
    sc->ehci_transfer_chain.queue_heads = NULL;	
	printf("freeing queue heads \n");
	USECDELAY( 1000000);
    dma_free( sc->ehci_transfer_chain.queue_heads_dma);
	printf("ehci_queue_heads_destroy() end\n");
    
    return(0);
}




int ehci_transfer_control( pciehci_soft_t sc, EHCI_USB_packet_t *packet){
    int i; 
	ehci_qh_t *qh, *qh_selected = NULL;
	ehci_qtd_t *qt, *qtds; 
	dma_node_t *dma_qtd, *dma_buf;
	uint32_t *p32;


    printf("ehci_transfer_control() start\n");
    USECDELAY(1000000);
	for( i = 0; i < MAX_QUEUE_HEADS; i++){
	    qh = &sc->ehci_transfer_chain.queue_heads[i];	    
		if( (qh->status && QH_IN_USE) == 0){
		    qh_selected = qh;
			list_add_tail( &qh->qh_list, &sc->ehci_transfer_chain.queue_heads_list);
			break;
		}
		
    }
	
	if( qh_selected == NULL){
	    printf("Could not get a free QueueHead\n");
	    return( -1);
	}
	
	printf("alloc dma for qtds \n");
	USECDELAY(1000000);	
	/* create 3 qtds */
    dma_qtd = dma_alloc( sc->ps_conn, &sc->dma_list, sizeof(ehci_qtd_t) * 3, 0);
    if( dma_qtd == NULL){
	    printf( "ehci_transfer_chain_alloc(): error in dma_alloc, could not reserve dma_mem for queue heads\n");
	    return(-2);
	}
	
	printf("alloc dma for buffer \n");
	USECDELAY(1000000);	

	/* create transference buffer (size = 512) */
	dma_buf = dma_alloc( sc->ps_conn, &sc->dma_list, 1024, 0);
    if( dma_buf == NULL){
		printf("dma_buf == NULL, calling dma_mark( qtds)\n");
		USECDELAY( 1000000);
		dma_mark( dma_qtd);
	    printf( "ehci_transfer_chain_alloc(): error in dma_alloc, could not reserve dma_mem for buffers\n");
	    return(-3);
	}
	
	

	
	qh = qh_selected;
	qh->status = QH_IN_USE & QH_ACTIVE; 
	
	
	
	
	/* Setup the first Qtd */
	qtds = (ehci_qtd_t *) dma_qtd->mem;
	printf("cleaning qtds memory \n");
	USECDELAY(1000000);	

    bzero( (void *)dma_qtd->mem, sizeof(ehci_qh_t) * 3);

	printf("cleaning buf memory \n");
	USECDELAY(1000000);	
    bzero( (void *)dma_buf->mem, 1024); 

	printf("setting up first qtd \n");
	USECDELAY(1000000);	
    qt = qtds;
    qt->qtd_next = kaddr_2_daddr(dma_qtd->daddr, dma_qtd->kaddr, (qt+1));
    qt->qtd_altnext = EHCI_LINK_TERMINATE;
    qt->qtd_status = (uint32_t) (  
        EHCI_QTD_SET_BYTES( packet->usb_command.command_size) |
        EHCI_QTD_ACTIVE |
        EHCI_QTD_IOC |
        EHCI_QTD_SET_PID( EHCI_QTD_PID_SETUP) |
        EHCI_QTD_SET_CERR( 3));
              
    qt->qtd_buffer[0] = (uint32_t) dma_buf->daddr;
    
   	printf("copy data from user space (usb command)\n");
	USECDELAY(1000000);	

    bcopy( (void *) &packet->usb_command, (caddr_t) dma_buf->kaddr, packet->usb_command.command_size);
    printf("dump of 8 bytes of kaddr (USB command dump)\n");
    p32 = (uint32_t *) dma_buf->mem;
    printf("p32[0] = 0x%x\n", p32[0]);
    printf("p32[1] = 0x%x\n", p32[1]);
    /*fill_td_buffer( qt, packet->max_data, packet->command, packet->command_size);*/
    
    
    qt++;
    
	printf("setting up second qtd \n");
	USECDELAY(1000000);	
    
    if( packet->usb_command.data_size != 0){
        qt->qtd_next = (uint32_t) kaddr_2_daddr(dma_qtd->daddr, dma_qtd->kaddr, (qt+1));
        qt->qtd_altnext = EHCI_LINK_TERMINATE;
        qt->qtd_status = (uint32_t) (  
            EHCI_QTD_SET_BYTES( packet->usb_command.command_size) |
            EHCI_QTD_ACTIVE |
            EHCI_QTD_IOC |
            EHCI_QTD_SET_PID( packet->usb_command.dir ? EHCI_QTD_PID_IN : EHCI_QTD_PID_OUT)|
            EHCI_QTD_SET_CERR( 3) |
            EHCI_QTD_SET_TOGGLE(1) );

      	printf("copy data from user space  (data)\n");
	    USECDELAY(1000000);	
            
        copyin( packet->usb_command.data, (caddr_t) dma_buf->kaddr + 512, packet->usb_command.data_size);
        qt->qtd_buffer[0] = (uint32_t) (dma_buf->daddr+512);
		/*fill_td_buffer( qt, packet->max_data, packet->data, packet->data_size);*/
		qt++;
	}
	
	printf("setting up last qtd \n");
	USECDELAY(1000000);	

	qt->qtd_next = EHCI_LINK_TERMINATE;
	qt->qtd_altnext = EHCI_LINK_TERMINATE;
	qt->qtd_status = (uint32_t) (  
	    EHCI_QH_HRECL |
	    EHCI_QTD_SET_TOGGLE(1) |
        EHCI_QTD_ACTIVE |
        EHCI_QTD_IOC |
        EHCI_QTD_SET_PID( packet->usb_command.dir ? EHCI_QTD_PID_OUT : EHCI_QTD_PID_IN)|
        EHCI_QTD_SET_CERR( 3));
        
/*                                   
	    pipe->qh.info1 = (␊
495	        (1 << QH_MULT_SHIFT) | (speed != USB_HIGHSPEED ? QH_CONTROL : 0)␊
496	        | (maxpacket << QH_MAXPACKET_SHIFT)␊
497	        | QH_TOGGLECONTROL␊
498	        | (speed << QH_SPEED_SHIFT)␊
499	        | (pipe->pipe.ep << QH_EP_SHIFT)␊
500	        | (pipe->pipe.devaddr << QH_DEVADDR_SHIFT));␊
501	    pipe->qh.info2 = ((1 << QH_MULT_SHIFT)␊
502	                      | (pipe->pipe.tt_port << QH_HUBPORT_SHIFT)␊
503	                      | (pipe->pipe.tt_devaddr << QH_HUBADDR_SHIFT));␊          
*/         

	printf("setting up queue_head \n");
	USECDELAY(1000000);	

    qh->qh_endp = (uint32_t) ( 
        EHCI_QH_SET_MULT(1) |
        EHCI_QH_DTC |
        packet->pipe.control ? EHCI_QH_CTL : 0|
        EHCI_QH_SET_MPL(packet->max_data) |  
        EHCI_QH_SPEED_FULL |
        EHCI_QH_SET_ENDPT( packet->pipe.endpoint) |
        EHCI_QH_SET_ADDR( packet->pipe.device_address));
            
          
    qh->qh_endphub = (uint32_t)(
        EHCI_QH_SET_MULT(1) |
        EHCI_QH_SET_PORT( packet->pipe.port_number) |
        EHCI_QH_SET_HUBA( packet->pipe.hub_address));
        
    printf("Waiting for USB transfer\n");
    USECDELAY(1000000);
	qh->qh_qtd.qtd_next = htohc32(sc, EHCI_LINK_TERMINATE);
	qh->qh_qtd.qtd_altnext = htohc32(sc, EHCI_LINK_TERMINATE);
	qh->qh_qtd.qtd_status = htohc32(sc, EHCI_QTD_HALTED);	
    qh->status = 0x00;
    
    
    p32 = (uint32_t *) dma_buf->mem + 512;
    printf( "data0 = 0x%x\n", p32[0]);
    printf( "data1 = 0x%x\n", p32[1]);
    printf( "data2 = 0x%x\n", p32[2]);
    printf( "data3 = 0x%x\n", p32[3]);
 /*   printf("copying out user data\n");
    USECDELAY(1000000);
    copyout( (caddr_t) dma_buf->kaddr + 512, packet->usb_command.data, packet->usb_command.data_size);
    */
    
    
    ehci_hcreset( sc);
    
    
    printf("marking dma segments\n");
    USECDELAY(1000000);

    
    dma_mark( dma_buf);
    dma_mark( dma_qtd);
    printf("sweep dma list\n");
    USECDELAY(1000000);

    dma_list_sweep( &sc->dma_list);
/*    qh->qh_link = (uint32_t) dma_qtd->daddr | EHCI_LINK_QH;*/
    return(0);
}




int ehci_reset(pciehci_soft_t sc){
    uint32_t hcr;
    int i;

	
    EOWRITE4(sc, EHCI_USBCMD, EHCI_CMD_HCRESET);
    for (i = 0; i < 100; i++) {
        USECDELAY(10000);
        hcr = EOREAD4(sc, EHCI_USBCMD) & EHCI_CMD_HCRESET;
        if (!hcr) { 
		
		    /* VIA controller has not EHCI_USBMODE register */
			printf("VIA controller jumping, sc->controller_desc.vendor_id=0x%x\n", 
			         sc->controller_desc.vendor_id);
		    if( sc->controller_desc.vendor_id != 0x1106){
                if (sc->sc_flags & (EHCI_SCFLG_SETMODE | EHCI_SCFLG_BIGEMMIO)) {
                    /*
                     * Force USBMODE as requested.  Controllers
                     * may have multiple operating modes.
                     */
                    uint32_t usbmode = EOREAD4(sc, EHCI_USBMODE);
                    if (sc->sc_flags & EHCI_SCFLG_SETMODE) {
                        usbmode = (usbmode &~ EHCI_UM_CM) | EHCI_UM_CM_HOST;
                        printf( "set host controller mode\n");
                    }

                    if (sc->sc_flags & EHCI_SCFLG_BIGEMMIO) {
                        usbmode = (usbmode &~ EHCI_UM_ES) | EHCI_UM_ES_BE;
                        printf( "set big-endian mode\n");
                    }
                    EOWRITE4(sc,  EHCI_USBMODE, usbmode);
				}
            }
            printf("hcreset success, HCRESET = 0\n");
            return(0);
        }
    }
    printf("hcreset timeout, HCRESET != 0\n");
	return(-1);
}


int ehci_hcreset(pciehci_soft_t sc){
    uint32_t hcr;
    int i;

    EOWRITE4(sc, EHCI_USBCMD, 0);/* Halt controller */
    for (i = 0; i < 100; i++) {
        USECDELAY(10000);
        hcr = EOREAD4(sc, EHCI_USBSTS) & EHCI_STS_HCH;
        if (hcr){
		    printf("Host controller halted (HCH = 1)\n");
            return(0);
		}
    }

    printf("Host controller halttimeout (HCH = 0 after one second, it didn't stop)\n");
	return( -1);
 }

 
int ehci_port_reset( pciehci_soft_t sc, int port){
    int rc, i, reset = 0;
	uint32_t v;
	
	printf("Starting port %d reset\n", port);
	rc = ehci_hcreset(sc);
	if( rc != 0) {
	    printf("got error from ehci_hcreset(), resetting port anyway\n");
	}
	
	USECDELAY(100000);/*
	rc = ehci_reset( sc);
	if( rc != 0) {
	    printf("got error from ehci_reset(), resetting port anyway\n");
	}
	
	*/
	v = EOREAD4(sc, EHCI_PORTSC(port))  & ~EHCI_PS_CLEAR;

	
	/* op reset port */
    v &= ~(EHCI_PS_PE | EHCI_PS_PR); 
	EOWRITE4(sc, EHCI_PORTSC(port), v | EHCI_PS_PR);
	USECDELAY(10000);
	EOWRITE4(sc, EHCI_PORTSC(port), v );
	for( i = 0; i < 10000;  i++){
        USECDELAY(100);
        v = EOREAD4(sc, EHCI_PORTSC(port)) ;
        if( (v & EHCI_PS_PR) == 0){
		    printf("port reset complete\n");
		    reset = 1; 
		    break;	
		}
	}
    
    if( reset == 0){ 
	    printf("port didn't reset!!! EHCI_PS_PR still enable... quitting with rc=-1\n"); 	
	    return( -1);
	}
	USECDELAY(200000);
	if( !( v & EHCI_PS_PE)){
		/* Not a high speed device, give up ownership.*/
		printf("Not a high speed device, give up ownership here\n");
    }
    
	sc->sc_isreset = 1;				
	    
	return(0);
	
	
}

void ehci_init(pciehci_soft_t sc){
    uint32_t                version;
    uint32_t                sparams;
    uint32_t                cparams;
    uint16_t                i;
    uint32_t                cmd;
    ehci_qh_t               *qh;
    dma_node_t              *dma_node = NULL;
    int hcr;  
	
    printf("ehci_init()\n");
    printf("sc = %x\n", sc);

    sc->sc_offs = EHCI_CAPLENGTH(EREAD4(sc, EHCI_CAPLEN_HCIVERSION));
    printf( "sc_offs=0x%x\n", sc->sc_offs);

    sc->ps_event = 0x0000;


    version = EHCI_HCIVERSION(EREAD4(sc, EHCI_CAPLEN_HCIVERSION));
    printf( "EHCI version %x.%x\n", version >> 8, version & 0xff);
    sparams = EREAD4(sc, EHCI_HCSPARAMS);
    printf("sparams=0x%x\n", sparams);
    sc->sc_noport = EHCI_HCS_N_PORTS(sparams);
    printf("ports number=%d\n", sc->sc_noport);
    global_num_ports = sc->sc_noport;

    cparams = EREAD4(sc, EHCI_HCCPARAMS);

    if (EHCI_HCC_64BIT(cparams)) {
        printf("HCC uses 64-bit structures\n");
        /* MUST clear segment register if 64 bit capable */
        EWRITE4(sc, EHCI_CTRLDSSEGMENT, 0);
    }
    /* Reset the controller */
    printf("EHCI controller resetting\n");


    ehci_hcreset(sc);
	printf("CMD_FLS\n");


    /*
     * use current frame-list-size selection 0: 1024*4 bytes 1:  512*4
     * bytes 2:  256*4 bytes 3:      unknown
     */

    if (EHCI_CMD_FLS(EOREAD4(sc, EHCI_USBCMD)) == 3) {
        printf("invalid frame-list-size\n");
    }
	
    printf("Writing in DMA1\n");   
    USECDELAY(1000000);
	
    dma_node = sc->dma_node; 	
    qh = (ehci_qh_t *) dma_node->mem;

    printf("Writing in DMA2\n");   
    USECDELAY(1000000);
    qh->qh_link = (uint32_t) dma_node->daddr | EHCI_LINK_QH;
	qh->qh_endp = htohc32(sc, EHCI_QH_SET_EPS(EHCI_QH_SPEED_HIGH) | EHCI_QH_HRECL);
	qh->qh_endphub = htohc32(sc, EHCI_QH_SET_MULT(1));
	
	qh->qh_curqtd = 0;

		/* fill the overlay qTD */

	qh->qh_qtd.qtd_next = htohc32(sc, EHCI_LINK_TERMINATE);
	qh->qh_qtd.qtd_altnext = htohc32(sc, EHCI_LINK_TERMINATE);
	qh->qh_qtd.qtd_status = htohc32(sc, EHCI_QTD_HALTED);

    
    printf("written, setting EHCI_ASYNCLISTADDR \n");
    USECDELAY(200000);

    EOWRITE4(sc, EHCI_ASYNCLISTADDR, (uint32_t)(dma_node->daddr|EHCI_LINK_QH));
	USECDELAY(200000);
    printf("Turn on controller\n");
    /* turn on controller */
    /* as the PCI bus in the O2 is too slow (33 mhz). we need to slowdown the interrupts max rate to 64 microframes (8 ms).
       default is 1 microframe (0x08)
    */

    /*
    EOWRITE4(sc, EHCI_USBCMD,
                 EHCI_CMD_ITC_1 | 
                (EOREAD4(sc, EHCI_USBCMD) & EHCI_CMD_FLS_M) |
                 EHCI_CMD_ASE |
                 EHCI_CMD_PSE |
                 EHCI_CMD_RS);
*/
    EOWRITE4(sc, EHCI_USBCMD,
                 EHCI_CMD_ITC_1 | /* 1 microframes interrupt delay */
                (EOREAD4(sc, EHCI_USBCMD)) |
                 EHCI_CMD_ASE |
               /*  EHCI_CMD_PSE |*/
			     EHCI_CMD_IAAD |
                 EHCI_CMD_RS);

				 
    printf("Enable interrupts\n");
    /* enable interrupts */
    sc->sc_eintrs = EHCI_NORMAL_INTRS;
    EOWRITE4(sc, EHCI_USBINTR, sc->sc_eintrs);


    /* Take over port ownership */
    printf("EHCI driver now has ports ownership\n");
    EOWRITE4(sc, EHCI_CONFIGFLAG, EHCI_CONF_CF);

    for (i = 0; i < 10000; i++) {
        USECDELAY(100);
        hcr = EOREAD4(sc, EHCI_USBSTS) & EHCI_STS_HCH;
        if (!hcr) {
            break;
        }
    }

    if (hcr) {
        printf("run timeout\n");
    }

    /* EHCI take property of all ports first */
    USECDELAY(100000);
    printf("EHCI driver taking property of all ports over another *HCI\n");
    for (i = 1; i <= sc->sc_noport; i++) {
        cmd = EOREAD4(sc, EHCI_PORTSC(i));
        EOWRITE4(sc, EHCI_PORTSC(i), cmd & ~EHCI_PS_PO);
        global_ports_status[i-1] = 0;
    }


    printf("ehci_init() exit\n");
}



/*
 * shut down the controller when the system is going down
 */
void ehci_detach(pciehci_soft_t sc){
    EOWRITE4(sc, EHCI_USBINTR, 0);
    ehci_hcreset(sc);
}

int ehci_suspend(pciehci_soft_t sc){
    uint32_t cmd;
    uint32_t hcr;
    uint8_t i;

    for (i = 1; i <= sc->sc_noport; i++) {
        cmd = EOREAD4(sc, EHCI_PORTSC(i));
        if (((cmd & EHCI_PS_PO) == 0) && ((cmd & EHCI_PS_PE) == EHCI_PS_PE)) {
            EOWRITE4(sc, EHCI_PORTSC(i), cmd | EHCI_PS_SUSP);
        }
    }

    sc->sc_cmd = EOREAD4(sc, EHCI_USBCMD);

    cmd = sc->sc_cmd & ~(EHCI_CMD_ASE | EHCI_CMD_PSE);
    EOWRITE4(sc, EHCI_USBCMD, cmd);

    for (i = 0; i < 100; i++) {
        hcr = EOREAD4(sc, EHCI_USBSTS) & (EHCI_STS_ASS | EHCI_STS_PSS);

        if (hcr == 0) {
            break;
        }
    }

    if (hcr != 0) {
        printf("reset timeout\n");
    }
    cmd &= ~EHCI_CMD_RS;
    EOWRITE4(sc, EHCI_USBCMD, cmd);

    for (i = 0; i < 100; i++) {
        hcr = EOREAD4(sc, EHCI_USBSTS) & EHCI_STS_HCH;
        if (hcr == EHCI_STS_HCH) {
            break;
        }
    }

    if (hcr != EHCI_STS_HCH) {
        printf("config timeout\n");
		return(-1);
    }
	return(0);
}




int ehci_resume(pciehci_soft_t sc){
    uint32_t cmd;
    uint32_t hcr;
    uint8_t i;

    /* restore things in case the bios doesn't */
    EOWRITE4(sc, EHCI_CTRLDSSEGMENT, 0);
    EOWRITE4(sc, EHCI_USBINTR, sc->sc_eintrs);

    hcr = 0;
    for (i = 1; i <= sc->sc_noport; i++) {
        cmd = EOREAD4(sc, EHCI_PORTSC(i));
        if (((cmd & EHCI_PS_PO) == 0) && ((cmd & EHCI_PS_SUSP) == EHCI_PS_SUSP)) {
            EOWRITE4(sc, EHCI_PORTSC(i), cmd | EHCI_PS_FPR);
            hcr = 1;
        }
    }

    if (hcr) {
        for (i = 1; i <= sc->sc_noport; i++) {
            cmd = EOREAD4(sc, EHCI_PORTSC(i));
            if (((cmd & EHCI_PS_PO) == 0) && ((cmd & EHCI_PS_SUSP) == EHCI_PS_SUSP)) {
                EOWRITE4(sc, EHCI_PORTSC(i), cmd & ~EHCI_PS_FPR);
            }
        }
    }
    EOWRITE4(sc, EHCI_USBCMD, sc->sc_cmd);

    for (i = 0; i < 100; i++) {
        hcr = EOREAD4(sc, EHCI_USBSTS) & EHCI_STS_HCH;
        if (hcr != EHCI_STS_HCH) {
            break;
        }
    }
    if (hcr == EHCI_STS_HCH) {
        printf( "config timeout\n");
		return(-1);
    }

    return(0);
}

void ehci_shutdown(pciehci_soft_t sc){
    printf("stopping the HC\n");
    ehci_hcreset(sc);
}

void ehci_dump_regs(pciehci_soft_t sc){
    uint32_t i;

    i = EOREAD4(sc, EHCI_USBCMD);
    printf("cmd=0x%08x\n", i);

    if (i & EHCI_CMD_ITC_1)
        printf(" EHCI_CMD_ITC_1\n");
    if (i & EHCI_CMD_ITC_2)
        printf(" EHCI_CMD_ITC_2\n");
    if (i & EHCI_CMD_ITC_4)
        printf(" EHCI_CMD_ITC_4\n");
    if (i & EHCI_CMD_ITC_8)
        printf(" EHCI_CMD_ITC_8\n");
    if (i & EHCI_CMD_ITC_16)
        printf(" EHCI_CMD_ITC_16\n");
    if (i & EHCI_CMD_ITC_32)
        printf(" EHCI_CMD_ITC_32\n");
    if (i & EHCI_CMD_ITC_64)
        printf(" EHCI_CMD_ITC_64\n");
    if (i & EHCI_CMD_ASPME)
        printf(" EHCI_CMD_ASPME\n");
    if (i & EHCI_CMD_ASPMC)
        printf(" EHCI_CMD_ASPMC\n");
    if (i & EHCI_CMD_LHCR)
        printf(" EHCI_CMD_LHCR\n");
    if (i & EHCI_CMD_IAAD)
        printf(" EHCI_CMD_IAAD\n");
    if (i & EHCI_CMD_ASE)
        printf(" EHCI_CMD_ASE\n");
    if (i & EHCI_CMD_PSE)
        printf(" EHCI_CMD_PSE\n");
    if (i & EHCI_CMD_FLS_M)
        printf(" EHCI_CMD_FLS_M\n");
    if (i & EHCI_CMD_HCRESET)
        printf(" EHCI_CMD_HCRESET\n");
    if (i & EHCI_CMD_RS)
        printf(" EHCI_CMD_RS\n");

    i = EOREAD4(sc, EHCI_USBSTS);

    printf("sts=0x%08x\n", i);

    if (i & EHCI_STS_ASS)
        printf(" EHCI_STS_ASS\n");
    if (i & EHCI_STS_PSS)
        printf(" EHCI_STS_PSS\n");
    if (i & EHCI_STS_REC)
        printf(" EHCI_STS_REC\n");
    if (i & EHCI_STS_HCH)
        printf(" EHCI_STS_HCH\n");
    if (i & EHCI_STS_IAA)
        printf(" EHCI_STS_IAA\n");
    if (i & EHCI_STS_HSE)
        printf(" EHCI_STS_HSE\n");
    if (i & EHCI_STS_FLR)
        printf(" EHCI_STS_FLR\n");
    if (i & EHCI_STS_PCD)
        printf(" EHCI_STS_PCD\n");
    if (i & EHCI_STS_ERRINT)
        printf(" EHCI_STS_ERRINT\n");
    if (i & EHCI_STS_INT)
        printf(" EHCI_STS_INT\n");

    printf("intr=0x%08x\n",
    EOREAD4(sc, EHCI_USBINTR));
    printf("frindex=0x%08x ctrdsegm=0x%08x periodic=0x%08x async=0x%08x\n",
    EOREAD4(sc, EHCI_FRINDEX),
    EOREAD4(sc, EHCI_CTRLDSSEGMENT),
    EOREAD4(sc, EHCI_PERIODICLISTBASE),
    EOREAD4(sc, EHCI_ASYNCLISTADDR));
    for (i = 1; i <= sc->sc_noport; i++) {
        printf("port %d status=0x%08x\n", i,
        EOREAD4(sc, EHCI_PORTSC(i)));
    }
}

void ehci_enumerate( pciehci_soft_t sc){

}

void ehci_dump_data( pciehci_soft_t sc){
}

int ehci_dummy_enable( pciehci_soft_t sc){
	int rc; 
	ehci_queue_heads_init( sc);
	rc = ehci_queue_heads_alloc( sc);
	return(0);
}

int ehci_dummy_disable( pciehci_soft_t sc){
	int rc;
	rc = ehci_queue_heads_destroy( sc);
	
	return(0);
}

/*------------------------------------------------------------------------*
 *ehci_interrupt - EHCI interrupt handler
 *
 * NOTE: Do not access "sc->sc_bus.bdev" inside the interrupt handler,
 * hence the interrupt handler will be setup before "sc->sc_bus.bdev"
 * is present !
 *------------------------------------------------------------------------*/
void ehci_interrupt(pciehci_soft_t sc){
    uint32_t status, port_connect, pr, port_status;
    int i, j;
    unsigned char *ptr;

    printf("real interrupt()\n");
    printf("sc = %x\n", sc);
    printf("sc->pci_io_caps=0x%x\n", sc->pci_io_caps);

    printf("checking if interrupt was for us\n");
    status = EHCI_STS_INTRS(EOREAD4(sc, EHCI_USBSTS));
    if (status == 0) {
        /* the interrupt was not for us */
        return;
    }

    
    
    if (!(status & sc->sc_eintrs)) {
        return;
    }

    
    printf("ack, status=0x%08x\n", status);
    EOWRITE4(sc, EHCI_USBSTS, status);/* acknowledge */

    status &= sc->sc_eintrs;

    
    if (status & EHCI_STS_HSE) {
        printf("unrecoverable error, controller halted\n");
        ehci_dump_regs(sc);
		
    }
    
	if( status & EHCI_STS_IAA){
	    printf("Got USB data from ASYNC, ready for transfers\n");
		
	}else if( status & EHCI_STS_PCD){
    
        printf("port change detected!\n");
	    for (i = 1; i <= sc->sc_noport; i++){
	        port_status = EOREAD4(sc, EHCI_PORTSC(i));
	        printf("port %d = 0x%x\n", i, port_status);
	        port_connect = port_status & EHCI_PS_CLEAR;

            sc->ps_event |= EHCI_EVENT_PORT_ACTIVITY;

	        /* pick out CHANGE bits from the status register */
	        if( port_connect){
                printf("activity in port %d\n", i);
	            if( (port_status & EHCI_PS_CS) == 0){
	                printf("Device disconnected\n");
                    sc->ps_event |= EHCI_EVENT_PORT_DISCONNECT;
                    global_ports_status[i-1] = 0;
                }else{
                    printf("Device connected\n");
                    sc->ps_event |= EHCI_EVENT_PORT_CONNECT;
	                global_ports_status[i-1] = 1;
	                if( EHCI_PS_IS_LOWSPEED(port_status)){
	                    printf("No High speed device, passing to companion controller\n");
                    }else{
  		                printf("Device is High speed, resetting\n");
	                    ehci_port_reset( sc, i);

		            }
	            }/*
	            EOWRITE4( sc, EHCI_PORTSC(i),  EOREAD4(sc, EHCI_PORTSC(i)) | EHCI_PS_CSC);*/
                
	        }
        }
    }	

    printf("sc->ps_event=0x%x, global_pciehci_inuse=%d\n", sc->ps_event, global_pciehci_inuse);    
    if( (sc->ps_event & EHCI_EVENT_PORT_ACTIVITY) != 0){
        
        if( global_pciehci_inuse != 0){
            printf("call pollwakeup()\n");
            printf("pollhead=0x%x, global_pollhead=0x%x\n", sc->ps_pollhead, global_soft->ps_pollhead );
            pollwakeup( sc->ps_pollhead, POLLIN);
        }
    }
    /*if (status & EHCI_STS_PCD) {*/
        /*
         * Disable PCD interrupt for now, because it will be
         * on until the port has been reset.
         */
/*        sc->sc_eintrs &= ~EHCI_STS_PCD;
        EOWRITE4(sc, EHCI_USBINTR, sc->sc_eintrs);*/
    /*}*/


    status &= ~(EHCI_STS_INT | EHCI_STS_ERRINT | EHCI_STS_PCD | EHCI_STS_IAA);

    if (status != 0) {
        /* block unprocessed interrupts */
/*        sc->sc_eintrs &= ~status;*/
        printf("ok02\n");
/*
        EOWRITE4(sc, EHCI_USBINTR, sc->sc_eintrs);
        printf("blocking interrupts 0x%x\n",  status);*/
    }

    /* Dump all */
/*    if(  sc->dma_node != NULL){
        ptr = (unsigned char *)  sc->dma_node;
        TRACE(  TRC_GC_DMA, 12, "\n\nALL: ", NULL);
        dumphex(  TRC_GC_DMA, 12, (unsigned char *) ptr, 512);
        dumphex(  TRC_GC_DMA, 12, (unsigned char *) ptr + 512, 512);
        dumphex(  TRC_GC_DMA, 12, (unsigned char *) ptr + 1024, 512);

    }
	*/
}

void ehci_takecontroller(pciehci_soft_t sc){
    uint32_t cparams;
    uint32_t eec;
    uint16_t to;
    uint8_t eecp;
    uint8_t bios_sem;

    printf("ehci_pci_takecontroller()\n");
    printf("Take controller1: reading HCCPARAMS: 0x%x\n", EHCI_HCCPARAMS);
    USECDELAY(100000);

    cparams = EREAD4(sc, EHCI_HCCPARAMS);
    printf( "cparams = %d\n", cparams);
    /* Synchronise with the BIOS if it owns the controller. */
    for (eecp = EHCI_HCC_EECP(cparams); eecp != 0; eecp = EHCI_EECP_NEXT(eec)) {
        eec = (uint8_t) pci_read_config(sc->ps_conn, eecp, 4);
        if (EHCI_EECP_ID(eec) != EHCI_EC_LEGSUP) {
            continue;
        }
        bios_sem = (uint8_t) pci_read_config(sc->ps_conn, eecp + EHCI_LEGSUP_BIOS_SEM, 1);
        if (bios_sem == 0) {
            continue;
        }
        printf( "waiting for BIOS to give up control\n");
        pci_write_config(sc->ps_conn, eecp + EHCI_LEGSUP_OS_SEM, 1, 1);
        to = 500;
        while (1) {
            bios_sem = (uint8_t)pci_read_config(sc->ps_conn, eecp + EHCI_LEGSUP_BIOS_SEM, 1);
            USECDELAY(100000);
            if (bios_sem == 0)
                break;

            if (--to == 0) {
                printf("timed out waiting for BIOS\n");
                break;
            }
        }
    }
    printf("ehci_pci_takecontroller() exit\n");

}





