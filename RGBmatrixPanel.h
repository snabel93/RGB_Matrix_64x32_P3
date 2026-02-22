/*!
 * @file RGBmatrixPanel.h
 *
 * RGB LED Matrix Panel library adapted for RP2040/RP2350 (Pico 2).
 * Based on Adafruit's RGBmatrixPanel library.
 *
 * Written by Limor Fried/Ladyada & Phil Burgess/PaintYourDragon for
 * Adafruit Industries. RP2040 port adapted for Pico 2 RP2350.
 *
 * BSD license, all text here must be included in any redistribution.
 */

#ifndef RGBMATRIXPANEL_H
#define RGBMATRIXPANEL_H

#include "Arduino.h"
#include "Adafruit_GFX.h"
#include "fonts.h"

typedef uint32_t PortType;

class RGBmatrixPanel : public Adafruit_GFX {

public:
  // Constructor for 16x32 panel
  RGBmatrixPanel(uint8_t a, uint8_t b, uint8_t c, uint8_t clk, uint8_t lat,
                 uint8_t oe, boolean dbuf, uint8_t *pinlist = NULL);

  // Constructor for 32x32 or 32x64 panel
  RGBmatrixPanel(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t clk,
                 uint8_t lat, uint8_t oe, boolean dbuf, uint8_t width = 32,
                 uint8_t *pinlist = NULL);

  // Constructor for 64x64 panel
  RGBmatrixPanel(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e,
                 uint8_t clk, uint8_t lat, uint8_t oe, boolean dbuf,
                 uint8_t width = 64, uint8_t *pinlist = NULL);

  void begin(void);
  void DrawString_CN(uint8_t Xstart, uint8_t Ystart, const char *pString,
                     cFONT *font, uint16_t color);
  void drawPixel(int16_t x, int16_t y, uint16_t c);
  void fillScreen(uint16_t c);
  void updateDisplay(void);
  void swapBuffers(boolean);
  void dumpMatrix(void);
  uint8_t *backBuffer(void);

  uint16_t Color333(uint8_t r, uint8_t g, uint8_t b);
  uint16_t Color444(uint8_t r, uint8_t g, uint8_t b);
  uint16_t Color888(uint8_t r, uint8_t g, uint8_t b);
  uint16_t Color888(uint8_t r, uint8_t g, uint8_t b, boolean gflag);
  uint16_t ColorHSV(long hue, uint8_t sat, uint8_t val, boolean gflag);

  void display_image(int16_t x, int16_t y, const uint16_t bitmap[], int16_t w,
                     int16_t h);
  void setFont(const GFXfont *f);

private:
  uint8_t *matrixbuff[2];
  uint8_t nRows;
  volatile uint8_t backindex;
  volatile boolean swapflag;

  void init(uint8_t rows, uint8_t a, uint8_t b, uint8_t c, uint8_t clk,
            uint8_t lat, uint8_t oe, boolean dbuf, uint8_t width,
            uint8_t *rgbpins);

  uint8_t _clk;
  uint8_t _lat;
  uint8_t _oe;
  uint8_t _a;
  uint8_t _b;
  uint8_t _c;
  uint8_t _d;
  uint8_t _e;

  // RP2040/RP2350: use pin numbers directly for fast GPIO
  uint8_t rgbpins[6];        // R1, G1, B1, R2, G2, B2 pin numbers
  PortType rgbclkmask;        // Combined mask for all RGB + CLK pins
  PortType clkmask;           // CLK pin bitmask
  PortType expand[256];       // 6-to-32 bit converter table

  volatile uint8_t row;
  volatile uint8_t plane;
  volatile uint8_t *buffptr;
};

#endif // RGBMATRIXPANEL_H
