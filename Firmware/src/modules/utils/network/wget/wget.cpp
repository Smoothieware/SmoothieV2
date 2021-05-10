/*-
 * Copyright 2012 Matthew Endsley
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <string>
#include <vector>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

#include "http.h"

#include "OutputStream.h"

//#define DEBUG_PRINTF printf
#define DEBUG_PRINTF(...)
#define ERROR_PRINTF printf

static bool splitURL(const char *furl, std::string& host, std::string& rurl)
{

    if(furl != NULL && strlen(furl) > 0) {
        std::string loc(furl);
        size_t n1 = loc.find("http://");
        if(n1 != std::string::npos) {
            n1 += 7;
            size_t n2 = loc.find("/", n1);
            if(n2 != std::string::npos) {
                host = loc.substr(n1, n2 - n1);
                rurl = loc.substr(n2);
                return true;
            }
        }
    }
    return false;
}

// return a socket connected to a hostname, or 0
static Socket_t connectsocket(const char* host, int port)
{
    uint32_t dest_ip;

    // try dotted ip first
    dest_ip= FreeRTOS_inet_addr(host);
    if(dest_ip == 0) {
        // try to resolve hostname via DNS
        dest_ip = FreeRTOS_gethostbyname(host);
    }

    if(dest_ip == 0) {
        ERROR_PRINTF("wget connectsocket: failed to get hosts address\n");
        return 0;
    }

    struct freertos_sockaddr addr;
    addr.sin_port = FreeRTOS_htons(port);
    addr.sin_addr = dest_ip;

    /* create a TCP socket */
    Socket_t sock = FreeRTOS_socket(FREERTOS_AF_INET, FREERTOS_SOCK_STREAM, FREERTOS_IPPROTO_TCP);
    if(sock == FREERTOS_INVALID_SOCKET) {
        ERROR_PRINTF("wget connectsocket: failed no memory\n");
        return 0;
    }

    // tell it to use random internal port
    FreeRTOS_bind(sock, NULL, sizeof(freertos_sockaddr));

    BaseType_t rc = FreeRTOS_connect(sock, &addr, sizeof(addr));
    if(rc != 0 ) {
        ERROR_PRINTF("wget connectsocket: connect failed: %ld\n", rc);
        FreeRTOS_closesocket(sock);
        return 0;
    }

    return sock;
}

// TODO we could replace this http parser with the llhttp we already use in the server
// Response data/funcs
using HttpResponse_t = struct {
    int code, error;
    FILE *fp;
    OutputStream& os;
};

static void* response_realloc(void* opaque, void* ptr, int size)
{
    // NOTE the standared says size == 0 is implementation dependent and not guaranteed to free
    // so we do it explicitly here, used this way realloc seems to work ok otherwise we get a memory leak
    if(ptr == 0) {
        return malloc(size);
    }
    if(ptr != 0 && size == 0) {
        free(ptr);
        return 0;
    }
    return realloc(ptr, size);
}

static void response_body(void* opaque, const char* data, int size)
{
    DEBUG_PRINTF("wget response_bodyBody: size %d\n", size);

    HttpResponse_t* response = (HttpResponse_t*)opaque;
    if(response->fp == NULL) {
        response->os.write(data, size);

    } else if(response->error == 0) {
        int n = fwrite(data, 1, size, response->fp);
        if(n != size) {
            DEBUG_PRINTF("wget response_body: file write error: %d", errno);
            response->error = 1;
        }
    }
}

static void response_header(void* opaque, const char* ckey, int nkey, const char* cvalue, int nvalue)
{
    std::string k(ckey, nkey);
    std::string v(cvalue, nvalue);
    DEBUG_PRINTF("%s: %s\n", k.c_str(), v.c_str());
}

static void response_code(void* opaque, int code)
{
    HttpResponse_t* response = (HttpResponse_t*)opaque;
    response->code = code;
}

bool wget(const char *url, const char *fn, OutputStream& os)
{
    std::string host, req;

    if(!splitURL(url, host, req)) {
        ERROR_PRINTF("wget: bad url: %s - must be of the form http://ip_or_name/file\n", url);
        return false;
    }

    std::string request("GET ");
    request.append(req);
    request.append(" HTTP/1.0\r\nHost: ");
    request.append(host);
    request.append("\r\n\r\n\r\n");

    DEBUG_PRINTF("wget: request: %s to host: %s\n", request.c_str(), host.c_str());

    Socket_t conn = connectsocket(host.c_str(), 80);
    if (conn == 0) {
        ERROR_PRINTF("wget: Failed to connect socket\n");
        return false;
    }

    // FIXME allow for partial send of data
    BaseType_t len = FreeRTOS_send(conn, request.c_str(), request.size(), 0);
    if (len != (BaseType_t)request.size()) {
        ERROR_PRINTF("wget: Failed to send request: %ld\n", len);
        FreeRTOS_closesocket(conn);
        return false;
    }

    HttpResponse_t response{0, 0, NULL, os};

    if(fn != nullptr) {
        FILE *fp = fopen(fn, "w");
        if(fp == NULL) {
            ERROR_PRINTF("wget: failed to open file: %s\n", fn);
            FreeRTOS_closesocket(conn);
            return false;
        }
        response.fp = fp;
    }

    http_funcs responseFuncs {
        response_realloc,
        response_body,
        response_header,
        response_code,
    };

    http_roundtripper rt;
    http_init(&rt, responseFuncs, &response);

    bool needmore = true;
    char buffer[1024];
    while (needmore) {
        const char* data = buffer;
        BaseType_t ndata = FreeRTOS_recv(conn, buffer, sizeof(buffer), 0);
        if (ndata < 0) {
            ERROR_PRINTF("wget: Error receiving data: %ld\n", ndata);
            http_free(&rt);
            FreeRTOS_closesocket(conn);
            if(response.fp != NULL) fclose(response.fp);
            return false;
        }

        while (needmore && ndata) {
            int read;
            needmore = http_data(&rt, data, ndata, &read);
            ndata -= read;
            data += read;
        }
    }

    if (http_iserror(&rt)) {
        ERROR_PRINTF("wget: Error parsing HTTP data\n");
        http_free(&rt);
        FreeRTOS_closesocket(conn);
        if(response.fp != NULL) fclose(response.fp);
        return false;
    }

    if (response.error != 0) {
        ERROR_PRINTF("wget: Error writing data to file\n");
        http_free(&rt);
        FreeRTOS_closesocket(conn);
        if(response.fp != NULL) fclose(response.fp);
        return false;
    }

    http_free(&rt);
    FreeRTOS_closesocket(conn);
    if(response.fp != NULL) fclose(response.fp);

    return response.code == 200;
}

