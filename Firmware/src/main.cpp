#include "main.h"

#include "Adc.h"
#include "Adc3.h"
#include "CommandShell.h"
#include "ConfigReader.h"
#include "Consoles.h"
#include "Conveyor.h"
#include "CurrentControl.h"
#include "Dispatcher.h"
#include "Endstops.h"
#include "Extruder.h"
#include "FastTicker.h"
#include "GCode.h"
#include "GCodeProcessor.h"
#include "KillButton.h"
#include "Laser.h"
#include "MessageQueue.h"
#include "Module.h"
#include "Network.h"
#include "OutputStream.h"
#include "Pin.h"
#include "Planner.h"
#include "Player.h"
#include "Pwm.h"
#include "RingBuffer.h"
#include "Robot.h"
#include "SlowTicker.h"
#include "StepTicker.h"
#include "StringUtils.h"
#include "Switch.h"
#include "TemperatureControl.h"
#include "ZProbe.h"
#include "version.h"
#include "leds.h"

#include "FreeRTOS.h"
#include "task.h"
#include "ff.h"
#include "semphr.h"

#include "uart_debug.h"
#include "benchmark_timer.h"

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


static bool system_running = false;
static Pin *aux_play_led = nullptr;
Pin *msc_led = nullptr;
extern "C" int config_dfu_required;
extern "C" int config_msc_enable;

static std::string config_error_msg;

const char *get_config_error_msg() {
    if(!config_error_msg.empty()) {
        return config_error_msg.c_str();
    }
    return nullptr;
}

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

extern "C" bool setup_sdmmc();
static FATFS fatfs; /* File system object */
extern bool config_override;

