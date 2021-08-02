#pragma once
#include <cstdint>

class SPI
{

public:
	static SPI *getInstance(int channel);
	static void deleteInstance(int channel);

	/** Configure the data transmission format
	 *
	 *  @param bits Number of bits per SPI frame (4 - 16)
	 *  @param mode Clock polarity and phase mode (0 - 3)
	 *
	 * @code
	 * mode | POL PHA
	 * -----+--------
	 *   0  |  0   0
	 *   1  |  0   1
	 *   2  |  1   0
	 *   3  |  1   1
	 * @endcode
	 */
	bool init(int bits=8, int mode=0, int frequency=1000000);

	bool write_read(void *wvalue, void *rvalue, uint32_t n);
	bool valid() const { return _valid; }
	void *get_hspi() const { return _hspi; }
    uint8_t get_mode() const { return _mode; }
    bool begin_transaction(uint32_t tmoms=10000);
    void end_transaction();

	static SPI *spi_channel[2];

private:
	SPI(int channel);
	virtual ~SPI();
	void *_hspi;
    void *mutex;
	bool _valid;
	int _channel;
	int _bits;
	int _mode;
	int _hz;
};
