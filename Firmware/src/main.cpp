#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <malloc.h>
#include <fstream>
#include <vector>
#include <functional>

#include "FreeRTOS.h"
#include "task.h"
#include "ff.h"
#include "semphr.h"

#include "uart_comms.h"
#include "benchmark_timer.h"

#include "Module.h"
#include "OutputStream.h"
#include "MessageQueue.h"
#include "GCode.h"
#include "GCodeProcessor.h"
#include "Dispatcher.h"
#include "Robot.h"
#include "RingBuffer.h"
#include "Conveyor.h"
#include "Pin.h"
#include "Network.h"
#include "StringUtils.h"

extern "C" {
    void Board_LED_Toggle(int);
    void Board_LED_Set(int, bool);
    bool Board_LED_Test(int);
}

static bool system_running = false;
// static bool rpi_port_enabled = false;
// static uint32_t rpi_baudrate = 115200;
static Pin *aux_play_led = nullptr;
extern "C" int config_dfu_required;
extern "C" int config_second_usb_serial;

// for ?, $I or $S queries
// for ? then query_line will be nullptr
struct query_t {
    OutputStream *query_os;
    char *query_line;
};
static RingBuffer<struct query_t, 8> queries; // thread safe FIFO

static FILE *upload_fp = nullptr;
static std::string config_error_msg;

// TODO maybe move to Dispatcher
static GCodeProcessor gp;
static bool loaded_configuration = false;
static bool config_override = false;
static const char *OVERRIDE_FILE = "/sd/config-override";

#ifdef BOARD_NUCLEO
#define MAGIC_NUMBER 0x1234567898765401LL
#elif defined(BOARD_DEVEBOX)
#define MAGIC_NUMBER 0x1234567898765402LL
#elif defined(BOARD_PRIME)
#define MAGIC_NUMBER 0x1234567898765403LL
#else
#error not a recognized BOARD defined
#endif

#include "md5.h"
bool check_flashme_file(OutputStream& os, bool errors)
{
    // check the flashme.bin is on the disk first
    FILE *fp = fopen("/sd/flashme.bin", "r");
    if(fp == NULL) {
        if(errors) os.printf("ERROR: No flashme.bin file found\n");
        return false;
    }
    // check it has the correct magic number for this build
    int fs = fseek(fp, -8 - 32, SEEK_END);
    if(fs != 0) {
        os.printf("ERROR: could not seek to end of file to check magic number: %d\n", errno);
        fclose(fp);
        return false;
    }
    long ft = ftell(fp) + 8; // end of file but before md5
    uint64_t magicno;
    size_t n = fread(&magicno, 1, sizeof(magicno), fp);
    if(n != sizeof(magicno)) {
        os.printf("ERROR: could not read magic number: %d\n", errno);
        fclose(fp);
        return false;
    }

    // this is a different magic for different boards
    if(magicno != MAGIC_NUMBER) {
        os.printf("ERROR: bad magic number in file, this is not a valid firmware binary image for this board\n");
        fclose(fp);
        return false;
    }

    // read the md5 at the end of the file
    char md5[33];
    n = fread(md5, 1, sizeof(md5) - 1, fp);
    if(n != sizeof(md5) - 1) {
        os.printf("ERROR: could not read md5: %d\n", errno);
        fclose(fp);
        return false;
    }
    md5[32] = '\0';

    // read file except for last 32 bytes and calculate the md5sum
    rewind(fp);
    MD5 md5sum;
    uint8_t buf[1024];
    size_t cnt = 0;
    do {
        n = fread(buf, 1, sizeof buf, fp);
        if(n <= 0) break;
        cnt += n;
        if(cnt <= (size_t)ft) {
            md5sum.update(buf, n);
        } else {
            n = n - (cnt - ft);
            if(n > 0) md5sum.update(buf, n);
            break;
        }
    } while(!feof(fp));
    fclose(fp);

    std::string calc = md5sum.finalize().hexdigest();
    //printf("cnt: %d, ft: %ld - 1: %s, 2: %s\n", cnt, ft, calc.c_str(), md5);

    // check md5sum of the file
    if(strcmp(calc.c_str(), md5) != 0) {
        os.printf("ERROR: md5sum does not match\n");
        return false;
    }

    return true;
}

// load configuration from override file
static bool load_config_override(OutputStream& os)
{
    std::fstream fsin(OVERRIDE_FILE, std::fstream::in);
    if(fsin.is_open()) {
        std::string s;
        OutputStream nullos;
        // foreach line dispatch it
        while (std::getline(fsin, s)) {
            if(s[0] == ';') continue;
            // Parse the Gcode
            GCodeProcessor::GCodes_t gcodes;
            gp.parse(s.c_str(), gcodes);
            // dispatch it
            for(auto& i : gcodes) {
                if(i.get_code() >= 500 && i.get_code() <= 503) continue; // avoid recursion death
                if(!THEDISPATCHER->dispatch(i, nullos)) {
                    os.printf("WARNING: load_config_override: this line was not handled: %s\n", s.c_str());
                }
            }
        }
        loaded_configuration = true;
        fsin.close();

    } else {
        loaded_configuration = false;
        return false;
    }

    return true;
}