static void smoothie_startup(void *)
{
    printf("INFO: Smoothie V2 Build for %s - starting up\n", BUILD_TARGET);
    {
        Version vers;
        printf("INFO: Build version: %s, Build date: %s\n", vers.get_build(), vers.get_build_date());
    }

    // led 3 indicates boot phase 2 starts
    Board_LED_Set(3, false); Board_LED_Set(2, true);

    // create the FastTicker here as it is used by some modules
    FastTicker *fast_ticker = FastTicker::getInstance();

    // create the SlowTicker here as it is used by some modules
    SlowTicker *slow_ticker = SlowTicker::getInstance();

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
    bool adcok= false;

    // open the config file
    do {
        if(!setup_sdmmc()) {
            printf("ERROR: setting up sdmmc\n");
            break;
        }

        // TODO check the card is inserted flash 4 leds if not

        int ret = f_mount(&fatfs, "sd", 1);
        if(FR_OK != ret) {
            printf("ERROR: mounting: /sd: %d\n", ret);
            break;
        }

        std::fstream fs;
        fs.open("/sd/config.ini", std::fstream::in);
        if(!fs.is_open()) {
            printf("ERROR: opening file: /sd/config.ini\n");
            // unmount sdcard
            //f_unmount("sd");
            break;
        }

        ConfigReader cr(fs);
        printf("DEBUG: Starting configuration of modules from sdcard...\n");

        // led 2 indicates boot phase 3 starts
        Board_LED_Set(2, false); Board_LED_Set(1, true);

        {
            // get general settings
            ConfigReader::section_map_t gm;
            if(cr.get_section("general", gm)) {
                bool f = cr.get_bool(gm, "grbl_mode", false);
                THEDISPATCHER->set_grbl_mode(f);
                printf("INFO: grbl mode %s\n", f ? "set" : "not set");
                config_override = cr.get_bool(gm, "config-override", false);
                printf("INFO: use config override is %s\n", config_override ? "set" : "not set");
            }
        }
        {
            // get system settings
            ConfigReader::section_map_t sm;
            if(cr.get_section("system", sm)) {
                std::string p = cr.get_string(sm, "aux_play_led", "nc");
                aux_play_led = new Pin(p.c_str(), Pin::AS_OUTPUT);
                if(!aux_play_led->connected()) {
                    delete aux_play_led;
                    aux_play_led = nullptr;
                } else {
                    printf("INFO: auxilliary play led set to %s\n", aux_play_led->to_string().c_str());
                }
                flash_on_boot = cr.get_bool(sm, "flash_on_boot", true);
                printf("INFO: flash on boot is %s\n", flash_on_boot ? "enabled" : "disabled");
                bool enable_dfu = cr.get_bool(sm, "dfu_enable", false);
                config_dfu_required = enable_dfu ? 1 : 0; // set it in the USB stack
                printf("INFO: dfu is %s\n", enable_dfu ? "enabled" : "disabled");
                config_msc_enable = cr.get_bool(sm, "msc_enable", true) ? 1 : 0;
                printf("INFO: MSC is %s\n", config_msc_enable ? "enabled" : "disabled");
                if(config_msc_enable) {
                    #ifdef BOARD_PRIME
                    const char *default_msc_led= "PF13";
                    #else
                    const char *default_msc_led= "nc";
                    #endif
                    p = cr.get_string(sm, "msc_led", default_msc_led);
                    if(p != "nc") {
                        msc_led = new Pin(p.c_str(), Pin::AS_OUTPUT);
                        if(!msc_led->connected()) {
                            delete msc_led;
                            msc_led = nullptr;
                            printf("ERROR: MSC led set to an invalid pin: %s\n", p.c_str());
                        } else {
                            printf("INFO: MSC led set to %s\n", msc_led->to_string().c_str());
                        }
                    }
                }

            }else{
                printf("WARNING: no [system] section found, some defaults used\n");
            }
        }

        configure_consoles(cr);

        printf("DEBUG: configure the planner\n");
        Planner *planner = Planner::getInstance();
        planner->configure(cr);

        printf("DEBUG: configure the conveyor\n");
        Conveyor *conveyor = Conveyor::getInstance();
        conveyor->configure(cr);

        printf("DEBUG: configure the robot\n");
        Robot *robot = Robot::createInstance();
        if(!robot->configure(cr)) {
            printf("ERROR: Configuring robot failed\n");
            break;
        }

        ///////////////////////////////////////////////////////////
        // configure core modules here
        {
            // Pwm needs to be initialized, there are two PWM timers and each
            // can have a frequency.
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
            voltage_monitors["vbat"] = std::make_tuple(-2, 4.0F);
            // setup board defaults if not defined
#ifdef BOARD_NUCLEO
            std::map<std::string, std::tuple<int32_t, float>> names { {"vmotor", {1, 11.0F}},  {"vfet", {2, 11.0F}} };
#elif defined(BOARD_DEVEBOX)
            std::map<std::string, std::tuple<int32_t, float>> names { {"vmotor", {0, 11.0F}},  {"vfet", {3, 11.0F}} };
#elif defined(BOARD_PRIME)
            std::map<std::string, std::tuple<int32_t, float>> names { {"vmotor", {5, 11.0F}},  {"vfet", {2, 11.0F}} };
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
        adcok= Adc::post_config_setup();
        if(!adcok) {
            printf("ERROR: ADC failed to setup\n");
        }

        // setup PWM
        if(!Pwm::post_config_setup()) {
            printf("ERROR: Pwm::post_config_setup failed\n");
        }

        // close the file stream
        fs.close();

        // unmount sdcard
        //f_unmount("sd");

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

    // create the command shell, it is dependent on some of the above
    CommandShell *commandshell= CommandShell::getInstance();


    if(ok) {
        // led 1 indicates boot phase 4 starts
        Board_LED_Set(1, false); Board_LED_Set(0, true);

        if(!fast_ticker->start()) {
            printf("WARNING: failed to start FastTicker (maybe nothing is using it?)\n");
        }

        if(!slow_ticker->start()) {
            printf("WARNING: failed to start SlowTicker\n");
        }

        if(!step_ticker->start()) {
            printf("Error: failed to start StepTicker\n");
        }

        if(adcok && !Adc::start()) {
            printf("Error: failed to start ADC\n");
        }

    } else {
        printf("ERROR: Configure failed\n");
        config_error_msg = "There was a fatal error in the config.ini this must be fixed to continue\nOnly some shell commands are allowed and sdcard access\n";
        printf(config_error_msg.c_str());
        // Module::broadcast_halt(true);
        Board_LED_Set(1, false); Board_LED_Set(0, false);
    }

    extern bool config_error_detected;
    if(config_error_detected) {
        config_error_msg = "There was an error detected in the config.ini\nPlease check the uart log\n";
        printf(config_error_msg.c_str());
    }

    if(!start_consoles()) {
        printf("ERROR: consoles failed to start\n");
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
            os.printf("INFO: configuration override %s loaded\n", DEFAULT_OVERRIDE_FILE);

        } else {
            os.printf("INFO: No saved configuration override\n");
        }
    }

    if(ok) {
        // led 1 off indicates boot complete
        Board_LED_Set(0, false);
    }

    if(flash_on_boot) {
        OutputStream os(&std::cout);
        if(check_flashme_file(os, false)) {
            // we have a valid flashme file, so flash
            std::string str;
            commandshell->flash_cmd(str, os);
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
    // it is in Consoles.cpp
    command_handler();

    // does not return from above
}

extern "C" void setup_xprintf();
extern "C" void main_system_setup();
extern "C" int rtc_init();

std::string get_mcu();
uint8_t board_id= 0;

int main(int argc, char *argv[])
{
    // setup clock and caches etc (in HAL)
    main_system_setup();

    Board_LED_Init();
    // led 4 indicates start of boot phase 1
    Board_LED_Set(3, true);

    // allows cout to work again (not sure why)
    std::ios_base::sync_with_stdio(false);

    benchmark_timer_init();

    setup_xprintf();

    if(setup_uart() < 0) {
        printf("FATAL: UART setup failed\n");
    }

    // get board id
    // 0 is first Prime with tmc2590 drivers
    // 1 is second Prime with tmc2660 drivers
    Pin bid3("PE10^", Pin::AS_INPUT),
        bid2("PF3^", Pin::AS_INPUT),
        bid1("PF5^", Pin::AS_INPUT),
        bid0("PF7^", Pin::AS_INPUT);

    board_id= 0x0F & ~(((bid3.get()?1:0)<<3) | ((bid2.get()?1:0)<<2) | ((bid1.get()?1:0)<<1) | ((bid0.get()?1:0)));
    printf("INFO: %s on %s. board id: %02X\n", get_mcu().c_str(), BUILD_TARGET, board_id);
    printf("INFO: MCU clock rate= %lu Hz\n", SystemCoreClock);
    // de init them
    bid0.deinit(); bid1.deinit(); bid2.deinit(); bid3.deinit();

    if(rtc_init() != 1) {
        printf("ERROR: Failed to init RTC\n");
    }

    // launch the startup thread which will become the command thread that executes all incoming commands
    // set to be lower priority than comms, although it maybe better to invert them as we don't really
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
    Board_LED_Set(2, true);
    Board_LED_Set(3, true);
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
    printf("ERROR: vAssertCalled( %s, %ld )\n", pcFile, ulLine);

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
