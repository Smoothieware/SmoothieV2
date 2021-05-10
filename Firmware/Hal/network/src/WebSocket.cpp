// WebSocket.cpp

#include "OutputStream.h"
#include "main.h"

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

/* FreeRTOS Protocol includes. */
#include "FreeRTOS_TCP_server.h"
#include "FreeRTOS_server_private.h"

#include <string>
#include <map>

class WebSocketState
{

public:
    WebSocketState(Socket_t c, bool cmd) : conn(c), is_command(cmd) {}
    ~WebSocketState() {  }
    Socket_t conn;
    bool is_command;
    std::string data;
    const uint8_t *payload;
    char line[132];
    using READ_STATE_T = enum{HEADER, BODY, DONE};
    READ_STATE_T read_state{HEADER};
    size_t cnt{0};
    bool discard{false};
    OutputStream *os{nullptr};
    uint16_t o, m, plen;
};

/* Read data from a websocket and decode it.
    This is state based and returns:
    0 when it needs more data
    1-n when it has a packet of size n
    -1 when it is the end
    -2 when there is an error
*/
static BaseType_t websocket_decode(WebSocketState& state, char *buf, size_t len)
{
    if(state.read_state == WebSocketState::HEADER) {
        // We need at least 6 bytes
        if(state.data.size() < 6) return 0;

        uint8_t *hdr= (uint8_t*)state.data.data();

        // parse initial 6 bytes of header
        if((hdr[0] & 0x80) == 0) {
            // must have fin bit
            // TODO need to handle this correctly, as it would notify the end of a large file upload
            printf("websocket_read: WARNING FIN bit not set\n");
            return -2;
        }

        if((hdr[1] & 0x80) == 0) {
            // must have mask bit
            printf("websocket_read: MASK bit not set\n");
            return -2;
        }

        state.plen = hdr[1] & 0x7F;
        if(state.plen < 126) {
            state.o = 6;
            state.m = 2;
        } else if(state.plen == 126) {
            state.plen = (hdr[2] << 8) | (hdr[3] & 0xFF);
            state.o = 8;
            state.m = 4;
        } else {
            if(state.data.size() < 10) return 0; // need more header bytes
            uint32_t s1, s2;
            s1 = ((uint32_t)hdr[2] << 24) | ((uint32_t)hdr[3] << 16) | ((uint32_t)hdr[4] << 8) | ((uint32_t)hdr[5] & 0xFF);
            s2 = ((uint32_t)hdr[6] << 24) | ((uint32_t)hdr[7] << 16) | ((uint32_t)hdr[8] << 8) | ((uint32_t)hdr[9] & 0xFF);
            printf("websocket_read: unsupported length: %d - %08lX %08lX\n", state.plen, s1, s2);
            return -2;
        }

        state.read_state= WebSocketState::BODY;
    }

    if(len < state.plen) {
        printf("websocket_read: ERROR provided buffer is not big enough\n");
        return -2;
    }

    // make sure we have the full payload
    if(state.data.size() < state.o+state.plen) return 0;

    const uint8_t *hdr= (const uint8_t *)state.data.data(); // pointer to header

    // point payload at the payload part
    const uint8_t *payload= (const uint8_t *)(state.data.data()+state.o);

    // now unmask the data
    uint8_t opcode = hdr[0] & 0x0F;
    switch (opcode) {
        case 0x00: // continuation
        case 0x01: // text
        case 0x02: // bin
            /* unmask */
            for (int i = 0; i < state.plen; i++) {
                buf[i]= payload[i] ^ hdr[state.m + i % 4];
            }
            break;
        case 0x08: // close
            return -1;

        default:
            printf("websocket_read: unhandled opcode %d\n", opcode);
            return -2;
    }

    // reset the state
    state.read_state= WebSocketState::HEADER;
    if(state.data.size() > (state.o + state.plen)) {
        // leave partial buffer in data
        state.data= state.data.substr(state.o + state.plen);

    }else{
        state.data.clear();
    }

    return state.plen;
}

static BaseType_t handle_upload(HTTPClient_t *pclient, WebSocketState& state)
{
    return -1;
}

#if 0
    BaseType_t err;
    WebsocketState state(conn);
    const uint16_t buflen = 2000;
    uint8_t *buf = (uint8_t *)malloc(buflen);
    if(buf == NULL) return ERR_MEM;
    uint16_t n;
    std::string name;
    uint32_t size= 0;
    uint32_t filecnt= 0;
    enum STATE { NAME, SIZE, BODY};
    enum STATE uploadstate= NAME;
    FILE *fp= nullptr;

    // read from connection until it closes
    while ((err = websocket_read(state, buf, buflen, n)) == ERR_OK) {
        //printf("handle_upload: got len %d, complete: %d, state: %d\n", n, state.complete, uploadstate);
        if(uploadstate == NAME) {
            name.assign((char*)buf, n);
            uploadstate= SIZE;

        } else if(uploadstate == SIZE) {
            std::string s((char*)buf, n);
            size= strtoul(s.c_str(), nullptr, 10);
            // open file, if it fails send error message and close connection
            fp= fopen(name.c_str(), "wb");
            if(fp == NULL) {
                printf("handle_upload: failed to open file for write\n");
                websocket_write(conn, "error file open failed", 22);
                break;
            }
            uploadstate= BODY;
            filecnt= 0;

        } else if(uploadstate == BODY) {
            // write to file, if it fails send error message and close connection
            size_t l= fwrite(buf, 1, n, fp);
            if(l != n) {
                printf("handle_upload: failed to write to file\n");
                websocket_write(conn, "error file write failed", 23);
                fclose(fp);
                break;
            }
#if 0
            int cnt = 0;
            for (int i = 0; i < n; ++i) {
                printf("%02X(%c) ", buf[i], buf[i] >= ' ' && buf[i] <= '~' ? buf[i] : '_');
                if(++cnt >= 8) {
                    printf("\n");
                    cnt = 0;
                }
            }
            printf("\n");
#endif
            filecnt += n;
            if(filecnt >= size) {
                // close file
                fclose(fp);
                printf("handle_upload: Done upload of file %s, of size: %lu (%lu)\n", name.c_str(), size, filecnt);
                websocket_write(conn, "ok upload successful", 17);
                uploadstate= NAME;
                break;
            }

        } else {
            printf("handle_upload: state error\n");
        }
    }
    free(buf);

    // send exit string
    netconn_write(conn, endbuf, sizeof(endbuf), NETCONN_NOCOPY);
    printf("handle_upload: websocket closing\n");
    return ERR_OK;
}
#endif