// can be called by modules when in command thread context
bool dispatch_line(OutputStream& os, const char *ln)
{
    // need a mutable copy
    std::string line(ln);

    // map some special M codes to commands as they violate the gcode spec and pass a string parameter
    // M23, M32, M117, M30 => m23, m32, m117, rm and handle as a command
    // also M28
    if(line.rfind("M23 ", 0) == 0) line[0] = 'm';
    else if(line.rfind("M30 ", 0) == 0) line.replace(0, 3, "rm");   // make into an rm command
    else if(line.rfind("M32 ", 0) == 0) line[0] = 'm';
    else if(line.rfind("M117 ", 0) == 0) line[0] = 'm';
    else if(line.rfind("M28 ", 0) == 0) {
        // handle save to file:- M28 filename
        const char *upload_filename = line.substr(4).c_str();
        upload_fp = fopen(upload_filename, "w");
        if(upload_fp != nullptr) {
            os.set_uploading(true);
            os.printf("Writing to file: %s\nok\n", upload_filename);
        } else {
            os.printf("open failed, File: %s.\nok\n", upload_filename);
        }
        return true;
    }

    // see if a command
    if(islower(line[0]) || line[0] == '$') {
        // dispatch command
        if(!THEDISPATCHER->dispatch(line.c_str(), os)) {
            if(line[0] == '$') {
                os.puts("error:Invalid statement\n");
            } else {
                os.printf("error:Unsupported command - %s\n", line.c_str());
            }

        } else if(!os.is_no_response()) {
            os.puts("ok\n");
        }
        os.set_no_response(false);

        return true;
    }

    // Handle Gcode
    GCodeProcessor::GCodes_t gcodes;

    // Parse gcode
    if(!gp.parse(line.c_str(), gcodes)) {
        if(gcodes.empty()) {
            // line failed checksum, send resend request
            os.printf("rs N%d\n", gp.get_line_number() + 1);
            return true;
        }

        auto& g = gcodes.back();
        // we have to check for certain gcodes which are known to violate spec (eg M28)
        if(g.has_error()) {
            // Word parse Error
            if(THEDISPATCHER->is_grbl_mode()) {
                os.printf("error:gcode parse failed %s - %s\n", g.get_error_message(), line.c_str());
            } else {
                os.printf("// WARNING gcode parse failed %s - %s\n", g.get_error_message(), line.c_str());
            }
            // TODO add option to HALT in this case
        } else {
            // this shouldn't happen
            printf("WARNING: parse returned false but no error\n");
        }
        gcodes.pop_back();
    }

    // if we are uploading (M28) just save entire line, we do this here to take advantage
    // of the line resend if needed
    if(os.is_uploading()) {
        if(line == "M29") {
            // done uploading, close file
            fclose(upload_fp);
            upload_fp = nullptr;
            os.set_uploading(false);
            os.printf("Done saving file.\nok\n");
            return true;
        }
        // just save the line to the file
        if(upload_fp != nullptr) {
            // write out line
            if(fputs(line.c_str(), upload_fp) < 0 || fputc('\n', upload_fp) < 0) {
                // we got an error
                fclose(upload_fp);
                upload_fp = nullptr;
                os.printf("Error:error writing to file.\n");
            }
        }
        os.printf("ok\n");
        return true;
    }

    if(gcodes.empty()) {
        // if gcodes is empty then was a M110, just send ok
        os.puts("ok\n");
        return true;
    }

    // dispatch gcodes
    // NOTE return one ok per line instead of per GCode only works for regular gcodes like G0-G3, G92 etc
    // gcodes returning data like M114 should NOT be put on multi gcode lines.
    int ngcodes = gcodes.size();
    for(auto& i : gcodes) {
        //i.dump(os);
        if(i.has_m() || i.has_g()) {
            // potentially handle M500 - M503 here
            OutputStream *pos = &os;
            std::fstream *fsout = nullptr;
            bool m500 = false;

            if(i.has_m() && (i.get_code() >= 500 && i.get_code() <= 503)) {
                if(i.get_code() == 500) {
                    // we have M500 so redirect os to a config-override file
                    fsout = new std::fstream(OVERRIDE_FILE, std::fstream::out | std::fstream::trunc);
                    if(!fsout->is_open()) {
                        os.printf("ERROR: opening file: %s\n", OVERRIDE_FILE);
                        delete fsout;
                        return true;
                    }
                    pos = new OutputStream(fsout);
                    m500 = true;

                } else if(i.get_code() == 501) {
                    if(load_config_override(os)) {
                        os.printf("configuration override loaded\nok\n");
                    } else {
                        os.printf("failed to load configuration override\nok\n");
                    }
                    return true;

                } else if(i.get_code() == 502) {
                    remove(OVERRIDE_FILE);
                    os.printf("configuration override file deleted\nok\n");
                    return true;

                } else if(i.get_code() == 503) {
                    if(loaded_configuration) {
                        os.printf("// NOTE: config override loaded\n");
                    } else {
                        os.printf("// NOTE: No config override loaded\n");
                    }
                    i.set_command('M', 500, 3); // change gcode to be M500.3
                }
            }

            // if this is a multi gcode line then dispatch must not send ok unless this is the last one
            if(!THEDISPATCHER->dispatch(i, *pos, ngcodes == 1 && !m500)) {
                // no handler processed this gcode, return ok - ignored
                if(ngcodes == 1) os.puts("ok - ignored\n");
            }

            // clean up after M500
            if(m500) {
                m500 = false;
                fsout->close();
                delete fsout;
                delete pos; // this would be the file output stream
                if(!config_override) {
                    os.printf("WARNING: override will NOT be loaded on boot\n", OVERRIDE_FILE);
                }
                os.printf("Settings Stored to %s\nok\n", OVERRIDE_FILE);
            }

        } else {
            // if it has neither g or m then it was a blank line or comment
            os.puts("ok\n");
        }
        --ngcodes;
    }

    return true;
}


