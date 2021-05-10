/*
 * FreeRTOS+TCP V2.3.2
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://aws.amazon.com/freertos
 * http://www.FreeRTOS.org
 */

/* Standard includes. */
#include <stdio.h>
#include <stdlib.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

/* FreeRTOS Protocol includes. */
#include "FreeRTOS_HTTP_commands.h"
#include "FreeRTOS_TCP_server.h"
#include "FreeRTOS_server_private.h"

/* Remove the whole file if HTTP is not supported. */
#if ( ipconfigUSE_HTTP == 1 )

/* FreeRTOS+FAT includes. */
#include "ff_stdio.h"

#ifndef HTTP_SERVER_BACKLOG
#define HTTP_SERVER_BACKLOG    ( 12 )
#endif

#ifndef USE_HTML_CHUNKS
#define USE_HTML_CHUNKS    ( 0 )
#endif

#if !defined( ARRAY_SIZE )
#define ARRAY_SIZE( x )    ( BaseType_t ) ( sizeof( x ) / sizeof( x )[ 0 ] )
#endif

/* Some defines to make the code more readable */
#define pcCOMMAND_BUFFER                      pxClient->pxParent->pcCommandBuffer
#define pcFILE_BUFFER                         pxClient->pxParent->pcFileBuffer

#ifndef ipconfigHTTP_REQUEST_CHARACTER
#define ipconfigHTTP_REQUEST_CHARACTER    '?'
#endif

/*_RB_ Need comment block, although fairly self evident. */
static void prvFileClose( HTTPClient_t * pxClient );
#if 0
static BaseType_t prvProcessCmd( HTTPClient_t * pxClient,
                                 BaseType_t xIndex );
static BaseType_t prvOpenURL( HTTPClient_t * pxClient );
static const char pcEmptyString[ 1 ] = { '\0' };
#endif
static const char * pcGetContentsType( const char * apFname );

static BaseType_t prvSendFile( HTTPClient_t * pxClient );
static BaseType_t prvSendReply( HTTPClient_t * pxClient,
                                BaseType_t xCode );

extern int parse_request_release(void *request);


typedef struct xTYPE_COUPLE {
    const char * pcExtension;
    const char * pcType;
} TypeCouple_t;

static TypeCouple_t pxTypeCouples[] = {
    { "html", "text/html"              },
    { "css",  "text/css"               },
    { "js",   "text/javascript"        },
    { "png",  "image/png"              },
    { "jpg",  "image/jpeg"             },
    { "gif",  "image/gif"              },
    { "txt",  "text/plain"             },
    { "mp3",  "audio/mpeg3"            },
    { "wav",  "audio/wav"              },
    { "flac", "audio/ogg"              },
    { "pdf",  "application/pdf"        },
    { "ttf",  "application/x-font-ttf" },
    { "ttc",  "application/x-font-ttf" }
};

void vHTTPClientDelete( TCPClient_t * pxTCPClient )
{
    HTTPClient_t * pxClient = ( HTTPClient_t * ) pxTCPClient;

    /* This HTTP client stops, close / release all resources. */
    if( pxClient->xSocket != FREERTOS_NO_SOCKET ) {
        FreeRTOS_FD_CLR( pxClient->xSocket, pxClient->pxParent->xSocketSet, eSELECT_ALL );
        if(!pxClient->bits.bUpgraded) {
            FreeRTOS_closesocket( pxClient->xSocket );
        }
        pxClient->xSocket = FREERTOS_NO_SOCKET;
    }

    if(pxClient->request != NULL) {
        // free up request parsing resources
        parse_request_release(pxClient->request);
        pxClient->request = NULL;
    }

    prvFileClose( pxClient );
}
/*-----------------------------------------------------------*/

static void prvFileClose( HTTPClient_t * pxClient )
{
    if( pxClient->pxFileHandle != NULL ) {
        FreeRTOS_printf( ( "Closing file: %s\n", pxClient->pcCurrentFilename ) );
        ff_fclose( pxClient->pxFileHandle );
        pxClient->pxFileHandle = NULL;
    }
}
/*-----------------------------------------------------------*/

