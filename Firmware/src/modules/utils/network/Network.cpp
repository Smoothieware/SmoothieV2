#include "Network.h"

#include "main.h"
#include "ConfigReader.h"
#include "Dispatcher.h"
#include "OutputStream.h"
#include "StringUtils.h"

#include "FreeRTOS.h"
#include "task.h"

#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_TCP_server.h"
#include "FreeRTOS_DHCP.h"


#define network_enable_key "enable"
#define shell_enable_key "shell_enable"
#define ftp_enable_key "ftp_enable"
#define webserver_enable_key "webserver_enable"
#define ip_address_key  "ip_address"
#define ip_mask_key "ip_mask"
#define ip_gateway_key "ip_gateway"
#define hostname_key "hostname"
#define dns_server_key "dns_server"

REGISTER_MODULE(Network, Network::create)

bool Network::create(ConfigReader& cr)
{
    printf("DEBUG: configure network\n");
    Network *network = Network::getInstance();
    if(!network->configure(cr)) {
        printf("INFO: Network not enabled\n");
        delete network;
        instance = nullptr;
        return false;
    }

    // register a startup function
    register_startup(std::bind(&Network::start, network));

    return true;
}

Network *Network::instance = nullptr;
Network *Network::getInstance()
{
    if(instance == nullptr) {
        instance = new Network();
    }
    return instance;
}

Network::Network() : Module("network")
{
}

Network::~Network()
{
}

// FIXME replace with H/W crc
static uint32_t crc32(uint8_t* buf, int length)
{
    static const uint32_t crc32_table[] =
    {
        0x4DBDF21C, 0x500AE278, 0x76D3D2D4, 0x6B64C2B0,
        0x3B61B38C, 0x26D6A3E8, 0x000F9344, 0x1DB88320,
        0xA005713C, 0xBDB26158, 0x9B6B51F4, 0x86DC4190,
        0xD6D930AC, 0xCB6E20C8, 0xEDB71064, 0xF0000000
    };

    int n;
    uint32_t crc=0;

    for (n = 0; n < length; n++)
    {
        crc = (crc >> 4) ^ crc32_table[(crc ^ (buf[n] >> 0)) & 0x0F];  /* lower nibble */
        crc = (crc >> 4) ^ crc32_table[(crc ^ (buf[n] >> 4)) & 0x0F];  /* upper nibble */
    }

    return crc;
}


static TCPServer_t *pxTCPServer = NULL;

// this is set by config
static uint8_t ucMACAddress[ 6 ] = { 0x44, 0x11, 0x22, 0x33, 0x44, 0x55 };
static uint8_t ucIPAddress[ 4 ] = { 192, 168, 1, 45 };
static uint8_t ucNetMask[ 4 ] = { 255, 255, 255, 0 };
static uint8_t ucGatewayAddress[ 4 ] = { 192, 168, 1, 1 };
static uint8_t ucDNSServerAddress[ 4 ] = { 192, 168, 1, 1 };
extern "C" uint64_t get_uid();