static std::set<OutputStream*> output_streams;

// this is here so we do not need to duplicate this logic for
// USB serial, UART serial, Network Shell, SDCard player thread
// NOTE this can block if message queue is full. set wait to false to not wait too long
bool process_command_buffer(size_t n, char *rx_buf, OutputStream *os, char *line, size_t& cnt, bool& discard, bool wait)
{
    for (size_t i = 0; i < n; ++i) {
        line[cnt] = rx_buf[i];
        if(os->capture_fnc) {
            os->capture_fnc(line[cnt]);
            continue;
        }

        if(line[cnt] == 24) { // ^X
            if(!Module::is_halted()) {
                Module::broadcast_halt(true);
                os->puts("ALARM: Abort during cycle\n");
            }
            discard = false;
            cnt = 0;

        } else if(line[cnt] == 25) { // ^Y
            if(Module::is_halted()) {
                // will also do what $X does
                Module::broadcast_halt(false);
                os->puts("[Caution: Unlocked]\n");

            } else {
                // there is a race condition where the host may send the ^Y so fast after
                // the $J -c that it is executed first, which would leave the system in cont mode
                // We set the stop_request flag if we are not in continuous jog mode and
                // check that before setting cont mode.
                if(Conveyor::getInstance()->get_continuous_mode()) {
                    // stop continuous jog mode
                    Conveyor::getInstance()->set_continuous_mode(false);
                } else {
                    // set generic stop request, currently used to see if we got ^Y before cont mode and to abort
                    // some file commands
                    os->set_stop_request(true);
                }
            }

        } else if(line[cnt] == '?') {
            if(!queries.full()) {
                queries.push_back({os, nullptr});
            }

        } else if(discard) {
            // we discard long lines until we get the newline
            if(line[cnt] == '\n') discard = false;

        } else if(cnt >= MAX_LINE_LENGTH - 1) {
            // discard long lines
            discard = true;
            cnt = 0;
            os->puts("error:Discarding long line\n");

        } else if(line[cnt] == '\n') {
            os->clear_flags(); // clear the done flag here to avoid race conditions
            line[cnt] = '\0'; // remove the \n and nul terminate
            if(cnt >= 2 && line[0] == '$' && (line[1] == 'I' || line[1] == 'S' || line[1] == 'X')) {
                if(line[1] == 'X') {
                    // handle $X here
                    if(Module::is_halted()) {
                        Module::broadcast_halt(false);
                        os->puts("[Caution: Unlocked]\n");
                    }
                    os->puts("ok\n");

                } else if(!queries.full()) {
                    // Handle $I and $S as instant queries
                    queries.push_back({os, strdup(line)});
                }

            } else {
                if(!send_message_queue(line, os, wait)) {
                    // we were told not to wait and the queue was full
                    // the caller will now need to call send_message_queue()
                    cnt = 0;
                    return false;
                }
            }
            cnt = 0;

        } else if(line[cnt] == '\r') {
            // ignore CR
            continue;

        } else if(line[cnt] == 8 || line[cnt] == 127) { // BS or DEL
            if(cnt > 0) --cnt;

        } else {
            ++cnt;
        }
    }

    return true;
}

static volatile bool abort_comms = false;

extern "C" size_t write_cdc(uint8_t, const char *buf, size_t len);
extern "C" size_t read_cdc(uint8_t, char *buf, size_t len);
extern "C" int setup_cdc();
extern "C" int vcom_is_connected(uint8_t);
extern "C" uint32_t get_dropped_bytes(uint8_t);

