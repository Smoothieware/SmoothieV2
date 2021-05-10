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
    char *buffer{nullptr};
    size_t bufsize{0};
    using READ_STATE_T = enum {HEADER, BODY};
    READ_STATE_T read_state{HEADER};
    char line[132];
    size_t cnt{0};
    size_t file_cnt{0};
    uint32_t size;
    union {
        OutputStream *os;
        FILE *fp;
    };
    uint16_t plen;
    uint16_t o;
    uint16_t m;
    bool discard{false};
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

        uint8_t *hdr = (uint8_t*)state.data.data();

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

        state.read_state = WebSocketState::BODY;
    }

    if(len < state.plen) {
        printf("websocket_read: ERROR provided buffer is not big enough\n");
        return -2;
    }

    // make sure we have the full payload
    if(state.data.size() < state.o + state.plen) return 0;

    const uint8_t *hdr = (const uint8_t *)state.data.data(); // pointer to header

    // point payload at the payload part
    const uint8_t *payload = (const uint8_t *)(state.data.data() + state.o);

    // now unmask the data
    uint8_t opcode = hdr[0] & 0x0F;
    switch (opcode) {
        case 0x00: // continuation
        case 0x01: // text
        case 0x02: // bin
            /* unmask */
            for (int i = 0; i < state.plen; i++) {
                buf[i] = payload[i] ^ hdr[state.m + i % 4];
            }
            break;
        case 0x08: // close
            return -1;

        default:
            printf("websocket_read: unhandled opcode %d\n", opcode);
            return -2;
    }

    // reset the state
    state.read_state = WebSocketState::HEADER;
    if(state.data.size() > (state.o + state.plen)) {
        // leave partial buffer in data
        state.data = state.data.substr(state.o + state.plen);

    } else {
        state.data.clear();
    }

    return state.plen;
}

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

static BaseType_t handle_upload(HTTPClient_t *pclient, WebSocketState& state)
{
    BaseType_t rc;

    // check if the socket has any data to read
    if((FreeRTOS_FD_ISSET(state.conn, pclient->pxParent->xSocketSet) & (eSELECT_READ | eSELECT_EXCEPT)) != 0) {
        // read a buffer of data
        rc = FreeRTOS_recv(state.conn, state.buffer, state.bufsize, 0);
        if(rc < 0) {
            printf("websocket handle_upload: error reading: %ld\n", rc);
            return rc;
        }
        if(rc > 0) {
            state.data.append(state.buffer, rc);
        }
    }

    // decode the websocket packet and copy it to buf
    rc = websocket_decode(state, state.buffer, state.bufsize);
    if(rc == 0) {
        // we need more data
        return 0;

    } else if(rc < 0) {
        // we got an error
        if(rc == -1) {
            // send exit string if we got one
            send_all(state.conn, endbuf, sizeof(endbuf));
        }
        printf("handle_command: websocket closing: rc=%ld\n", rc);
        return rc;
    }

    // we have a complete message
    if(state.cnt == 0) {
        // get name
        if((size_t)rc >= sizeof(state.line)-1) {
            // truncate it
            rc= sizeof(state.line)-1;
        }
        memcpy(state.line, state.buffer, rc);
        state.line[rc]= '\0';
        state.cnt = 1;
        printf("handle_upload: got file name: %s\n", state.line);

    } else if(state.cnt == 1) {
        // get file size
        std::string s(state.buffer, rc);
        state.size = strtoul(s.c_str(), nullptr, 10);
        // open file, if it fails send error message and close connection
        state.fp = fopen(state.line, "wb");
        if(state.fp == NULL) {
            printf("handle_upload: failed to open file for write: %d\n", errno);
            websocket_write(state.conn, "error file open failed", 22);
            rc = -1;
        } else {
            state.cnt = 2;
            state.file_cnt = 0;
            printf("handle_upload: got file size: %lu\n", state.size);
        }

    } else if(state.cnt == 2) {
        // write to file, if it fails send error message and close connection
        int l = fwrite(state.buffer, 1, rc, state.fp);
        if(l != rc) {
            printf("handle_upload: failed to write to file: %d\n", errno);
            websocket_write(state.conn, "error file write failed", 23);
            fclose(state.fp);
            rc = -1;
        }

        state.file_cnt += rc;
        if(state.file_cnt >= state.size) {
            // close file
            fclose(state.fp);
            printf("handle_upload: Done upload of file %s, of size: %lu (%u)\n", state.line, state.size, state.file_cnt);
            websocket_write(state.conn, "ok upload successful", 17);
            printf("handle_upload: websocket closing\n");
            // send exit string
            rc = -1;
        }

    } else {
        printf("handle_upload: state error: %d\n", state.cnt);
        rc = -1;
    }

    if(rc == -1) {
        printf("handle_upload: sending endbuf\n");
        send_all(state.conn, endbuf, sizeof(endbuf));
    }

    return rc;
}

