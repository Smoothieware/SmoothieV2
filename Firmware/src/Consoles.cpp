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

#include "benchmark_timer.h"
#include "CommandShell.h"
#include "ConfigReader.h"
#include "Consoles.h"
#include "Conveyor.h"
#include "Dispatcher.h"
#include "GCode.h"
#include "GCodeProcessor.h"
#include "main.h"
#include "MessageQueue.h"
#include "Module.h"
#include "Network.h"
#include "OutputStream.h"
#include "Pin.h"
#include "RingBuffer.h"
#include "Robot.h"
#include "StringUtils.h"
#include "uart_debug.h"
#include "leds.h"

#include "FreeRTOS.h"
#include "task.h"
#include "ff.h"
#include "semphr.h"

extern "C" int config_second_usb_serial;
static GCodeProcessor gp;

// for ?, $I or $S queries
// for ? then query_line will be nullptr
struct query_t {
    OutputStream *query_os;
    char *query_line;
};
static RingBuffer<struct query_t, 8> queries; // thread safe FIFO

static FILE *upload_fp = nullptr;
static bool loaded_configuration = false;
bool config_override = false;

// load configuration from override file
bool load_config_override(OutputStream& os, const char *fn)
{
    std::fstream fsin(fn, std::fstream::in);
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
    configASSERT(strncmp(pcTaskGetName(NULL), "CommandThread", configMAX_TASK_NAME_LEN-1) == 0);

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
                    fsout = new std::fstream(DEFAULT_OVERRIDE_FILE, std::fstream::out | std::fstream::trunc);
                    if(!fsout->is_open()) {
                        os.printf("ERROR: opening file: %s\n", DEFAULT_OVERRIDE_FILE);
                        delete fsout;
                        return true;
                    }
                    pos = new OutputStream(fsout);
                    m500 = true;

                } else if(i.get_code() == 501) {
                    if(load_config_override(os)) {
                        os.printf("configuration override %s loaded\nok\n", DEFAULT_OVERRIDE_FILE);
                    } else {
                        os.printf("failed to load configuration override %s\nok\n", DEFAULT_OVERRIDE_FILE);
                    }
                    return true;

                } else if(i.get_code() == 502) {
                    remove(DEFAULT_OVERRIDE_FILE);
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
                    os.printf("WARNING: override will NOT be loaded on boot\n", DEFAULT_OVERRIDE_FILE);
                }
                os.printf("Settings Stored to %s\nok\n", DEFAULT_OVERRIDE_FILE);
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
// NOTE this can block if message queue is full. set wait to false to not wait at all
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
                print_to_all_consoles("ALARM: Abort during cycle\n");
            }
            discard = false;
            cnt = 0;

        } else if(line[cnt] == 25) { // ^Y
            if(Module::is_halted()) {
                // will also do what $X does
                Module::broadcast_halt(false);
                os->puts("[Caution: Unlocked]\n");

            } else {
                os->set_stop_request(true);
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
extern "C" uint32_t vcom_get_dropped_bytes(uint8_t);

static void usb_comms(void *param)
{
    int inst = (int)param;
    printf("DEBUG: USB Comms%d thread running\n", inst + 1);

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
        OutputStream *os = new OutputStream([inst](const char *buf, size_t len) { return write_cdc(inst, buf, len); });
        os->set_is_usb();
        output_streams.insert(os);
        vTaskDelay(pdMS_TO_TICKS(100));

        if(get_config_error_msg() != nullptr) {
            // if there was a serious config error we print it out here
            os->printf("\n%s\n", get_config_error_msg());
        }

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
            if((db = vcom_get_dropped_bytes(inst)) > 0) {
                printf("WARNING: dropped bytes detected on USB Comms%d: %lu\n", inst + 1, db);
            }
#endif
        }

        output_streams.erase(os);
        delete os;
    } while(false);

    free(usb_rx_buf);
    printf("DEBUG: USB Comms%d thread exiting\n", inst + 1);
    vTaskDelete(NULL);
}

// debug port
static void uart_debug_comms(void *)
{
    printf("DEBUG: UART Debug Comms thread running\n");
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
    printf("DEBUG: UART Debug Comms thread exiting\n");
    vTaskDelete(NULL);
}

