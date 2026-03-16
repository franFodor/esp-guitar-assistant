# ESP32 Guitar Tuner

A real-time guitar tuner and chord detector built with ESP32 and an INMP441 I2S microphone. Features a web-based interface with Gruvbox dark theme for visual feedback.

## Features

- **Real-time Processing**
- **Guitar Tuning Mode**
- **Chord Detection Mode**
- **Web Interface**

## Hardware Requirements

| Component | Description |
|-----------|-------------|
| ESP32 | ESP32-S3 is used but any ESP32 board with I2S support should work|
| INMP441 | I2S MEMS microphone |
| Jumper wires | For connecting microphone to ESP32 |

### Wiring Diagram

```
INMP441    ESP32
------     -----
VDD        3.3V
GND        GND
WS         GPIO25
SCK        GPIO26
SD         GPIO33
L/R        GND
```

## How It Works

### Signal Processing Pipeline

1. **Audio Capture**: I2S microphone captures audio
2. **FFT**: FFT realised using ESP-DSP library

### Tuning Algorithm

1. **Harmonic Product Spectrum (HPS)**: Compresses spectrum at multiple harmonic ratios to find fundamental frequency
2. **Quadratic Interpolation**: Refines frequency estimate between FFT bins
3. **Octave Correction**: Validates detected frequency against harmonic relationships
4. **Note Matching**: Finds closest note in chromatic scale and calculates cents offset

### Chord Detection Algorithm

1. **Pitch Class Extraction**: Maps frequency components to pitch classes (C, C#, D, etc.)
2. **Energy Accumulation**: Aggregates pitch energy over multiple frames
3. **Template Matching**: Compares against major/minor chord templates using dot product
4. **Classification**: Returns chord with highest match score above threshold

## Installation

### Prerequisites

- ESP-IDF v5.0 or later

### Build and Flash

```bash
# Navigate to project directory
cd src

# Build the project
idf.py build

# Flash to ESP32 (replace PORT with your serial port)
# Linux: ls /dev/ttyUSB0 or /dev/ttyACM0
# macOS: ls /dev/cu.usbserial-*
idf.py -p PORT flash

# Monitor serial output
idf.py -p PORT monitor
```

You could also use the ESP-IDF extension in Visual Studio Code.

## Usage

### Tuning Mode (Default)

1. Flash the firmware to the ESP32
2. Connect to WiFi network `ESP_GUITAR_TUNER` (password: `12345678`)
3. Open browser and navigate to IP adress provided, usually `192.168.4.1`
4. Go to the Tuner page and play a string
5. The display shows detected note, frequency, and cents deviation

### Chord Detection Mode

1. Open `main/main.c` and change the mode flag:
   ```c
   #define CHORD_DETECTION_MODE  1
   ```
2. Rebuild and flash the firmware
3. Connect to the WiFi and web interface as above
4. Strum a chord and view detection in serial monitor

## References

- [FFT-based Pitch Detection](http://musicweb.ucsd.edu/~trsmyth/analysis/analysis.pdf)
- [ESP-DSP Library](https://github.com/espressif/esp-dsp)
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/)

