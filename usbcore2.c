/**************************************************************************
 *									  *
 *		 Copyright (C) 1990-1993, Silicon Graphics, Inc		  *
 *									  *
 *  These coded instructions, statements, and computer programs	 contain  *
 *  unpublished	 proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may	not be disclosed  *
 *  to	third  parties	or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"io/lio/xiocfg.c: $Revision: 1.2 $"

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/cred.h>
#include <sys/mload.h>
#include <sys/kmem.h>
#include <sys/uio.h>
#include <ksys/ddmap.h>

#define NEW(ptr)	(ptr = kmem_zalloc(sizeof (*(ptr)), KM_SLEEP))
#define DEL(ptr)	(kmem_free(ptr, sizeof (*(ptr))))

char                   *usbcore_mversion = M_VERSION;
int                     usbcore_devflag = D_MP;

/* busy counts the reasons why we can not
 * currently unload this driver.
 */
unsigned		usbcore_busy = 0;

typedef struct usbcore_soft_s *usbcore_soft_t;

#define	usbcore_soft_get(v)	(usbcore_soft_t)hwgraph_fastinfo_get(v)

struct usbcore_soft_s {
    vertex_hdl_t            conn;
    vertex_hdl_t            vhdl;
	int                     mode;
};

void
usbcore_init(void)
{
    usbcore_soft_t            soft;

    NEW(soft);
    soft->conn = conn;

    hwgraph_char_device_add(conn, "usb1", "usbcore_", &soft->vhdl);
    hwgraph_fastinfo_set(soft->vhdl, (arbitrary_info_t) soft);

}

int
usbcore_unload(void)
{
    if (usbcore_busy)
	return -1;

    vertex_hdl_t            vhdl;
    usbcore_soft_t            soft;

    if (GRAPH_SUCCESS !=
	hwgraph_edge_remove(conn, "usb1", &vhdl))
	return -1;
    soft = usbcore_soft_get(vhdl);
    if (!soft)
	return -1;
    hwgraph_fastinfo_set(vhdl, 0);
    hwgraph_vertex_destroy(vhdl);
    DEL(soft);

    return 0;				/* always ok to unload */
}

int
usbcore_reg(void)
{
    driver_register(-1, -1, "usbcore_", 0);
    return 0;
}

int
usbcore_unreg(void)
{
    if (usbcore_busy)
	return -1;

    driver_unregister("usbcore_");
    return 0;
}

int
usbcore_attach(vertex_hdl_t conn)
{

    return 0;
}

int
usbcore_detach(vertex_hdl_t conn)
{
    return 0;

}

/*ARGSUSED */
int
usbcore_open(dev_t *devp, int flag, int otyp, struct cred *crp)
{
    return 0;
}

/*ARGSUSED */
int
usbcore_close(dev_t dev)
{
    return 0;
}

/*ARGSUSED */
int
usbcore_read(dev_t dev, uio_t * uiop, cred_t *crp)
{
    return EOPNOTSUPP;
}

/*ARGSUSED */
int
usbcore_write()
{
    return EOPNOTSUPP;
}

/*ARGSUSED*/
int
usbcore_ioctl(dev_t dev, int cmd, void *uarg, int mode, cred_t *crp, int *rvalp)
{
    return *rvalp = ENOTTY;
}

/*ARGSUSED*/
int
usbcore_map(dev_t dev, vhandl_t *vt,
	  off_t off, size_t len, uint_t prot)
{

    return 0;
}

/*ARGSUSED*/
int
usbcore_unmap(dev_t dev, vhandl_t *vt)
{
    return (0);
}