static void usb_comms(void *param)
{
    int inst= (int)param;
    printf("DEBUG: USB Comms%d thread running\n", inst+1);

    // we set this to 1024 so ymodem will run faster (but if not needed then it can be as low as 256)
    const size_t usb_rx_buf_sz = 1024;
    char *usb_rx_buf = (char *)malloc(usb_rx_buf_sz);
    if(usb_rx_buf == nullptr) {
        printf("FATAL: no memory for usb_rx_buf\n");
        return;
    }

    do {
        // on first connect we send a welcome message
        while(!abort_comms) {
            // when we get the first connection it sends a one byte message to wake us up
            // it will block here until a connection is available
            size_t n = read_cdc(inst, usb_rx_buf, 1);
            if(n > 0 && vcom_is_connected(inst)) {
                break;
            }
        }

        if(abort_comms) break;

        //printf("DEBUG: CDC%d connected\n", inst+1);

        // create an output stream that writes to the cdc
        OutputStream *os= new OutputStream([inst](const char *buf, size_t len) { return write_cdc(inst, buf, len); });
        output_streams.insert(os);
        vTaskDelay(pdMS_TO_TICKS(100));
        os->printf("Welcome to Smoothie\nok\n");

        // now read lines and dispatch them
        char line[MAX_LINE_LENGTH];
        size_t cnt = 0;
        bool discard = false;

        while(!abort_comms) {
            // this read will block if no data is available
            size_t n = read_cdc(inst, usb_rx_buf, usb_rx_buf_sz);
            if(n > 0) {
                if(os->fast_capture_fnc) {
                    if(!os->fast_capture_fnc(usb_rx_buf, n)) {
                        os->fast_capture_fnc = nullptr; // we are done ok
                    }
                } else {
                    process_command_buffer(n, usb_rx_buf, os, line, cnt, discard);
                }
            }
            #if 1
            uint32_t db;
            if((db= get_dropped_bytes(inst)) > 0) {
                printf("WARNING: dropped bytes detected on USB Comms%d: %lu\n", inst+1, db);
            }
            #endif
        }

        output_streams.erase(os);
        delete os;
    }while(false);

    free(usb_rx_buf);
    printf("DEBUG: USB Comms%d thread exiting\n", inst+1);
    vTaskDelete(NULL);
}

static void uart_comms(void *)
{
    printf("DEBUG: UART Comms thread running\n");
    set_notification_uart(xTaskGetCurrentTaskHandle());

    // create an output stream that writes to the uart
    static OutputStream os([](const char *buf, size_t len) { return write_uart(buf, len); });
    output_streams.insert(&os);

    const TickType_t waitms = pdMS_TO_TICKS( 300 );

    char rx_buf[256];
    char line[MAX_LINE_LENGTH];
    size_t cnt = 0;
    bool discard = false;
    while(!abort_comms) {
        // Wait to be notified that there has been a UART irq. (it may have been rx or tx so may not be anything to read)
        uint32_t ulNotificationValue = ulTaskNotifyTake( pdFALSE, waitms );

        if( ulNotificationValue != 1 ) {
            /* The call to ulTaskNotifyTake() timed out. check anyway */
            if(abort_comms) break;
        }

        size_t n = read_uart(rx_buf, sizeof(rx_buf));
        if(n > 0) {
            process_command_buffer(n, rx_buf, &os, line, cnt, discard);
        }
    }
    output_streams.erase(&os);
    printf("DEBUG: UART Comms thread exiting\n");
    vTaskDelete(NULL);
}

void set_abort_comms()
{
    abort_comms = true;

#ifndef NONETWORK
    Network *network = static_cast<Network *>(Module::lookup("network"));
    if(network != nullptr) network->set_abort();
#endif
}

// this prints the string to all consoles that are connected and active
// must be called in command thread context
void print_to_all_consoles(const char *str)
{
    for(auto i : output_streams) {
        i->puts(str);
    }
}

static void handle_query(bool need_done)
{
    // set in comms thread, and executed in the command thread to avoid thread clashes.
    // the trouble with this is that ? does not reply if a long command is blocking call to dispatch_line
    // test commands for instance or a long line when the queue is full or G4 etc
    // so long as safe_sleep() is called then this will still be processed
    // also dispatch any instant queries we have recieved
    while(!queries.empty()) {
        struct query_t q = queries.pop_front();
        if(q.query_line == nullptr) { // it is a ? query
            std::string r;
            Robot::getInstance()->get_query_string(r);
            q.query_os->puts(r.c_str());

        } else {
            Dispatcher::getInstance()->dispatch(q.query_line, *q.query_os);
            free(q.query_line);
        }
        // on last one (Does presume they are the same os though)
        // FIXME may not work as expected when there are multiple I/O channels and output streams
        if(need_done && queries.empty()) q.query_os->set_done();
    }
}

/*
 * All commands must be executed in the context of this thread. It is equivalent to the main_loop in v1.
 * Commands are sent to this thread via the message queue from things that can block (like I/O)
 * Other things can call dispatch_line direct from the in_command_ctx call.
 */
extern "C" bool DFU_requested_detach();
static void command_handler()
{
    printf("DEBUG: Command thread running\n");

    for(;;) {
        char *line;
        OutputStream *os = nullptr;
        bool idle = false;

        // This will timeout after 100 ms
        if(receive_message_queue(&line, &os)) {
            //printf("DEBUG: got line: %s\n", line);
            dispatch_line(*os, line);
            handle_query(false);
            os->set_done(); // set after all possible output

        } else {
            // timed out or other error
            idle = true;
            if(config_error_msg.empty()) {
                // toggle led to show we are alive, but idle
                Board_LED_Toggle(0);
            }
            handle_query(true);

            // special case if we see we got a DFU detach we call the dfu command
            if(config_dfu_required == 1 && DFU_requested_detach()) {
                print_to_all_consoles("DFU firmware download has been requested, going down for update\n");
                vTaskDelay(pdMS_TO_TICKS(100));
                OutputStream nullos;
                dispatch_line(nullos, "dfu");
                // we should not return from this, if we do it means the dfu loader is not in qspi
                config_dfu_required  = 0; // disable it for now
            }
        }

        // call in_command_ctx for all modules that want it
        // dispatch_line can be called from that
        Module::broadcast_in_commmand_ctx(idle);

        // we check the queue to see if it is ready to run
        // we specifically deal with this in append_block, but need to check for other places
        if(Conveyor::getInstance() != nullptr) {
            Conveyor::getInstance()->check_queue();
        }
    }
}

