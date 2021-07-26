#pragma once

#include <cstdint>
#include <cstddef>

class UART
{
public:
	static UART *getInstance(int channel);
	static void deleteInstance(int channel);
    using settings_t = struct {
        uint32_t baudrate:24;
        uint8_t  bits:4;
        uint8_t  stop_bits:2;
        uint8_t  parity:2;
    };
	bool init(settings_t settings);

	size_t write(uint8_t *buf, uint16_t len, uint32_t timeout=0xFFFFFFFF); // portMAX_DELAY
    size_t read(uint8_t *buf, uint16_t len, uint32_t timeout=0xFFFFFFFF);
	bool valid() const { return _valid; }
	void *get_huart() const { return _huart; }
    int get_channel() const { return _channel; }
    void tx_cplt_callback();
    void rx_event_callback(uint16_t size);
	static UART *uart_channel[2];


private:
	UART(int channel);
	virtual ~UART();

	void *_huart{nullptr};
    void *tx_rb{nullptr};
    void *rx_rb{nullptr};
	int _channel;
    settings_t settings;
    uint32_t overflow{0};
	bool _valid{false};
    uint8_t old_pos{0};
    volatile uint16_t tx_dma_current_len{0};
};
