#pragma once

class OutputStream;
class ConfigReader;
class GCode;

class TMCBase
{
public:
	TMCBase(){};
	virtual ~TMCBase(){};

	virtual void init()=0;
	virtual void setMicrosteps(int number_of_steps)=0;
	virtual int getMicrosteps(void)=0;
	virtual void setEnabled(bool enabled)=0;
	virtual bool isEnabled()=0;
	virtual void setCurrent(unsigned int current)=0;
	virtual bool set_raw_register(OutputStream& stream, uint32_t reg, uint32_t val)=0;
	virtual bool check_errors()=0;
	virtual bool config(ConfigReader& cr, const char *actuator_name)=0;
	virtual void dump_status(OutputStream& stream, bool readable = true)=0;
	virtual bool set_options(const GCode& gcode)=0;
};