// called only in command thread context, it will sleep (and yield) thread but will also
// process things like instant query
void safe_sleep(uint32_t ms)
{
    // here we need to sleep (and yield) for 10ms then check if we need to handle the query command
    TickType_t delayms = pdMS_TO_TICKS(10); // 10 ms sleep
    while(ms > 0) {
        vTaskDelay(delayms);
        // presumably there is a long running command that
        // may need Outputstream which will set done flag when it is done
        handle_query(false);

        if(ms > 10) {
            ms -= 10;
        } else {
            break;
        }
    }
}

#include "CommandShell.h"
#include "SlowTicker.h"
#include "FastTicker.h"
#include "StepTicker.h"
#include "ConfigReader.h"
#include "Switch.h"
#include "Planner.h"
#include "Robot.h"
#include "KillButton.h"
#include "Extruder.h"
#include "TemperatureControl.h"
#include "Adc.h"
#include "Adc3.h"
#include "Pwm.h"
#include "CurrentControl.h"
#include "Laser.h"
#include "Endstops.h"
#include "ZProbe.h"
#include "Player.h"

#define SD_CONFIG

#ifndef SD_CONFIG
#include STRING_CONFIG_H
static std::string str(string_config);
static std::stringstream ss(str);
#else
extern "C" bool setup_sdmmc();
static FATFS fatfs; /* File system object */
#endif

// voltage monitors, name -> <channel,scale>
static std::map<std::string, std::tuple<int32_t, float>> voltage_monitors;

float get_voltage_monitor(const char* name)
{
    auto p = voltage_monitors.find(name);
    if(p == voltage_monitors.end()) return 0;
    Adc3 *adc = Adc3::getInstance();
    float v = adc->read_voltage(std::get<0>(p->second));
    if(isinf(v)) {
        return v;
    } else {
        // scale it appropriately
        return v * std::get<1>(p->second);
    }
}

int get_voltage_monitor_names(const char *names[])
{
    int i = 0;
    for(auto& p : voltage_monitors) {
        if(names != nullptr)
            names[i] = p.first.c_str();
        ++i;
    }
    return i;
}

// this is used to add callback functions to be called once the system is running
static std::vector<StartupFunc_t> startup_fncs;
void register_startup(StartupFunc_t sf)
{
    startup_fncs.push_back(sf);
}