static BaseType_t handle_command(HTTPClient_t *pclient, WebSocketState & state)
{
    BaseType_t rc;

    // check if the socket has any data to read
    if((FreeRTOS_FD_ISSET(state.conn, pclient->pxParent->xSocketSet) & (eSELECT_READ | eSELECT_EXCEPT)) != 0) {
        // read a buffer of data
        rc = FreeRTOS_recv(state.conn, state.buffer, state.bufsize, 0);
        if(rc < 0) {
            printf("websocket handle_command: error reading: %ld\n", rc);
            return rc;
        }
        if(rc > 0) {
            state.data.append(state.buffer, rc);
        }
    }

    // decode the websocket packet and copy it to buf
    rc = websocket_decode(state, state.buffer, state.bufsize);
    if(rc == 0) {
        // we need more data
        return 0;

    } else if(rc < 0) {
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
    process_command_buffer(rc, state.buffer, state.os, state.line, state.cnt, state.discard);
    return rc;
}

// setup the websocket handler, for command or upload, buf/len are the excess already read and part of the payload
extern "C" BaseType_t create_websocket_handler(HTTPClient_t *pclient, int is_command, char *buf, uint32_t len)
{
    WebSocketState *ws = new WebSocketState(pclient->xSocket, is_command == 1);
    pclient->websocketstate = ws;
    if(buf != nullptr && len > 0) {
        // initial payload if any
        ws->data.assign(buf, len);
    }

    if(is_command == 1) {
        // create the OutputStream that commands can write to
        // FIXME this may need to stay around until command thread is done with it
        ws->os = new OutputStream([ws](const char *ibuf, size_t ilen) { return websocket_write(ws->conn, ibuf, ilen, 0x01); });
        ws->buffer = (char*)malloc(132);
        ws->bufsize = 132;

    } else {
        // it is upload
        ws->fp = nullptr;
        ws->buffer = (char*)malloc(1024);
        ws->bufsize = 1024;
    }
    printf("create_websocket_handler: %p\n", ws);
    return 0;
}

extern "C" BaseType_t delete_websocket_handler(HTTPClient_t *pclient)
{
    if(pclient->websocketstate != nullptr) {
        WebSocketState* ws = (WebSocketState*)pclient->websocketstate;
        if(ws->os != nullptr) {
            // we may need to delay this
            delete ws->os;
            ws->os = nullptr;
        }
        if(ws->buffer != nullptr) {
            free(ws->buffer);
            ws->buffer = nullptr;
        }
        delete ws;
        pclient->websocketstate = nullptr;
        printf("delete_websocket_handler: %p\n", ws);
    }
    return 0;
}

extern "C" BaseType_t websocket_work(HTTPClient_t *pclient)
{
    WebSocketState *ws = static_cast<WebSocketState*>(pclient->websocketstate);
    if(ws->is_command) {
        return handle_command(pclient, *ws);

    } else {
        return handle_upload(pclient, *ws);
    }
    return -1;
}
