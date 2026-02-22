#include "RGBmatrixPanel.h"
#include "fonts.h"
#include "gb_image.h"
#include <string.h>
#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_AHTX0.h>

RTC_DS1307 rtc;
Adafruit_AHTX0 aht;

// RTC + AHT20 share I2C1: physical pin 31 = GP26 (SDA), physical pin 32 = GP27 (SCL)
#define RTC_SDA_PIN 26
#define RTC_SCL_PIN 27

// Pin definitions for PICO 2 RP2350
#define R1_PIN  0
#define G1_PIN  1
#define B1_PIN  2
#define R2_PIN  3
#define G2_PIN  4
#define B2_PIN  5
#define A_PIN   6
#define B_PIN   7
#define C_PIN   8
#define D_PIN   9
#define CLK_PIN 10
#define LAT_PIN 11
#define OE_PIN  12

uint8_t rgbpins[] = {R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN};

RGBmatrixPanel matrix(A_PIN, B_PIN, C_PIN, D_PIN, CLK_PIN, LAT_PIN, OE_PIN, false, 64, rgbpins);

// ---- Temperature history logging ----
#define TEMP_LOG_INTERVAL_S  10   // seconds between readings — change as needed
float         tempLog[64];        // circular buffer: up to 64 readings
int           tempLogHead  = 0;
int           tempLogCount = 0;
unsigned long lastTempLogMs = 0;

// ---- Today's temperature high / low / mean (resets at midnight) ----
float todayHigh  = -999.0f;
float todayLow   =  999.0f;
float todaySum   =    0.0f;
int   todayCount =       0;
int   lastLogDay =      -1;  // -1 forces reset on first read

// ---- Today's humidity high / low / mean (resets at midnight alongside temp) ----
float humidHigh  = -999.0f;
float humidLow   =  999.0f;
float humidSum   =    0.0f;
int   humidCount =       0;

void logTempReading() {
  sensors_event_t h, t;
  aht.getEvent(&h, &t);
  float temp  = t.temperature;
  float humid = h.relative_humidity;

  // Rolling buffer
  tempLog[tempLogHead] = temp;
  tempLogHead = (tempLogHead + 1) % 64;
  if (tempLogCount < 64) tempLogCount++;

  // Daily stats — reset both when the calendar day changes
  int today = rtc.now().day();
  if (today != lastLogDay) {
    todayHigh  = temp;   todayLow  = temp;  todaySum  = temp;  todayCount = 1;
    humidHigh  = humid;  humidLow  = humid; humidSum  = humid; humidCount = 1;
    lastLogDay = today;
  } else {
    if (temp  > todayHigh) todayHigh = temp;
    if (temp  < todayLow)  todayLow  = temp;
    todaySum  += temp;   todayCount++;
    if (humid > humidHigh) humidHigh = humid;
    if (humid < humidLow)  humidLow  = humid;
    humidSum  += humid;  humidCount++;
  }

  Serial.print("[TempLog] ");
  Serial.print(temp, 1);
  Serial.print(" C  humidity ");
  Serial.print(humid, 1);
  Serial.print("%  (sample ");
  Serial.print(tempLogCount);
  Serial.println("/64)");
}

// Call anywhere in the main loop — logs only when the interval has elapsed
void logIfDue() {
  if (millis() - lastTempLogMs >= (unsigned long)TEMP_LOG_INTERVAL_S * 1000UL) {
    logTempReading();
    lastTempLogMs = millis();
  }
}

void setup()
{
  Serial.begin(115200);
  Reginit();
  matrix.begin();

  // Initialize I2C1 for RTC and AHT20 on GP26 (SDA) / GP27 (SCL)
  Wire1.setSDA(RTC_SDA_PIN);
  Wire1.setSCL(RTC_SCL_PIN);
  Wire1.begin();
  rtc.begin(&Wire1);
  aht.begin(&Wire1);

  // Uncomment ONCE to set the RTC to compile time, then re-comment and re-upload:
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  delay(500);
}

void loop()
{
  logIfDue();
  demo_rainbowScroll();
  demo_scrollClock();         // enters from right, exits to top
  demo_scrollDate();          // enters from bottom, exits to top
  demo_scrollTemp();          // enters from right, exits to top
  demo_scrollTempStats();     // enters from bottom, exits to top
  demo_scrollHumidity();      // enters from bottom, exits to top
  demo_scrollHumidityStats(); // enters from bottom, exits to top
  demo_showGBImage();          // full-screen image, holds 4 s
}

