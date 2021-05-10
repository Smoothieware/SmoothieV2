// WebSocket.cpp

#include "OutputStream.h"
#include "main.h"

#include <string>
#include <map>

#if 0
class WebsocketState
{

public:
    WebsocketState(Socket_t c) : conn(c) {}
    ~WebsocketState() {  }

    Socket_t conn;
    std::string buf;
    bool complete{false};
};

// Read data until we have at least len bytes available.
// conn is the connection
// cp is a pbuf chain containing read bytes so far
// len is the minimum amount of data we need to have read
// NOTE that there may be excess data left in the pbuf chain
// usage: one would call pbuf_copy_partial() after this returns to get the len bytes of data
//        then call pbuf_free_header(cp, len) to free up the pbufs that had that data
static BaseType_t read_len_bytes(Socket_t conn, struct pbuf* &cp, uint16_t len)
{
    while(true) {
        if(cp != NULL && cp->tot_len >= len) {
            return 0;
        }

        // Read more data from the port
        struct pbuf *p = NULL;
        BaseType_t err = netconn_recv_tcp_pbuf(conn, &p);
        if(err != 0) {
            if(p != NULL) pbuf_free(p);
            return err;
        }

        if(cp == NULL) {
            cp = p;
        } else {
            pbuf_cat(cp, p);
        }
    }
}

// Read data from a websocket and decode it.
// buf is provided and buflen must contain the size of that buf
// -1 is returned if it is not big enough and the size it would need to be is assigned to readlen
// readlen is set to the actual number of bytes read
// TODO may need to be able to return partial buffers of payload
static BaseType_t websocket_read(WebsocketState& state, uint8_t *buf, uint16_t buflen, uint16_t& readlen)
{
    BaseType_t err;
    if(state.p == nullptr || state.p->tot_len < 6) {
        // get at least 6 bytes
        if(ERR_OK != (err = read_len_bytes(state.conn, state.p, 6))) {
            return err;
        }
    }

    uint8_t hdr[8];
    // get initial 6 bytes of header
    uint16_t n = pbuf_copy_partial(state.p, hdr, 6, 0);
    if(n != 6) {
        printf("websocket_read: pbuf_copy_partial failed\n");
        return ERR_VAL;
    }
    if((hdr[0] & 0x80) == 0) {
        // must have fin bit
        // TODO need to handle this correctly, as it would notify the end of a large file upload
        printf("websocket_read: WARNING FIN bit not set\n");
        state.complete = false;
        //return ERR_VAL;
    } else {
        state.complete = true;
    }

    if((hdr[1] & 0x80) == 0) {
        // must have mask bit
        printf("websocket_read: MASK bit not set\n");
        return ERR_VAL;
    }
    uint16_t o, m;
    uint16_t plen = hdr[1] & 0x7F;
    if(plen < 126) {
        o = 6;
        m = 2;
    } else if(plen == 126) {
        plen = (hdr[2] << 8) | (hdr[3] & 0xFF);
        o = 8;
        m = 4;
    } else {
        uint32_t s1, s2;
        uint8_t hdr2[10];
        pbuf_copy_partial(state.p, hdr2, 10, 0);
        s1 = ((uint32_t)hdr2[2] << 24) | ((uint32_t)hdr2[3] << 16) | ((uint32_t)hdr2[4] << 8) | ((uint32_t)hdr2[5] & 0xFF);
        s2 = ((uint32_t)hdr2[6] << 24) | ((uint32_t)hdr2[7] << 16) | ((uint32_t)hdr2[8] << 8) | ((uint32_t)hdr2[9] & 0xFF);
        printf("websocket_read: unsupported length: %d - %08lX %08lX\n", plen, s1, s2);
        return ERR_VAL;
    }

    // FIXME we really do not want to have to load the entire packet into memory/pbufs

    // check buffer size for payload before trying to read it all
    if(buflen < plen) {
        // the buffer provided is not big enough, tell caller what size it needs to be
        printf("websocket_read: provided buffer (%d) is not big enough for %d bytes\n", buflen, plen);
        readlen = plen;
        return ERR_BUF;
    }

    // make sure we have all data
    if(ERR_OK == (err = read_len_bytes(state.conn, state.p, plen + o))) {
        // read entire set of header bytes into hdr
        n = pbuf_copy_partial(state.p, hdr, o, 0);

        // read entire payload into provided buffer
        n = pbuf_copy_partial(state.p, buf, plen, o);
        // free up read pbufs
        state.p = pbuf_free_header(state.p, plen + o);

        if(n != plen) {
            printf("websocket_read: pbuf_copy_partial failed: %d\n", plen);
            return ERR_VAL;
        }

        // now unmask the data
        uint8_t opcode = hdr[0] & 0x0F;
        switch (opcode) {
            case 0x00: // continuation
            case 0x01: // text
            case 0x02: // bin
                /* unmask */
                for (int i = 0; i < plen; i++) {
                    buf[i] ^= hdr[m + i % 4];
                }
                readlen = plen;
                break;
            case 0x08: // close
                return -20;
                break;
            default:
                printf("websocket_read: unhandled opcode %d\n", opcode);
                return ERR_VAL;
        }
        return ERR_OK;

    } else {
        return err;
    }

    return ERR_VAL;
}

static int websocket_write(Socket_t conn, const char *data, uint16_t len, uint8_t mode = 0x01)
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
    BaseType_t err = netconn_write(conn, hdr, l, NETCONN_COPY);
    err = netconn_write(conn, data, len, NETCONN_COPY);
    if(err != ERR_OK) {
        printf("websocket_write: error writing: %d\n", err);
    }
    return len;
}

static const char endbuf[] = {0x88, 0x02, 0x03, 0xe8};

// this may need to stay around until command thread is done with it so we do not delete it until we need to create another
static OutputStream *os = nullptr;
static BaseType_t handle_command(Socket_t conn)
{
    const u16_t bufsize = 256;
    char buf[bufsize];
    char line[132];
    size_t cnt = 0;
    bool discard = false;

    // if there is an old Outputstream delete it first
    if(os != nullptr) delete(os);
    // create the OutputStream that commands can write to
    os = new OutputStream([conn](const char *ibuf, size_t ilen) { return websocket_write(conn, ibuf, ilen, 0x01); });

    uint16_t n;
    BaseType_t err;
    WebsocketState state(conn);
    // read packets from connection until it closes
    while ((err = websocket_read(state, (uint8_t *)buf, bufsize, n)) == ERR_OK) {
        // we now have a decoded websocket payload in buf, that is n bytes long
        process_command_buffer(n, buf, os, line, cnt, discard);
    }

    // make sure command thread does not try to write to the soon to be closed (and deleted) conn
    os->set_closed();

    if(err == -20) {
        // send exit string if we got one
        netconn_write(conn, endbuf, sizeof(endbuf), NETCONN_NOCOPY);
    }

    printf("handle_command: websocket closing\n");
    return ERR_OK;
}

static BaseType_t handle_upload(Socket_t conn)
{
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

BaseType_t websocket_work(HTTPClient_t *pclient)
{

}
#endif