#include "Uart.h"
static uint8_t uart_channel;
static UART::settings_t uart_console_settings;
static void uart_console_comms(void *)
{
    printf("DEBUG: UART Console Comms thread running\n");
    UART *uart = UART::getInstance(uart_channel);
    if(!uart->init(uart_console_settings)) {
        printf("ERROR: Failed in init uart 1\n");
        return;
    }

    // create an output stream that writes to this uart
    static OutputStream os([uart](const char *buf, size_t len) { return uart->write((uint8_t*)buf, len); });
    output_streams.insert(&os);

    const TickType_t waitms = pdMS_TO_TICKS(300);

    char rx_buf[256];
    char line[MAX_LINE_LENGTH];
    size_t cnt = 0;
    bool discard = false;
    while(!abort_comms) {
        // this will block until a character is available or timeout
        size_t n = uart->read((uint8_t*)rx_buf, sizeof(rx_buf), waitms);
        if(n > 0) {
            process_command_buffer(n, rx_buf, &os, line, cnt, discard);
        }
    }
    output_streams.erase(&os);
    printf("DEBUG: UART Console Comms thread exiting\n");
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
extern "C" void DFU_reset_requested_detach();
extern "C" int config_dfu_required;
void command_handler()
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
            if(get_config_error_msg() == nullptr) {
                // toggle led to show we are alive, but idle
                Board_LED_Toggle(0);
            }
            handle_query(true);

            // special case if we see we got a DFU detach we call the dfu command
            if(config_dfu_required == 1 && DFU_requested_detach()) {
                if(!CommandShell::is_busy()) {
                    print_to_all_consoles("DFU firmware download has been requested, going down for update\n");
                    vTaskDelay(pdMS_TO_TICKS(100));
                    OutputStream stdout_os(&std::cout);
                    std::string str;
                    CommandShell::getInstance()->dfu_cmd(str, stdout_os);
                    // we should not return from this, if we do it means the dfu loader is not in qspi
                    config_dfu_required  = 0; // disable it for now
                    print_to_all_consoles("DFU is not supported or not in flash\n");
                } else {
                    print_to_all_consoles("DFU is not allowed while printing or heaters are on\n");
                    DFU_reset_requested_detach();
                }
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

static bool uart_console_enabled;
bool start_consoles()
{
    // create queue for incoming buffers from the I/O ports
    if(!create_message_queue()) {
        // Failed to create the queue.
        printf("Error: failed to create comms i/o queue\n");
    }

    // Start debug comms threads higher priority than the command thread
    // fixed stack size of 4k Bytes each
    xTaskCreate(uart_debug_comms, "UARTDebugThread", 1500 / 4, NULL, (tskIDLE_PRIORITY + COMMS_PRI), (TaskHandle_t *) NULL);

    if(uart_console_enabled) {
        xTaskCreate(uart_console_comms, "UARTConsoleThread", 1500 / 4, NULL, (tskIDLE_PRIORITY + COMMS_PRI), (TaskHandle_t *) NULL);
    }

    // setup usb and cdc first
    if(setup_cdc()) {
        xTaskCreate(usb_comms, "USBCommsThread", 1500 / 4, 0, (tskIDLE_PRIORITY + COMMS_PRI), (TaskHandle_t *) NULL);
        if(config_second_usb_serial == 1) {
            xTaskCreate(usb_comms, "USBCommsThread", 1500 / 4, (void*)1, (tskIDLE_PRIORITY + COMMS_PRI), (TaskHandle_t *) NULL);
        }

    } else {
        printf("FATAL: USB and/or CDC setup failed\n");
    }

    return true;
}

bool configure_consoles(ConfigReader& cr)
{
    printf("DEBUG: configure the consoles\n");

    // get config system settings
    ConfigReader::section_map_t cm;
    if(cr.get_section("consoles", cm)) {
        config_second_usb_serial = cr.get_bool(cm, "second_usb_serial_enable", false) ? 1 : 0;
        printf("INFO: second usb serial is %s\n", config_second_usb_serial ? "enabled" : "disabled");

    }

    ConfigReader::section_map_t ucm;
    if(cr.get_section("uart console", ucm)) {
        uart_console_enabled = cr.get_bool(ucm, "enable", false);
        printf("INFO: uart console is %s\n", uart_console_enabled ? "enabled" : "disabled");
        if(uart_console_enabled) {
            uart_channel = cr.get_int(ucm, "channel", 0);
            uart_console_settings.baudrate = cr.get_int(ucm, "baudrate", 115200);
            uart_console_settings.bits = cr.get_int(ucm, "bits", 8);
            uart_console_settings.stop_bits = cr.get_int(ucm, "stop_bits", 1);
            std::string parity = cr.get_string(ucm, "parity", "none");
            if(parity == "none") uart_console_settings.parity = 0;
            else if(parity == "odd") uart_console_settings.parity = 1;
            else if(parity == "even") uart_console_settings.parity = 2;
            else printf("ERROR: uart console parity must be one of none|odd|even\n");
            printf("INFO: uart console settings: channel=%d, baudrate=%lu, bits=%d, stop_bits=%d, parity=%d\n",
                   uart_channel, uart_console_settings.baudrate, uart_console_settings.bits, uart_console_settings.stop_bits, uart_console_settings.parity);
        }

    } else {
        printf("INFO: no uart console section found, disabled\n");
        uart_console_enabled = false;
    }

    return true;
}
