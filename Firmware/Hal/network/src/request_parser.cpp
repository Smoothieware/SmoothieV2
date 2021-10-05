
#include <cstdint>
#include <malloc.h>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>

#include "llhttp.h"

typedef struct _request {
    std::map<std::string, std::string> headers;
    uint8_t method;
    std::string url;
    std::string last_hdr;
    std::string hdr_value;
    std::string body;
    llhttp_t *parser;
    bool done{false};
} Request_t;

static int handle_on_message_complete(llhttp_t  *llh)
{
    Request_t *req = (Request_t*)llh->data;
    req->done = true;
    //printf("on_message_complete: %p - %p\n", req, llh);
    return HPE_OK;
}

static int handle_on_url(llhttp_t *llh, const char *at, size_t length)
{
    Request_t *req = (Request_t*)llh->data;
    req->url.append(at, length);
    req->method = llh->method;
    return HPE_OK;
}

static int handle_on_header_field(llhttp_t *llh, const char *at, size_t length)
{
    Request_t *req = (Request_t*)llh->data;
    req->last_hdr.append(at, length);
    return HPE_OK;
}

static int handle_on_header_value(llhttp_t *llh, const char *at, size_t length)
{
    Request_t *req = (Request_t*)llh->data;
    req->hdr_value.append(at, length);
    return HPE_OK;
}

static int handle_on_header_value_complete(llhttp_t *llh)
{
    Request_t *req = (Request_t*)llh->data;

    if(!req->last_hdr.empty()) {
        req->headers[req->last_hdr] = req->hdr_value;
        //printf("on_header_value_complete: %s: %s\n", req->last_hdr.c_str(), req->hdr_value.c_str());
        req->last_hdr.clear();
        req->hdr_value.clear();

    } else {
        //printf("ERROR: request_parser: got header value with no header field\n");
        return HPE_INVALID_HEADER_TOKEN;
    }
    return HPE_OK;
}

static int handle_on_body(llhttp_t *llh, const char *at, size_t length)
{
    //printf("on_body: at %p, len %u\n", at, length);
    Request_t *req = (Request_t*)llh->data;
    req->body.assign(at, length);
    return HPE_OK;
}

extern "C" void *parse_request_create()
{
    Request_t *p_request = new Request_t;
    //printf("request_parser: create: %p\n", p_request);

    if(p_request != nullptr) {
        llhttp_t *parser = new llhttp_t;
        llhttp_settings_t *settings = new llhttp_settings_t;
        if(parser != nullptr && settings != nullptr) {
            /* Initialize user callbacks and settings */
            llhttp_settings_init(settings);

            /* Set user callback */
            settings->on_message_complete = handle_on_message_complete;
            settings->on_url = handle_on_url;
            settings->on_header_value = handle_on_header_value;
            settings->on_header_value_complete = handle_on_header_value_complete;
            settings->on_header_field = handle_on_header_field;
            settings->on_body = handle_on_body;

            llhttp_init(parser, HTTP_REQUEST, settings);
            parser->data = p_request;
            p_request->parser = parser;

        }else{
            delete p_request;
            p_request= nullptr;
        }
    }

    return p_request;
}

extern "C" int parse_request_release(Request_t *p_request)
{
    // release
    //printf("request_parser: release: %p\n", p_request);
    delete (llhttp_settings_t*)p_request->parser->settings;
    delete p_request->parser;
    delete p_request;
    return 1;
}

/*
    returns  0 if more data needed
             1 if completed ok
             2-n if completed but was a UPGRADE request, and data offset-2 is returned
            -1 if there was a parse error
*/
extern "C" int parse_request(const char *buf, uint32_t len, Request_t *p_request)
{
    enum llhttp_errno err = llhttp_execute(p_request->parser, buf, len);
    if (err == HPE_OK) {
        /* Successfully parsed! */
        return p_request->done ? 1 : 0;
    } else if (err == HPE_PAUSED_UPGRADE) {
        uint32_t offset= p_request->parser->error_pos - buf;
        //printf("parse: UPGRADE: %d, error_pos: %p, offset: %lu\n", p_request->parser->upgrade, p_request->parser->error_pos,            offset);
        return (int)(2+offset);
    } else {
        //printf("Parse error: %s %s\n", llhttp_errno_name(err), p_request->parser->reason);
        return -err;
    }
}

extern "C" int parse_request_get_method(void *request)
{
    Request_t *req = static_cast<Request_t*>(request);
    if(req != nullptr && req->parser != nullptr) {
        return req->parser->method;
    }
    return -1;
}

extern "C" const char *parse_request_get_url(void *request)
{
    Request_t *req = static_cast<Request_t*>(request);
    if(req != nullptr) {
        return req->url.c_str();
    }
    return nullptr;
}

extern "C" const char *parse_request_get_header(const char *hdr, void *request)
{
    Request_t *req = static_cast<Request_t*>(request);
    if(req != nullptr) {
        auto h = req->headers.find(hdr);
        if(h != req->headers.end()) {
            return h->second.c_str();
        }
    }
    return nullptr;
}
extern "C" const int parse_request_get_headers(const char *hdrs[], int len, void *request)
{
    Request_t *req = static_cast<Request_t*>(request);
    int n = 0;
    if(req != nullptr) {
        for(auto& h : req->headers) {
            hdrs[n++] = h.first.c_str();
        }
    }
    return n;
}
