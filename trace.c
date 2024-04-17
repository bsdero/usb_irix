/*
********************************************************************
* PCI EHCI Device Driver for Irix 6.5                              *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: trace.c                                                    *
* Description: Tracing and debug macros and variable               *
*                                                                  *
*                                                                  *
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


#define __FUNCTION__                                __func__
#define TRACE_LEVEL_DELAY                           200

#define TRACE( trc_class, trc_level, fmt, ...)                                                                      \
            if(( global_trace_class.level >= trc_level) && ( (trc_class & global_trace_class.class) != 0)){         \
                if( global_trace_class.level > TRACE_LEVEL_DELAY)                                                   \
                    USECDELAY( 500000);                                                                             \
                printf( "USB:%s,%s,%d:"fmt"\n", __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__);                     \
            }

#define TRACERR( trc_class, fmt, ...)                                                                               \
            if(((trc_class | TRC_ERROR) & global_trace_class.class) != 0){                                          \
                if( global_trace_class.level > TRACE_LEVEL_DELAY)                                                   \
                    USECDELAY( 500000);                                                                             \
                printf( "USB ERROR:%s,%s,%d:"fmt"\n", __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__);               \
            }

#define TRACEWAR( trc_class, trc_level, fmt, ...)                                                                           \
            if(( global_trace_class.level >= trc_level) && ( ((trc_class | TRC_WARNING) & global_trace_class.class) != 0)){ \
                if( global_trace_class.level > TRACE_LEVEL_DELAY)                                                           \
                    USECDELAY( 100000);                                                                                     \
                printf( "USB WARNING:%s,%s,%d:"fmt"\n", __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__);                     \
            }


