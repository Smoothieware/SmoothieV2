#pragma once

#include <cstdint>

class I2C
{
public:
    static I2C *getInstance(int channel);
	static void deleteInstance(int channel);

	bool init(int frequency);
	bool read(uint8_t slave_addr, uint8_t *buf, int len);
	bool write(uint8_t slave_addr, uint8_t *buf, int len);
	bool valid() const { return _valid; }

	void set_event(uint8_t ev);
	void *get_hi2c() const { return _hi2c; }
	static I2C *i2c_channel[2];

private:
	I2C(int channel);
	virtual ~I2C();

	void *_hi2c;
	void *_peventg;
	int _frequency;
	int channel;
	bool _valid;
};
