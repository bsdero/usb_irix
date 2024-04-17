/*
********************************************************************
* PCI EHCI Device Driver for Irix 6.5                              *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: ehci.h                                                     *
* Description: Definitions and macros for EHCI                     *
*                                                                  *
*                                                                  *
********************************************************************
*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      09-10-2011     -Initial version, rippen from NetBSD source code                         *
*                                                                                                     *
*******************************************************************************************************

*/

/* $FreeBSD: src/sys/dev/usb/controller/ehci.h,v 1.8.2.5.2.1 2010/12/21 17:09:25 kensmith Exp $ */
/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _EHCI_H_
#define _EHCI_H_

#define EHCI_MAX_DEVICES 16

/*
 * Alignment NOTE: structures must be aligned so that the hardware can index
 * without performing addition.
 */
#define EHCI_FRAMELIST_ALIGN          0x1000    /* bytes */
#define EHCI_FRAMELIST_COUNT            1024    /* units */
#define EHCI_VIRTUAL_FRAMELIST_COUNT     128    /* units */

#if ((8*EHCI_VIRTUAL_FRAMELIST_COUNT) < USB_MAX_HS_ISOC_FRAMES_PER_XFER)
#error "maximum number of high-speed isochronous frames is higher than supported!"
#endif

#if (EHCI_VIRTUAL_FRAMELIST_COUNT < USB_MAX_FS_ISOC_FRAMES_PER_XFER)
#error "maximum number of full-speed isochronous frames is higher than supported!"
#endif

/* Link types */
#define EHCI_LINK_TERMINATE     0x00000001
#define EHCI_LINK_TYPE(x)       ((x) & 0x00000006)
#define EHCI_LINK_ITD           0x0
#define EHCI_LINK_QH            0x2
#define EHCI_LINK_SITD          0x4
#define EHCI_LINK_FSTN          0x6
#define EHCI_LINK_ADDR(x)       ((x) &~0x1f)

/* Structures alignment (bytes) */
#define EHCI_ITD_ALIGN  128
#define EHCI_SITD_ALIGN 64
#define EHCI_QTD_ALIGN  64
#define EHCI_QH_ALIGN   128
#define EHCI_FSTN_ALIGN 32
/* Data buffers are divided into one or more pages */
#define EHCI_PAGE_SIZE  0x1000
/*
   #if    ((USB_PAGE_SIZE < EHCI_PAGE_SIZE) || (EHCI_PAGE_SIZE == 0) ||    \
        (USB_PAGE_SIZE < EHCI_ITD_ALIGN) || (EHCI_ITD_ALIGN == 0) ||    \
        (USB_PAGE_SIZE < EHCI_SITD_ALIGN) || (EHCI_SITD_ALIGN == 0) ||    \
        (USB_PAGE_SIZE < EHCI_QTD_ALIGN) || (EHCI_QTD_ALIGN == 0) ||    \
        (USB_PAGE_SIZE < EHCI_QH_ALIGN) || (EHCI_QH_ALIGN == 0) ||    \
        (USB_PAGE_SIZE < EHCI_FSTN_ALIGN) || (EHCI_FSTN_ALIGN == 0))
   #error    "Invalid USB page size!"
   #endif
 */

/*
 * Isochronous Transfer Descriptor.  This descriptor is used for high speed
 * transfers only.
 */