// ---- Helper: draw text ----
void display_text(int x, int y, const char *str, const GFXfont *f, int color, int pixels_size)
{
  matrix.setTextSize(pixels_size);
  matrix.setTextWrap(false);
  matrix.setFont(f);
  matrix.setCursor(x, y);
  matrix.setTextColor(color);
  matrix.println(str);
}

// ---- Rainbow scrolling text ----
void demo_rainbowScroll()
{
  const char *msg = "Welcome to my World!";
  int msgWidth = strlen(msg) * 12;
  uint16_t hue = 0;

  for (int x = 64; x > -msgWidth; x--) {
    matrix.fillScreen(0);
    matrix.setTextSize(2);
    matrix.setTextWrap(false);
    matrix.setFont(NULL);
    for (int i = 0; msg[i] != '\0'; i++) {
      int charX = x + i * 12;
      if (charX > -12 && charX < 64) {
        uint16_t h = (hue + i * 800) % 6000;
        uint8_t r, g, b;
        int sector = h / 1000;
        int frac = (h % 1000) * 255 / 1000;
        switch (sector) {
          case 0: r = 255; g = frac;       b = 0;          break;
          case 1: r = 255 - frac; g = 255; b = 0;          break;
          case 2: r = 0;   g = 255;        b = frac;       break;
          case 3: r = 0;   g = 255 - frac; b = 255;        break;
          case 4: r = frac; g = 0;         b = 255;        break;
          default: r = 255; g = 0;         b = 255 - frac; break;
        }
        matrix.setCursor(charX, 8);  // vertically centred: (32-16)/2 = 8
        matrix.setTextColor(matrix.Color888(r, g, b, true));
        matrix.print(msg[i]);
      }
    }
    hue = (hue + 40) % 6000;
    delay(20);
  }
}

// ---- Clock: draw helper + scroll in from right / hold / scroll out to top ----
void drawClock(int offsetX, int offsetY)
{
  char buf[16];
  DateTime now = rtc.now();
  matrix.fillScreen(0);
  // Flash the colon every 0.5 s
  bool colonOn = (millis() / 500) % 2 == 0;
  sprintf(buf, "%02d%c%02d", now.hour(), colonOn ? ':' : ' ', now.minute());
  display_text(2 + offsetX, 1 + offsetY, buf, NULL, matrix.Color333(0, 7, 7), 2);
  sprintf(buf, ":%02d", now.second());
  display_text(23 + offsetX, 20 + offsetY, buf, NULL, matrix.Color333(0, 4, 4), 1);
}

void demo_scrollClock()
{
  for (int ox = 64; ox >= 0; ox -= 2)  { drawClock(ox, 0); delay(20); }
  unsigned long start = millis();
  while (millis() - start < 4000)      { drawClock(0, 0); logIfDue(); delay(100); }
  for (int oy = 0; oy >= -32; oy -= 2) { drawClock(0, oy); delay(20); }
}

// ---- Date: draw helper + scroll in from bottom / out to top ----
// Date line: day(12px) + 5px + month(18px) + 5px + year(24px) = 64px exactly
void drawDate(int offsetY, const char *dayName, int dayNameX,
              const char *dayStr, const char *monthStr, const char *yearStr)
{
  matrix.fillScreen(0);
  display_text(dayNameX, 4  + offsetY, dayName,  NULL, matrix.Color333(7, 5, 0), 1);
  display_text(0,        18 + offsetY, dayStr,   NULL, matrix.Color333(0, 7, 0), 1);
  display_text(17,       18 + offsetY, monthStr, NULL, matrix.Color333(0, 7, 0), 1);
  display_text(40,       18 + offsetY, yearStr,  NULL, matrix.Color333(0, 7, 0), 1);
}

void demo_scrollDate()
{
  const char *monthNames[] = {
    "", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };
  const char *dayNames[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"
  };

  DateTime now = rtc.now();
  int dow = now.dayOfTheWeek();
  int dayNameX = (64 - (int)strlen(dayNames[dow]) * 6) / 2;

  char dayStr[4], monthStr[5], yearStr[6];
  sprintf(dayStr,   "%02d", now.day());
  sprintf(monthStr, "%s",   monthNames[now.month()]);
  sprintf(yearStr,  "%04d", now.year());

  for (int oy = 32; oy >= 0; oy -= 2)  { drawDate(oy, dayNames[dow], dayNameX, dayStr, monthStr, yearStr); delay(20); }
  delay(4000);
  for (int oy = 0; oy >= -32; oy -= 2) { drawDate(oy, dayNames[dow], dayNameX, dayStr, monthStr, yearStr); delay(20); }
}