static BaseType_t prvSendReply( HTTPClient_t * pxClient,
                                BaseType_t xCode )
{
    struct xTCP_SERVER * pxParent = pxClient->pxParent;
    BaseType_t xRc;

    /* A normal command reply on the main socket (port 21). */
    char * pcBuffer = pxParent->pcFileBuffer;

    xRc = snprintf( pcBuffer, sizeof( pxParent->pcFileBuffer ),
                    "HTTP/1.1 %d %s\r\n"
#if USE_HTML_CHUNKS
                    "Transfer-Encoding: chunked\r\n"
#endif
                    "Content-Type: %s\r\n"
                    "Connection: keep-alive\r\n"
                    "%s\r\n",
                    ( int ) xCode,
                    webCodename( xCode ),
                    pxClient->pcContentsType[ 0 ] ? pxClient->pcContentsType : "text/html",
                    pxClient->pcExtraContents );

    pxClient->pcContentsType[ 0 ] = '\0';
    pxClient->pcExtraContents[ 0 ] = '\0';

    xRc = FreeRTOS_send( pxClient->xSocket, ( const void * ) pcBuffer, xRc, 0 );
    pxClient->bits.bReplySent = pdTRUE_UNSIGNED;

    return xRc;
}
/*-----------------------------------------------------------*/

static BaseType_t prvSendFile( HTTPClient_t * pxClient )
{
    size_t uxSpace;
    size_t uxCount;
    BaseType_t xRc = 0;

    if( pxClient->bits.bReplySent == pdFALSE_UNSIGNED ) {
        pxClient->bits.bReplySent = pdTRUE_UNSIGNED;

        strcpy( pxClient->pcContentsType, pcGetContentsType( pxClient->pcCurrentFilename ) );
        snprintf( pxClient->pcExtraContents, sizeof( pxClient->pcExtraContents ),
                  "Content-Length: %d\r\n", ( int ) pxClient->uxBytesLeft );

        // FIXME this presumes it is entirely sent, which is false
        xRc = prvSendReply( pxClient, WEB_REPLY_OK );
    }

    if( xRc >= 0 ) {
        do {
            uxSpace = FreeRTOS_tx_space( pxClient->xSocket );

            if( pxClient->uxBytesLeft < uxSpace ) {
                uxCount = pxClient->uxBytesLeft;
            } else {
                uxCount = uxSpace;
            }

            if( uxCount > 0u ) {
                if( uxCount > sizeof( pcFILE_BUFFER ) ) {
                    uxCount = sizeof( pcFILE_BUFFER );
                }

                ff_fread( pcFILE_BUFFER, 1, uxCount, pxClient->pxFileHandle );
                pxClient->uxBytesLeft -= uxCount;

                // FIXME this presumes it is entirely sent, which is false
                xRc = FreeRTOS_send( pxClient->xSocket, pcFILE_BUFFER, uxCount, 0 );

                if( xRc < 0 ) {

                    break;
                }
            }
        } while( uxCount > 0u );
    }

    if( pxClient->uxBytesLeft == 0u ) {
        /* Writing is ready, no need for further 'eSELECT_WRITE' events. */
        FreeRTOS_FD_CLR( pxClient->xSocket, pxClient->pxParent->xSocketSet, eSELECT_WRITE );
        prvFileClose( pxClient );
    } else {
        /* Wake up the TCP task as soon as this socket may be written to. */
        FreeRTOS_FD_SET( pxClient->xSocket, pxClient->pxParent->xSocketSet, eSELECT_WRITE );
    }

    return xRc;
}
/*-----------------------------------------------------------*/

// new httpserver handling with llhttp parser
extern int parse_request(const char *buf, uint32_t len, void *p_request);
extern void *parse_request_create();
extern int parse_request_get_method(void *request);
extern const char *parse_request_get_url(void *request);
extern const char *parse_request_get_header(const char *hdr, void *request);
extern const int parse_request_get_headers(const char *hdrs[], int len, void *request);

static BaseType_t prvHandleFileRequest(HTTPClient_t *pxClient)
{
    // its a file request
    BaseType_t xRc;

    if(parse_request_get_method(pxClient->request) == 1) { // GET
        const char *url = parse_request_get_url(pxClient->request);

        if(strcmp(url, "/") == 0) {
            url = "/index.html";
        }

        snprintf( pxClient->pcCurrentFilename, sizeof( pxClient->pcCurrentFilename ), "%s%s%s",
                  pxClient->pcRootDir,
                  "",
                  url );

        pxClient->pxFileHandle = ff_fopen( pxClient->pcCurrentFilename, "rb" );

        FreeRTOS_printf( ( "Open file '%s': %s-(%d) %s\n", pxClient->pcCurrentFilename,
                           pxClient->pxFileHandle != NULL ? "Ok" : "Failed", stdioGET_ERRNO(), strerror( stdioGET_ERRNO() ) ) );

        if( pxClient->pxFileHandle == NULL ) {
            strcpy( pxClient->pcExtraContents, "Content-Length: 0\r\n" );
            /* "404 File not found". */
            xRc = prvSendReply( pxClient, WEB_NOT_FOUND );
        } else {
            pxClient->uxBytesLeft = ( size_t ) getFileSize(pxClient->pxFileHandle);
            xRc = prvSendFile( pxClient );
        }

    } else {
        // Not GET
        strcpy( pxClient->pcExtraContents, "Content-Length: 0\r\n" );
        xRc = prvSendReply( pxClient, WEB_BAD_REQUEST );
    }

    return xRc;
}