struct ehci_itd {
    uint32_t itd_next;
    uint32_t itd_status[8];
#define EHCI_ITD_SET_LEN(x)     ((x) << 16)
#define EHCI_ITD_GET_LEN(x)     (((x) >> 16) & 0xFFF)
#define EHCI_ITD_IOC            (1 << 15)
#define EHCI_ITD_SET_PG(x)      ((x) << 12)
#define EHCI_ITD_GET_PG(x)      (((x) >> 12) & 0x7)
#define EHCI_ITD_SET_OFFS(x)    (x)
#define EHCI_ITD_GET_OFFS(x)    (((x) >> 0) & 0xFFF)
#define EHCI_ITD_ACTIVE         (1 << 31)
#define EHCI_ITD_DATABUFERR     (1 << 30)
#define EHCI_ITD_BABBLE         (1 << 29)
#define EHCI_ITD_XACTERR        (1 << 28)
    uint32_t itd_bp[7];
    /* itd_bp[0] */
#define EHCI_ITD_SET_ADDR(x)    (x)
#define EHCI_ITD_GET_ADDR(x)    (((x) >> 0) & 0x7F)
#define EHCI_ITD_SET_ENDPT(x)   ((x) << 8)
#define EHCI_ITD_GET_ENDPT(x)   (((x) >> 8) & 0xF)
    /* itd_bp[1] */
#define EHCI_ITD_SET_DIR_IN     (1 << 11)
#define EHCI_ITD_SET_DIR_OUT    (0 << 11)
#define EHCI_ITD_SET_MPL(x)     (x)
#define EHCI_ITD_GET_MPL(x)     (((x) >> 0) & 0x7FF)
    uint32_t itd_bp_hi[7];
/*
 * Extra information needed:
 */
    list_entry_t list;
};

typedef struct ehci_itd ehci_itd_t;

/*
 * Split Transaction Isochronous Transfer Descriptor.  This descriptor is used
 * for full speed transfers only.
 */
struct ehci_sitd {
    uint32_t sitd_next;
    uint32_t sitd_portaddr;
#define EHCI_SITD_SET_DIR_OUT   (0 << 31)
#define EHCI_SITD_SET_DIR_IN    (1 << 31)
#define EHCI_SITD_SET_ADDR(x)   (x)
#define EHCI_SITD_GET_ADDR(x)   ((x) & 0x7F)
#define EHCI_SITD_SET_ENDPT(x)  ((x) << 8)
#define EHCI_SITD_GET_ENDPT(x)  (((x) >> 8) & 0xF)
#define EHCI_SITD_GET_DIR(x)    ((x) >> 31)
#define EHCI_SITD_SET_PORT(x)   ((x) << 24)
#define EHCI_SITD_GET_PORT(x)   (((x) >> 24) & 0x7F)
#define EHCI_SITD_SET_HUBA(x)   ((x) << 16)
#define EHCI_SITD_GET_HUBA(x)   (((x) >> 16) & 0x7F)
    uint32_t sitd_mask;
#define EHCI_SITD_SET_SMASK(x)  (x)
#define EHCI_SITD_SET_CMASK(x)  ((x) << 8)
    uint32_t sitd_status;
#define EHCI_SITD_COMPLETE_SPLIT        (1<<1)
#define EHCI_SITD_START_SPLIT           (0<<1)
#define EHCI_SITD_MISSED_MICRO_FRAME    (1<<2)
#define EHCI_SITD_XACTERR               (1<<3)
#define EHCI_SITD_BABBLE                (1<<4)
#define EHCI_SITD_DATABUFERR            (1<<5)
#define EHCI_SITD_ERROR                 (1<<6)
#define EHCI_SITD_ACTIVE                (1<<7)
#define EHCI_SITD_IOC                   (1<<31)
#define EHCI_SITD_SET_LEN(len)          ((len)<<16)
#define EHCI_SITD_GET_LEN(x)            (((x)>>16) & 0x3FF)
    uint32_t sitd_bp[2];
    uint32_t sitd_back;
    uint32_t sitd_bp_hi[2];
/*
 * Extra information needed:
 */
    list_entry_t list;
};

typedef struct ehci_sitd ehci_sitd_t;

