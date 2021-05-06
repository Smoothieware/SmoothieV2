#include "main.h"
#include "OutputStream.h"
#include "RingBuffer.h"
#include "MessageQueue.h"

#include "FreeRTOS.h"
#include "timers.h"
#include "queue.h"
#include "task.h"
#include "semphr.h"

#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

#include <set>

//#define DEBUG_PRINTF(...)
#define DEBUG_PRINTF printf

#define MAX_SERV 3
#define BUFSIZE 256
#define MAGIC 0x6013D852
struct shell_state_t {
    Socket_t socket;
    SocketSet_t ss;
    struct freertos_sockaddr cliaddr;
    socklen_t clilen;
    OutputStream *os;
    QueueHandle_t tx_queue;
    size_t wr_off;
    char line[132];
    size_t cnt;
    bool discard;
    uint32_t magic;
};
using shell_t = struct shell_state_t;
static std::set<shell_t*> shells;

using tx_msg_t = struct{char buf[128]; size_t size;};

// Stores OutputStreams that need to be deleted when done
using gc_t = std::tuple<OutputStream*, QueueHandle_t>;
static RingBuffer<gc_t, MAX_SERV*2> gc;

// callback from command thread to write data to the socket
// we use a queue to get the shell thread to do the write
static int write_back(shell_t *p_shell, const char *rbuf, size_t len)
{
    if(p_shell->magic != MAGIC) {
        // we probably went away
        printf("shell: write_back: ERROR magic was bad\n");
        return 0;
    }
    size_t sz= len;
    tx_msg_t msg;
    while(sz > 0) {
        if(p_shell->os->is_closed()) break;
        size_t n= std::min(sz, sizeof(msg.buf));
        memcpy(msg.buf, rbuf, n);
        msg.size= n;
        if(xQueueSend(p_shell->tx_queue, (void *)&msg, portMAX_DELAY) != pdTRUE) {
            return 0;
        }
        sz-=n;
        rbuf += n;
    }
    FreeRTOS_FD_SET(p_shell->socket, p_shell->ss, eSELECT_WRITE);
    return len;
}

/**************************************************************
 * Close the socket and remove this shell_t from the list.
 **************************************************************/
static void close_shell(shell_t *p_shell)
{
    DEBUG_PRINTF("shell: closing shell connection: %p\n", p_shell->socket);

    p_shell->magic= 0; // safety

    FreeRTOS_FD_CLR(p_shell->socket, p_shell->ss, eSELECT_ALL);
    FreeRTOS_closesocket(p_shell->socket);

    // if we delete the OutputStream now and command thread is still outputting stuff we will crash
    // it needs to stick around until the command has completed
    // this is also true of the tx_queue
    if(p_shell->os->is_done()) {
        DEBUG_PRINTF("shell: releasing output stream: %p\n", p_shell->os);
        delete p_shell->os;
        if(p_shell->tx_queue != 0) vQueueDelete(p_shell->tx_queue);

    }else{
        DEBUG_PRINTF("shell: delaying releasing output stream: %p\n", p_shell->os);
        p_shell->os->set_closed();
        xQueueReset(p_shell->tx_queue);
        gc.push_back({p_shell->os, p_shell->tx_queue});
    }

    // Free shell state
    if(shells.erase(p_shell) != 1) {
        printf("shell: erasing shell not found\n");
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
        auto t= gc.peek_front();
        OutputStream *os= std::get<0>(t);
        if(os->is_done()) {
            auto td= gc.pop_front();
            delete std::get<0>(td);
            vQueueDelete(std::get<1>(td));
            DEBUG_PRINTF("shell: releasing output stream: %p\n", os);
        } else {
            // if this is not done then we presume the newer ones aren't either
            break;
        }
    }
}

