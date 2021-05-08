// TEST_http_parser.cpp
#include "../Unity/src/unity.h"
#include <stdlib.h>
#include <stdio.h>
#include <cstring>

#include "TestRegistry.h"

extern "C" int parse_request(const char *buf, uint32_t len, void **request);
extern "C" int parse_request_get_method(void *request);
extern "C" const char *parse_request_get_url(void *request);
extern "C" const char *parse_request_get_header(const char *hdr, void *request);

REGISTER_TEST(HTTPparserTest, basic)
{
    void *request = nullptr;
    TEST_ASSERT_EQUAL_INT(1, parse_request(NULL, 0, &request));
    TEST_ASSERT_NOT_NULL(request);

    const char* req = "GET /test HTTP/1.1\r\nTEST1: 1234\r\n\r\n";
    int req_len = strlen(req);

    int needmore = parse_request(req, req_len, &request);
    TEST_ASSERT_EQUAL_INT(1, needmore);

    TEST_ASSERT_EQUAL_INT(1, parse_request_get_method(request));
    TEST_ASSERT_EQUAL_STRING("/test", parse_request_get_url(request));
    TEST_ASSERT_EQUAL_STRING("1234", parse_request_get_header("TEST1", request));

    TEST_ASSERT_NULL(parse_request_get_header("TEST2", request));

    // free up resources]
    TEST_ASSERT_EQUAL_INT(1, parse_request(NULL, 0, &request));
    TEST_ASSERT_NULL(request);
}

REGISTER_TEST(HTTPparserTest, split)
{
    void *request = nullptr;
    TEST_ASSERT_EQUAL_INT(1, parse_request(NULL, 0, &request));
    TEST_ASSERT_NOT_NULL(request);

    // split up the data as if we had it fragmented in incoming packets
    const char* req1 = "GET /tes";
    const char* req2 = "t HTTP/1.1\r\n";
    const char* req3 = "TEST";
    const char* req4 = "1: 12";
    const char* req5 = "34\r\n\r\n";
    int req1_len = strlen(req1);
    int req2_len = strlen(req2);
    int req3_len = strlen(req3);
    int req4_len = strlen(req4);
    int req5_len = strlen(req5);

    int needmore = parse_request(req1, req1_len, &request);
    TEST_ASSERT_EQUAL_INT(0, needmore);
    needmore = parse_request(req2, req2_len, &request);
    TEST_ASSERT_EQUAL_INT(0, needmore);
    needmore = parse_request(req3, req3_len, &request);
    TEST_ASSERT_EQUAL_INT(0, needmore);
    needmore = parse_request(req4, req4_len, &request);
    TEST_ASSERT_EQUAL_INT(0, needmore);
    needmore = parse_request(req5, req5_len, &request);
    TEST_ASSERT_EQUAL_INT(1, needmore);

    TEST_ASSERT_EQUAL_INT(1, parse_request_get_method(request));
    TEST_ASSERT_EQUAL_STRING("/test", parse_request_get_url(request));
    TEST_ASSERT_EQUAL_STRING("1234", parse_request_get_header("TEST1", request));

    // free up resources]
    TEST_ASSERT_EQUAL_INT(1, parse_request(NULL, 0, &request));
    TEST_ASSERT_NULL(request);
}
