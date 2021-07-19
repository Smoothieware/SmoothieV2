#include "main.h"
#include "OutputStream.h"
#include "RingBuffer.h"
#include "MessageQueue.h"

#include "FreeRTOS.h"
#include "timers.h"
#include "task.h"
#include "semphr.h"

#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

#include <set>
#include <vector>

#define MAX_SERV 3
#define BUFSIZE 256
#define MAGIC 0x6013D852
struct shell_state_t {
    Socket_t socket;
    SocketSet_t ss;
    struct freertos_sockaddr cliaddr;
    socklen_t clilen;
    OutputStream *os;
    char line[132];
    size_t cnt;
    bool discard;
    uint32_t magic;
};
using shell_t = struct shell_state_t;
static std::set<shell_t*> shells;

// Stores OutputStreams that need to be deleted when done
using gc_t = OutputStream*;
static RingBuffer<gc_t, MAX_SERV * 2> gc;

// callback from command thread to write data to the socket
// we can write it here, however only one thread can write to a socket
// this should be ok as it should only be the command thread
// this blocks until all data is written
static int write_back(shell_t *p_shell, const char *rbuf, size_t len)
{
    if(p_shell->magic != MAGIC) {
        // we probably went away
        printf("ERROR: shell: write_back() magic was bad\n");
        return 0;
    }
    size_t sz = len;
    while(sz > 0) {
        if(p_shell->os->is_closed()) return 0;
        BaseType_t n = FreeRTOS_send(p_shell->socket, rbuf, sz, 0);
        if(n < 0) {
            printf("DEBUG: shell: error writing: %ld\n", n);
            // send signal to quit this socket
            FreeRTOS_SignalSocket(p_shell->socket);
            return 0;
        }

        sz -= n;
        rbuf += n;
    }
    return len;
}

/**************************************************************
 * Close the socket and remove this shell_t from the list.
 **************************************************************/
static void close_shell(shell_t *p_shell)
{
    FreeRTOS_printf( ("shell: closing shell connection: %p\n", p_shell->socket) );

    p_shell->magic = 0; // safety

    FreeRTOS_FD_CLR(p_shell->socket, p_shell->ss, eSELECT_ALL);
    FreeRTOS_closesocket(p_shell->socket);

    // if we delete the OutputStream now and command thread is still outputting stuff we will crash
    // it needs to stick around until the command has completed
    // this is also true of the tx_queue
    if(p_shell->os->is_done()) {
        FreeRTOS_printf( ("shell: releasing output stream: %p\n", p_shell->os) );
        delete p_shell->os;

    } else {
        FreeRTOS_printf( ("shell: delaying releasing output stream: %p\n", p_shell->os) );
        p_shell->os->set_closed();
        gc.push_back(p_shell->os);
    }

    // Free shell state
    if(shells.erase(p_shell) != 1) {
        printf("DEBUG: shell: erasing shell not found\n");
    }
    free(p_shell);
}

// This will delete any OutputStreams that are done
// we need to do this so that we don't crash when an OutputStream is deleted before it is done
// ditto with the tx_queue
static void os_garbage_collector( TimerHandle_t xTimer )
{
    while(!gc.empty()) {
        // we only check the oldest, presuming it will be done before newer ones
        OutputStream *os = gc.peek_front();
        if(os->is_done()) {
            os = gc.pop_front();
            delete os;
            FreeRTOS_printf( ("shell: releasing output stream: %p\n", os) );
        } else {
            // if this is not done then we presume the newer ones aren't either
            break;
        }
    }
}

