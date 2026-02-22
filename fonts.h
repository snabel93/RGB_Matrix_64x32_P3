#ifndef __CN_H
#define __CN_H

#include <stdint.h>

#ifdef __AVR__
#include "avr/pgmspace.h"
#else
#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#endif
#endif

#define MAX_HEIGHT_FONT         64
#define MAX_WIDTH_FONT          64


typedef struct                  // Chinese font data structure
{
       unsigned char index[3];
       const unsigned char matrix[MAX_HEIGHT_FONT*MAX_WIDTH_FONT/8];
}CH_CN;

typedef struct
{
  const CH_CN *table;
  uint16_t size;
  uint16_t Width;

}cFONT;

extern cFONT Font16CN;
extern cFONT Font32CN;

#endif