#include "base64.h"
#include "sha1.h"
static BaseType_t handle_incoming_websocket(HTTPClient_t *pxClient, const char *keystr)
{
    unsigned char encoded_key[32];
    int len = strlen(keystr);
    const char WS_GUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char key[len + sizeof(WS_GUID)];

    /* Concatenate key */
    memcpy(key, keystr, len);
    strcpy(&key[len], WS_GUID); // FIXME needs to be length checked
    //printf("Resulting key: %s\n", key);

    unsigned char sha1sum[20];
    mbedtls_sha1((unsigned char *) key, sizeof(WS_GUID) + len - 1, sha1sum);
    /* Base64 encode */
    unsigned int olen;
    mbedtls_base64_encode(NULL, 0, &olen, sha1sum, 20); //get length
    int ok = mbedtls_base64_encode(encoded_key, sizeof(encoded_key), &olen, sha1sum, 20);
    if (ok == 0) {
        encoded_key[olen] = '\0';
        //printf("Base64 encoded: %s\n", encoded_key);
    } else {
        printf("handle_incoming_websocket: base64 encode failed\n");
        return -1;
    }

    strcpy( pxClient->pcExtraContents, "HTTP/1.1 101 Switching Protocols\r\n" );
    strcat( pxClient->pcExtraContents, "Upgrade: websocket\r\n" );
    strcat( pxClient->pcExtraContents, "Connection: Upgrade\r\n" );
    strcat( pxClient->pcExtraContents, "Sec-WebSocket-Accept: " );
    strcat( pxClient->pcExtraContents, (char *)encoded_key );
    strcat( pxClient->pcExtraContents, "\r\n");

    // TODO we need to make sure this gets entirely sent
    len = strlen(pxClient->pcExtraContents);
    BaseType_t xRc = FreeRTOS_send( pxClient->xSocket, ( const void * ) pxClient->pcExtraContents, len, 0);

    if(xRc == len) {
        printf("handle_incoming_websocket: websocket now open\n");
        xRc = 0;
    } else if(xRc >= 0) {
        printf("handle_incoming_websocket: more to send\n");
        // TODO handle this
    }
    return xRc;
}

static BaseType_t prvHandleUpgradeRequest(HTTPClient_t *pxClient, char *buf, uint32_t len)
{
    // its a websocket upgrade request
    BaseType_t xRc;
    pxClient->bits.ulFlags = 0; // clear flags

    if(parse_request_get_method(pxClient->request) == 1) { // GET
        const char *url = parse_request_get_url(pxClient->request);

        if(strcmp(url, "/command") == 0 || strcmp(url, "/upload") == 0) {
            const char *v = parse_request_get_header("Upgrade", pxClient->request);
            if(v != NULL && strcmp(v, "websocket") == 0) {
                v = parse_request_get_header("Sec-WebSocket-Key", pxClient->request);
                if(v != NULL) {
                    if(handle_incoming_websocket(pxClient, v) == 0) {
                        // TODO at this point the socket no longer belongs to the HTTP server
                        // so we need to pretend the client is closed, so HTTPD no longer
                        // deals with it, but we need to leave the socket open
                        // Fireoff a thread to manage the websocket data
                        // close the http server client instance except leave the socket alone

                        // if(strcmp(url, "/command") == 0) {
                        //     handle_command(pxClient, buf, len);
                        // } else {
                        //     handle_upload(pxClient, buf, len);
                        // }
                        return 0;
                    }
                }
            }

            printf("prvHandleUpgradeRequest: badly formatted websocket request\n");
            strcpy( pxClient->pcExtraContents, "Content-Length: 0\r\n" );
            xRc = prvSendReply( pxClient, WEB_BAD_REQUEST );

        } else {
            strcpy( pxClient->pcExtraContents, "Content-Length: 0\r\n" );
            /* "404 File not found". */
            xRc = prvSendReply( pxClient, WEB_NOT_FOUND );
        }

    } else {
        // Not GET
        strcpy( pxClient->pcExtraContents, "Content-Length: 0\r\n" );
        xRc = prvSendReply( pxClient, WEB_BAD_REQUEST );
    }

    // we need to check xRc to make sure everythign was sent ok before we shutdown
    (void) xRc;
    return -1;
}