/* Queue Element Transfer Descriptor */
struct ehci_qtd {
    uint32_t qtd_next;
    uint32_t qtd_altnext;
    uint32_t qtd_status;
#define EHCI_QTD_GET_STATUS(x)  (((x) >>  0) & 0xff)
#define EHCI_QTD_SET_STATUS(x)  ((x) << 0)
#define EHCI_QTD_ACTIVE         0x80
#define EHCI_QTD_HALTED         0x40
#define EHCI_QTD_BUFERR         0x20
#define EHCI_QTD_BABBLE         0x10
#define EHCI_QTD_XACTERR        0x08
#define EHCI_QTD_MISSEDMICRO    0x04
#define EHCI_QTD_SPLITXSTATE    0x02
#define EHCI_QTD_PINGSTATE      0x01
#define EHCI_QTD_STATERRS       0x74
#define EHCI_QTD_GET_PID(x)     (((x) >>  8) & 0x3)
#define EHCI_QTD_SET_PID(x)     ((x) <<  8)
#define EHCI_QTD_PID_OUT        0x0
#define EHCI_QTD_PID_IN         0x1
#define EHCI_QTD_PID_SETUP      0x2
#define EHCI_QTD_GET_CERR(x)    (((x) >> 10) &  0x3)
#define EHCI_QTD_SET_CERR(x)    ((x) << 10)
#define EHCI_QTD_GET_C_PAGE(x)  (((x) >> 12) &  0x7)
#define EHCI_QTD_SET_C_PAGE(x)  ((x) << 12)
#define EHCI_QTD_GET_IOC(x)     (((x) >> 15) &  0x1)
#define EHCI_QTD_IOC            0x00008000
#define EHCI_QTD_GET_BYTES(x)   (((x) >> 16) &  0x7fff)
#define EHCI_QTD_SET_BYTES(x)   ((x) << 16)
#define EHCI_QTD_GET_TOGGLE(x)  (((x) >> 31) &  0x1)
#define EHCI_QTD_SET_TOGGLE(x)  ((x) << 31)
#define EHCI_QTD_TOGGLE_MASK    0x80000000
#define EHCI_QTD_NBUFFERS       5
#define EHCI_QTD_PAYLOAD_MAX ((EHCI_QTD_NBUFFERS-1)*EHCI_PAGE_SIZE)
    uint32_t qtd_buffer[EHCI_QTD_NBUFFERS];
    uint32_t qtd_buffer_hi[EHCI_QTD_NBUFFERS];
/*
 * Extra information needed:
 */
    list_entry_t list;
};

typedef struct ehci_qtd ehci_qtd_t;

/* Queue Head Sub Structure */
struct ehci_qh_sub {
    uint32_t qtd_next;
    uint32_t qtd_altnext;
    uint32_t qtd_status;
    uint32_t qtd_buffer[EHCI_QTD_NBUFFERS];
    uint32_t qtd_buffer_hi[EHCI_QTD_NBUFFERS];
};

