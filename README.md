# RGB Matrix 64×32 P3 — Raspberry Pi Pico 2 (RP2350)

A scrolling information display for a **64×32 P3 HUB75 RGB LED matrix panel**, driven by a **Raspberry Pi Pico 2 (RP2350)**. Cycles through eight screens showing a welcome animation, clock, date, temperature, temperature statistics, humidity, humidity statistics, and a UK map image — all sourced from a DS1307 real-time clock and an AHT20 temperature/humidity sensor.

---

## Hardware Requirements

| Component | Notes |
|-----------|-------|
| Raspberry Pi Pico 2 (RP2350) | Arduino-Pico core required |
| P3 64×32 HUB75 RGB LED Matrix Panel | Any standard HUB75 64×32 panel |
| DS1307 RTC module | I²C, with CR2032 backup battery |
| AHT20 temperature & humidity sensor | I²C |
| 5 V power supply | Matrix panel can draw up to 4 A at full brightness |
| Logic level consideration | Pico 2 is 3.3 V; most HUB75 panels tolerate 3.3 V signals |

---

## Wiring

### RGB Matrix Panel → Pico 2

The panel uses a standard **HUB75** 16-pin IDC connector. Connect each HUB75 signal to the Pico 2 GPIO as follows:

| HUB75 Pin | Signal | Pico 2 GPIO | Pico 2 Physical Pin |
|-----------|--------|-------------|---------------------|
| 1 | R1 | GP0 | Pin 1 |
| 2 | G1 | GP1 | Pin 2 |
| 3 | B1 | GP2 | Pin 4 |
| 4 | GND | GND | Pin 3 / 38 |
| 5 | R2 | GP3 | Pin 5 |
| 6 | G2 | GP4 | Pin 6 |
| 7 | B2 | GP5 | Pin 7 |
| 8 | GND | GND | — |
| 9 | A | GP6 | Pin 9 |
| 10 | B | GP7 | Pin 10 |
| 11 | C | GP8 | Pin 11 |
| 12 | D | GP9 | Pin 12 |
| 13 | CLK | GP10 | Pin 14 |
| 14 | LAT/STB | GP11 | Pin 15 |
| 15 | OE | GP12 | Pin 16 |
| 16 | GND | GND | — |

> **Power:** Connect the panel's dedicated 5 V and GND power leads directly to your 5 V supply — **not** through the Pico 2. The Pico 2's 3.3 V output cannot supply enough current for the panel.

```
HUB75 Connector (rear of panel, pin 1 top-left)
┌────────────────────────────────┐
│  R1   G1   B1  GND   R2   G2  │  ← pins 1-6
│  B2  GND    A    B    C    D  │  ← pins 7-12
│ CLK  LAT   OE  GND  GND  GND  │  ← pins 13-16
└────────────────────────────────┘
```

---

### DS1307 RTC & AHT20 Sensor → Pico 2

Both devices share the **I²C1** bus on GP26/GP27. Wire them in parallel (same SDA and SCL lines).

| Signal | Pico 2 GPIO | Pico 2 Physical Pin | DS1307 Pin | AHT20 Pin |
|--------|-------------|---------------------|------------|-----------|
| SDA | GP26 | Pin 31 | SDA | SDA |
| SCL | GP27 | Pin 32 | SCL | SCL |
| 3.3 V | 3V3 OUT | Pin 36 | VCC | VIN |
| GND | GND | Pin 38 | GND | GND |

> Add **4.7 kΩ pull-up resistors** from SDA to 3.3 V and SCL to 3.3 V if your modules do not already include them.

---

## Libraries Required

Install all of the following via the Arduino Library Manager:

| Library | Purpose |
|---------|---------|
| `RGBmatrixPanel` (Adafruit) | Drives the HUB75 LED matrix |
| `Adafruit GFX Library` | Graphics primitives (dependency of above) |
| `RTClib` (Adafruit) | DS1307 real-time clock |
| `Adafruit AHTX0` | AHT20 temperature/humidity sensor |

Also required: the **Arduino-Pico** board package (Earle Philhower) for RP2350 support.

---

## First-Time Setup — Setting the RTC

The DS1307 needs to be set once after fitting a new battery or on first use.

1. Open `RGB_Matrix_64x32_P3.ino`
2. Uncomment this line in `setup()`:
   ```cpp
   // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
   ```
3. Upload the sketch — the RTC is set to the compile time automatically
4. **Re-comment the line** and upload again to prevent the clock being reset on every reboot

---

## Configuration

| Constant | Location | Default | Description |
|----------|----------|---------|-------------|
| `TEMP_LOG_INTERVAL_S` | top of sketch | `10` | Seconds between sensor readings |

---

## How the Program Works

### Startup (`setup`)

1. Starts serial output at 115200 baud
2. Calls `Reginit()` to send the two initialisation sequences required by the HUB75 panel driver IC (configures internal shift register chain length and grayscale mode)
3. Initialises the matrix panel via `matrix.begin()`
4. Starts I²C1 on GP26/GP27 and initialises the DS1307 RTC and AHT20 sensor

---

### Main Loop — Screen Sequence