// process any write requests coming from other threads
// return false if the shell closed
static bool process_writes(shell_t *p_shell)
{
    tx_msg_t msg;
    if(xQueuePeek(p_shell->tx_queue, &msg, 0) == pdTRUE) {
        int sz= msg.size - p_shell->wr_off;
        BaseType_t n= FreeRTOS_send(p_shell->socket, msg.buf+p_shell->wr_off, sz, 0);
        if(n < 0) {
            printf("shell: error writing: %ld\n", n);
            close_shell(p_shell);
            return false;
        }
        if(n == sz) {
            // all written, so remove from queue
            xQueueReceive(p_shell->tx_queue, &msg, 0);
            p_shell->wr_off= 0;

        }else{
            // can't write anymore at the moment try again next time
            p_shell->wr_off += n;
        }
    }
    return true;
}

static volatile bool abort_shell= false;
static void shell_thread(void *arg)
{
    Socket_t listenfd;
    struct freertos_sockaddr shell_saddr;
    SocketSet_t socketset;

    printf("Network: Shell thread started\n");

    memset(&shell_saddr, 0, sizeof (shell_saddr));

    /* First acquire our socket for listening for connections */
    listenfd = FreeRTOS_socket(FREERTOS_AF_INET, FREERTOS_SOCK_STREAM, FREERTOS_IPPROTO_TCP);
    configASSERT( listenfd != FREERTOS_INVALID_SOCKET );
    if(listenfd != FREERTOS_INVALID_SOCKET) {
        printf("shell_thread: ERROR: Socket create failed\n");
        return;
    }

    shell_saddr.sin_port = FreeRTOS_htons(23);
    BaseType_t err;
    if ((err=FreeRTOS_bind(listenfd, &shell_saddr, sizeof (shell_saddr))) != 0) {
        printf("shell_thread: ERROR: Socket bind failed: %ld\n", err);
        return;
    }

    /* Put socket into listening mode */
    if ((err=FreeRTOS_listen(listenfd, MAX_SERV)) != 0) {
        printf("shell_thread: ERROR: Listen failed: %ld", err);
        return;
    }

    // OutputStream garbage collector timer
    TimerHandle_t timer_handle= xTimerCreate("osgarbage", pdMS_TO_TICKS(1000), pdTRUE, nullptr, os_garbage_collector);
    if(timer_handle == NULL || xTimerStart(timer_handle, 1000) != pdPASS) {
        printf("shell_thread: WARNING: Failed to start the osgarbage timer\n");
    }

    TickType_t timeout= portMAX_DELAY; // pdMS_TO_TICKS(1000); // 1 second

    // create a socket set to handle all the clients
    socketset= FreeRTOS_CreateSocketSet( );
    configASSERT( socketset != NULL );

    FreeRTOS_FD_SET(listenfd, socketset, eSELECT_READ | eSELECT_EXCEPT);

    // Wait forever for network input: This could be connections or data
    while(!abort_shell) {
        // Wait for data or a new connection
        BaseType_t i= FreeRTOS_select(socketset, timeout);

        if (i == 0) {
            continue;
        }

        if(i < 0) {
            // we got an error
            printf("shell: ERROR: select returned an error: %ld\n", i);
            continue;
        }

        // At least one descriptor is ready
        if (FreeRTOS_FD_ISSET(listenfd, socketset)) {
            // We have a new connection request create a new shell
            shell_t *p_shell = (shell_t *)malloc(sizeof(shell_t));
            if(p_shell != nullptr) {
                p_shell->socket= FreeRTOS_accept(listenfd, &p_shell->cliaddr, &p_shell->clilen);
                if (p_shell->socket == FREERTOS_INVALID_SOCKET) {
                    free(p_shell);
                    printf("shell: accept socket error\n");

                } else {
                    // add shell state to our set of shells
                    shells.insert(p_shell);

                    DEBUG_PRINTF("shell: accepted shell connection: %p\n", p_shell->socket);

                    // initialise command buffer state
                    p_shell->cnt= 0;
                    p_shell->wr_off= 0;
                    p_shell->discard= false;
                    p_shell->os= new OutputStream([p_shell](const char *ibuf, size_t ilen) { return write_back(p_shell, ibuf, ilen); });

                    // we need it to know this so we can clr itself
                    p_shell->ss= socketset;

                    // setup tx queue so we keep reads and writes in the same thread
                    p_shell->tx_queue= xQueueCreate(4, sizeof(tx_msg_t));
                    if(p_shell->tx_queue == 0) {
                        // Failed to create the queue.
                        printf("shell: failed to create tx_queue - out of memory\n");
                        close_shell(p_shell);

                    } else {
                        // add it to the socket set
                        FreeRTOS_FD_SET(p_shell->socket, p_shell->ss, eSELECT_READ | eSELECT_EXCEPT);

                        //output_streams.push_back(p_shell->os);
                        p_shell->magic= MAGIC;
                        static const char msg[]= "Welcome to the Smoothie Shell\n";
                        configASSERT((unsigned long)FreeRTOS_maywrite(p_shell->socket) >= sizeof(msg));
                        BaseType_t n= FreeRTOS_send(p_shell->socket, msg, sizeof(msg), 0);
                        configASSERT(n == sizeof(msg));
                    }
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
                printf("shell: out of memory on listen\n");
            }
        }

        // Go through list of connected clients and process write requests
        // we do this first to avoid deadlock when a read request tries to write
        // can still deadlock though if there is a lot to write (eg cat file)
        for(auto p_shell : shells) {
            if (FreeRTOS_FD_ISSET(p_shell->socket, p_shell->ss) & eSELECT_WRITE) {
                // request to write data
                if(process_writes(p_shell)) {
                    if(uxQueueMessagesWaiting(p_shell->tx_queue) == 0) {
                        // if nothing left to write don't select on write ready anymore
                        FreeRTOS_FD_CLR(p_shell->socket, p_shell->ss, eSELECT_WRITE);
                    }

                }else {
                    break;
                }
            }
        }

        // check for read requests
        for (auto p_shell : shells) {
            if (FreeRTOS_FD_ISSET(p_shell->socket, p_shell->ss) & eSELECT_READ) {
                char buf[BUFSIZE];
                // This socket is ready for reading.
                int n= FreeRTOS_recv(p_shell->socket, buf, BUFSIZE, FREERTOS_MSG_DONTWAIT);
                if (n > 0) {
                    if(strncmp(buf, "quit\n", 5) == 0 || strncmp(buf, "quit\r\n", 6) == 0) {
                        FreeRTOS_send(p_shell->socket, "Goodbye!\n", 9, 0);
                        close_shell(p_shell);
                        break;
                    }
                    // this could block which would then also block any output that the
                    // command thread needs to make causing deadlock
                    // so tell it to not wait, and if it returns false it means it gave up waiting
                    // process writes while waiting so coammnad thread won't block
                    if(!process_command_buffer(n, buf, p_shell->os, p_shell->line, p_shell->cnt, p_shell->discard, false)) {
                        // and keep trying to resubmit, this will yield for about 100ms
                        while(!send_message_queue(p_shell->line, p_shell->os, false)) {
                            // process any writes
                            if(!process_writes(p_shell)) {
                                p_shell= nullptr;
                                // the shell closed here we may lose the last command sent
                                break;
                            }
                        }
                        if(p_shell == nullptr) break;
                    }

                } else {
                    DEBUG_PRINTF("shell: got close on read: %d\n", n);
                    close_shell(p_shell);
                    break;
                }
            }
        }
    }

    for (auto p_shell : shells){
        close_shell(p_shell);
    }

    FreeRTOS_closesocket(listenfd);
    xTimerDelete(timer_handle, 0);
    FreeRTOS_DeleteSocketSet(socketset);
}

void shell_init(void)
{
    // make same priority as other comms threads
    xTaskCreate(shell_thread, "shell_thread", 350, NULL, COMMS_PRI, (xTaskHandle *) NULL);
}

void shell_close()
{
    abort_shell= true;
}