static void smoothie_startup(void *)
{
    printf("INFO: Smoothie V2.alpha Build for %s - starting up\n", BUILD_TARGET);

    // led 4 indicates boot phase 2 starts
    Board_LED_Set(3, true);

    // create the FastTicker here as it is used by some modules
    FastTicker *fast_ticker = FastTicker::getInstance();

    // create the StepTicker, don't start it yet
    StepTicker *step_ticker = StepTicker::getInstance();

#ifdef DEBUG
    // when debug is enabled we cannot run stepticker at full speed
    step_ticker->set_frequency(50000); // 50KHz
#else
    step_ticker->set_frequency(200000); // 200KHz
#endif
    step_ticker->set_unstep_time(1); // 1us step pulse by default

    bool flash_on_boot = true;
    bool ok = false;

    // open the config file
    do {
#ifdef SD_CONFIG
        if(!setup_sdmmc()) {
            std::cout << "Error: setting up sdmmc\n";
            break;
        }

        // TODO check the card is inserted

        int ret = f_mount(&fatfs, "sd", 1);
        if(FR_OK != ret) {
            std::cout << "Error: mounting: " << "/sd: " << ret << "\n";
            break;
        }



        std::fstream fs;
        fs.open("/sd/config.ini", std::fstream::in);
        if(!fs.is_open()) {
            std::cout << "Error: opening file: " << "/sd/config.ini" << "\n";
            // unmount sdcard
            //f_unmount("sd");
            break;
        }


        ConfigReader cr(fs);
        printf("DEBUG: Starting configuration of modules from sdcard...\n");
#else
        ConfigReader cr(ss);
        printf("DEBUG: Starting configuration of modules from memory...\n");
#endif

        {
            // get general system settings
            ConfigReader::section_map_t m;
            if(cr.get_section("general", m)) {
                bool f = cr.get_bool(m, "grbl_mode", false);
                THEDISPATCHER->set_grbl_mode(f);
                printf("INFO: grbl mode %s\n", f ? "set" : "not set");
                config_override = cr.get_bool(m, "config-override", false);
                printf("INFO: use config override is %s\n", config_override ? "set" : "not set");
                // rpi_port_enabled = cr.get_bool(m, "rpi_port_enable", false);
                // rpi_baudrate = cr.get_int(m, "rpi_baudrate", 115200);
                // printf("INFO: rpi port is %senabled, at baudrate: %lu\n", rpi_port_enabled ? "" : "not ", rpi_baudrate);
                std::string p = cr.get_string(m, "aux_play_led", "nc");
                aux_play_led = new Pin(p.c_str(), Pin::AS_OUTPUT);
                if(!aux_play_led->connected()) {
                    delete aux_play_led;
                    aux_play_led = nullptr;
                } else {
                    printf("INFO: auxilliary play led set to %s\n", aux_play_led->to_string().c_str());
                }
                flash_on_boot = cr.get_bool(m, "flash_on_boot", true);
                printf("INFO: flash on boot is %s\n", flash_on_boot?"enabled":"disabled");
                bool enable_dfu= cr.get_bool(m, "dfu_enable", false);
                config_dfu_required= enable_dfu ? 1 : 0; // set it in the USB stack
                printf("INFO: dfu is %s\n", enable_dfu?"enabled":"disabled");
                config_second_usb_serial= cr.get_bool(m, "second_usb_serial_enable", false) ? 1 : 0;
                printf("INFO: second usb serial is %s\n", config_second_usb_serial?"enabled":"disabled");
            }
        }

        printf("DEBUG: configure the planner\n");
        Planner *planner = Planner::getInstance();
        planner->configure(cr);

        printf("DEBUG: configure the conveyor\n");
        Conveyor *conveyor = Conveyor::getInstance();
        conveyor->configure(cr);

        printf("DEBUG: configure the robot\n");
        Robot *robot = Robot::getInstance();
        if(!robot->configure(cr)) {
            printf("ERROR: Configuring robot failed\n");
            break;
        }

        ///////////////////////////////////////////////////////////
        // configure core modules here
        {
            // Pwm needs to be initialized, there are two PWM timers and each
            // can a frequency.
            // This needs to be done before any module that could use it
            // NOTE that Pwm::post_config_setup() needs to be called after all modules have been created
            uint32_t deffreq = 10000; // default is 10KHz
            ConfigReader::section_map_t m;
            if(cr.get_section("pwm1", m)) {
                uint32_t freq = cr.get_int(m, "frequency", deffreq);
                Pwm::setup(0, freq);
                printf("INFO: PWM1 frequency set to %lu Hz\n", freq);
            }
            if(cr.get_section("pwm2", m)) {
                uint32_t freq = cr.get_int(m, "frequency", deffreq);
                Pwm::setup(1, freq);
                printf("INFO: PWM2 frequency set to %lu Hz\n", freq);
            }
        }

        {
            printf("DEBUG: configure extruder\n");
            // this creates any configured extruders then we can remove it
            Extruder ex("extruder loader");
            if(!ex.configure(cr)) {
                printf("INFO: no Extruders loaded\n");
            }
        }

        {
            printf("DEBUG: configure temperature control\n");
            // this creates any configured temperature controls
            if(!TemperatureControl::load_controls(cr)) {
                printf("INFO: no Temperature Controls loaded\n");
            }
        }

        ///////////////////////////////////////////////////////////////////////////////////////
        // create all registered modules, the addresses are stored in a known location in flash
        extern uint32_t __registered_modules_start;
        extern uint32_t __registered_modules_end;
        uint32_t *g_pfnModules = &__registered_modules_start;
        while (g_pfnModules < &__registered_modules_end) {
            uint32_t *addr = g_pfnModules++;
            bool (*pfnModule)(ConfigReader & cr) = (bool (*)(ConfigReader & cr)) * addr;
            // this calls the registered create function for the module
            pfnModule(cr);
        }

        // end of module creation and configuration
        ////////////////////////////////////////////////////////////////

        {
            // configure voltage monitors if any
            // NOTE all voltage monitors must use ADC3_n where n is 0-5
            Adc3 *adc = Adc3::getInstance();
            ConfigReader::section_map_t m;
            if(cr.get_section("voltage monitor", m)) {
                for(auto& s : m) {
                    std::string k = s.first;
                    std::string v = s.second;
                    float scale = 11.0F;
                    int32_t ch = adc->from_string(v.c_str(), scale);
                    if(ch < 0) continue;

                    if(!adc->allocate(ch)) {
                        printf("WARNING: Failed to allocate %s voltage monitor, illegal or inuse ADC3 channel: %lu\n", k.c_str(), ch);
                    } else {
                        voltage_monitors[k] = std::make_tuple(ch, scale);
                        printf("DEBUG: added voltage monitor: %s, ADC3 channel: %lu, scale: %f\n", k.c_str(), ch, scale);
                    }
                }
            }
            // special internal ADC3 channels
            voltage_monitors["vref"] = std::make_tuple(-1, 1.0F);
            voltage_monitors["vbat"] = std::make_tuple(-2, 1.0F);
            // setup board defaults if not defined
#ifdef BOARD_NUCLEO
            std::map<std::string, std::tuple<int32_t, float>> names { {"vmotor", {1, 11.0F}},  {"vfet", {2, 11.0F}} };
#elif defined(BOARD_DEVEBOX)
            std::map<std::string, std::tuple<int32_t, float>> names { {"vmotor", {0, 11.0F}},  {"vfet", {3, 11.0F}} };
#elif defined(BOARD_PRIME)
            std::map<std::string, std::tuple<int32_t, float>> names { {"vmotor", {0, 11.0F}},  {"vfet", {1, 11.0F}} };
#endif
            for(auto& n : names) {
                if(voltage_monitors.find(n.first) == voltage_monitors.end()) {
                    int32_t ch = std::get<0>(n.second);
                    float scale = std::get<1>(n.second);
                    if(adc->allocate(ch)) {
                        voltage_monitors[n.first] = std::make_tuple(ch, scale);
                        printf("DEBUG: allocated %s voltage monitor to channel: ADC3_%lu, scale: %f\n", n.first.c_str(), ch, scale);
                    } else {
                        printf("WARNING: Failed to allocate %s voltage monitor, channel: %lu\n", n.first.c_str(), ch);
                    }
                }
            }
        }

        // setup ADC
        if(!Adc::post_config_setup()) {
            printf("ERROR: ADC failed to setup\n");
        }

        // setup PWM
        if(!Pwm::post_config_setup()) {
            printf("ERROR: Pwm::post_config_setup failed\n");
        }

#ifdef SD_CONFIG
        // close the file stream
        fs.close();

        // unmount sdcard
        //f_unmount("sd");
#endif

        // initialize planner before conveyor this is when block queue is created
        // which needs to know how many actuators there are, which it gets from robot
        if(!planner->initialize(robot->get_number_registered_motors())) {
            printf("FATAL: planner failed to initialize, out of memory?\n");
            break;
        }

        // start conveyor last
        conveyor->start();

        printf("DEBUG: ...Ending configuration of modules\n");
        ok = true;
    } while(0);

    // create the commandshell, it is dependent on some of the above
    CommandShell *shell = new CommandShell();
    shell->initialize();

    if(ok) {
        if(!fast_ticker->start()) {
            printf("WARNING: failed to start FastTicker (maybe nothing is using it?)\n");
        }

        if(!step_ticker->start()) {
            printf("Error: failed to start StepTicker\n");
        }

        if(!Adc::start()) {
            printf("Error: failed to start ADC\n");
        }

    } else {
        printf("ERROR: Configure failed\n");
        config_error_msg = "There was a fatal error in the config.ini this must be fixed to continue\nOnly some shell commands are allowed and sdcard access\n";
        printf(config_error_msg.c_str());
        // Module::broadcast_halt(true);
    }

    // create queue for incoming buffers from the I/O ports
    if(!create_message_queue()) {
        // Failed to create the queue.
        printf("Error: failed to create comms i/o queue\n");
    }

    // Start comms threads higher priority than the command thread
    // fixed stack size of 4k Bytes each
    xTaskCreate(uart_comms, "UARTCommsThread", 1500 / 4, NULL, (tskIDLE_PRIORITY + COMMS_PRI), (TaskHandle_t *) NULL);

    // setup usb and cdc first
    if(setup_cdc()) {
        xTaskCreate(usb_comms, "USBCommsThread", 1500 / 4, 0, (tskIDLE_PRIORITY + COMMS_PRI), (TaskHandle_t *) NULL);
        if(config_second_usb_serial == 1) {
            xTaskCreate(usb_comms, "USBCommsThread", 1500 / 4, (void*)1, (tskIDLE_PRIORITY + COMMS_PRI), (TaskHandle_t *) NULL);
        }

    }else{
        printf("FATAL: USB and/or CDC setup failed\n");
    }

    // run any startup functions that have been registered
    for(auto& f : startup_fncs) {
        f();
    }
    startup_fncs.clear();
    startup_fncs.shrink_to_fit();

    struct mallinfo mi = mallinfo();
    printf("DEBUG: Initial: free malloc memory= %d, free sbrk memory= %d, Total free= %d\n", mi.fordblks, xPortGetFreeHeapSize() - mi.fordblks, xPortGetFreeHeapSize());

    // indicate we are up and running
    system_running = ok;

    // load config override if set
    if(ok && config_override) {
        OutputStream os(&std::cout);
        if(load_config_override(os)) {
            os.printf("INFO: configuration override loaded\n");

        } else {
            os.printf("INFO: No saved configuration override\n");
        }
    }

    if(ok) {
        // led 3,4 off indicates boot phase 2 complete
        Board_LED_Set(2, false);
        Board_LED_Set(3, false);
    }

    if(flash_on_boot) {
        OutputStream os(&std::cout);
        if(check_flashme_file(os, false)) {
            // we have a valid flashme file, so flash
            THEDISPATCHER->dispatch("flash", os);
            // we should not get here, if we did there was a problem with the flashme.bin file
            printf("ERROR: Problem with the flashme.bin file, no update done\n");
            config_error_msg = "ERROR: There was a problem flashing the flashme.bin file, it seems to be invalid\n";

        } else {
            printf("INFO: No valid flashme.bin file found\n");
        }
    } else {
        printf("INFO: flash on boot is disabled\n");
    }

    // run the command handler in this thread
    command_handler();

    // does not return from above
}

