/*
 * Original code...
 * (C) 2014 David Lettier.
 *
 * http://www.lettier.com/
 *
 * NTP client.
 * ported to FreeRTOS TCP by smoothieware.org
 *
 */
#include "OutputStream.h"

#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

#include <time.h>

extern "C" int rtc_setdatetime(uint8_t year, uint8_t month, uint8_t day, uint8_t weekday, uint8_t hour, uint8_t minute, uint8_t seconds);

#define NTP_TIMESTAMP_DELTA 2208988800ull

#define LI(packet)   (uint8_t) ((packet.li_vn_mode & 0xC0) >> 6) // (li   & 11 000 000) >> 6
#define VN(packet)   (uint8_t) ((packet.li_vn_mode & 0x38) >> 3) // (vn   & 00 111 000) >> 3
#define MODE(packet) (uint8_t) ((packet.li_vn_mode & 0x07) >> 0) // (mode & 00 000 111) >> 0

bool get_ntp_time()
{
    int portno = 123; // NTP UDP port number.
    const char* host_name = "europe.pool.ntp.org"; // NTP server host-name.

    // Structure that defines the 48 byte NTP packet protocol.

    typedef struct {

        uint8_t li_vn_mode;      // Eight bits. li, vn, and mode.
        // li.   Two bits.   Leap indicator.
        // vn.   Three bits. Version number of the protocol.
        // mode. Three bits. Client will pick mode 3 for client.

        uint8_t stratum;         // Eight bits. Stratum level of the local clock.
        uint8_t poll;            // Eight bits. Maximum interval between successive messages.
        uint8_t precision;       // Eight bits. Precision of the local clock.

        uint32_t rootDelay;      // 32 bits. Total round trip delay time.
        uint32_t rootDispersion; // 32 bits. Max error aloud from primary clock source.
        uint32_t refId;          // 32 bits. Reference clock identifier.

        uint32_t refTm_s;        // 32 bits. Reference time-stamp seconds.
        uint32_t refTm_f;        // 32 bits. Reference time-stamp fraction of a second.

        uint32_t origTm_s;       // 32 bits. Originate time-stamp seconds.
        uint32_t origTm_f;       // 32 bits. Originate time-stamp fraction of a second.

        uint32_t rxTm_s;         // 32 bits. Received time-stamp seconds.
        uint32_t rxTm_f;         // 32 bits. Received time-stamp fraction of a second.

        uint32_t txTm_s;         // 32 bits and the most important field the client cares about. Transmit time-stamp seconds.
        uint32_t txTm_f;         // 32 bits. Transmit time-stamp fraction of a second.

    } ntp_packet;              // Total: 384 bits or 48 bytes.

    // Create and zero out the packet. All 48 bytes worth.

    ntp_packet packet;
    memset(&packet, 0, sizeof(ntp_packet));

    // Set the first byte's bits to 00,011,011 for li = 0, vn = 3, and mode = 3. The rest will be left set to zero.
    *((char*)&packet + 0) = 0x1b; // Represents 27 in base 10 or 00011011 in base 2.

    uint32_t server = FreeRTOS_gethostbyname(host_name); // Convert URL to IP.
    if(server == 0) {
        printf("ERROR: ntpclient: no such host: %s", host_name);
        return false;
    }

    struct freertos_sockaddr serv_addr;
    serv_addr.sin_port = FreeRTOS_htons(portno);
    serv_addr.sin_addr = server;

    Socket_t sockfd = FreeRTOS_socket(FREERTOS_AF_INET, FREERTOS_SOCK_DGRAM, FREERTOS_IPPROTO_UDP); // Create a UDP socket.

    if (sockfd == FREERTOS_INVALID_SOCKET) {
        printf("ERROR: ntpclient: opening socket");
        return false;
    }

    // Send it the NTP packet it wants. If n == -1, it failed.
    BaseType_t len = FreeRTOS_sendto(sockfd, &packet, sizeof(ntp_packet), 0, &serv_addr, sizeof(serv_addr));
    if (len != (BaseType_t)sizeof(ntp_packet)) {
        printf("ERROR: ntpclient: Failed to send request: %ld\n", len);
        FreeRTOS_closesocket(sockfd);
        return false;
    }

    // set a timeout
    BaseType_t timeout= pdMS_TO_TICKS(10000); // 10 seconds
    FreeRTOS_setsockopt(sockfd, 0, FREERTOS_SO_RCVTIMEO, (void *)&timeout, sizeof(BaseType_t));

    // Wait and receive the packet back from the server. If n == -1, it failed.
    uint32_t xAddressSize= sizeof(serv_addr);
    BaseType_t ndata = FreeRTOS_recvfrom(sockfd, &packet, sizeof(ntp_packet), 0, &serv_addr, &xAddressSize);
    if (ndata < 0) {
        printf("ERROR: ntpclient: receiving data: %ld\n", ndata);
        FreeRTOS_closesocket(sockfd);
        return false;
    }

    FreeRTOS_closesocket(sockfd);

    // These two fields contain the time-stamp seconds as the packet left the NTP server.
    // The number of seconds correspond to the seconds passed since 1900.
    // ntohl() converts the bit/byte order from the network's to host's "endianness".
    packet.txTm_s = FreeRTOS_ntohl( packet.txTm_s ); // Time-stamp seconds.
    packet.txTm_f = FreeRTOS_ntohl( packet.txTm_f ); // Time-stamp fraction of a second.

    // Extract the 32 bits that represent the time-stamp seconds (since NTP epoch) from when the packet left the server.
    // Subtract 70 years worth of seconds from the seconds since 1900.
    // This leaves the seconds since the UNIX epoch of 1970.
    // (1900)------------------(1970)**************************************(Time Packet Left the Server)
    time_t txTm = ((time_t)packet.txTm_s - NTP_TIMESTAMP_DELTA);
    struct tm *tm = gmtime(&txTm) ;
    printf("DEBUG: ntpclient: %d-%d-%d %d:%d:%d\n", tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);

    rtc_setdatetime(tm->tm_year+1900-2000, tm->tm_mon+1, tm->tm_mday, tm->tm_wday==0?7:tm->tm_wday,
                    tm->tm_hour, tm->tm_min, tm->tm_sec);
    return true;
}