static volatile bool abort_shell = false;
static Socket_t listenfd;
static void shell_thread(void *arg)
{
    struct freertos_sockaddr shell_saddr;
    SocketSet_t socketset;

    printf("DEBUG: Network: Shell thread started\n");

    memset(&shell_saddr, 0, sizeof (shell_saddr));

    /* First acquire our socket for listening for connections */
    listenfd = FreeRTOS_socket(FREERTOS_AF_INET, FREERTOS_SOCK_STREAM, FREERTOS_IPPROTO_TCP);
    configASSERT( listenfd != FREERTOS_INVALID_SOCKET );
    if(listenfd == FREERTOS_INVALID_SOCKET) {
        printf("ERROR: shell_thread: Socket create failed\n");
        return;
    }

    shell_saddr.sin_port = FreeRTOS_htons(23);
    BaseType_t err;
    if ((err = FreeRTOS_bind(listenfd, &shell_saddr, sizeof (shell_saddr))) != 0) {
        printf("ERROR: shell_thread: Socket bind failed: %ld\n", err);
        return;
    }

    /* Put socket into listening mode */
    if ((err = FreeRTOS_listen(listenfd, MAX_SERV)) != 0) {
        printf("ERROR: shell_thread: Listen failed: %ld", err);
        return;
    }

    // OutputStream garbage collector timer
    TimerHandle_t timer_handle = xTimerCreate("osgarbage", pdMS_TO_TICKS(1000), pdTRUE, nullptr, os_garbage_collector);
    if(timer_handle == NULL || xTimerStart(timer_handle, 1000) != pdPASS) {
        printf("WARNING: shell_thread: WARNING: Failed to start the osgarbage timer\n");
    }


    // create a socket set to handle all the clients
    socketset = FreeRTOS_CreateSocketSet( );
    configASSERT( socketset != NULL );

    FreeRTOS_FD_SET(listenfd, socketset, eSELECT_READ | eSELECT_EXCEPT);

    TickType_t timeout = portMAX_DELAY; // pdMS_TO_TICKS(1000); // 1 second
    // Wait forever for network input: This could be connections or data
    while(!abort_shell) {
        // Wait for data or a new connection
        BaseType_t i = FreeRTOS_select(socketset, timeout);

        FreeRTOS_printf( ("DEBUG: shell: select returned %ld\n", i) );

        if(abort_shell) break;

        if (i == 0) {
            // timeout
            continue;
        }

        if(i < 0) {
            // we got an error
            printf("ERROR: shell: select returned an error: %ld\n", i);
            break;
        }

        // At least one descriptor is ready
        if (FreeRTOS_FD_ISSET(listenfd, socketset)) {
            // We have a new connection request create a new shell
            shell_t *p_shell = (shell_t *)malloc(sizeof(shell_t));
            if(p_shell != nullptr) {
                p_shell->socket = FreeRTOS_accept(listenfd, &p_shell->cliaddr, &p_shell->clilen);
                if (p_shell->socket == FREERTOS_INVALID_SOCKET) {
                    free(p_shell);
                    printf("shell: accept socket error\n");

                } else {
                    // add shell state to our set of shells
                    shells.insert(p_shell);

                    FreeRTOS_printf( ("shell: accepted shell connection: %p\n", p_shell->socket) );

                    // initialise command buffer state
                    p_shell->cnt = 0;
                    p_shell->discard = false;
                    p_shell->os = new OutputStream([p_shell](const char *ibuf, size_t ilen) { return write_back(p_shell, ibuf, ilen); });

                    // we need it to know this so it can clr itself on close
                    p_shell->ss = socketset;

                    // add it to the socket set
                    FreeRTOS_FD_SET(p_shell->socket, p_shell->ss, eSELECT_READ | eSELECT_EXCEPT);

                    //output_streams.push_back(p_shell->os);
                    p_shell->magic = MAGIC;

                    // the only time this thread writes to the socket
                    static const char msg[] = "Welcome to the Smoothie Shell\n";
                    configASSERT((unsigned long)FreeRTOS_maywrite(p_shell->socket) >= sizeof(msg));
                    FreeRTOS_send(p_shell->socket, msg, sizeof(msg), 0);
                }

            } else {
                /* No memory to accept connection. Just accept and then close */
                Socket_t sock;
                struct freertos_sockaddr cliaddr;
                socklen_t clilen;

                sock = FreeRTOS_accept(listenfd, &cliaddr, &clilen);
                if (sock != FREERTOS_INVALID_SOCKET) {
                    FreeRTOS_closesocket(sock);
                }
                printf("ERROR: shell: out of memory on listen\n");
            }
        }

        // check for read requests
        for (auto p_shell : shells) {
            if (FreeRTOS_FD_ISSET(p_shell->socket, p_shell->ss)) {
                FreeRTOS_printf( ("DEBUG: shell: pshell %p was SET: %02lX\n", p_shell, FreeRTOS_FD_ISSET(p_shell->socket, p_shell->ss)) );
                char buf[BUFSIZE];
                // This socket is ready for reading.
                int n = FreeRTOS_recv(p_shell->socket, buf, BUFSIZE, FREERTOS_MSG_DONTWAIT);
                if (n > 0) {
                    #if 0
                    // we can't do this here, it needs to be a net command
                    if(strncmp(buf, "quit\n", 5) == 0 || strncmp(buf, "quit\r\n", 6) == 0) {
                        FreeRTOS_send(p_shell->socket, "Goodbye!\n", 9, 0);
                        close_shell(p_shell);
                        break;
                    }
                    #endif

                    // this could block which would then also block any output that the
                    // command thread needs to make causing deadlock
                    // so tell it to not wait, and if it returns false it means it gave up waiting
                    if(!process_command_buffer(n, buf, p_shell->os, p_shell->line, p_shell->cnt, p_shell->discard, false)) {
                        // and keep trying to resubmit, this will yield for about 100ms
                        while(!send_message_queue(p_shell->line, p_shell->os, false)) {
                            // make sure we are still connected
                            if(FreeRTOS_issocketconnected(p_shell->socket) != pdTRUE) {
                                printf("DEBUG: shell: socket disconnected while waiting for command thread\n");
                                close_shell(p_shell);
                                break;
                            }
                        }
                    }

                } else if(n < 0) {
                    FreeRTOS_printf( ("shell: got error on read: %d\n", n) );
                    close_shell(p_shell);
                    break;
                }
            }
        }
    }

    printf("DEBUG: Network: Shell thread request end\n");

    // we collect pointers to the shells from the set as close_shell will delete itself from the set
    std::vector<shell_t*> psl;
    for (auto p_shell : shells) {
        psl.push_back(p_shell);
    }
    // now close them all
    for(auto s : psl) {
        close_shell(s);
    }

    FreeRTOS_DeleteSocketSet(socketset);
    FreeRTOS_closesocket(listenfd);
    xTimerDelete(timer_handle, 0);
    printf("DEBUG: Network: Shell thread ended\n");
    vTaskDelete(NULL);
}

void shell_init(void)
{
    // make same priority as other comms threads
    xTaskCreate(shell_thread, "shell_thread", 350, NULL, tskIDLE_PRIORITY + COMMS_PRI, NULL);
}

void shell_deinit()
{
    abort_shell = true;
    FreeRTOS_SignalSocket(listenfd);
}
