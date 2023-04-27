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
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, int color=0);
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, int color=1);

    typedef struct {
      uint16_t bitmapOffset; ///< Pointer into GFXfont->bitmap
      uint8_t width;         ///< Bitmap dimensions in pixels
      uint8_t height;        ///< Bitmap dimensions in pixels
      uint8_t xAdvance;      ///< Distance to advance cursor (x axis)
      int8_t xOffset;        ///< X dist from cursor pos to UL corner
      int8_t yOffset;        ///< Y dist from cursor pos to UL corner
    } GFXglyph;

    /// Data stored for FONT AS A WHOLE
    typedef struct {
      uint8_t *bitmap;  ///< Glyph bitmaps, concatenated
      GFXglyph *glyph;  ///< Glyph array
      uint16_t first;   ///< ASCII extents (first char)
      uint16_t last;    ///< ASCII extents (last char)
      uint8_t yAdvance; ///< Newline distance (y axis)
    } GFXfont;
    void displayAFString(int x, int y, int color, const char *ptr, int length=0);
    void setFont(const GFXfont *f) { gfxFont= f; }
    void charBounds(unsigned char c, int16_t *x, int16_t *y, int16_t *minx, int16_t *miny, int16_t *maxx, int16_t *maxy);
    void getTextBounds(const char *str, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h);

private:
    void renderChar(uint8_t *fb, char c, int ox, int oy);
    void displayChar(int row, int column, char inpChr);
    void drawByte(int index, uint8_t mask, int color);
    int drawAFChar(int x, int y, uint8_t c, int color);

    void spi_write(uint8_t v);
    const GFXfont *gfxFont{nullptr};

    Pin *clk{nullptr};
    Pin *mosi{nullptr};
    // Pin *miso{nullptr};
    uint8_t *fb{nullptr};
    bool inited{false};
    bool dirty{false};
};

