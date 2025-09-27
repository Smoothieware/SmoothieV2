#include "CommandShell.h"
#include "OutputStream.h"
#include "Dispatcher.h"
#include "Module.h"
#include "StringUtils.h"
#include "Robot.h"
#include "AutoPushPop.h"
#include "StepperMotor.h"
#include "main.h"
#include "TemperatureControl.h"
#include "ConfigWriter.h"
#include "Conveyor.h"
#include "version.h"
#include "ymodem.h"
#include "Adc.h"
#include "FastTicker.h"
#include "StepTicker.h"
#include "Adc.h"
#include "Adc3.h"
#include "GCodeProcessor.h"
#include "Consoles.h"
#include "BaseSolution.h"
#include "Uart.h"

#include "FreeRTOS.h"
#include "task.h"
#include "ff.h"
#include "benchmark_timer.h"
#include "semphr.h"

#include <functional>
#include <set>
#include <cmath>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <malloc.h>

#define HELP(m) if(params == "-h") { os.printf("%s\n", m); return true; }

CommandShell* CommandShell::instance = nullptr;

CommandShell::CommandShell()
{
    mounted = false;
    initialize();
}

bool CommandShell::initialize()
{
    // register command handlers
    using std::placeholders::_1;
    using std::placeholders::_2;

    THEDISPATCHER->add_handler( "help", std::bind( &CommandShell::help_cmd, this, _1, _2) );

    //THEDISPATCHER->add_handler( "mount", std::bind( &CommandShell::mount_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "ls", std::bind( &CommandShell::ls_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "rm", std::bind( &CommandShell::rm_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "mv", std::bind( &CommandShell::mv_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "cp", std::bind( &CommandShell::cp_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "cd", std::bind( &CommandShell::cd_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "mkdir", std::bind( &CommandShell::mkdir_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "cat", std::bind( &CommandShell::cat_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "md5sum", std::bind( &CommandShell::md5sum_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "load", std::bind( &CommandShell::load_cmd, this, _1, _2) );

    THEDISPATCHER->add_handler( "config-set", std::bind( &CommandShell::config_set_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "config-get", std::bind( &CommandShell::config_get_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "ry", std::bind( &CommandShell::ry_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "dl", std::bind( &CommandShell::download_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "truncate", std::bind( &CommandShell::truncate_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "date", std::bind( &CommandShell::date_cmd, this, _1, _2) );

    THEDISPATCHER->add_handler( "mem", std::bind( &CommandShell::mem_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "switch", std::bind( &CommandShell::switch_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "gpio", std::bind( &CommandShell::gpio_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "modules", std::bind( &CommandShell::modules_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "get", std::bind( &CommandShell::get_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "$#", std::bind( &CommandShell::grblDP_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "$G", std::bind( &CommandShell::grblDG_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "$H", std::bind( &CommandShell::grblDH_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "$I", std::bind( &CommandShell::grblDG_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "$J", std::bind( &CommandShell::jog_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "$P", std::bind( &CommandShell::probe_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "$S", std::bind( &CommandShell::switch_poll_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "test", std::bind( &CommandShell::test_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "version", std::bind( &CommandShell::version_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "break", std::bind( &CommandShell::break_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "reset", std::bind( &CommandShell::reset_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "ed", std::bind( &CommandShell::edit_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "dfu", std::bind( &CommandShell::dfu_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "qspi", std::bind( &CommandShell::qspi_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "flash", std::bind( &CommandShell::flash_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "msc", std::bind( &CommandShell::msc_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "echo", std::bind( &CommandShell::echo_cmd, this, _1, _2) );

    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 20, std::bind(&CommandShell::m20_cmd, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 115, std::bind(&CommandShell::m115_cmd, this, _1, _2));

    return true;
}

// lists all the registered commands
bool CommandShell::help_cmd(std::string& params, OutputStream& os)
{
    HELP("Show available commands");
    auto cmds = THEDISPATCHER->get_commands();
    for(auto& i : cmds) {
        os.printf("%s\n", i.c_str());
        // Display the help string for each command
        //    std::string cmd(i);
        //    cmd.append(" -h");
        //    THEDISPATCHER->dispatch(cmd.c_str(), os);
    }
    os.puts("\nuse cmd -h to get help on that command\n");

    return true;
}


bool CommandShell::m20_cmd(GCode& gcode, OutputStream& os)
{
    if(THEDISPATCHER->is_grbl_mode()) return false;

    os.printf("Begin file list\n");
    std::string params("-1 /sd");
    ls_cmd(params, os);
    os.printf("End file list\n");
    return true;
}

bool CommandShell::ls_cmd(std::string& params, OutputStream& os)
{
    HELP("list files: dir [-1] [folder]");
    std::string path;
    std::string opts;
    while(!params.empty()) {
        std::string s = stringutils::shift_parameter( params );
        if(s.front() == '-') {
            opts.append(s);
        } else {
            path = s;
            if(!params.empty()) {
                path.append(" ");
                path.append(params);
            }
            break;
        }
    }

#if 0
    DIR *d;
    struct dirent *p;
    d = opendir(path.c_str());
    if (d != NULL) {
        while ((p = readdir(d)) != NULL) {
            if(os.get_stop_request()) { os.set_stop_request(false); break; }
            os.printf("%s", p->d_name);
            struct stat buf;
            std::string sp = path + "/" + p->d_name;
            if (stat(sp.c_str(), &buf) >= 0) {
                if (S_ISDIR(buf.st_mode)) {
                    os.printf("/");

                } else if(opts.find("-s", 0, 2) != std::string::npos) {
                    os.printf(" %d", buf.st_size);
                }
            } else {
                os.printf(" - Could not stat: %s", sp.c_str());
            }
            os.printf("\n");
        }
        closedir(d);
    } else {
        os.printf("Could not open directory %s\n", path.c_str());
    }
#else
    // newlib does not support dirent so use ff lib directly
    DIR dir;
    FILINFO finfo;
    FATFS *fs;
    FRESULT res = f_opendir(&dir, path.c_str());
    if(FR_OK != res) {
        os.printf("Could not open directory %s (%d)\n", path.c_str(), res);
        return true;
    }

    DWORD p1, s1, s2;
    p1 = s1 = s2 = 0;
    bool simple = false;
    if(opts.find("-1", 0, 2) != std::string::npos) {
        simple = true;
    }

    for(;;) {
        if(os.get_stop_request()) {
            os.set_stop_request(false);
            f_closedir(&dir);
            os.printf("aborted\n");
            return true;
        }
        res = f_readdir(&dir, &finfo);
        if ((res != FR_OK) || !finfo.fname[0]) break;
        if(simple) {
            if(finfo.fattrib & AM_DIR) {
                os.printf("%s/\n", finfo.fname);
            } else {
                os.printf("%s\n", finfo.fname);
            }
        } else {
            if (finfo.fattrib & AM_DIR) {
                s2++;
            } else {
                s1++; p1 += finfo.fsize;
            }
            os.printf("%c%c%c%c%c %u/%02u/%02u %02u:%02u %9lu  %s\n",
                      (finfo.fattrib & AM_DIR) ? 'D' : '-',
                      (finfo.fattrib & AM_RDO) ? 'R' : '-',
                      (finfo.fattrib & AM_HID) ? 'H' : '-',
                      (finfo.fattrib & AM_SYS) ? 'S' : '-',
                      (finfo.fattrib & AM_ARC) ? 'A' : '-',
                      (finfo.fdate >> 9) + 1980, (finfo.fdate >> 5) & 15, finfo.fdate & 31,
                      (finfo.ftime >> 11), (finfo.ftime >> 5) & 63,
                      (DWORD)finfo.fsize, finfo.fname);
        }
    }
    if(!simple) {
        os.printf("%4lu File(s),%10lu bytes total\n%4lu Dir(s)", s1, p1, s2);
        res = f_getfree("/sd", (DWORD*)&p1, &fs);
        if(FR_OK == res) {
            os.printf(", %10lu bytes free\n", p1 * fs->csize * 512);
        } else {
            os.printf("\n");
        }
        os.set_no_response();
    }
    f_closedir(&dir);
#endif
    return true;
}

bool CommandShell::rm_cmd(std::string& params, OutputStream& os)
{
    HELP("delete: file(s) or directory. quote names with spaces");
    std::string fn = stringutils::shift_parameter( params );
    while(!fn.empty()) {
        int s = remove(fn.c_str());
        if (s != 0) {
            os.printf("Could not delete %s\n", fn.c_str());
        } else {
            os.printf("deleted %s\n", fn.c_str());
        }
        fn = stringutils::shift_parameter( params );
    }
    os.set_no_response();
    return true;
}

bool CommandShell::load_cmd(std::string& params, OutputStream& os)
{
    HELP("load named config override file");
    std::string fn = stringutils::shift_parameter( params );
    if(!fn.empty()) {
        if(!load_config_override(os, fn.c_str())) {
            os.printf("failed to load config override file %s\n", fn.c_str());
        } else {
            os.printf("loaded config override file %s\n", fn.c_str());
        }
    } else {
        os.printf("filename required\n");
    }

    return true;
}

bool CommandShell::cd_cmd(std::string& params, OutputStream& os)
{
    HELP("change directory");
    std::string fn = stringutils::shift_parameter( params );
    if(fn.empty()) {
        fn = "/";
    }
    if(FR_OK != f_chdir(fn.c_str())) {
        os.puts("failed to change to directory\n");
    }
    return true;
}

bool CommandShell::mkdir_cmd(std::string& params, OutputStream& os)
{
    HELP("make directory");
    std::string fn = stringutils::shift_parameter( params );
    if(fn.empty()) {
        os.puts("directory name required\n");
        return true;
    }
    if(FR_OK != f_mkdir(fn.c_str())) {
        os.puts("failed to make directory\n");
    }
    return true;
}

bool CommandShell::mv_cmd(std::string& params, OutputStream& os)
{
    HELP("rename: from to");
    std::string fn1 = stringutils::shift_parameter( params );
    std::string fn2 = stringutils::shift_parameter( params );
    if(fn1.empty() || fn2.empty()) {
        os.puts("from and to files required\n");
        return true;
    }
    int s = rename(fn1.c_str(), fn2.c_str());
    if (s != 0) os.printf("Could not rename %s to %s\n", fn1.c_str(), fn2.c_str());
    return true;
}

bool CommandShell::cp_cmd(std::string& params, OutputStream& os)
{
    HELP("copy a file: from to");
    std::string fn1 = stringutils::shift_parameter( params );
    std::string fn2 = stringutils::shift_parameter( params );

    if(fn1.empty() || fn2.empty()) {
        os.puts("from and to files required\n");
        return true;
    }

    std::fstream fsin;
    std::fstream fsout;

    fsin.rdbuf()->pubsetbuf(0, 0); // set unbuffered
    fsout.rdbuf()->pubsetbuf(0, 0); // set unbuffered
    fsin.open(fn1, std::fstream::in | std::fstream::binary);
    if(!fsin.is_open()) {
        os.printf("File %s does not exist\n", fn1.c_str());
        return true;
    }

    fsout.open(fn2, std::fstream::out | std::fstream::binary | std::fstream::trunc);
    if(!fsout.is_open()) {
        os.printf("Could not open File %s for write\n", fn2.c_str());
        return true;
    }

    // allocate from heap rather than the limited stack
    char *buffer = (char *)malloc(4096);
    if(buffer != nullptr) {
        /* Copy source to destination */
        while (!fsin.eof()) {
            fsin.read(buffer, sizeof(buffer));
            int br = fsin.gcount();
            if(br > 0) {
                fsout.write(buffer, br);
                if(!fsout.good()) {
                    os.printf("Write failed to File %s\n", fn2.c_str());
                    break;
                }
            }
            if(os.get_stop_request()) {
                os.printf("copy aborted\n");
                os.set_stop_request(false);
                break;
            }
        }
        free(buffer);

    } else {
        os.printf("Not enough memory for operation\n");
    }

    /* Close open files */
    fsin.close();
    fsout.close();

    return true;
}

static void printTaskList(OutputStream& os)
{
    TaskStatus_t *pxTaskStatusArray;
    char cStatus;

    //vTaskSuspendAll();
    UBaseType_t uxArraySize = uxTaskGetNumberOfTasks();

    /* Allocate an array index for each task. */
    pxTaskStatusArray = (TaskStatus_t *)malloc( uxArraySize * sizeof( TaskStatus_t ) );

    if( pxTaskStatusArray != NULL ) {
        /* Generate the (binary) data. */
        uxArraySize = uxTaskGetSystemState( pxTaskStatusArray, uxArraySize, NULL );

        /* Create a human readable table from the binary data. */
        for(UBaseType_t x = 0; x < uxArraySize; x++ ) {
            switch( pxTaskStatusArray[ x ].eCurrentState ) {
                case eRunning:      cStatus = 'X'; break;
                case eReady:        cStatus = 'R'; break;
                case eBlocked:      cStatus = 'B'; break;
                case eSuspended:    cStatus = 'S'; break;
                case eDeleted:      cStatus = 'D'; break;
                default:            cStatus = '?'; break;
            }

            /* Write the task name */
            os.printf("%12s ", pxTaskStatusArray[x].pcTaskName);

            /* Write the rest of the string. */
            os.printf(" %c %2u %6u %2u\n", cStatus, ( unsigned int ) pxTaskStatusArray[ x ].uxCurrentPriority, ( unsigned int ) pxTaskStatusArray[ x ].usStackHighWaterMark, ( unsigned int ) pxTaskStatusArray[ x ].xTaskNumber );
        }

        free( pxTaskStatusArray );
    }
    //xTaskResumeAll();
}

#include "MemoryPool.h"
bool CommandShell::mem_cmd(std::string& params, OutputStream& os)
{
    HELP("show memory allocation and threads");

    printTaskList(os);
    // os->puts("\n\n");
    // vTaskGetRunTimeStats(pcWriteBuffer);
    // os->puts(pcWriteBuffer);

    struct mallinfo mem = mallinfo();
    os.printf("\n\nfree sbrk memory= %d, Total free= %d\n", xPortGetFreeHeapSize() - mem.fordblks, xPortGetFreeHeapSize());
    os.printf("malloc:      total       used       free    largest\n");
    os.printf("Mem:   %11d%11d%11d%11d\n", mem.arena, mem.uordblks, mem.fordblks, mem.ordblks);

    os.printf("DTCMRAM: %lu used, %lu bytes free\n", _DTCMRAM->get_size() - _DTCMRAM->available(), _DTCMRAM->available());
    if(!params.empty()) {
        os.printf("-- DTCMRAM --\n"); _DTCMRAM->debug(os);
    }
    os.printf("SRAM_1: %lu used, %lu bytes free\n", _SRAM_1->get_size() - _SRAM_1->available(), _SRAM_1->available());
    if(!params.empty()) {
        os.printf("-- SRAM_1 --\n"); _SRAM_1->debug(os);
    }

    os.set_no_response();
    return true;
}

#if 0
bool CommandShell::mount_cmd(std::string& params, OutputStream& os)
{
    HELP("mount sdcard on /sd (or unmount if already mounted)");

    const char g_target[] = "sd";
    if(mounted) {
        os.printf("Already mounted, unmounting\n");
        int ret = f_unmount("g_target");
        if(ret != FR_OK) {
            os.printf("Error unmounting: %d", ret);
            return true;
        }
        mounted = false;
        return true;
    }

    int ret = f_mount(f_mount(&fatfs, g_target, 1);
    if(FR_OK == ret) {
    mounted = true;
    os.printf("Mounted /%s\n", g_target);

    } else {
        os.printf("Failed to mount sdcard: %d\n", ret);
    }

    return true;
}
#endif

bool CommandShell::cat_cmd(std::string& params, OutputStream& os)
{
    HELP("display file: nnn option will show first nnn lines, -d will delay output and send ^D at end to terminate");
    // Get params ( filename and line limit )
    std::string filename          = stringutils::shift_parameter( params );
    std::string limit_parameter   = stringutils::shift_parameter( params );

    if(filename.empty()) {
        os.puts("file name required\n");
        return true;
    }

    bool delay = false;
    int limit = -1;

    if(!limit_parameter.empty() && limit_parameter.substr(0, 2) == "-d") {
        delay = true;
        limit_parameter = stringutils::shift_parameter( params );
    }

    if (!limit_parameter.empty()) {
        char *e = NULL;
        limit = strtol(limit_parameter.c_str(), &e, 10);
        if (e <= limit_parameter.c_str())
            limit = -1;
    }

    // Open file
    FILE *lp = fopen(filename.c_str(), "r");
    if (lp != NULL) {
        if(delay) {
            os.puts("you have 5 seconds to initiate the upload command on the host...\n");
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
        char buffer[132];
        int newlines = 0;
        // Print each line of the file
        while (fgets (buffer, sizeof(buffer) - 1, lp) != nullptr) {
            if(os.get_stop_request()) {
                os.printf("cat aborted\n");
                os.set_stop_request(false);
                delay = false;
                break;
            }

            os.puts(buffer);
            if ( limit > 0 && ++newlines >= limit ) {
                break;
            }
        };
        fclose(lp);
        if(delay) {
            char c = 26;
            os.write(&c, 1);
        }

    } else {
        os.printf("File not found: %s\n", filename.c_str());
    }

    os.set_no_response();
    return true;
}

#include "md5.h"
bool CommandShell::md5sum_cmd(std::string& params, OutputStream& os)
{
    HELP("calculate the md5sum of given filename(s)");
    std::string filename = stringutils::shift_parameter( params );

    while(!filename.empty()) {
        // Open file
        FILE *lp = fopen(filename.c_str(), "r");
        if (lp != NULL) {
            MD5 md5;
            uint8_t buf[256];
            do {
                size_t n = fread(buf, 1, sizeof buf, lp);
                if(n > 0) md5.update(buf, n);
            } while(!feof(lp));

            os.printf("%s %s\n", md5.finalize().hexdigest().c_str(), filename.c_str());
            fclose(lp);

        } else {
            os.printf("File not found: %s\n", filename.c_str());
        }
        filename = stringutils::shift_parameter( params );
    }

    os.set_no_response();
    return true;
}

#include "Switch.h"
// set or get switch state for a named switch
bool CommandShell::switch_cmd(std::string& params, OutputStream& os)
{
    HELP("list switches or get/set named switch. if 2nd parameter is on/off it sets state if it is numeric it sets value");

    std::string name = stringutils::shift_parameter( params );
    std::string value = stringutils::shift_parameter( params );

    if(name.empty()) {
        // just list all the switches
        std::vector<Module*> mv = Module::lookup_group("switch");
        if(mv.size() > 0) {
            for(auto m : mv) {
                Switch *s = static_cast<Switch*>(m);
                os.printf("%s:\n", m->get_instance_name());
                os.printf(" %s\n", s->get_info().c_str());
            }
        } else {
            os.printf("No switches found\n");
        }

        return true;
    }

    Module *m = Module::lookup("switch", name.c_str());
    if(m == nullptr) {
        os.printf("no such switch: %s\n", name.c_str());
        return true;
    }

    bool ok = false;
    if(value.empty()) {
        // get switch state
        bool state;
        ok = m->request("state", &state);
        if (!ok) {
            os.printf("unknown command %s.\n", "state");
            return true;
        }
        os.printf("switch %s is %d\n", name.c_str(), state);

    } else {
        const char *cmd;
        // set switch state
        if(value == "on" || value == "off") {
            bool b = value == "on";
            cmd = "set-state";
            ok =  m->request(cmd, &b);

        } else {
            float v = strtof(value.c_str(), NULL);
            cmd = "set-value";
            ok = m->request(cmd, &v);
        }

        if (ok) {
            os.printf("switch %s set to: %s\n", name.c_str(), value.c_str());
        } else {
            os.printf("unknown command %s.\n", cmd);
        }
    }

    return true;
}

bool CommandShell::switch_poll_cmd(std::string& params, OutputStream& os)
{
    HELP("returns switch poll query")
    std::string name = stringutils::shift_parameter( params );

    while(!name.empty()) {
        Module *m = Module::lookup("switch", name.c_str());
        if(m != nullptr) {
            // get switch state
            bool state;
            bool ok = m->request("state", &state);
            if (ok) {
                os.printf("switch %s is %d\n", name.c_str(), state);
            }
        }
        name = stringutils::shift_parameter( params );
    }

    os.set_no_response();

    return true;
}

static bool get_spindle_state()
{
    // get spindle switch state
    Module *m = Module::lookup("switch", "spindle");
    if(m != nullptr) {
        // get switch state
        bool state;
        if(m->request("state", &state)) {
            return state;
        }
    }
    return false;
}

// set or get gpio
bool CommandShell::gpio_cmd(std::string& params, OutputStream& os)
{
    HELP("set and get gpio pins: use PA1 o[ut]/i[n] [on/off | 1/0] [timeout]");

    std::string gpio = stringutils::shift_parameter( params );
    std::string dir = stringutils::shift_parameter( params );

    if(gpio.empty()) {
        os.printf("incorrect usage\n");
        return true;
    }

    char port = 0;
    uint16_t pin_no = 0;
    size_t pos;
    if(!Pin::parse_pin(gpio, port, pin_no, pos)) {
        os.printf("Illegal pin name: %s\n", gpio.c_str());
        return true;
    }

    // FIXME need to handle allocated pins, but that would require a list of all allocated pins
    // Maybe if we add a -f flag we could deinit the pin first but that would leave the pin deallocated
    if(Pin::is_allocated(port, pin_no)) {
        os.printf("Pin is already allocated: %s\n", gpio.c_str());
        return true;
    }

    if(dir.empty() || dir == "in" || dir == "i") {
        // read pin
        Pin pin(gpio.c_str(), Pin::AS_INPUT);
        if(!pin.connected()) {
            os.printf("Not a valid GPIO\n");
            return true;
        }

        os.printf("%s\n", pin.to_string().c_str());
        pin.deinit();
        return true;
    }

    if(dir == "out" || dir == "o") {
        std::string v = stringutils::shift_parameter( params );
        if(v.empty()) {
            os.printf("on|off required\n");
            return true;
        }
        Pin pin(gpio.c_str(), Pin::AS_OUTPUT);
        if(!pin.connected()) {
            os.printf("Not a valid GPIO\n");
            return true;
        }
        bool b = (v == "on" || v == "1");
        pin.set(b);
        os.printf("%s: was set to %s\n", pin.to_string().c_str(), v.c_str());
        v = stringutils::shift_parameter( params ); // get timeout
        if(v.empty()) {
            safe_sleep(2000);
        } else {
            safe_sleep(atoi(v.c_str()));
        }
        pin.deinit();
        return true;
    }

    os.printf("incorrect usage\n");
    return true;
}

bool CommandShell::modules_cmd(std::string& params, OutputStream& os)
{
    HELP("List all registered modules\n");

    os.set_no_response();

    std::vector<std::string> l = Module::print_modules();

    if(l.empty()) {
        os.printf("No modules found\n");
        return true;
    }

    for(auto& i : l) {
        os.printf("%s\n", i.c_str());
    }

    return true;
}

bool CommandShell::get_cmd(std::string& params, OutputStream& os)
{
    HELP("get pos|wcs|state|status|temp|volts|fk|ik")
    std::string what = stringutils::shift_parameter( params );
    bool handled = true;
    if (what == "temp") {
        std::string type = stringutils::shift_parameter( params );
        if(type.empty()) {
            // scan all temperature controls
            std::vector<Module*> controllers = Module::lookup_group("temperature control");
            for(auto m : controllers) {
                TemperatureControl::pad_temperature_t temp;
                if(m->request("get_current_temperature", &temp)) {
                    os.printf("%s: %s (%d) temp: %f/%f @%d\n", m->get_instance_name(), temp.designator.c_str(), temp.tool_id, temp.current_temperature, temp.target_temperature, temp.pwm);
                } else {
                    os.printf("temp request failed\n");
                }
            }
            // get chip temperature
            Adc3 *adc = Adc3::getInstance();
            float t = adc->read_temp();
            os.printf("chip temp: %f\n", t);

        } else if(type == "chip") {
            // get chip temperature
            Adc3 *adc = Adc3::getInstance();
            float t = adc->read_temp();
            os.printf("chip temp: %f\n", t);

        } else {
            Module *m = Module::lookup("temperature control", type.c_str());
            if(m == nullptr) {
                os.printf("%s is not a known temperature control", type.c_str());

            } else {
                TemperatureControl::pad_temperature_t temp;
                m->request("get_current_temperature", &temp);
                os.printf("%s temp: %f/%f @%d\n", type.c_str(), temp.current_temperature, temp.target_temperature, temp.pwm);
            }
        }

    } else if (what == "fk" || what == "ik") {
        std::string p = stringutils::shift_parameter( params );
        bool move = false;
        if(p == "-m") {
            move = true;
            p = stringutils::shift_parameter( params );
        }

        std::vector<float> v = stringutils::parse_number_list(p.c_str());
        if(p.empty() || v.size() < 1) {
            os.printf("error:usage: get [fk|ik] [-m] x[,y,z]\n");
            return true;
        }

        float x = v[0];
        float y = (v.size() > 1) ? v[1] : x;
        float z = (v.size() > 2) ? v[2] : y;

        if(what == "fk") {
            // do forward kinematics on the given actuator position and display the cartesian coordinates
            ActuatorCoordinates apos{x, y, z};
            float pos[3];
            Robot::getInstance()->arm_solution->actuator_to_cartesian(apos, pos);
            os.printf("cartesian= X %f, Y %f, Z %f\n", pos[0], pos[1], pos[2]);
            x = pos[0];
            y = pos[1];
            z = pos[2];

        } else {
            // do inverse kinematics on the given cartesian position and display the actuator coordinates
            float pos[3] {x, y, z};
            ActuatorCoordinates apos;
            Robot::getInstance()->arm_solution->cartesian_to_actuator(pos, apos);
            os.printf("actuator= X %f, Y %f, Z %f\n", apos[0], apos[1], apos[2]);
        }

        if(move) {
            // move to the calculated, or given, XYZ
            Robot::getInstance()->push_state();
            Robot::getInstance()->absolute_mode = true;
            Robot::getInstance()->next_command_is_MCS = true; // must use machine coordinates
            THEDISPATCHER->dispatch(os, 'G', 0, 'X', x, 'Y', y, 'Z', z, 0);
            Conveyor::getInstance()->wait_for_idle();
            Robot::getInstance()->pop_state();
        }

    } else if (what == "pos") {
        // convenience to call all the various M114 variants, shows ABC axis where relevant
        std::string buf;
        Robot::getInstance()->print_position(0, buf); os.printf("last %s\n", buf.c_str()); buf.clear();
        Robot::getInstance()->print_position(1, buf); os.printf("realtime %s\n", buf.c_str()); buf.clear();
        Robot::getInstance()->print_position(2, buf); os.printf("%s\n", buf.c_str()); buf.clear();
        Robot::getInstance()->print_position(3, buf); os.printf("%s\n", buf.c_str()); buf.clear();
        Robot::getInstance()->print_position(4, buf); os.printf("%s\n", buf.c_str()); buf.clear();
        Robot::getInstance()->print_position(5, buf); os.printf("%s\n", buf.c_str()); buf.clear();
        os.printf("RAW:");
        for (int i = 0; i < Robot::getInstance()->get_number_registered_motors(); ++i) {
            os.printf(" %11d", Robot::getInstance()->actuators[i]->get_step_count());
        }
        os.printf("\n");

    } else if (what == "wcs") {
        // print the wcs state
        std::string cmd("-v");
        grblDP_cmd(cmd, os);

    } else if (what == "state") {
        // so we can see it in smoopi we strip off [...]
        std::string str;
        std::ostringstream oss;
        OutputStream tos(&oss);
        grblDG_cmd(str, tos);
        str = oss.str();
        os.printf("%s\n", str.substr(1, str.size() - 3).c_str());

    } else if (what == "status") {
        // also ? on serial and usb
        std::string str;
        Robot::getInstance()->get_query_string(str);
        // so we can see this in smoopi we strip off the <...>
        os.printf("%s\n", str.substr(1, str.size() - 3).c_str());

    } else if (what == "volts") {
        std::string type = stringutils::shift_parameter( params );
        if(type.empty()) {
            int n = get_voltage_monitor_names(nullptr);
            if(n > 0) {
                const char *names[n];
                get_voltage_monitor_names(names);
                for (int i = 0; i < n; ++i) {
                    float v = get_voltage_monitor(names[i]);
                    if(!isinf(v)) {
                        os.printf("%s: %f v\n", names[i], v);
                    } else {
                        os.printf("%s: INF\n", names[i]);
                    }
                }
            } else {
                os.printf("No voltage monitors configured\n");
            }
        } else {
            float v = get_voltage_monitor(type.c_str());
            if(isinf(v)) {
                os.printf("%s: INF\n", type.c_str());
            } else {
                os.printf("%s: %f v\n", type.c_str(), v);
            }
        }

    } else {

        handled = false;
    }

    return handled;
}

bool CommandShell::grblDG_cmd(std::string& params, OutputStream& os)
{
    // also $G (sends ok) and $I (no ok) and get state (no [...])
    // [GC:G0 G54 G17 G21 G90 G94 M0 M5 M9 T0 F0.]
    os.printf("[GC:G%d %s G%d G%d G%d G94 M0 M%c M9 T%d F%1.4f S%1.4f]\n",
              GCodeProcessor::get_group1_modal_code(),
              stringutils::wcs2gcode(Robot::getInstance()->get_current_wcs()).c_str(),
              Robot::getInstance()->plane_axis_0 == X_AXIS && Robot::getInstance()->plane_axis_1 == Y_AXIS && Robot::getInstance()->plane_axis_2 == Z_AXIS ? 17 :
              Robot::getInstance()->plane_axis_0 == X_AXIS && Robot::getInstance()->plane_axis_1 == Z_AXIS && Robot::getInstance()->plane_axis_2 == Y_AXIS ? 18 :
              Robot::getInstance()->plane_axis_0 == Y_AXIS && Robot::getInstance()->plane_axis_1 == Z_AXIS && Robot::getInstance()->plane_axis_2 == X_AXIS ? 19 : 17,
              Robot::getInstance()->inch_mode ? 20 : 21,
              Robot::getInstance()->absolute_mode ? 90 : 91,
              get_spindle_state() ? '3' : '5',
              0, // TODO get_active_tool(),
              Robot::getInstance()->from_millimeters(Robot::getInstance()->get_feed_rate()),
              Robot::getInstance()->get_s_value());
    return true;
}

bool CommandShell::grblDH_cmd(std::string& params, OutputStream& os)
{
    // Always home regardless of grbl mode setting
    if(THEDISPATCHER->is_grbl_mode()) {
        return THEDISPATCHER->dispatch(os, 'G', 28, 2, 0); // G28.2 to home
    } else {
        return THEDISPATCHER->dispatch(os, 'G', 28, 0); // G28 to home
    }
}

bool CommandShell::probe_cmd(std::string& params, OutputStream& os)
{
    std::string cmd = "G30 P1 "; // forces G30 to be simple probe regardless of grbl mode
    // append any parameters that G30 takes
    std::string p = stringutils::shift_parameter(params);
    while(!p.empty()) {
        cmd.append(p).append(" ");
        p = stringutils::shift_parameter(params);
    }

    return dispatch_line(os, cmd.c_str());
}

bool CommandShell::grblDP_cmd(std::string& params, OutputStream& os)
{
    /*
    [G54:95.000,40.000,-23.600]
    [G55:0.000,0.000,0.000]
    [G56:0.000,0.000,0.000]
    [G57:0.000,0.000,0.000]
    [G58:0.000,0.000,0.000]
    [G59:0.000,0.000,0.000]
    [G28:0.000,0.000,0.000]
    [G30:0.000,0.000,0.000]
    [G92:0.000,0.000,0.000]
    [TLO:0.000]
    [PRB:0.000,0.000,0.000:0]
    */

    HELP("show grbl $ command")

    bool verbose = stringutils::shift_parameter( params ).find_first_of("Vv") != std::string::npos;

    std::vector<Robot::wcs_t> v = Robot::getInstance()->get_wcs_state();
    if(verbose) {
        char current_wcs = std::get<0>(v[0]);
        os.printf("[current WCS: %s]\n", stringutils::wcs2gcode(current_wcs).c_str());
    }

    int n = std::get<1>(v[0]);
    for (int i = 1; i <= n; ++i) {
        os.printf("[%s:%1.4f,%1.4f,%1.4f]\n", stringutils::wcs2gcode(i - 1).c_str(),
                  Robot::getInstance()->from_millimeters(std::get<0>(v[i])),
                  Robot::getInstance()->from_millimeters(std::get<1>(v[i])),
                  Robot::getInstance()->from_millimeters(std::get<2>(v[i])));
    }

    float rd[] {0, 0, 0};
    //PublicData::get_value( endstops_checksum, saved_position_checksum, &rd ); TODO use request
    os.printf("[G28:%1.4f,%1.4f,%1.4f]\n",
              Robot::getInstance()->from_millimeters(rd[0]),
              Robot::getInstance()->from_millimeters(rd[1]),
              Robot::getInstance()->from_millimeters(rd[2]));

    os.printf("[G30:%1.4f,%1.4f,%1.4f]\n",  0.0F, 0.0F, 0.0F); // not implemented

    os.printf("[G92:%1.4f,%1.4f,%1.4f]\n",
              Robot::getInstance()->from_millimeters(std::get<0>(v[n + 1])),
              Robot::getInstance()->from_millimeters(std::get<1>(v[n + 1])),
              Robot::getInstance()->from_millimeters(std::get<2>(v[n + 1])));

    if(verbose) {
        os.printf("[Tool Offset:%1.4f,%1.4f,%1.4f]\n",
                  Robot::getInstance()->from_millimeters(std::get<0>(v[n + 2])),
                  Robot::getInstance()->from_millimeters(std::get<1>(v[n + 2])),
                  Robot::getInstance()->from_millimeters(std::get<2>(v[n + 2])));
    } else {
        os.printf("[TLO:%1.4f]\n", Robot::getInstance()->from_millimeters(std::get<2>(v[n + 2])));
    }

    // this is the last probe position, updated when a probe completes, also stores the number of steps moved after a homing cycle
    float px, py, pz;
    uint8_t ps;
    std::tie(px, py, pz, ps) = Robot::getInstance()->get_last_probe_position();
    os.printf("[PRB:%1.4f,%1.4f,%1.4f:%d]\n", Robot::getInstance()->from_millimeters(px), Robot::getInstance()->from_millimeters(py), Robot::getInstance()->from_millimeters(pz), ps);

    return true;
}

// runs several types of test on the mechanisms
// TODO this will block the command thread, and queries will stop,
// may want to run the long running commands in a thread
bool CommandShell::test_cmd(std::string& params, OutputStream& os)
{
    HELP("test [jog|circle|square|raw|pulse]");

    if(Module::is_halted()) {
        os.set_no_response(true);
        os.printf("error:Alarm lock\n");
        return true;
    }

    AutoPushPop app; // this will save the state and restore it on exit
    std::string what = stringutils::shift_parameter( params );
    OutputStream nullos;
    bool disas = false;
    if (what == "jog") {
        // jogs back and forth usage: axis [-d] distance iterations [feedrate], -d means disable arm solution
        std::string axis = stringutils::shift_parameter(params);
        if(axis == "-d") {
            disas = true;
            axis = stringutils::shift_parameter(params);
        }
        std::string dist = stringutils::shift_parameter(params);
        std::string iters = stringutils::shift_parameter(params);
        std::string speed = stringutils::shift_parameter(params);
        if(axis.empty() || dist.empty() || iters.empty()) {
            os.printf("usage: jog [-d] axis distance iterations [feedrate]\n");
            return true;
        }
        float d = strtof(dist.c_str(), NULL);
        float f = speed.empty() ? Robot::getInstance()->get_feed_rate(1) : strtof(speed.c_str(), NULL);
        uint32_t n = strtol(iters.c_str(), NULL, 10);

        bool toggle = false;
        Robot::getInstance()->absolute_mode = false;
        if(disas) Robot::getInstance()->disable_arm_solution = true;
        for (uint32_t i = 0; i < n; ++i) {
            THEDISPATCHER->dispatch(nullos, 'G', 1, 'F', f, toupper(axis[0]), toggle ? -d : d, 0);
            if(Module::is_halted()) break;
            toggle = !toggle;
        }
        if(disas) Robot::getInstance()->disable_arm_solution = false;

    } else if (what == "circle") {
        // draws a circle around origin. usage: radius iterations [feedrate]
        std::string radius = stringutils::shift_parameter( params );
        std::string iters = stringutils::shift_parameter( params );
        std::string speed = stringutils::shift_parameter( params );
        if(radius.empty() || iters.empty()) {
            os.printf("usage: circle radius iterations [feedrate]\n");
            return true;
        }

        float r = strtof(radius.c_str(), NULL);
        uint32_t n = strtol(iters.c_str(), NULL, 10);
        float f = speed.empty() ? Robot::getInstance()->get_feed_rate(1) : strtof(speed.c_str(), NULL);

        Robot::getInstance()->absolute_mode = false;
        THEDISPATCHER->dispatch(nullos, 'G', 1, 'X', -r, 'F', f, 0);
        Robot::getInstance()->absolute_mode = true;

        for (uint32_t i = 0; i < n; ++i) {
            if(Module::is_halted()) break;
            THEDISPATCHER->dispatch(nullos, 'G', 2, 'I', r, 'J', 0.0F, 'F', f, 0);
        }

        // leave it where it started
        if(!Module::is_halted()) {
            Robot::getInstance()->absolute_mode = false;
            THEDISPATCHER->dispatch(nullos, 'G', 1, 'X', r, 'F', f, 0);
            Robot::getInstance()->absolute_mode = true;
        }

    } else if (what == "square") {
        // draws a square usage: size iterations [feedrate]
        std::string size = stringutils::shift_parameter( params );
        std::string iters = stringutils::shift_parameter( params );
        std::string speed = stringutils::shift_parameter( params );
        if(size.empty() || iters.empty()) {
            os.printf("usage: square size iterations [feedrate]\n");
            return true;
        }
        float d = strtof(size.c_str(), NULL);
        float f = speed.empty() ? Robot::getInstance()->get_feed_rate(1) : strtof(speed.c_str(), NULL);
        uint32_t n = strtol(iters.c_str(), NULL, 10);

        Robot::getInstance()->absolute_mode = false;

        for (uint32_t i = 0; i < n; ++i) {
            THEDISPATCHER->dispatch(nullos, 'G', 1, 'X', d, 'F', f, 0);
            THEDISPATCHER->dispatch(nullos, 'G', 1, 'Y', d, 0);
            THEDISPATCHER->dispatch(nullos, 'G', 1, 'X', -d, 0);
            THEDISPATCHER->dispatch(nullos, 'G', 1, 'Y', -d, 0);
            if(Module::is_halted()) break;
        }

    } else if (what == "raw" || what == "acc") {

        // issues raw steps to the specified axis usage: axis steps steps/sec
        std::string axis = stringutils::shift_parameter( params );
        std::string stepstr = stringutils::shift_parameter( params );
        std::string stepspersec = stringutils::shift_parameter( params );
        if(axis.empty() || stepstr.empty() || stepspersec.empty()) {
            os.printf("usage: raw axis steps steps/sec\n");
            return true;
        }

        char ax = toupper(axis[0]);
        uint8_t a = ax >= 'X' ? ax - 'X' : ax - 'A' + 3;
        int steps = strtol(stepstr.c_str(), NULL, 10);
        bool dir = steps < 0;
        steps = std::abs(steps);

        if(a > C_AXIS) {
            os.printf("error: axis must be x, y, z, a, b, c\n");
            return true;
        }

        if(a >= Robot::getInstance()->get_number_registered_motors()) {
            os.printf("error: axis is out of range\n");
            return true;
        }

        uint32_t sps = strtol(stepspersec.c_str(), NULL, 10);

        if(what == "acc") {
            // convert actuator units to steps
            steps = lroundf(Robot::getInstance()->actuators[a]->get_steps_per_mm() * steps);
            // convert steps per unit to steps/sec
            sps = lroundf(Robot::getInstance()->actuators[a]->get_steps_per_mm() * sps);
        }

        sps = std::max(sps, (uint32_t)1);

        os.printf("issuing %d steps at a rate of %d steps/sec on the %c axis\n", steps, sps, ax);

        // make sure motors are enabled, as manual step does not check
        if(!Robot::getInstance()->actuators[a]->is_enabled()) Robot::getInstance()->actuators[a]->enable(true);

        uint32_t delayus = 1000000.0F / sps;
        for(int s = 0; s < steps; s++) {
            if(Module::is_halted()) break;
            Robot::getInstance()->actuators[a]->manual_step(dir);
            // delay
            if(delayus >= 10000) {
                safe_sleep(delayus / 1000);
            } else {
                uint32_t st = benchmark_timer_start();
                while(benchmark_timer_as_us(benchmark_timer_elapsed(st)) < delayus) ;
            }
        }

        // reset the position based on current actuator position
        Robot::getInstance()->reset_position_from_current_actuator_position();

    } else if (what == "pulse") {
        // issues a step pulse then waits then unsteps, for testing if step pin needs to be inverted
        std::string axis = stringutils::shift_parameter( params );
        std::string reps = stringutils::shift_parameter( params );
        if(axis.empty()) {
            os.printf("error: Need axis [iterations]\n");
            return true;
        }

        char ax = toupper(axis[0]);
        uint8_t a = ax >= 'X' ? ax - 'X' : ax - 'A' + 3;

        if(a > C_AXIS) {
            os.printf("error: axis must be x, y, z, a, b, c\n");
            return true;
        }

        if(a >= Robot::getInstance()->get_number_registered_motors()) {
            os.printf("error: axis is out of range\n");
            return true;
        }

        int nreps = 1;
        if(!reps.empty()) {
            nreps = strtol(reps.c_str(), NULL, 10);
        }

        uint32_t delayms = 5000.0F; // step every five seconds
        for(int s = 0; s < nreps; s++) {
            if(Module::is_halted()) break;
            os.printf("// leading edge should step\n");
            Robot::getInstance()->actuators[a]->step();
            safe_sleep(delayms);
            os.printf("// trailing edge should not step\n");
            Robot::getInstance()->actuators[a]->unstep();
            if(Module::is_halted()) break;
            safe_sleep(delayms);
        }

        // reset the position based on current actuator position
        Robot::getInstance()->reset_position_from_current_actuator_position();

    } else {
        os.printf("usage:\n test jog axis distance iterations [feedrate]\n");
        os.printf(" test square size iterations [feedrate]\n");
        os.printf(" test circle radius iterations [feedrate]\n");
        os.printf(" test raw axis steps steps/sec\n");
        os.printf(" test acc axis units units/sec\n");
        os.printf(" test pulse axis iterations\n");
    }

    // wait for the test to complete
    Conveyor::getInstance()->wait_for_idle();

    return true;
}

bool CommandShell::jog_cmd(std::string& params, OutputStream& os)
{
    HELP("instant jog: $J [-c] [-r] X0.01 [Y1] [Z1] [S0.5|F300] - axis can be XYZABCE, optional speed (Snnn) is scale of max_rate. -c turns on continuous jog mode, -r returns ok when done");

    AutoPushPop app;

    int n_motors = Robot::getInstance()->get_number_registered_motors();

    // get axis to move and amount (X0.1)
    // may specify multiple axis
    int is_extruder = -1;
    float rate_mm_s = NAN;
    float scale = 1.0F;
    float fr = NAN;
    float delta[n_motors];
    for (int i = 0; i < n_motors; ++i) {
        delta[i] = NAN;
    }

    if(params.empty()) {
        os.printf("usage: $J [-c] [-r] X0.01 [Y1] [Z1] [S0.5|Fnnn] - axis can be XYZABCE, optional speed is scale of max_rate or specify Feedrate. -c turns on continuous jog mode\n");
        return true;
    }

    os.set_no_response(true);
    bool cont_mode = false;
    while(!params.empty()) {
        std::string p = stringutils::shift_parameter(params);
        if(p.size() == 2 && p[0] == '-') {
            // process option
            switch(toupper(p[1])) {
                case 'C':
                    cont_mode = true;
                    break;
                case 'R': // send ok when done use this when sending $J in a gcode file
                    os.set_no_response(false);
                    break;

                default:
                    os.printf("error:illegal option %c\n", p[1]);
                    return true;
            }
            continue;
        }

        char ax = toupper(p[0]);
        if(ax == 'S') {
            // get speed scale
            scale = strtof(p.substr(1).c_str(), NULL);
            fr = NAN;
            continue;
        } else if(ax == 'F') {
            // OR specify feedrate (last one wins)
            scale = 1.0F;
            fr = strtof(p.substr(1).c_str(), NULL) / 60.0F; // we want mm/sec but F is specified in mm/min
            continue;
        } else if(ax == ';') {
            // skip comments at end of line
            break;
        }

        if(ax == 'E') {
            // find out which is the active extruder
            is_extruder= Robot::getInstance()->get_active_extruder();
            if(is_extruder > 0) ax = 'A' + is_extruder - 3;
        }

        if(!((ax >= 'X' && ax <= 'Z') || (ax >= 'A' && ax <= 'C'))) {
            os.printf("error:bad axis %c\n", ax);
            return true;
        }

        uint8_t a = ax >= 'X' ? ax - 'X' : ax - 'A' + 3;
        if(a >= n_motors) {
            os.printf("error:axis out of range %c\n", ax);
            return true;
        }

        delta[a] = strtof(p.substr(1).c_str(), NULL);
    }

    // select slowest axis rate to use
    int cnt = 0;
    for (int i = 0; i < n_motors; ++i) {
        if(!isnan(delta[i])) {
            ++cnt;
            if(isnan(rate_mm_s)) {
                rate_mm_s = Robot::getInstance()->actuators[i]->get_max_rate();
            } else {
                rate_mm_s = std::min(rate_mm_s, Robot::getInstance()->actuators[i]->get_max_rate());
            }
            //printf("%d %f S%f\n", i, delta[i], rate_mm_s);
        }
    }

    if(cnt == 0) {
        os.printf("error:no delta jog specified\n");
        return true;
    }

    if(Module::is_halted()) {
        // if we are in ALARM state and cont mode we send relevant error response
        if(THEDISPATCHER->is_grbl_mode()) {
            os.printf("error:Alarm lock\n");
        } else {
            os.printf("!!\n");
        }
        return true;
    }

    if(Robot::getInstance()->is_must_be_homed()) {
        Module::broadcast_halt(true);
        os.printf("Error: Must be homed before moving\n");
        return true;
    }

    // set feedrate, either scale of max or actual feedrate
    if(isnan(fr)) {
        fr = rate_mm_s * scale;

    } else {
        // make sure we do not exceed maximum
        if(fr > rate_mm_s) fr = rate_mm_s;
    }

    if(cont_mode) {
        // continuous jog mode, will move until told to stop and sends ok when stopped
        os.set_no_response(false);

        if(os.get_stop_request()) {
            // there is a race condition where the host may send the ^Y so fast after
            // the $J -c that it is executed first, which would leave the system in cont mode
            os.set_stop_request(false);
            return true;
        }

        // get lowest acceleration of selected axis
        float acc = Robot::getInstance()->get_default_acceleration();
        for (int i = 0; i < n_motors; ++i) {
            if(isnan(delta[i])) continue;
            float ma =  Robot::getInstance()->actuators[i]->get_acceleration(); // in mm/sec²
            if(ma > 0.0001F && ma < acc) acc = ma;
        }

        // calculate minimum distance to travel to accomodate acceleration and feedrate
        float d = (fr * fr) / (2*acc); // distance required to fully accelerate to feedrate in mm (d = v*v / 2*a)
        d = std::max(d, 0.3333F); // get minimum distance to move being 1mm overall so 0.3333mm for each segment

        // we need to check if the feedrate is too slow, for continuous jog if it takes over 5 seconds it is too slow
        float t = d * 3 / fr; // time it will take to do all three blocks
        if(t > 5) {
            // increase feedrate so it will not take more than 5 seconds
            fr = (d * 3) / 5;
            if(fr > rate_mm_s) fr = rate_mm_s;
        }

        // we need to move at least this distance to reach full speed
        for (int i = 0; i < n_motors; ++i) {
            if(!isnan(delta[i])) {
                delta[i] = d * (delta[i] < 0 ? -1 : 1);
            }
        }

        if(is_extruder > 0) {
            // convert mm to mm^3
            auto& fnc = Robot::getInstance()->get_e_scale_fnc;
            delta[is_extruder] /= ( fnc ? fnc() : 1.0F);
        }

        //printf("distance: %f, time:%f, X%f Y%f Z%f A%f, speed:%f, acc:%f\n", d, t, delta[0], delta[1], delta[2], delta[3], fr, acc);

        // wait for any activity to stop
        Conveyor::getInstance()->wait_for_idle();
        // turn off any compensation transform so Z does not move as we jog
        auto savect = Robot::getInstance()->compensationTransform;
        Robot::getInstance()->reset_compensated_machine_position();

        // feed three blocks that allow full acceleration, full speed and full deceleration
        // NOTE this only works if it is a primary axis, ABC by default are not primary so not planned when solo so will acc/dec every move
        Conveyor::getInstance()->set_hold(true);

        Robot::getInstance()->delta_move(delta, fr, n_motors); // accelerates upto speed
        Robot::getInstance()->delta_move(delta, fr, n_motors); // continues at full speed
        Robot::getInstance()->delta_move(delta, fr, n_motors); // decelerates to zero

        //Conveyor::getInstance()->dump_queue();

        // tell it to run the first block to acclerate upto speed
        // then continue to send the second block until told to stop
        // then send third block to decelerate
        if(!Conveyor::getInstance()->set_continuous_mode(true)) {
            os.printf("error:unable to set continuous jog mode\n");
            Robot::getInstance()->compensationTransform = savect;
            Conveyor::getInstance()->set_hold(false);
            return true;
        }

        // start it running
        Conveyor::getInstance()->set_hold(false);
        Conveyor::getInstance()->force_queue();

        // run until we get a stop request
        while(!os.get_stop_request()) {
            safe_sleep(10); // services status requests etc
            if(Module::is_halted()) break;
        }

        Conveyor::getInstance()->set_continuous_mode(false);
        os.set_stop_request(false);
        Conveyor::getInstance()->wait_for_idle();

        // reset the position based on current actuator position
        Robot::getInstance()->reset_position_from_current_actuator_position();

        // if it was jogging an extruder set it back to zero
        if(is_extruder > 0) Robot::getInstance()->reset_axis_position(0.0F, is_extruder);

        // restore compensationTransform
        Robot::getInstance()->compensationTransform = savect;

    } else {
        if(is_extruder > 0) {
            // convert mm to mm^3
            auto& fnc = Robot::getInstance()->get_e_scale_fnc;
            delta[is_extruder] /= ( fnc ? fnc() : 1.0F);
        }

        Robot::getInstance()->delta_move(delta, fr, n_motors);
        Conveyor::getInstance()->force_queue();
    }

    return true;
}

std::string get_mcu();
bool CommandShell::version_cmd(std::string& params, OutputStream& os)
{
    HELP("version - print version");

    Version vers;

    os.printf("%s on %s. board_id: %02X\n", get_mcu().c_str(), BUILD_TARGET, board_id);
    os.printf("Build version: %s, Build date: %s, System Clock: %ldMHz\r\n", vers.get_build(), vers.get_build_date(), SystemCoreClock / 1000000);
    os.printf("%d axis, %d primary axis\n", MAX_ROBOT_ACTUATORS, N_PRIMARY_AXIS);

    os.set_no_response();

    return true;
}

bool CommandShell::m115_cmd(GCode& gcode, OutputStream& os)
{
    Version vers;

    os.printf("FIRMWARE_NAME:Smoothieware2, FIRMWARE_URL:http%%3A//smoothieware.org, X-SOURCE_CODE_URL:https://github.com/Smoothieware/SmoothieV2, FIRMWARE_VERSION:%s, PROTOCOL_VERSION:1.0, X-FIRMWARE_BUILD_DATE:%s, X-SYSTEM_CLOCK:%ldMHz, X-AXES:%d, X-PAXIS:%d, X-GRBL_MODE:%d, X-ARCS:1\n", vers.get_build(), vers.get_build_date(), SystemCoreClock / 1000000, MAX_ROBOT_ACTUATORS, N_PRIMARY_AXIS, Dispatcher::getInstance()->is_grbl_mode() ? 1 : 0);

    return true;
}

bool CommandShell::config_get_cmd(std::string& params, OutputStream& os)
{
    HELP("config-get \"section name\"");
    std::string sectionstr = stringutils::shift_parameter( params );
    if(sectionstr.empty()) {
        os.printf("Usage: config-get section\n");
        return true;
    }

    std::fstream fsin;
    fsin.open("/sd/config.ini", std::fstream::in);
    if(!fsin.is_open()) {
        os.printf("Error opening file /sd/config.ini\n");
        return true;
    }

    ConfigReader cr(fsin);
    ConfigReader::section_map_t m;
    bool b = cr.get_section(sectionstr.c_str(), m);
    if(b) {
        for(auto& s : m) {
            std::string k = s.first;
            std::string v = s.second;
            os.printf("%s = %s\n", k.c_str(), v.c_str());
        }
    } else {
        os.printf("No section named %s\n", sectionstr.c_str());
    }

    fsin.close();

    os.set_no_response();

    return true;
}

bool CommandShell::config_set_cmd(std::string& params, OutputStream& os)
{
    HELP("config-set \"section name\" key [=] [value]");
    os.set_no_response();

    std::string sectionstr = stringutils::shift_parameter( params );
    std::string keystr = stringutils::shift_parameter( params );
    std::string valuestr = stringutils::shift_parameter( params );
    if(valuestr == "=") {
        // ignore optional =
        valuestr = stringutils::shift_parameter( params );
    }

    if(sectionstr.empty() || keystr.empty()) {
        os.printf("Usage: config-set section key [value]\n");
        return true;
    }

    // remove the optional [ ... ] around section
    auto pos = sectionstr.find("[");
    if(pos != sectionstr.npos) {
        sectionstr.erase(pos, 1);
        pos = sectionstr.find("]");
        if(pos != sectionstr.npos) sectionstr.erase(pos, 1);
    }

    std::fstream fsin;
    std::fstream fsout;
    fsin.open("/sd/config.ini", std::fstream::in);
    if(!fsin.is_open()) {
        os.printf("Error opening file /sd/config.ini\n");
        return true;
    }

    fsout.open("/sd/config.tmp", std::fstream::out);
    if(!fsout.is_open()) {
        os.printf("Error opening file /sd/config.tmp\n");
        fsin.close();
        return true;
    }

    ConfigWriter cw(fsin, fsout);

    const char *section = sectionstr.c_str();
    const char *key = keystr.c_str();
    const char *value = nullptr;

    // empty valuestr will delete the key
    if(!valuestr.empty()) value = valuestr.c_str();

    if(cw.write(section, key, value)) {
        if(value == nullptr) {
            os.printf("deleted config: [%s] %s\n", section, key);
        } else {
            os.printf("set config: [%s] %s = %s\n", section, key, value);
        }

    } else {
        if(value == nullptr) {
            os.printf("failed to delete config: [%s] %s\n", section, key);
        } else {
            os.printf("failed to set config: [%s] %s = %s\n", section, key, value);
        }
        return true;
    }

    fsin.close();
    fsout.close();

    // now rename the config.ini file to config.bak and config.tmp file to config.ini
    // remove old backup file first
    remove("/sd/config.bak");
    int s = rename("/sd/config.ini", "/sd/config.bak");
    if(s == 0) {
        s = rename("/sd/config.tmp", "/sd/config.ini");
        if(s != 0) os.printf("Failed to rename config.tmp to config.ini\n");

    } else {
        os.printf("Failed to rename config.ini to config.bak - aborting\n");
    }
    return true;
}

// checks if idle and no heaters are on
bool CommandShell::is_busy()
{
    if(!Conveyor::getInstance()->is_idle()) {
        return true;
    }

    // scan all temperature controls and make sure they are all off
    std::vector<Module*> controllers = Module::lookup_group("temperature control");
    for(auto m : controllers) {
        TemperatureControl::pad_temperature_t temp;
        if(m->request("get_current_temperature", &temp)) {
            if(temp.target_temperature > 0) {
                return true;
            }
        }
    }
    return false;
}

bool CommandShell::download_cmd(std::string& params, OutputStream& os)
{
    HELP("dl filename size - fast streaming binary download over USB serial");

    std::string fn = stringutils::shift_parameter( params );
    std::string sizestr = stringutils::shift_parameter( params );

    os.set_no_response(true);

    if(fn.empty() || sizestr.empty()) {
        os.printf("FAIL - Usage dl filename size\n");
        return true;
    }

    if(is_busy()) {
        os.printf("FAIL - download not allowed while printing or heaters are on\n");
        return true;
    }

    if(!os.is_usb()) {
        os.printf("FAIL - download only allowed over USB\n");
        return true;
    }

    char *e = NULL;
    ssize_t file_size = strtol(sizestr.c_str(), &e, 10);
    if (e <= sizestr.c_str() || file_size <= 0) {
        os.printf("FAIL - file size is bad: %d\n", file_size);
        return true;
    }

    FILE *fp = fopen(fn.c_str(), "w");
    if(fp == nullptr) {
        os.printf("FAIL - could not open file: %d\n", errno);
        return true;
    }

    printf("DEBUG: fast download over USB serial started\n");

    volatile ssize_t state = 0;
    SemaphoreHandle_t xSemaphore = xSemaphoreCreateBinary();

    os.fast_capture_fnc = [&fp, &state, xSemaphore, file_size](char *buf, size_t len) {
        // note this is being run in the Comms thread
        if(state < 0 || state >= file_size) return true; // we are in an error state or done

        if(fwrite(buf, 1, len, fp) != len) {
            state = -1;
            printf("DEBUG: fast download fwrite failed\n");
            xSemaphoreGive(xSemaphore);
        } else {
            state += len;
            if(state >= file_size) {
                printf("DEBUG: fast download all bytes received\n");
                xSemaphoreGive(xSemaphore);
                return false; // indicates we are done successfully
            }
        }
        return true;
    };

    // tell host we are ready for the file
    os.printf("READY - %d\n", file_size);

    // wait for completion or timeout
    // Note this will block the command thread, and also query processing
    // may need to call handle_query every now and then (like safe_sleep)
    if(xSemaphoreTake(xSemaphore, pdMS_TO_TICKS(20000)) != pdTRUE) {
        printf("DEBUG: fast download timed out\n");
        state = -2;
        errno = ETIMEDOUT;
    }

    os.printf(state <= 0 ? "FAIL - %d\n" : "SUCCESS\n", errno);

    fclose(fp);
    vSemaphoreDelete(xSemaphore);

    printf("DEBUG: fast download over USB serial ended: %d\n", state);

    if(state <= 0) {
        // allow incoming buffers to drain
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    os.fast_capture_fnc = nullptr;

    return true;
}

bool CommandShell::ry_cmd(std::string& params, OutputStream& os)
{
    HELP("ymodem recieve");

    if(is_busy()) {
        os.printf("FAIL - ymodem not allowed while printing or heaters are on\n");
        return true;
    }

    if(params.empty()) {
        os.printf("start ymodem transfer\n");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    YModem ymodem([&os](char c) {os.write(&c, 1);});
    // check we did not run out of memory
    if(!ymodem.is_ok()) {
        os.printf("error: not enough memory\n");
        return true;
    }

    os.capture_fnc = [&ymodem](char c) { ymodem.add(c); };
    int ret = ymodem.receive();
    os.capture_fnc = nullptr;

    if(params.empty()) {
        if(ret > 0) {
            os.printf("downloaded %d file(s) ok\n", ret);
        } else {
            os.printf("download failed with error %d\n", ret);
        }
    } else {
        os.set_no_response(true);
    }

    return true;
}

bool CommandShell::truncate_file(const char *fn, int size, OutputStream& os)
{
    FIL fp;  /* File object */
    // Open file
    int ret = f_open(&fp, fn, FA_WRITE);
    if(FR_OK != ret) {
        os.printf("file %s does not exist\n", fn);
        return false;
    }

    ret = f_lseek(&fp, size);
    if(FR_OK != ret) {
        f_close(&fp);
        os.printf("error %d seeking to %d bytes\n", ret, size);
        return false;
    }

    ret = f_truncate(&fp);
    f_close(&fp);
    if(FR_OK != ret) {
        os.printf("error %d truncating file\n", ret);
        return false;
    }

    return true;
}

bool CommandShell::truncate_cmd(std::string& params, OutputStream& os)
{
    HELP("truncate file to size: truncate filename size-in-bytes");
    std::string fn = stringutils::shift_parameter( params );
    std::string sizestr = stringutils::shift_parameter( params );

    if(fn.empty() || sizestr.empty()) {
        os.printf("Usage: truncate filename size\n");
        return true;
    }

    char *e = NULL;
    int size = strtol(sizestr.c_str(), &e, 10);
    if (e <= sizestr.c_str() || size <= 0) {
        os.printf("size must be > 0\n");
        return true;
    }

    if(truncate_file(fn.c_str(), size, os)) {
        os.printf("File %s truncated to %d bytes\n", fn.c_str(), size);
    }

    return true;
}

bool CommandShell::echo_cmd(std::string& params, OutputStream& os)
{
    HELP("echo to consoles: [-n] don't send NL [-1] only send to Aux Uart, everything else gets sent");
    std::string s;
    bool send_nl = true;
    bool send_to_uart = false;

    // -n don't send NL
    // -1 send only to uart
    while(true) {
        std::string opts = stringutils::shift_parameter(params);
        if(opts.size() == 2 && opts[0] == '-') {
            // process option
            switch(toupper(opts[1])) {
                case 'N':
                    send_nl = false;
                    break;
                case '1':
                    send_to_uart = true;
                    break;
                default:
                    s.append(opts);
                    if(params.size() > 0) s.append(" ");
                    break;
            }
            continue;

        } else {
            s.append(opts);
            if(params.size() > 0) s.append(" ");
            break;
        }
    }

    s.append(params);
    if(send_nl) s.append("\n");
    if(!send_to_uart) {
        print_to_all_consoles(s.c_str());
    } else {
        UART *uart = get_aux_uart();
        if(uart != nullptr) {
            uart->write((uint8_t*)s.data(), s.size());
        }
    }
    return true;
}

extern "C" void rtc_get_datetime(char *buf, size_t len);
extern "C" int rtc_setdatetime(uint8_t year, uint8_t month, uint8_t day, uint8_t weekday, uint8_t hour, uint8_t minute, uint8_t seconds);

bool CommandShell::date_cmd(std::string& params, OutputStream& os)
{
    HELP("date [YYMMDDhhmmss] - set or get current date/time");
    std::string dt = stringutils::shift_parameter(params);

    if(dt.empty()) {
        char buf[20];
        rtc_get_datetime(buf, sizeof(buf));
        os.printf("%s\n", buf);

    } else {
        // set date/time
        uint8_t yr = atoi(dt.substr(0, 2).c_str());
        uint8_t mn = atoi(dt.substr(2, 2).c_str());
        uint8_t dy = atoi(dt.substr(4, 2).c_str());
        uint8_t hh = atoi(dt.substr(6, 2).c_str());
        uint8_t mm = atoi(dt.substr(8, 2).c_str());
        uint8_t ss = atoi(dt.substr(10, 2).c_str());
        rtc_setdatetime(yr, mn, dy, 1, hh, mm, ss);
    }
    return true;
}

bool CommandShell::break_cmd(std::string& params, OutputStream& os)
{
    HELP("force s/w break point");
    //*(volatile int*)0xa5a5a5a4 = 1; // force hardware fault
    __asm("bkpt #0");
    return true;
}

extern "C" void shutdown_cdc();
bool CommandShell::reset_cmd(std::string& params, OutputStream& os)
{
    HELP("reset board");
    os.printf("Reset will occur in 1 second\n");
    vTaskDelay(pdMS_TO_TICKS(500));
    f_unmount("sd");
    shutdown_cdc();
    vTaskDelay(pdMS_TO_TICKS(500));
    NVIC_SystemReset();
    return true;
}

#include "uart_debug.h"
extern "C" void shutdown_sdmmc();

static void stop_everything(void)
{
    // stop stuff
    f_unmount("sd");
    FastTicker::getInstance()->stop();
    StepTicker::getInstance()->stop();
    Adc::stop();
    shutdown_sdmmc(); // NVIC_DisableIRQ(SDIO_IRQn);
    set_abort_comms();

    stop_uart();
    shutdown_cdc(); // NVIC_DisableIRQ(USB0_IRQn);
    vTaskSuspendAll();
    vTaskEndScheduler(); // NVIC_DisableIRQ(SysTick_IRQn);
    taskENABLE_INTERRUPTS(); // we want to set the base pri back
}

// linker added pointers to the included binary
extern uint8_t _binary_flashloader_bin_start[];
extern uint8_t _binary_flashloader_bin_end[];
extern uint8_t _binary_flashloader_bin_size[];
extern "C" void jump_to_program(uint32_t prog_addr);
bool check_flashme_file(OutputStream& os, bool showerrors);

bool CommandShell::flash_cmd(std::string& params, OutputStream& os)
{
    HELP("flash the flashme.bin file");

    if(is_busy()) {
        os.printf("FAIL - flash not allowed while printing or heaters are on\n");
        return true;
    }

    if(!check_flashme_file(os, true)) {
        return true;
    }

    os.printf("flashing please standby for reset\n");

    vTaskDelay(pdMS_TO_TICKS(100));
    stop_everything();

    // Disable all interrupts
    __disable_irq();

    // binary file compiled to load and run at 0x20000000
    // this program will flash the flashme.bin found on the sdcard then reset
    uint8_t *data_start     = _binary_flashloader_bin_start;
    //uint8_t *data_end       = _binary_flashloader_bin_end;
    size_t data_size  = (size_t)_binary_flashloader_bin_size;
    // copy to DTCMRAM
    uint32_t *addr = (uint32_t*)0x20000000;
    // copy to execution area at addr
    memcpy(addr, data_start, data_size);
    jump_to_program((uint32_t)addr);
    // should never get here
    __asm("bkpt #0");
    return true;
}

bool CommandShell::dfu_cmd(std::string& params, OutputStream& os)
{
    HELP("start dfu");

    if(is_busy()) {
        os.printf("FAIL - DFU not allowed while printing or heaters are on\n");
        return true;
    }

    os.printf("NOTE: A reset will be required to resume if dfu-util is not run\n");
#ifdef BOARD_NUCLEO
    // no qspi so dfu flash loader is in high flash 0x081E0000
    // check the first entry is a valid stack pointer and jump into qspi
    if(((*(__IO uint32_t *) 0x081E0000) & 0xFFFF0000) == 0x20020000 &&
       ((*(__IO uint32_t *) 0x081E0004) & 0xFFFF0000) == 0x081E0000) {
        stop_everything();
        jump_to_program((uint32_t)0x081E0000);
        // should never get here
        __asm("bkpt #0");
    } else {
        os.printf("There does not appear to be a valid executable at top of flash\n");
    }
    return true;

#else
    std::string cmd("run");
    return qspi_cmd(cmd, os);
#endif
}

/*
    TODO
    init flash by erasing whole thing to 0xFF.
    create a filesystem where the first 64KB is a directory with name, flash address, size (each entry is 256 bytes?)
    terminated by name being 0xFF (unflashed area), skip names that are 0x00.
    when we write a file we find the next available flash address that is after the first 64k but on a 256 byte boundary
    (flash write block size). (as we can only erase in 64KB blocks maybe files need to be on 64KB boundary).
    flash new file to that flash address, then when done update the directory block with that new entry.
    (copy it, erase the first block, and write the new 64kb block, or if we leave the unused blocks as 0xFF we can just write the new entry if entries are on a 256 byte boundary).
    We can execute from QSPI at the address mapped for the file.
    We can list files.
    We can erase files which zeroes out the directory entry, but unless the files are stored in 64k chunks we cannot
    erase the file, so eventually the filesystem will fill up, and we would need to erase the entire flash to start over.
*/
extern "C" bool qspi_flash(const char *fn);
extern "C" bool qspi_mount();
bool CommandShell::qspi_cmd(std::string& params, OutputStream& os)
{
    HELP("issue a qspi command - [flash fn] | [mount] | [run]");

    std::string cmd = stringutils::shift_parameter(params);
    if(cmd.empty()) {
        os.printf("need one of flash, mount, run\n");
        return true;
    }

    if(cmd == "flash") {
        std::string fn = stringutils::shift_parameter(params);
        if(fn.empty()) {
            os.printf("need filename to flash\n");
            return true;
        }

        if(qspi_flash(fn.c_str())) {
            os.printf("flashing file %s to qspi succeeded\n", fn.c_str());
        } else {
            os.printf("qspi flash failed see logs for details\n");
        }

    } else if(cmd == "mount" || cmd == "run") {
        if(qspi_mount()) {
            os.printf("mounting qspi to 0x9000000 succeeded\n");
        } else {
            os.printf("mounting qspi failed see logs for details\n");
            return true;
        }

        if(cmd == "run") {
            if(is_busy()) {
                os.printf("FAIL - qspi run not allowed while printing or heaters are on\n");
                return true;
            }

            // check the first entry is a valid stack pointer and jump into qspi
            if(((*(__IO uint32_t *) 0x90000000) & 0xFFFF0000) == 0x20020000 &&
               ((*(__IO uint32_t *) 0x90000004) & 0xFFFF0000) == 0x90000000) {
                stop_everything();
                jump_to_program((uint32_t)0x90000000);
                // should never get here
                __asm("bkpt #0");
            } else {
                os.printf("There does not appear to be a valid program in qspi\n");
            }
        }

    } else {
        os.printf("Unknown qspi command\n");
    }

    return true;
}

#include "usb_device.h"
extern "C" int config_msc_enable;
extern Pin *msc_led;
bool CommandShell::msc_cmd(std::string& params, OutputStream& os)
{
    HELP("switch to MSC file mode");
    if(config_msc_enable == 0) {
        os.printf("MSC is disabled in config.ini\n");
        return true;
    }

    if(is_busy()) {
        os.printf("FAIL - MSC not allowed while printing or heaters are on\n");
        return true;
    }

    os.printf("NOTE: The system will reboot when the MSC is ejected on the host, otherwise a reset maybe required to resume\n");

    // first shutdown selected things as MSC hogs the sdcard, and runs in interrupt mode, so RTOS will be blocked
    f_unmount("sd");
    FastTicker::getInstance()->stop();
    StepTicker::getInstance()->stop();
    Adc::stop();
    set_abort_comms(); // this should also shutdown Network
    stop_uart();

    vTaskSuspendAll();
    vTaskEndScheduler();
    taskENABLE_INTERRUPTS();

    // now startup MSC
    UsbDevice_Init_MSC();
    printf("DEBUG: MSC is now running\n");

    // msc led flashes when in msc mode
    // as nothing else can happen and MSC runs under Interrupts we sit in a tight loop here waiting for it to end
    uint32_t flash_time = HAL_GetTick();
    while(true) {
        if(check_MSC()) {
            // we have been ejected so reboot
            printf("DEBUG: MSC has been safely ejected, now rebooting\n");
            if(msc_led != nullptr) msc_led->set(true);
            HAL_Delay(250);
            NVIC_SystemReset();
        }
        if(msc_led != nullptr) {
            if((HAL_GetTick() - flash_time) > 300) { // 300ms flash period
                msc_led->set(!msc_led->get());
                flash_time = HAL_GetTick();
            }
        }
    }
}

namespace ecce
{
int main(const char *infile, const char *outfile, std::function<void(char)> outfnc);
void add_input(char c);
}
bool CommandShell::edit_cmd(std::string& params, OutputStream& os)
{
    HELP("ed infile outfile - the ecce line editor");

    os.set_no_response(true);
    if(is_busy()) {
        os.printf("FAIL - ed not allowed while printing or heaters are on\n");
        return true;
    }

    std::string infile = stringutils::shift_parameter(params);
    std::string outfile = stringutils::shift_parameter(params);

    if(infile.empty() || outfile.empty() || infile == outfile) {
        os.printf("Need unique input file and output file\n");
        return true;
    }

    if(FR_OK == f_stat(outfile.c_str(), NULL)) {
        os.printf("destination file already exists\n");
        return true;
    }

    os.capture_fnc = [](char c) { ecce::add_input(c); };
    int ret = ecce::main(infile.c_str(), outfile.c_str(), [&os](char c) {os.write(&c, 1);});
    os.capture_fnc = nullptr;
    if(ret == 0) {
        os.printf("edit was successful\n");
    } else {
        remove(outfile.c_str());
        os.printf("edit failed\n");
    }

    return true;
}