// ---- Temperature: draw helper + scroll in from right / hold / scroll out to top ----
void drawTemp(int offsetX, int offsetY, float tempVal)
{
  matrix.fillScreen(0);
  int labelX = (64 - 11 * 6) / 2;  // "Temperature" 11 chars — 1px clip each side
  display_text(labelX + offsetX, 1 + offsetY, "Temperature", NULL, matrix.Color333(7, 3, 0), 1);

  char numBuf[10];
  sprintf(numBuf, "%.1f", tempVal);
  int numWidth   = strlen(numBuf) * 12;
  int totalWidth = numWidth + 6 + 12;
  int startX     = (64 - totalWidth) / 2;
  int degX       = startX + numWidth;
  int cX         = degX + 6;

  display_text(startX + offsetX, 14 + offsetY, numBuf, NULL, matrix.Color333(7, 7, 0), 2);
  matrix.drawCircle(degX + offsetX + 2, 16 + offsetY, 2, matrix.Color333(7, 7, 0));
  display_text(cX + offsetX, 14 + offsetY, "C", NULL, matrix.Color333(7, 7, 0), 2);
}

void demo_scrollTemp()
{
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);
  float tempVal = temp.temperature;

  for (int ox = 64; ox >= 0; ox -= 2)  { drawTemp(ox, 0, tempVal); delay(20); }
  delay(4000);
  for (int oy = 0; oy >= -32; oy -= 2) { drawTemp(0, oy, tempVal); delay(20); }
}

// ---- Temperature stats: today's high, low, mean — scroll in from bottom / out to top ----
// Degree drawn as a radius-1 circle at top-left of C (same technique as drawTemp).
void drawTempStatLine(int x, int y, const char *label, float val, uint16_t col) {
  char buf[12];
  sprintf(buf, "%s%.1f", label, val);
  int w = strlen(buf) * 6;            // size-1: 6px per char
  display_text(x,         y, buf, NULL, col, 1);
  matrix.drawCircle(x + w + 1, y + 1, 1, col);  // 3px circle at top of C
  display_text(x + w + 4, y, "C",  NULL, col, 1);
}

void drawTempStats(int offsetY) {
  matrix.fillScreen(0);

  if (todayCount == 0) {
    display_text(4, 12 + offsetY, "No data yet", NULL, matrix.Color333(3, 3, 3), 1);
    return;
  }

  float meanT = todaySum / todayCount;
  drawTempStatLine(2, 3  + offsetY, "Hi: ", todayHigh, matrix.Color333(7, 3, 0));
  drawTempStatLine(2, 13 + offsetY, "Lo: ", todayLow,  matrix.Color333(0, 4, 7));
  drawTempStatLine(2, 23 + offsetY, "Av: ", meanT,     matrix.Color333(7, 7, 0));
}

void demo_scrollTempStats() {
  for (int oy = 32; oy >= 0; oy -= 2)  { drawTempStats(oy); delay(20); }
  delay(4000);
  for (int oy = 0; oy >= -32; oy -= 2) { drawTempStats(oy); delay(20); }
}

// ---- Humidity: draw helper + scroll in from bottom / hold / scroll out to top ----
void drawHumidity(int offsetY, const char *valLine)
{
  matrix.fillScreen(0);
  int labelX = (64 - 8 * 6) / 2;  // "Humidity" = 8 chars * 6px
  int valX   = (64 - (int)strlen(valLine) * 12) / 2;
  display_text(labelX, 3  + offsetY, "Humidity", NULL, matrix.Color333(0, 3, 7), 1);
  display_text(valX,   14 + offsetY, valLine,    NULL, matrix.Color333(0, 7, 7), 2);
}

void demo_scrollHumidity()
{
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);

  char valLine[10];
  sprintf(valLine, "%.1f%%", humidity.relative_humidity);

  for (int oy = 32; oy >= 0; oy -= 2)  { drawHumidity(oy, valLine); delay(20); }
  delay(4000);
  for (int oy = 0; oy >= -32; oy -= 2) { drawHumidity(oy, valLine); delay(20); }
}