bool Network::configure(ConfigReader& cr)
{
    ConfigReader::section_map_t m;
    if(!cr.get_section("network", m)) return false;

    bool enable = cr.get_bool(m, network_enable_key, false);
    if(!enable) {
        return false;
    }

    hostname = cr.get_string(m, hostname_key, "smoothiev2");

    std::string ip_address_str = cr.get_string(m, ip_address_key, "auto");
    if(!ip_address_str.empty() && ip_address_str != "auto") {
        use_dhcp = false;
        std::string ip_mask_str = cr.get_string(m, ip_mask_key, "255.255.255.0");
        std::string ip_gateway_str = cr.get_string(m, ip_gateway_key, "192.168.1.1");
        // TODO setup ucIPAddress, ucNetMask, ucGatewayAddress
    } else {
        use_dhcp = true;
    }

    std::string dns_server_str = cr.get_string(m, dns_server_key, "auto");
    if(!dns_server_str.empty() && dns_server_str != "auto") {
        // setup ucDNSServerAddress
    }

    // setup MAC address
    uint64_t uid= get_uid();
    uint32_t h = crc32((uint8_t *)&uid, sizeof(uint64_t));
    ucMACAddress[0] = 0x00;   // OUI
    ucMACAddress[1] = 0x1F;   // OUI
    ucMACAddress[2] = 0x11;   // OUI
    ucMACAddress[3] = 0x02;   // Openmoko allocation for smoothie board
    ucMACAddress[4] = 0x04;   // 04-14  03 bits -> chip id, 1 bits -> hashed serial
    ucMACAddress[5] = h & 0xFF; // 00-FF  8bits -> hashed serial

    enable_shell = cr.get_bool(m, shell_enable_key, false);
    enable_ftpd = cr.get_bool(m, ftp_enable_key, false);
    enable_httpd = cr.get_bool(m, webserver_enable_key, false);

    // register command handlers
    using std::placeholders::_1;
    using std::placeholders::_2;

    THEDISPATCHER->add_handler( "net", std::bind( &Network::handle_net_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "wget", std::bind( &Network::wget_cmd, this, _1, _2) );
    // THEDISPATCHER->add_handler( "update", std::bind( &Network::update_cmd, this, _1, _2) );

    return true;
}

extern "C" void netstat(OutputStream&);

#define HELP(m) if(params == "-h") { os.printf("%s\n", m); return true; }
bool Network::handle_net_cmd( std::string& params, OutputStream& os )
{
    HELP("net - show network status, -n also shows netstat, -k shuts down network");

    if(FreeRTOS_IsNetworkUp() == pdTRUE) {
        uint32_t ulIPAddress, ulNetMask, ulGatewayAddress, ulDNSServerAddress;
        char cBuffer[ 16 ];
        FreeRTOS_GetAddressConfiguration( &ulIPAddress, &ulNetMask, &ulGatewayAddress, &ulDNSServerAddress );
        FreeRTOS_inet_ntoa( ulIPAddress, cBuffer );
        os.printf( "IP Address: %s\n", cBuffer );
        FreeRTOS_inet_ntoa( ulNetMask, cBuffer );
        os.printf( "Subnet Mask: %s\n", cBuffer );
        FreeRTOS_inet_ntoa( ulGatewayAddress, cBuffer );
        os.printf( "Gateway Address: %s\n", cBuffer );
        FreeRTOS_inet_ntoa( ulDNSServerAddress, cBuffer );
        os.printf( "DNS Server Address: %s\n", cBuffer );

    } else {
        os.printf("Link DOWN\n");
    }

    bool bnetstat = false;
    bool bkill = false;
    while(!params.empty()) {
        std::string s = stringutils::shift_parameter( params );
        if(s == "-n") bnetstat = true;
        else if(s == "-k") bkill = true;
    }

    if(bnetstat) {
        // do netstat-like dump
        os.puts("\nNetstat...\n");
        netstat(os);
    }
    if(bkill) {
        set_abort();
    }

    os.set_no_response();

    return true;
}

// extern
bool wget(const char *url, const char *fn, OutputStream& os);

bool Network::wget_cmd( std::string& params, OutputStream& os )
{
    HELP("wget url [outfn] - fetch a url and save to outfn or print the contents");
    std::string url = stringutils::shift_parameter(params);
    if(url.empty()) {
        os.printf("url required: eg http://smoothieware.org/firmware.bin\n");
        return true;
    }

    std::string outfn;
    if(!params.empty()) {
        outfn = stringutils::shift_parameter(params);
    }

    if(!wget(url.c_str(), outfn.empty() ? nullptr : outfn.c_str(), os)) {
        os.printf("\nfailed to get url");
    }
    os.printf("\n");

    return true;
}

#if 0
#include "md5.h"
extern uint32_t _image_start;
extern uint32_t _image_end;

bool Network::update_cmd( std::string& params, OutputStream& os )
{
    HELP("update the firmware from web");
#ifdef BOARD_BAMBINO
    std::string urlbin = "http://smoothieware.org/_media/bin/bb.bin";
    std::string urlmd5 = "http://smoothieware.org/_media/bin/bb.md5";
#elif defined(BOARD_PRIME)
    std::string urlbin = "http://smoothieware.org/_media/bin/pa.bin";
    std::string urlmd5 = "http://smoothieware.org/_media/bin/pa.md5";
#else
#error "board not supported by update_cmd"
#endif

    // fetch the md5 of the file from the server
    std::ostringstream oss;
    OutputStream tos(&oss);
    // fetch the md5 into the ostringstream
    if(!wget(urlmd5.c_str(), nullptr, tos)) {
        os.printf("failed to get firmware checksum\n");
        return true;
    }

    {
        char *src_addr = (char *)&_image_start;
        char *src_end = (char *)&_image_end;
        uint32_t src_len = src_end - src_addr;
        MD5 md5;
        md5.update(src_addr, src_len);
        std::string md = md5.finalize().hexdigest();
        os.printf("current md5:  %s\n", md.c_str());
        os.printf("fetched md5:  %s\n", oss.str().c_str());
        if(oss.str() == md) {
            os.printf("You already have the latest firmware\n");
            return true;
        }
    }

    if(!params.empty()) {
        // just checking if there is a newer version
        os.printf("There is an updated version of the firmware available\n");
        return true;
    }

    // fetch firmware from server
    if(!wget(urlbin.c_str(), "/sd/flashme.bin", os)) {
        os.printf("failed to get updated firmware\n");
        return true;
    }

    // check md5
    FILE *lp = fopen("/sd/flashme.bin", "r");
    if (lp == NULL) {
        os.printf("firmware file was not found for verification\n");
        return true;
    }

    // calculate md5 of file
    MD5 md5;
    uint8_t buf[64];
    do {
        size_t n = fread(buf, 1, sizeof buf, lp);
        if(n > 0) md5.update(buf, n);
    } while(!feof(lp));
    fclose(lp);
    std::string md = md5.finalize().hexdigest();

    os.printf("new file md5: %s\n", md.c_str());

    // flash it
    if(oss.str() == md) {
        // md5 is correct
        os.printf("Updating the firmware from the web\n if successful the system will reboot\n this can take about 20 seconds\n");

        // flash it
        THEDISPATCHER->dispatch("flash", os);

        // does not return from this

    } else {
        os.printf("downloaded firmware failed verification:\n");
    }

    return true;
}
#endif

extern "C" void xNetworkDeInitialise();
void Network::set_abort()
{
#if 0
    // FIXME this causes a HardFault
    if(enable_ftpd || enable_httpd) {
        FreeRTOS_TCPServerSignal(pxTCPServer);
    }
    if(enable_shell) {
        extern void shell_close(void);
        shell_close();
    }
    pxTCPServer = nullptr;
#else
    // at least stop interrupts
    xNetworkDeInitialise();
#endif
    abort_network = true;
}

/* Main TCP thread loop */
void Network::network_thread()
{
    // setup the servers
    xSERVER_CONFIG xServerConfiguration[2];

    int ns = 0;
#if ipconfigUSE_HTTP != 0
    if(enable_httpd) {
        xServerConfiguration[ns].eType = eSERVER_HTTP;
        xServerConfiguration[ns].xPortNumber = 80;
        xServerConfiguration[ns].xBackLog = 12;
        xServerConfiguration[ns].pcRootDir = "/sd/www";
        ++ns;
        printf("Network: HTTPD started\n");
    }
#endif

#if ipconfigUSE_FTP != 0
    if(enable_ftpd) {
        xServerConfiguration[ns].eType = eSERVER_FTP;
        xServerConfiguration[ns].xPortNumber = 21;
        xServerConfiguration[ns].xBackLog = 12;
        xServerConfiguration[ns].pcRootDir = "";
        ++ns;
        printf("Network: FTPD started\n");
    }
#endif

    if(ns > 0) {
        /* Create the servers defined by the xServerConfiguration array above. */
        pxTCPServer = FreeRTOS_CreateTCPServer(xServerConfiguration, ns);
        configASSERT( pxTCPServer );
    }

    /* Initialize application(s) */
    if(enable_shell) {
        extern void shell_init(void);
        shell_init();
    }

    // This loop keeps the servers running if there are any to run
    const TickType_t xInitialBlockTime = pdMS_TO_TICKS( 5000UL );
    while (pxTCPServer != NULL && !abort_network) {
        // Run the HTTP and/or FTP servers, as configured above.
        FreeRTOS_TCPServerWork( pxTCPServer, xInitialBlockTime );
    }

    printf("DEBUG: Server thread: exiting\n");
}

void Network::vSetupIFTask(void *arg)
{
    Network *network = Network::getInstance();
    network->network_thread();
    vTaskDelete(NULL);
}

extern "C" void vApplicationIPNetworkEventHook( eIPCallbackEvent_t eNetworkEvent )
{
    static BaseType_t xTasksAlreadyCreated = pdFALSE;
    static bool up = false;

    /* Both eNetworkUp and eNetworkDown events can be processed here. */
    if(eNetworkEvent == eNetworkUp && !up) {
        printf("Network is up\n");

        /* Print out the network configuration, which may have come from a DHCP
        server. */
        uint32_t ulIPAddress, ulNetMask, ulGatewayAddress, ulDNSServerAddress;
        char cBuffer[ 16 ];
        FreeRTOS_GetAddressConfiguration( &ulIPAddress, &ulNetMask, &ulGatewayAddress, &ulDNSServerAddress );
        FreeRTOS_inet_ntoa( ulIPAddress, cBuffer );
        printf( "IP Address: %s\n", cBuffer );

        FreeRTOS_inet_ntoa( ulNetMask, cBuffer );
        printf( "Subnet Mask: %s\n", cBuffer );

        FreeRTOS_inet_ntoa( ulGatewayAddress, cBuffer );
        printf( "Gateway Address: %s\n", cBuffer );

        FreeRTOS_inet_ntoa( ulDNSServerAddress, cBuffer );
        printf( "DNS Server Address: %s\n", cBuffer );

        /* Create the tasks that use the TCP/IP stack if they have not already
        been created. */
        if( xTasksAlreadyCreated == pdFALSE ) {
            /*
             * For convenience, tasks that use FreeRTOS+TCP can be created here
             * to ensure they are not created before the network is usable.
             */
            xTasksAlreadyCreated = pdTRUE;
            xTaskCreate(Network::vSetupIFTask, "Servers", 1024, NULL, (tskIDLE_PRIORITY + 1UL), (xTaskHandle *) NULL);
        }
        up = true;

    } else if(eNetworkEvent == eNetworkDown && up) {
        printf("Network went down\n");
        up = false;
    }
}

extern "C" int setup_network_hal();
bool Network::start()
{
    printf("DEBUG: Network: starting\n");
    if(!setup_network_hal()) {
        printf("ERROR: setup_netork_hal() failed\n");
        return false;
    }

    /*
        Initialise the RTOS’s TCP/IP stack.  The tasks that use the network
        are created in the vApplicationIPNetworkEventHook() hook function.
        The hook function is called when the network connects.
    */
    if(!FreeRTOS_IPInit( ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress ) ) {
        printf("ERROR: FreeRTOS TCP Init failed\n");
        return false;
    }

    return true;
}

extern "C" const char *pcApplicationHostnameHook( void )
{
    return Network::getInstance()->get_hostname();
}

// used to enable or disable dhcp
extern "C" eDHCPCallbackAnswer_t xApplicationDHCPHook( eDHCPCallbackPhase_t eDHCPPhase, uint32_t ulIPAddress )
{
    eDHCPCallbackAnswer_t eReturn;
    bool use_dhcp = Network::getInstance()->is_dhcp();

    /* This hook is called in a couple of places during the DHCP process, as
    identified by the eDHCPPhase parameter. */
    switch( eDHCPPhase ) {
        case eDHCPPhasePreDiscover  :
            /* A DHCP discovery is about to be sent out.  eDHCPContinue is
            returned to allow the discovery to go out.

            If eDHCPUseDefaults had been returned instead then the DHCP process
            would be stopped and the statically configured IP address would be
            used.

            If eDHCPStopNoChanges had been returned instead then the DHCP
            process would be stopped and whatever the current network
            configuration was would continue to be used. */
            eReturn = use_dhcp ? eDHCPContinue : eDHCPUseDefaults;
            break;

        case eDHCPPhasePreRequest  :
            eReturn = use_dhcp ? eDHCPContinue : eDHCPUseDefaults;
            break;

        default :
            /* Cannot be reached, but set eReturn to prevent compiler warnings
            where compilers are disposed to generating one. */
            eReturn = eDHCPContinue;
            break;
    }

    return eReturn;
}