extern "C" void setup_xprintf();
extern "C" void main_system_setup();
extern "C" int rtc_init();

std::string get_mcu();

int main(int argc, char *argv[])
{
    // setup clock and caches etc (in HAL)
    main_system_setup();

    benchmark_timer_init();

    setup_xprintf();

    if(setup_uart() < 0) {
        printf("FATAL: UART setup failed\n");
    }

    printf("%s on %s\n", get_mcu().c_str(), BUILD_TARGET);
    printf("MCU clock rate= %lu Hz\n", SystemCoreClock);

    // led 4 indicates boot phase 1 complete
    Board_LED_Set(3, true);

    if(rtc_init() != 1) {
        printf("ERROR: Failed to init RTC\n");
    }

    // launch the startup thread which will become the command thread that executes all incoming commands
    // set to be lower priority than comms, although it maybe better to invert them as we don;t really
    // want the commandthread preempted by the comms thread everytime it gets data.
    // 10000 Bytes stack
    xTaskCreate(smoothie_startup, "CommandThread", 10000 / 4, NULL, (tskIDLE_PRIORITY + CMDTHRD_PRI), (TaskHandle_t *) NULL);

    /* Start the scheduler */
    vTaskStartScheduler();

    // never gets here
    return 1;
}

#define TICKS2MS( xTicks ) ( ((xTicks) * 1000.0F) / configTICK_RATE_HZ )

