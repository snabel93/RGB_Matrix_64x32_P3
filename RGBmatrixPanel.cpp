/*!
 * @file RGBmatrixPanel.cpp
 *
 * RGB LED Matrix Panel library adapted for RP2040/RP2350 (Pico 2).
 * Based on Adafruit's RGBmatrixPanel library.
 *
 * Uses RP2040 SIO registers for fast GPIO and core1 for
 * continuous display refresh (no timer interrupts).
 *
 * BSD license, all text here must be included in any redistribution.
 */

#include "RGBmatrixPanel.h"
#include "gamma.h"
#include "hardware/gpio.h"
#include <string.h>

#ifndef _swap_int16_t
#define _swap_int16_t(a, b) \
  {                         \
    int16_t t = a;          \
    a = b;                  \
    b = t;                  \
  }
#endif

#define nPlanes 4

static RGBmatrixPanel *activePanel = NULL;
static volatile bool core1Running = false;

// RP2040/RP2350 fast GPIO via SIO registers
static inline void gpio_set_mask_fast(uint32_t mask) {
  sio_hw->gpio_set = mask;
}
static inline void gpio_clr_mask_fast(uint32_t mask) {
  sio_hw->gpio_clr = mask;
}

// Core1 entry point: continuously refreshes the display
static void core1_display_loop() {
  core1Running = true;
  while (true) {
    if (activePanel) {
      activePanel->updateDisplay();
    }
  }
}

// Code common to all constructors
void RGBmatrixPanel::init(uint8_t rows, uint8_t a, uint8_t b, uint8_t c,
                          uint8_t clk, uint8_t lat, uint8_t oe, boolean dbuf,
                          uint8_t width, uint8_t *pinlist) {
  static const uint8_t defaultrgbpins[] = {0, 1, 2, 3, 4, 5};
  memcpy(rgbpins, pinlist ? pinlist : defaultrgbpins, sizeof(rgbpins));

  nRows = rows;

  int buffsize = width * nRows * 3;
  int allocsize = (dbuf == true) ? (buffsize * 2) : buffsize;
  if (NULL == (matrixbuff[0] = (uint8_t *)malloc(allocsize)))
    return;
  memset(matrixbuff[0], 0, allocsize);
  matrixbuff[1] = (dbuf == true) ? &matrixbuff[0][buffsize] : matrixbuff[0];

  _a = a;
  _b = b;
  _c = c;
  _clk = clk;
  _lat = lat;
  _oe = oe;

  plane = nPlanes - 1;
  row = nRows - 1;
  swapflag = false;
  backindex = 0;
}

// Constructor for 16x32 panel
RGBmatrixPanel::RGBmatrixPanel(uint8_t a, uint8_t b, uint8_t c, uint8_t clk,
                               uint8_t lat, uint8_t oe, boolean dbuf,
                               uint8_t *pinlist)
    : Adafruit_GFX(32, 16) {
  init(8, a, b, c, clk, lat, oe, dbuf, 32, pinlist);
}

// Constructor for 32x32 or 32x64 panel
RGBmatrixPanel::RGBmatrixPanel(uint8_t a, uint8_t b, uint8_t c, uint8_t d,
                               uint8_t clk, uint8_t lat, uint8_t oe,
                               boolean dbuf, uint8_t width, uint8_t *pinlist)
    : Adafruit_GFX(width, 32) {
  init(16, a, b, c, clk, lat, oe, dbuf, width, pinlist);
  _d = d;
}

// Constructor for 64x64 panel
RGBmatrixPanel::RGBmatrixPanel(uint8_t a, uint8_t b, uint8_t c, uint8_t d,
                               uint8_t e, uint8_t clk, uint8_t lat,
                               uint8_t oe, boolean dbuf, uint8_t width,
                               uint8_t *pinlist)
    : Adafruit_GFX(width, 64) {
  init(32, a, b, c, clk, lat, oe, dbuf, width, pinlist);
  _d = d;
  _e = e;
}

