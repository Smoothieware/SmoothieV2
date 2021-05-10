// we need to define this as the builtin one only does debug printf

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "list.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP_Private.h"
#include "FreeRTOS_ARP.h"
#include "FreeRTOS_UDP_IP.h"
#include "FreeRTOS_DHCP.h"
#include "NetworkInterface.h"
#include "NetworkBufferManagement.h"
#include "FreeRTOS_DNS.h"
#include "FreeRTOS_IP_Private.h"

#include "OutputStream.h"

extern List_t xBoundTCPSocketsList;
extern List_t xBoundUDPSocketsList;

extern "C" void netstat(OutputStream& os)
{
    /* Show a simple listing of all created sockets and their connections */
    taskENTER_CRITICAL();

    const ListItem_t * pxIterator;
    BaseType_t count = 0;

    if( !listLIST_IS_INITIALISED( &xBoundTCPSocketsList ) ) {
        os.printf( "PLUS-TCP not initialized\n" );
    } else {
        const ListItem_t * pxEndTCP = listGET_END_MARKER( &xBoundTCPSocketsList );
        const ListItem_t * pxEndUDP = listGET_END_MARKER( &xBoundUDPSocketsList );
        os.printf( "Prot Port IP-Remote         : Port  R/T Status       Alive   tmout Child\n" );

        for( pxIterator = listGET_HEAD_ENTRY( &xBoundTCPSocketsList );
             pxIterator != pxEndTCP;
             pxIterator = listGET_NEXT( pxIterator ) ) {
            const FreeRTOS_Socket_t * pxSocket = ipCAST_CONST_PTR_TO_CONST_TYPE_PTR( FreeRTOS_Socket_t, listGET_LIST_ITEM_OWNER( pxIterator ) );
#if ( ipconfigTCP_KEEP_ALIVE == 1 )
            TickType_t age = xTaskGetTickCount() - pxSocket->u.xTCP.xLastAliveTime;
#else
            TickType_t age = 0U;
#endif

            char ucChildText[ 16 ] = "";

            if( pxSocket->u.xTCP.ucTCPState == ( uint8_t ) eTCP_LISTEN ) {
                /* Using function "snprintf". */
                const int32_t copied_len = snprintf( ucChildText, sizeof( ucChildText ), " %ld/%ld",
                                                     ( int32_t ) pxSocket->u.xTCP.usChildCount,
                                                     ( int32_t ) pxSocket->u.xTCP.usBacklog );
                ( void ) copied_len;
                /* These should never evaluate to false since the buffers are both shorter than 5-6 characters (<=65535) */
                configASSERT( copied_len >= 0 );
                configASSERT( copied_len < ( int32_t ) sizeof( ucChildText ) );
            }

            os.printf( "TCP %5d %-16lxip:%5d %d/%d %-13.13s %6lu %6u%s\n",
                       pxSocket->usLocalPort,         /* Local port on this machine */
                       pxSocket->u.xTCP.ulRemoteIP,   /* IP address of remote machine */
                       pxSocket->u.xTCP.usRemotePort, /* Port on remote machine */
                       ( pxSocket->u.xTCP.rxStream != NULL ) ? 1 : 0,
                       ( pxSocket->u.xTCP.txStream != NULL ) ? 1 : 0,
                       FreeRTOS_GetTCPStateName( pxSocket->u.xTCP.ucTCPState ),
                       ( age > 999999u ) ? 999999u : age, /* Format 'age' for printing */
                       pxSocket->u.xTCP.usTimeout,
                       ucChildText );
            count++;
        }

        for( pxIterator = listGET_HEAD_ENTRY( &xBoundUDPSocketsList );
             pxIterator != pxEndUDP;
             pxIterator = listGET_NEXT( pxIterator ) ) {
            /* Local port on this machine */
            os.printf( "UDP Port %5u\n",
                       FreeRTOS_ntohs( listGET_LIST_ITEM_VALUE( pxIterator ) ) );
            count++;
        }
        os.printf("Total: %lu sockets\n", count);
    }

    taskEXIT_CRITICAL();
}