// helper that just sends the entire buffer
static bool send_all(Socket_t conn, const char *data, size_t len)
{
    size_t sent = 0;
    while(sent < len) {
        BaseType_t n;
        n = FreeRTOS_send(conn, data + sent, len - sent, 0);
        if(n < 0) {
            // we got an error
            printf("websocket send_all: error writing: %ld\n", n);
            return false;
        }
        sent += n;
        if(sent < len) {
            // yield up some time
            taskYIELD();
        }
    }
    return true;
}

// this is called from the command thread and must block until everything is written
static int websocket_write(Socket_t conn, const char *data, size_t len, uint8_t mode = 0x01)
{
    uint8_t hdr[4];
    hdr[0] = 0x80 | mode; // binary/text
    uint16_t l;
    if (len < 126) {
        l = 2;
        hdr[1] = len;
    } else if(len < 65535) {
        l = 4;
        hdr[1] = 126;
        hdr[2] = len >> 8;
        hdr[3] = len & 0xFF;
    } else {
        printf("websocket_write: buffer too big\n");
        return -1;
    }

    bool err = send_all(conn, (const char *)hdr, l);
    if(!err) {
        printf("websocket_write: error writing header\n");
        return -1;
    }
    err = send_all(conn, data, len);
    if(!err) {
        printf("websocket_write: error writing body\n");
        return -1;
    }
    return len;
}


static const char endbuf[] = {0x88, 0x02, 0x03, 0xe8};

static BaseType_t handle_command(HTTPClient_t *pclient, WebSocketState& state)
{
    const size_t bufsize = 1024;
    char buf[bufsize];
    BaseType_t rc;

    // check if the socket has any data to read
    if((FreeRTOS_FD_ISSET(state.conn, pclient->pxParent->xSocketSet) & (eSELECT_READ | eSELECT_EXCEPT)) != 0) {
        // read a buffer of data
        rc = FreeRTOS_recv(state.conn, buf, bufsize, 0);
        if(rc < 0) {
            printf("websocket handle_command: error reading: %ld\n", rc);
            return rc;
        }
        if(rc > 0) {
            state.data.append(buf, rc);
        }
    }

    // decode the websocket packet and copy it to buf
    rc= websocket_decode(state, buf, bufsize);
    if(rc == 0) {
        // we need more data
        return 0;

    }else if(rc < 0) {
        // we got an error
        // make sure command thread does not try to write to the soon to be closed (and deleted) conn
        state.os->set_closed();
        if(rc == -1) {
            // send exit string if we got one
            send_all(state.conn, endbuf, sizeof(endbuf));
        }
        printf("handle_command: websocket closing: rc=%ld\n", rc);
        return rc;
    }

    // we now have a decoded websocket payload that is rc bytes long
    process_command_buffer(rc, buf, state.os, state.line, state.cnt, state.discard);
    return rc;
}

// setup the websocket handler, for command or upload, buf/len are the excess already read and part of the payload
extern "C" BaseType_t create_websocket_handler(HTTPClient_t *pclient, int is_command, char *buf, uint32_t len)
{
    WebSocketState *ws= new WebSocketState(pclient->xSocket, is_command==1);
    pclient->websocketstate= ws;
    if(buf != nullptr && len > 0) {
        // initial payload if any
        ws->data.assign(buf, len);
    }

    if(is_command==1) {
        // create the OutputStream that commands can write to
        // FIXME this may need to stay around until command thread is done with it
        ws->os= new OutputStream([ws](const char *ibuf, size_t ilen) { return websocket_write(ws->conn, ibuf, ilen, 0x01); });
    }
    printf("create_websocket_handler: %p\n", ws);
    return 0;
}

extern "C" BaseType_t delete_websocket_handler(HTTPClient_t *pclient)
{
    if(pclient->websocketstate != nullptr) {
        WebSocketState* ws= (WebSocketState*)pclient->websocketstate;
        if(ws->os != nullptr) {
            // we may need to delay this
            delete ws->os;
            ws->os= nullptr;
        }
        delete ws;
        pclient->websocketstate= nullptr;
        printf("delete_websocket_handler: %p\n", ws);
    }
    return 0;
}

extern "C" BaseType_t websocket_work(HTTPClient_t *pclient)
{
    WebSocketState *ws= static_cast<WebSocketState*>(pclient->websocketstate);
    if(ws->is_command) {
        return handle_command(pclient, *ws);

    }else{
        return handle_upload(pclient, *ws);
    }
    return -1;
}