void RGBmatrixPanel::begin(void) {
  backindex = 0;
  buffptr = matrixbuff[1 - backindex];
  activePanel = this;

  // Set up control pins
  pinMode(_clk, OUTPUT);
  digitalWrite(_clk, LOW);
  pinMode(_lat, OUTPUT);
  digitalWrite(_lat, LOW);
  pinMode(_oe, OUTPUT);
  digitalWrite(_oe, HIGH); // Disable output

  // Address pins
  pinMode(_a, OUTPUT);
  digitalWrite(_a, LOW);
  pinMode(_b, OUTPUT);
  digitalWrite(_b, LOW);
  pinMode(_c, OUTPUT);
  digitalWrite(_c, LOW);
  if (nRows > 8) {
    pinMode(_d, OUTPUT);
    digitalWrite(_d, LOW);
  }
  if (nRows > 16) {
    pinMode(_e, OUTPUT);
    digitalWrite(_e, LOW);
  }

  // Set up RGB data pins and build expand table
  PortType rgbmask[6];
  clkmask = 1ul << _clk;
  rgbclkmask = clkmask;
  for (uint8_t i = 0; i < 6; i++) {
    pinMode(rgbpins[i], OUTPUT);
    rgbmask[i] = 1ul << rgbpins[i];
    rgbclkmask |= rgbmask[i];
  }

  // Build lookup table: maps packed byte data to GPIO pin mask
  // Buffer format: bits 2-7 contain RGB data for upper/lower halves
  // bit 2 = R1, bit 3 = G1, bit 4 = B1, bit 5 = R2, bit 6 = G2, bit 7 = B2
  for (int i = 0; i < 256; i++) {
    expand[i] = 0;
    if (i & 0x04)
      expand[i] |= rgbmask[0]; // R1
    if (i & 0x08)
      expand[i] |= rgbmask[1]; // G1
    if (i & 0x10)
      expand[i] |= rgbmask[2]; // B1
    if (i & 0x20)
      expand[i] |= rgbmask[3]; // R2
    if (i & 0x40)
      expand[i] |= rgbmask[4]; // G2
    if (i & 0x80)
      expand[i] |= rgbmask[5]; // B2
  }

  // Launch display refresh on core1
  if (!core1Running) {
    multicore_launch_core1(core1_display_loop);
  }
}

// Color conversion functions (same as original)

uint16_t RGBmatrixPanel::Color333(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0x7) << 13) | ((r & 0x6) << 10) | ((g & 0x7) << 8) |
         ((g & 0x7) << 5) | ((b & 0x7) << 2) | ((b & 0x6) >> 1);
}

uint16_t RGBmatrixPanel::Color444(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF) << 12) | ((r & 0x8) << 8) | ((g & 0xF) << 7) |
         ((g & 0xC) << 3) | ((b & 0xF) << 1) | ((b & 0x8) >> 3);
}

uint16_t RGBmatrixPanel::Color888(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) |
         (b >> 3);
}