The loop runs the eight screens in order, then repeats indefinitely. `logIfDue()` is called at the top of each loop iteration to take a sensor reading if the logging interval has elapsed.

#### 1. Rainbow Scroll — `demo_rainbowScroll()`
"Welcome to my World!" scrolls from right to left at size-2 text (16 px tall), vertically centred on the 32 px display. Each character is independently coloured using a smooth HSV rainbow that advances 40 hue steps per frame at 20 ms per frame.

#### 2. Clock — `demo_scrollClock()`
- **Enters** from the right (slides in 2 px per frame)
- **Holds** for 4 seconds showing the current time
- **Exits** upward (slides out 2 px per frame)

Display layout:
- Large cyan `HH:MM` (size 2, upper area) — the colon **flashes on/off every 0.5 seconds** using `millis()`
- Smaller cyan `:SS` (size 1, lower right)

Time is read live from the DS1307 on every redraw.

#### 3. Date — `demo_scrollDate()`
- **Enters** from the bottom, **exits** upward

Display layout:
- Row 1 (amber): full day name, horizontally centred — e.g. `Wednesday`
- Row 2 (green): day, abbreviated month, and 4-digit year laid out at fixed x positions to fit exactly 64 px — e.g. `22  Feb  2026`

#### 4. Temperature — `demo_scrollTemp()`
- **Enters** from the right, **exits** upward

Reads a live sample from the AHT20 at the start of the animation. Displays:
- Small amber `Temperature` label (size 1), centred at top
- Large yellow value to 1 decimal place (size 2), with a **drawn 5 px circle** as the degree symbol (to avoid font baseline artefacts) followed by `C`

#### 5. Temperature Stats — `demo_scrollTempStats()`
- **Enters** from the bottom, **exits** upward

Shows today's statistics accumulated since midnight (or since power-on on the first day):

| Line | Colour | Content |
|------|--------|---------|
| Hi | Orange | Highest reading of the day |
| Lo | Blue | Lowest reading of the day |
| Av | Yellow | Mean of all readings today |

Each line uses a **drawn 3 px circle** for the degree symbol at size-1 text. If no readings have been taken yet, "No data yet" is shown in grey.

#### 6. Humidity — `demo_scrollHumidity()`
- **Enters** from the bottom, **exits** upward

Reads a live AHT20 sample. Displays:
- Small blue `Humidity` label, centred
- Large cyan percentage value to 1 decimal place (e.g. `58.3%`)

#### 7. Humidity Stats — `demo_scrollHumidityStats()`
- **Enters** from the bottom, **exits** upward

Shows today's accumulated humidity statistics:

| Line | Colour | Content |
|------|--------|---------|
| Hi | Cyan | Highest % RH today |
| Lo | Dark blue | Lowest % RH today |
| Av | White | Mean % RH today |

#### 8. UK Map Image — `demo_showGBImage()`
Draws a full 64×32 pixel bitmap of the UK map (stored in `gb_image.h` as 2048 RGB565 `uint16_t` values). York City is highlighted with a single **red pixel**. The image is displayed statically for 4 seconds, then the loop restarts.

---

### Sensor Logging — `logTempReading()` / `logIfDue()`

`logIfDue()` is called at the top of every loop iteration. If `TEMP_LOG_INTERVAL_S` seconds have elapsed since the last reading, `logTempReading()` is called which:

1. Reads temperature and relative humidity from the AHT20
2. Stores temperature in a **64-entry circular buffer** (`tempLog[]`) for potential future use
3. Updates **daily statistics** for both temperature and humidity — detecting a day change via `rtc.now().day()` and resetting all accumulators at midnight
4. Prints a log line to the serial monitor at 115200 baud:
   ```
   [TempLog] 21.4 C  humidity 58.3%  (sample 6/64)
   ```

---

### Image Conversion

The file `gb_image.h` is generated from `uk64x32.png` using a Python script with Pillow:
- Source image scaled to 64×32 using Lanczos resampling
- Green channel boosted (×1.6), red channel slightly reduced (×0.8) to give the map a green tint
- Each pixel converted to **RGB565** (`uint16_t`): `((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)`
- York City marked with a single red pixel

To regenerate with a different image or dot position, run:
```python
from PIL import Image

img = Image.open('uk64x32.png').convert('RGB')
img = img.resize((64, 32), Image.LANCZOS)

# ... apply colour adjustments and set red dot ...

# Convert to RGB565
val = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
```

---

## File Structure

```
RGB_Matrix_64x32_P3/
├── RGB_Matrix_64x32_P3.ino   # Main sketch
├── RGBmatrixPanel.h/.cpp      # HUB75 matrix driver
├── fonts.h                    # Font data
├── gb_image.h                 # 64×32 RGB565 UK map bitmap
├── uk64x32.png                # Source image for gb_image.h
├── gb_preview.png             # Preview of the processed map image
└── README.md                  # This file
```

---

## Serial Monitor Output

Connect at **115200 baud** to see sensor readings:

```
[TempLog] 21.4 C  humidity 58.3%  (sample 1/64)
[TempLog] 21.5 C  humidity 57.9%  (sample 2/64)
```