/* Queue Head */
struct ehci_qh {
    uint32_t qh_link;
    uint32_t qh_endp;
#define EHCI_QH_GET_ADDR(x)     (((x) >>  0) & 0x7f)    /* endpoint addr */
#define EHCI_QH_SET_ADDR(x)     (x)
#define EHCI_QH_ADDRMASK        0x0000007f
#define EHCI_QH_GET_INACT(x)    (((x) >>  7) & 0x01)    /* inactivate on next */
#define EHCI_QH_INACT           0x00000080
#define EHCI_QH_GET_ENDPT(x)    (((x) >>  8) & 0x0f)    /* endpoint no */
#define EHCI_QH_SET_ENDPT(x)    ((x) <<  8)
#define EHCI_QH_GET_EPS(x)      (((x) >> 12) & 0x03)    /* endpoint speed */
#define EHCI_QH_SET_EPS(x)      ((x) << 12)
#define EHCI_QH_SPEED_FULL      0x0
#define EHCI_QH_SPEED_LOW       0x1
#define EHCI_QH_SPEED_HIGH      0x2
#define EHCI_QH_GET_DTC(x)      (((x) >> 14) & 0x01)    /* data toggle control */
#define EHCI_QH_DTC             0x00004000
#define EHCI_QH_GET_HRECL(x)    (((x) >> 15) & 0x01)    /* head of reclamation */
#define EHCI_QH_HRECL           0x00008000
#define EHCI_QH_GET_MPL(x)      (((x) >> 16) & 0x7ff)   /* max packet len */
#define EHCI_QH_SET_MPL(x)      ((x) << 16)
#define EHCI_QH_MPLMASK         0x07ff0000
#define EHCI_QH_GET_CTL(x)      (((x) >> 27) & 0x01)    /* control endpoint */
#define EHCI_QH_CTL             0x08000000
#define EHCI_QH_GET_NRL(x)      (((x) >> 28) & 0x0f)    /* NAK reload */
#define EHCI_QH_SET_NRL(x)      ((x) << 28)
    uint32_t qh_endphub;
#define EHCI_QH_GET_SMASK(x)    (((x) >>  0) & 0xff)    /* intr sched mask */
#define EHCI_QH_SET_SMASK(x)    ((x) <<  0)
#define EHCI_QH_GET_CMASK(x)    (((x) >>  8) & 0xff)    /* split completion mask */
#define EHCI_QH_SET_CMASK(x)    ((x) <<  8)
#define EHCI_QH_GET_HUBA(x)     (((x) >> 16) & 0x7f)    /* hub address */
#define EHCI_QH_SET_HUBA(x)     ((x) << 16)
#define EHCI_QH_GET_PORT(x)     (((x) >> 23) & 0x7f)    /* hub port */
#define EHCI_QH_SET_PORT(x)     ((x) << 23)
#define EHCI_QH_GET_MULT(x)     (((x) >> 30) & 0x03)    /* pipe multiplier */
#define EHCI_QH_SET_MULT(x)     ((x) << 30)
    uint32_t qh_curqtd;
    struct ehci_qh_sub qh_qtd;
/*
 * Extra information needed:
 */
 
#define           QH_IN_USE             0x00000001
#define           QH_ACTIVE             0x00000002
    uint32_t     status;     
    list_entry_t qh_list;
	list_entry_t qtd_list;
	dma_node_t   *dma_qtds;
	dma_node_t   *dma_buf;
};

typedef struct ehci_qh ehci_qh_t;

/* Periodic Frame Span Traversal Node */
struct ehci_fstn {
    uint32_t fstn_link;
    uint32_t fstn_back;
};

typedef struct ehci_fstn ehci_fstn_t;
/*
   struct ehci_hw_softc {
        struct usb_page_cache pframes_pc;
        struct usb_page_cache terminate_pc;
        struct usb_page_cache async_start_pc;
        struct usb_page_cache intr_start_pc[EHCI_VIRTUAL_FRAMELIST_COUNT];
        struct usb_page_cache isoc_hs_start_pc[EHCI_VIRTUAL_FRAMELIST_COUNT];
        struct usb_page_cache isoc_fs_start_pc[EHCI_VIRTUAL_FRAMELIST_COUNT];

        struct usb_page pframes_pg;
        struct usb_page terminate_pg;
        struct usb_page async_start_pg;
        struct usb_page intr_start_pg[EHCI_VIRTUAL_FRAMELIST_COUNT];
        struct usb_page isoc_hs_start_pg[EHCI_VIRTUAL_FRAMELIST_COUNT];
        struct usb_page isoc_fs_start_pg[EHCI_VIRTUAL_FRAMELIST_COUNT];
   };
 
struct ehci_config_desc {
    struct usb_config_descriptor confd;
    struct usb_interface_descriptor ifcd;
    struct usb_endpoint_descriptor endpd;
};

union ehci_hub_desc {
    struct usb_status stat;
    struct usb_port_status ps;
    struct usb_hub_descriptor hubd;
    uint8_t temp[128];
};

*/
#endif                                  /* _EHCI_H_ */