uint16_t RGBmatrixPanel::Color888(uint8_t r, uint8_t g, uint8_t b,
                                  boolean gflag) {
  if (gflag) {
    r = pgm_read_byte(&gamma_table[r]);
    g = pgm_read_byte(&gamma_table[g]);
    b = pgm_read_byte(&gamma_table[b]);
    return ((uint16_t)r << 12) | ((uint16_t)(r & 0x8) << 8) |
           ((uint16_t)g << 7) | ((uint16_t)(g & 0xC) << 3) | (b << 1) |
           (b >> 3);
  }
  return ((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) |
         (b >> 3);
}

uint16_t RGBmatrixPanel::ColorHSV(long hue, uint8_t sat, uint8_t val,
                                  boolean gflag) {
  uint8_t r, g, b, lo;
  uint16_t s1, v1;

  hue %= 1536;
  if (hue < 0)
    hue += 1536;
  lo = hue & 255;
  switch (hue >> 8) {
  case 0:
    r = 255;
    g = lo;
    b = 0;
    break;
  case 1:
    r = 255 - lo;
    g = 255;
    b = 0;
    break;
  case 2:
    r = 0;
    g = 255;
    b = lo;
    break;
  case 3:
    r = 0;
    g = 255 - lo;
    b = 255;
    break;
  case 4:
    r = lo;
    g = 0;
    b = 255;
    break;
  default:
    r = 255;
    g = 0;
    b = 255 - lo;
    break;
  }

  s1 = sat + 1;
  r = 255 - (((255 - r) * s1) >> 8);
  g = 255 - (((255 - g) * s1) >> 8);
  b = 255 - (((255 - b) * s1) >> 8);

  v1 = val + 1;
  if (gflag) {
    r = pgm_read_byte(&gamma_table[(r * v1) >> 8]);
    g = pgm_read_byte(&gamma_table[(g * v1) >> 8]);
    b = pgm_read_byte(&gamma_table[(b * v1) >> 8]);
  } else {
    r = (r * v1) >> 12;
    g = (g * v1) >> 12;
    b = (b * v1) >> 12;
  }
  return (r << 12) | ((r & 0x8) << 8) | (g << 7) | ((g & 0xC) << 3) |
         (b << 1) | (b >> 3);
}

void RGBmatrixPanel::DrawString_CN(uint8_t Xstart, uint8_t Ystart,
                                   const char *pString, cFONT *font,
                                   uint16_t color) {
  uint8_t bit, shift_x = 0, shift_y;
  uint16_t arr_sum;
  int x = Xstart, y = Ystart;
  const unsigned char *p_text = (const unsigned char *)pString;
  while (*p_text != 0) {
    for (int Num = 0; Num < font->size; Num++) {
      if ((*p_text == pgm_read_byte(&font->table[Num].index[0])) &&
          (*(p_text + 1) == pgm_read_byte(&font->table[Num].index[1])) &&
          (*(p_text + 2) == pgm_read_byte(&font->table[Num].index[2]))) {

        if (font->Width > 63) {
          shift_y = 3;
          arr_sum = 512;
        } else if (font->Width > 31) {
          shift_y = 2;
          arr_sum = 128;
        } else {
          shift_y = 1;
          arr_sum = 32;
        }

        for (uint16_t i = 0; i < arr_sum; i++) {
          bit = 0x01;
          if (shift_y == 2) {
            switch (i % 4) {
            case 0:
              shift_x = 0;
              break;
            case 2:
              shift_x = 16;
              break;
            default:
              break;
            }
          } else if (shift_y == 3) {
            switch (i % 8) {
            case 0:
              shift_x = 0;
              break;
            case 2:
              shift_x = 16;
              break;
            case 4:
              shift_x = 32;
              break;
            case 6:
              shift_x = 48;
              break;
            default:
              break;
            }
          }

          if (i % 2 == 0) {
            for (int j = 7; j > -1; j--) {
              if (bit & pgm_read_byte(&font->table[Num].matrix[i])) {
                drawPixel(x + j + shift_x, y + (i >> shift_y), color);
              }
              bit <<= 1;
            }
          } else {
            for (int j = 7; j > -1; j--) {
              if (bit & pgm_read_byte(&font->table[Num].matrix[i])) {
                drawPixel(x + j + 8 + shift_x, y + (i >> shift_y), color);
              }
              bit <<= 1;
            }
          }
        }
        break;
      }
    }
    p_text += 3;
    x += font->Width;
  }
}

void RGBmatrixPanel::drawPixel(int16_t x, int16_t y, uint16_t c) {
  uint8_t r, g, b, bit, limit, *ptr;

  if ((x < 0) || (x >= _width) || (y < 0) || (y >= _height))
    return;

  switch (rotation) {
  case 1:
    _swap_int16_t(x, y);
    x = WIDTH - 1 - x;
    break;
  case 2:
    x = WIDTH - 1 - x;
    y = HEIGHT - 1 - y;
    break;
  case 3:
    _swap_int16_t(x, y);
    y = HEIGHT - 1 - y;
    break;
  }

  r = c >> 12;
  g = (c >> 7) & 0xF;
  b = (c >> 1) & 0xF;

  bit = 2;
  limit = 1 << nPlanes;

  if (y < nRows) {
    ptr = &matrixbuff[backindex][y * WIDTH * (nPlanes - 1) + x];
    ptr[WIDTH * 2] &= ~0b00000011;
    if (r & 1)
      ptr[WIDTH * 2] |= 0b00000001;
    if (g & 1)
      ptr[WIDTH * 2] |= 0b00000010;
    if (b & 1)
      ptr[WIDTH] |= 0b00000001;
    else
      ptr[WIDTH] &= ~0b00000001;
    for (; bit < limit; bit <<= 1) {
      *ptr &= ~0b00011100;
      if (r & bit)
        *ptr |= 0b00000100;
      if (g & bit)
        *ptr |= 0b00001000;
      if (b & bit)
        *ptr |= 0b00010000;
      ptr += WIDTH;
    }
  } else {
    ptr = &matrixbuff[backindex][(y - nRows) * WIDTH * (nPlanes - 1) + x];
    *ptr &= ~0b00000011;
    if (r & 1)
      ptr[WIDTH] |= 0b00000010;
    else
      ptr[WIDTH] &= ~0b00000010;
    if (g & 1)
      *ptr |= 0b00000001;
    if (b & 1)
      *ptr |= 0b00000010;
    for (; bit < limit; bit <<= 1) {
      *ptr &= ~0b11100000;
      if (r & bit)
        *ptr |= 0b00100000;
      if (g & bit)
        *ptr |= 0b01000000;
      if (b & bit)
        *ptr |= 0b10000000;
      ptr += WIDTH;
    }
  }
}

void RGBmatrixPanel::fillScreen(uint16_t c) {
  if ((c == 0x0000) || (c == 0xffff)) {
    memset(matrixbuff[backindex], c, WIDTH * nRows * 3);
  } else {
    Adafruit_GFX::fillScreen(c);
  }
}

uint8_t *RGBmatrixPanel::backBuffer() { return matrixbuff[backindex]; }

void RGBmatrixPanel::swapBuffers(boolean copy) {
  if (matrixbuff[0] != matrixbuff[1]) {
    swapflag = true;
    while (swapflag == true)
      delay(1);
    if (copy == true)
      memcpy(matrixbuff[backindex], matrixbuff[1 - backindex],
             WIDTH * nRows * 3);
  }
}

void RGBmatrixPanel::dumpMatrix(void) {
  int i, buffsize = WIDTH * nRows * 3;

  Serial.print(F("\n\n"
                 "static const uint8_t img[] = {\n  "));

  for (i = 0; i < buffsize; i++) {
    Serial.print(F("0x"));
    if (matrixbuff[backindex][i] < 0x10)
      Serial.write('0');
    Serial.print(matrixbuff[backindex][i], HEX);
    if (i < (buffsize - 1)) {
      if ((i & 7) == 7)
        Serial.print(F(",\n  "));
      else
        Serial.write(',');
    }
  }
  Serial.println(F("\n};"));
}

void RGBmatrixPanel::display_image(int16_t x, int16_t y,
                                   const uint16_t bitmap[], int16_t w,
                                   int16_t h) {
  Adafruit_GFX::drawRGBBitmap(x, y, bitmap, w, h);
}

void RGBmatrixPanel::setFont(const GFXfont *f) { Adafruit_GFX::setFont(f); }

// -------------------- Display refresh (runs on core1) --------------------

// BCM timing constants (in microseconds for RP2040/RP2350)
// Tuned for ~200Hz refresh at 16 rows:
//   Plane 0: ~20us, Plane 1: ~42us, Plane 2: ~86us, Plane 3: ~174us
//   Total per row: ~322us, 16 rows = ~5.2ms/frame = ~194Hz
#define CALLOVERHEAD 2
#define LOOPTIME 18

void RGBmatrixPanel::updateDisplay(void) {
  uint8_t i, *ptr;
  uint16_t t, duration;

  // Disable LED output during row/plane switchover
  gpio_set_mask_fast(1ul << _oe);
  // Latch data loaded during prior call
  gpio_set_mask_fast(1ul << _lat);

  // Calculate display duration for BCM timing
  t = (nRows > 8) ? LOOPTIME : (LOOPTIME * 2);
  duration = ((t + CALLOVERHEAD * 2) << plane) - CALLOVERHEAD;

  if (++plane >= nPlanes) {
    plane = 0;
    if (++row >= nRows) {
      row = 0;
      if (swapflag == true) {
        backindex = 1 - backindex;
        swapflag = false;
      }
      buffptr = matrixbuff[1 - backindex];
    }
  } else if (plane == 1) {
    // Update row address lines
    if (row & 0x1)
      gpio_set_mask_fast(1ul << _a);
    else
      gpio_clr_mask_fast(1ul << _a);
    if (row & 0x2)
      gpio_set_mask_fast(1ul << _b);
    else
      gpio_clr_mask_fast(1ul << _b);
    if (row & 0x4)
      gpio_set_mask_fast(1ul << _c);
    else
      gpio_clr_mask_fast(1ul << _c);
    if (nRows > 8) {
      if (row & 0x8)
        gpio_set_mask_fast(1ul << _d);
      else
        gpio_clr_mask_fast(1ul << _d);
    }
    if (nRows > 16) {
      if (row & 0x10)
        gpio_set_mask_fast(1ul << _e);
      else
        gpio_clr_mask_fast(1ul << _e);
    }
  }

  ptr = (uint8_t *)buffptr;

  // Re-enable output and latch down
  gpio_clr_mask_fast(1ul << _oe);
  gpio_clr_mask_fast(1ul << _lat);

  if (plane > 0) {
    // Planes 1-3: data can be sent directly through expand table
    for (i = 0; i < WIDTH; i++) {
      gpio_clr_mask_fast(rgbclkmask);    // Clear all data and clock
      gpio_set_mask_fast(expand[*ptr++]); // Set RGB data
      gpio_set_mask_fast(clkmask);        // Clock high
    }
    gpio_clr_mask_fast(clkmask); // Clock low
    buffptr = ptr;
  } else {
    // Plane 0: data is packed into the 2 least significant bits
    for (i = 0; i < WIDTH; i++) {
      uint8_t b = (ptr[i] << 6) | ((ptr[i + WIDTH] << 4) & 0x30) |
                  ((ptr[i + WIDTH * 2] << 2) & 0x0C);
      gpio_clr_mask_fast(rgbclkmask);
      gpio_set_mask_fast(expand[b]);
      gpio_set_mask_fast(clkmask);
    }
    gpio_clr_mask_fast(clkmask);
  }

  // BCM delay: wait proportional to bit weight before next plane/row
  // On core1 we can freely busy-wait without blocking the main program
  delayMicroseconds(duration);
}