BaseType_t xHTTPClientWork( TCPClient_t * pxTCPClient )
{
    BaseType_t xRc;
    HTTPClient_t * pxClient = ( HTTPClient_t * ) pxTCPClient;

    if(pxClient->xSocket == FREERTOS_NO_SOCKET) return 0;

    if( pxClient->pxFileHandle != NULL ) {
        if((FreeRTOS_FD_ISSET(pxClient->xSocket, pxClient->pxParent->xSocketSet) & eSELECT_WRITE) != 0) {
            // continue sending file
            xRc= prvSendFile( pxClient );
            if(xRc < 0) {
                FreeRTOS_printf( ("xHTTPClientWork: prvSendFile failed: %d\n", xRc) );
                return xRc; // if it got an error we can stop here
            }
        }
    }

    // check if the socket has a read
    if((FreeRTOS_FD_ISSET(pxClient->xSocket, pxClient->pxParent->xSocketSet) & (eSELECT_READ | eSELECT_EXCEPT)) == 0) {
        // no it doesn't so nothing to do here
        return 0;
    }

    // we have something to read
    xRc = FreeRTOS_recv( pxClient->xSocket, ( void * ) pcCOMMAND_BUFFER, sizeof( pcCOMMAND_BUFFER ), 0 );

    if( xRc > 0 ) {
        if(pxClient->request == NULL) {
            pxClient->request = parse_request_create();
            if(pxClient->request == NULL) {
                return -pdFREERTOS_ERRNO_ENOMEM;
            }
        }
        int needmore = parse_request(pcCOMMAND_BUFFER, xRc, pxClient->request);
        if(needmore == 1) {
            // we have the request and headers
            pxClient->bits.ulFlags = 0; // clear flags
            FreeRTOS_printf( ("xHTTPClientWork: file request\n") );
            xRc = prvHandleFileRequest(pxClient);
            parse_request_release(pxClient->request);
            pxClient->request = NULL;

        } else if(needmore >= 2) {
            // we have a web socket request
            pxClient->bits.ulFlags = 0; // clear flags
            FreeRTOS_printf( ("xHTTPClientWork: upgrade request\n") );
            uint32_t off = needmore - 2;
            xRc = prvHandleUpgradeRequest(pxClient, &pcCOMMAND_BUFFER[off], xRc - off);
            parse_request_release(pxClient->request);
            pxClient->request = NULL;
            // we need to leave the socket open, and delete the rest
            if(xRc >= 0) pxClient->bits.bUpgraded = 1;
            xRc = -1;

        } else if(needmore == 0) {
            // go around again
            return xRc;

        } else if(needmore < 0) {
            FreeRTOS_printf( ("xHTTPClientWork: parser error = %d\n", needmore) );
            parse_request_release(pxClient->request);
            pxClient->request = NULL;
            xRc = -1;
        }

    } else if( xRc < 0 ) {
        /* The connection will be closed and the client will be deleted. */
        FreeRTOS_printf( ( "xHTTPClientWork: rc = %ld\n", xRc ) );
        if(pxClient->request != NULL) {
            parse_request_release(pxClient->request);
            pxClient->request = NULL;
        }
    }

    return xRc;
}

/*-----------------------------------------------------------*/

static const char * pcGetContentsType( const char * apFname )
{
    const char * slash = NULL;
    const char * dot = NULL;
    const char * ptr;
    const char * pcResult = "text/html";
    BaseType_t x;

    for( ptr = apFname; *ptr; ptr++ ) {
        if( *ptr == '.' ) {
            dot = ptr;
        }

        if( *ptr == '/' ) {
            slash = ptr;
        }
    }

    if( dot > slash ) {
        dot++;

        for( x = 0; x < ARRAY_SIZE( pxTypeCouples ); x++ ) {
            if( strcasecmp( dot, pxTypeCouples[ x ].pcExtension ) == 0 ) {
                pcResult = pxTypeCouples[ x ].pcType;
                break;
            }
        }
    }

    return pcResult;
}

#endif /* ipconfigUSE_HTTP */