// ---- Humidity stats: today's high, low, mean — scroll in from bottom / out to top ----
void drawHumidityStats(int offsetY) {
  matrix.fillScreen(0);

  if (humidCount == 0) {
    display_text(4, 12 + offsetY, "No data yet", NULL, matrix.Color333(3, 3, 3), 1);
    return;
  }

  float meanH = humidSum / humidCount;
  char buf[16];

  sprintf(buf, "Hi: %.1f%%", humidHigh);
  display_text(2, 3  + offsetY, buf, NULL, matrix.Color333(0, 7, 7), 1);

  sprintf(buf, "Lo: %.1f%%", humidLow);
  display_text(2, 13 + offsetY, buf, NULL, matrix.Color333(0, 3, 6), 1);

  sprintf(buf, "Av: %.1f%%", meanH);
  display_text(2, 23 + offsetY, buf, NULL, matrix.Color333(7, 7, 7), 1);
}

void demo_scrollHumidityStats() {
  for (int oy = 32; oy >= 0; oy -= 2)  { drawHumidityStats(oy); delay(20); }
  delay(4000);
  for (int oy = 0; oy >= -32; oy -= 2) { drawHumidityStats(oy); delay(20); }
}

// ---- GB image: 64x32 RGB565 bitmap, display for 4 seconds ----
void demo_showGBImage()
{
  matrix.fillScreen(0);
  for (int y = 0; y < 32; y++) {
    for (int x = 0; x < 64; x++) {
      matrix.drawPixel(x, y, gb_image[y * 64 + x]);
    }
  }
  delay(4000);
}

void Reginit()
{
    pinMode(R1_PIN, OUTPUT);
    pinMode(G1_PIN, OUTPUT);
    pinMode(B1_PIN, OUTPUT);
    pinMode(R2_PIN, OUTPUT);
    pinMode(G2_PIN, OUTPUT);
    pinMode(B2_PIN, OUTPUT);
    pinMode(CLK_PIN, OUTPUT);
    pinMode(OE_PIN, OUTPUT);
    pinMode(LAT_PIN, OUTPUT);

    digitalWrite(OE_PIN, HIGH);
    digitalWrite(LAT_PIN, LOW);
    digitalWrite(CLK_PIN, LOW);
    int MaxLed = 64;

    int C12[16] = {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    int C13[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0};

    for (int l = 0; l < MaxLed; l++)
    {
        int y = l % 16;
        digitalWrite(R1_PIN, LOW); digitalWrite(G1_PIN, LOW); digitalWrite(B1_PIN, LOW);
        digitalWrite(R2_PIN, LOW); digitalWrite(G2_PIN, LOW); digitalWrite(B2_PIN, LOW);
        if (C12[y] == 1) {
          digitalWrite(R1_PIN, HIGH); digitalWrite(G1_PIN, HIGH); digitalWrite(B1_PIN, HIGH);
          digitalWrite(R2_PIN, HIGH); digitalWrite(G2_PIN, HIGH); digitalWrite(B2_PIN, HIGH);
        }
        digitalWrite(LAT_PIN, l > MaxLed - 12 ? HIGH : LOW);
        digitalWrite(CLK_PIN, HIGH); delayMicroseconds(2); digitalWrite(CLK_PIN, LOW);
    }
    digitalWrite(LAT_PIN, LOW);

    for (int l = 0; l < MaxLed; l++)
    {
        int y = l % 16;
        digitalWrite(R1_PIN, LOW); digitalWrite(G1_PIN, LOW); digitalWrite(B1_PIN, LOW);
        digitalWrite(R2_PIN, LOW); digitalWrite(G2_PIN, LOW); digitalWrite(B2_PIN, LOW);
        if (C13[y] == 1) {
            digitalWrite(R1_PIN, HIGH); digitalWrite(G1_PIN, HIGH); digitalWrite(B1_PIN, HIGH);
            digitalWrite(R2_PIN, HIGH); digitalWrite(G2_PIN, HIGH); digitalWrite(B2_PIN, HIGH);
        }
        digitalWrite(LAT_PIN, l > MaxLed - 13 ? HIGH : LOW);
        digitalWrite(CLK_PIN, HIGH); delayMicroseconds(2); digitalWrite(CLK_PIN, LOW);
    }
    digitalWrite(LAT_PIN, LOW);
    digitalWrite(CLK_PIN, LOW);
}
