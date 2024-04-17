/****************************************************************************************
TRACE FEATURES
Set debug level 
0 = no debug messages
1 = informational/no recoverable error messages
2 = all error/warning messages
3 = entry/exit of functions and callbacks
4 = debug, all messages showing
****************************************************************************************/
/* Entry points */ 
#define TRC_INIT                                    0x00000001  /* pfxinit and pfxreg */
#define TRC_UNLOAD                                  0x00000002  /* pfxunreg and pfxunload */
#define TRC_ATTACH                                  0x00000004  /* pfxattach */
#define TRC_DETACH                                  0x00000008  /* pfxdetach */
#define TRC_OPEN                                    0x00000010  /* pfxopen */                
#define TRC_CLOSE                                   0x00000020  /* pfxclose */
#define TRC_READ                                    0x00000040  /* pfxread */
#define TRC_WRITE                                   0x00000080  /* pfxwrite */
#define TRC_IOCTL                                   0x00000100  /* pfxioctl */
#define TRC_STRATEGY                                0x00000200  /* pfxstrategy */
#define TRC_POLL                                    0x00000400  /* pfxread */
#define TRC_MAP                                     0x00000800  /* pfxmap and pfxunmap */
#define TRC_INTR                                    0x00001000  /* pfxintr */
#define TRC_ERR                                     0x00002000  /* pfxerr */
#define TRC_HALT                                    0x00004000  /* pfxhalt */

                
/* Help Functionalities */
#define TRC_GC_DMA                                  0x00100000  /* memory lists */


#define TRC_NO_MESSAGES                             0x00000000  /* no trace/debug messages */
#define TRC_ENTRY_POINTS                            0x0000ffff  /* all entry points */
#define TRC_ALL                                     0xffffffff  /* debug all        */




typedef struct{
    uchar_t     level;
    uint64_t    class; 
}USB_trace_class_t;
