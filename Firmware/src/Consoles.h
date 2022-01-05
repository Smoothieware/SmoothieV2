#pragma once

class OutputStream;
class ConfigReader;

#define DEFAULT_OVERRIDE_FILE "/sd/config-override"

// TODO may move to Dispatcher
bool dispatch_line(OutputStream& os, const char *line);
bool process_command_buffer(size_t n, char *rxBuf, OutputStream *os, char *line, size_t& cnt, bool& discard, bool wait=true);
bool configure_consoles(ConfigReader& cr);
bool start_consoles();
bool load_config_override(OutputStream& os, const char *fn=DEFAULT_OVERRIDE_FILE);
void command_handler();

// print string to all connected consoles
extern "C" void print_to_all_consoles(const char *);
extern "C" void set_abort_comms();

// the communications task priority (lower number is lower priority)
#define COMMS_PRI 3UL
// The command thread priority
#define CMDTHRD_PRI 2UL
