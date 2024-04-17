/*
********************************************************************
* PCI EHCI Device Driver for Irix 6.5                              *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* Trace and debug utilities                                        *
* 2011                                                             *
* File: trace.h                                                    *
********************************************************************

*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      01-09-2012     -Initial version                                                         *
*                                                                                                     *
*******************************************************************************************************
*/
#ifndef _TRACE_H_
#define _TRACE_H_

#define TRACE( trc_class, trc_level, fmt, ...)                                                                      \
            if(( trc_level >= global_trace_class.trc_level) && ( (trc_class & global_trace_class.trc_class) != 0))  \
			    printf( "PCIEHCI,%s,%s,%d:"fmt"\n", __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)

#endif
