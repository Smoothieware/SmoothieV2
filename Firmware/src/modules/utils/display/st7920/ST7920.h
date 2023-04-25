#pragma once

/**
 * Based loosely on st7920.h from http://mbed.org/users/Bas/code/ST7920 and parts of the Arduino U8glib library.
 * Written by Jim Morris originally for Smoothieware V1
 */

#include "Module.h"

class Pin;
class ConfigReader;

class ST7920  : public Module
{
public:
    ST7920 ();
    virtual ~ST7920();

    static bool create(ConfigReader& cr);
    bool configure(ConfigReader& cr);

    void initDisplay(void);
    void clearScreen(void);
    void displayString(int row, int column, const char *ptr, int length);
    void refresh();

    void fillGDRAM(const uint8_t *bitmap);

    // copy the bits in g, of X line size pixels, to x, y in frame buffer
    void renderGlyph(int x, int y, const uint8_t *g, int pixelWidth, int pixelHeight);
    void bltGlyph(int x, int y, int w, int h, const uint8_t *glyph, int span, int x_offset, int y_offset);

    void pixel(int x, int y, int color);
    void drawHLine(int x, int y, int w, int color);
    void drawVLine(int x, int y, int h, int color);
    void drawBox(int x, int y, int w, int h, int color);

private:
    void renderChar(uint8_t *fb, char c, int ox, int oy);
    void displayChar(int row, int column, char inpChr);
    void drawByte(int index, uint8_t mask, int color);
    void spi_write(uint8_t v);

    Pin *clk{nullptr};
    Pin *mosi{nullptr};
    // Pin *miso{nullptr};
    uint8_t *fb{nullptr};
    bool inited{false};
    bool dirty{false};
};