// hooks from freeRTOS
extern "C" void vApplicationIdleHook( void )
{
    static TickType_t last_time_check = xTaskGetTickCount();
    if(TICKS2MS(xTaskGetTickCount() - last_time_check) >= 300) {
        last_time_check = xTaskGetTickCount();
        if(!config_error_msg.empty()) {
            // handle config error
            // flash both leds
            Board_LED_Toggle(0);
            Board_LED_Toggle(1);
        } else {
            // handle play led 1 and aux play led
            if(system_running) {
                if(Module::is_halted()) {
                    Board_LED_Toggle(1);
                    if(aux_play_led != nullptr) {
                        aux_play_led->set(Board_LED_Test(1));
                    }
                } else {
                    bool f = !Conveyor::getInstance()->is_idle();
                    Board_LED_Set(1, f);
                    if(aux_play_led != nullptr) {
                        aux_play_led->set(f);
                    }
                }
            }
        }
    }
}

extern "C" void vApplicationTickHook( void )
{
    /* This function will be called by each tick interrupt if
    configUSE_TICK_HOOK is set to 1 in FreeRTOSConfig.h.  User code can be
    added here, but the tick hook is called from an interrupt context, so
    code must not attempt to block, and only the interrupt safe FreeRTOS API
    functions can be used (those that end in FromISR()). */
}

extern "C" void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
    ( void ) pcTaskName;
    ( void ) pxTask;

    /* Run time stack overflow checking is performed if
    configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
    function is called if a stack overflow is detected. */
    __disable_irq();
    Board_LED_Set(0, true);
    Board_LED_Set(1, false);
    Board_LED_Set(2, true);
    Board_LED_Set(3, true);
    __asm("bkpt #0");
    for( ;; );
}


extern "C" void vApplicationMallocFailedHook( void )
{
    /* vApplicationMallocFailedHook() will only be called if
    configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h.  It is a hook
    function that will get called if a call to pvPortMalloc() fails.
    pvPortMalloc() is called internally by the kernel whenever a task, queue,
    timer or semaphore is created.  It is also called by various parts of the
    demo application.  If heap_1.c or heap_2.c are used, then the size of the
    heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
    FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
    to query the size of free heap space that remains (although it does not
    provide information on how the remaining heap might be fragmented). */
#if 0
    taskDISABLE_INTERRUPTS();
    __asm("bkpt #0");
    for( ;; );
#else
    // we don't want to use any memory for this
    // returns NULL to the caller
    write_uart("FATAL: malloc/sbrk out of memory\n", 33);
    return;
#endif
}

extern "C" void HardFault_Handler(void)
{
    Board_LED_Set(0, true);
    Board_LED_Set(1, true);
    Board_LED_Set(2, false);
    Board_LED_Set(3, false);
    __asm("bkpt #0");
    for( ;; );
}

#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
extern "C" void assert_failed(uint8_t* file, uint32_t line)
{
    printf("ERROR: HAL assert failed: file %s on line %lu\n", file, line);

    __disable_irq();
    __asm("bkpt #0");
    /* Infinite loop */
    while (1) {
    }
}
#endif

#ifdef configASSERT
void vAssertCalled( const char *pcFile, uint32_t ulLine )
{
    volatile uint32_t ulBlockVariable = 0UL;
    volatile const char *pcAssertedFileName;
    volatile int iAssertedErrno;
    volatile uint32_t ulAssertedLine;

    ulAssertedLine = ulLine;
    iAssertedErrno = errno;
    pcAssertedFileName = strrchr( pcFile, '/' );

    /* These variables are set so they can be viewed in the debugger, but are
    not used in the code - the following lines just remove the compiler warning
    about this. */
    ( void ) ulAssertedLine;
    ( void ) iAssertedErrno;

    if( pcAssertedFileName == 0 ) {
        pcAssertedFileName = strrchr( pcFile, '\\' );
    }
    if( pcAssertedFileName != NULL ) {
        pcAssertedFileName++;
    } else {
        pcAssertedFileName = pcFile;
    }
    printf("ERROR: vAssertCalled( %s, %ld\n", pcFile, ulLine);

    // return;

    /* Setting ulBlockVariable to a non-zero value in the debugger will allow
    this function to be exited. */
    taskDISABLE_INTERRUPTS();
    {
        __asm volatile ("bkpt #0");
        while( ulBlockVariable == 0UL ) {
            __asm volatile( "NOP" );
        }
    }
    taskENABLE_INTERRUPTS();
}
#endif
