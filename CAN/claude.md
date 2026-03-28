# CAN Bus Protocol Educational Demo - Project Context

## Project Overview

This is an **educational CAN bus simulation** for the **PIC32CM3204GV00048** microcontroller on the APP-MASTERS25-1 development board. It simulates an interactive vehicle ECU with throttle control, acceleration, braking, and real-time dashboard display.

**Key Point:** This is a **simulation only** - the PIC32CM3204GV00048 does NOT have a built-in CAN controller. The board has an ATA6561 CAN transceiver but no controller to generate actual CAN frames.

### Hardware Platform
- **MCU:** PIC32CM3204GV00048 (32-bit ARM Cortex-M0+)
- **IDE:** MPLAB X IDE with XC32 compiler
- **Code Generator:** MCC Melody (Microchip Code Configurator)
- **Clock:** 8 MHz internal oscillator (OSC8M)
- **Development Board:** APP-MASTERS25-1

## Architecture

### Component Hierarchy
```
main.c (application logic)
  ├── OLED128x64.c (display driver, included directly)
  ├── src/config/default/peripheral/
  │   ├── sercom/i2c_master/ (OLED I2C - SERCOM2)
  │   ├── sercom/usart/ (UART debug - SERCOM0)
  │   └── adc/ (Potentiometer input)
  └── src/config/default/definitions.h (MCC-generated HAL)
```

### Key Files
- **`src/main.c`** - All application logic (CAN simulation, vehicle dynamics, display)
- **`src/OLED128x64.h`** - OLED driver interface
- **`src/OLED128x64.c`** - SSD1306 128x64 OLED driver implementation
- **`src/config/default/`** - MCC Melody generated peripheral libraries
- **`README.md`** - Complete hardware setup and usage guide
- **`doc/README_CAN_4_labs_arrangment.md`** - CAN protocol teaching guide for instructors

## Critical Design Patterns

### 1. Single-File Application Architecture
All application logic is in `src/main.c` for educational clarity. This is intentional - students can see the complete flow without jumping between files.

### 2. Active-Low LED Convention
**CRITICAL:** LEDs on this board are active-low:
```c
#define LED1_ON()   LED1_Clear()  // LOW = LED ON
#define LED1_OFF()  LED1_Set()    // HIGH = LED OFF
```
**Never** write `LED1_Set()` expecting the LED to turn on!

### 3. OLED Display Integration
The OLED driver (`OLED128x64.c`) is included directly in `main.c`:
```c
#define OLED_STANDALONE 1
#include "OLED128x64.c"
```
This avoids linking issues with MCC-generated code. **Do not** try to compile OLED128x64.c separately.

### 4. Timing Implementation
Uses software delays calibrated for 8 MHz CPU:
```c
#define CPU_FREQ 8000000UL
delay_us(uint32_t us)  // Busy-wait microsecond delay
delay_ms(uint32_t ms)  // Busy-wait millisecond delay
```
**Do not** use hardware timers - they conflict with MCC's peripheral usage.

### 5. Throttle-Based Acceleration Model
The potentiometer (throttle) **only affects acceleration rate when SW1 is held**:
- Throttle 80-100%: +8 km/h per update
- Throttle 60-79%: +6 km/h per update
- Throttle 40-59%: +4 km/h per update
- Throttle 10-39%: +2 km/h per update
- Throttle 0-9%: No acceleration (idle)

This mimics real vehicle behavior where throttle controls engine power, not speed directly.

## Hardware Configuration (MCC Melody)

### Pin Assignments - CRITICAL GOTCHAS

#### ⚠️ OLED I2C Pins - MOST COMMON MISTAKE
```
PA12 = SERCOM2_PAD0 (SDA)
PA13 = SERCOM2_PAD1 (SCL)
```
**MUST be configured as SERCOM2_PAD0/PAD1 in MCC Pin Manager, NOT GPIO!**

If set as GPIO with pull-ups, the I2C peripheral cannot control the pins and initialization will hang forever.

#### ⚠️ PB11 - DO NOT CONFIGURE
Leave PB11 (Pin 20) as "Available" in MCC. Any GPIO configuration on PB11 causes the WS2812B addressable LEDs to light up bright white due to hardware quirks.

#### ⚠️ Button Input Configuration
SW1 (PB10) and SW2 (PA15) require:
- Direction = Input
- Input Enable (INEN) = Checked
- Pull Up Enable = Checked

Without pull-ups enabled, buttons will not work reliably.

#### ⚠️ RGB1 LED (Common Cathode)
RGB1 is a **common-cathode** RGB LED:
```
PB09 (PWM1) = RED   - HIGH = ON
PA04 (PWM2) = GREEN - HIGH = ON
PA05 (PWM3) = BLUE  - HIGH = ON
```
Opposite behavior from the active-low discrete LEDs!

### Peripheral Configuration

#### UART (SERCOM0)
- **Baud:** 115200
- **Pins:** PA08 (TX), PA09 (RX)
- **Purpose:** Debug output and CAN frame explanation

#### ADC
- **Channel:** AIN1 (PA03)
- **Resolution:** 12-bit
- **Reference:** 1/2 VDDANA
- **Mode:** Free-running
- **Purpose:** Read potentiometer for throttle control

#### I2C (SERCOM2)
- **Speed:** 100 kHz
- **Pins:** PA12 (SDA), PA13 (SCL)
- **Device:** SSD1306 OLED (address 0x3C)

## Coding Conventions

### 1. Printf-style Debugging
Use custom `print()` and `println()` functions that wrap UART:
```c
print("Speed: ");
println("km/h");
```
**Never** include `<stdio.h>` or use standard `printf()` - it adds ~20KB of code.

### 2. String Formatting
Use custom `int_to_str()` and `hex_to_str()` functions for number formatting:
```c
int_to_str(rpm, buf, 4);  // Right-aligned, space-padded
hex_to_str(crc, buf, 4);  // Uppercase hex, 4 digits
```

### 3. No Dynamic Memory
**Never** use `malloc()`, `free()`, or dynamic allocation. All buffers are stack-allocated:
```c
char oled_buf[24];  // Stack buffer for OLED text
```

### 4. Configuration Flags
Use `#define` flags for optional features:
```c
#define BUZZER_ENABLED 0  // Set to 1 to enable buzzer feedback
```
This allows disabling annoying features during development.

## Common Tasks

### Adding a New LED Indicator
1. Verify pin is configured as GPIO Output in MCC
2. Remember active-low convention:
   ```c
   #define MYNEW_LED_ON()  MYNEW_LED_Clear()
   #define MYNEW_LED_OFF() MYNEW_LED_Set()
   ```

### Adding OLED Display Elements
1. Use 6x8 font for regular text: `OLED_DrawString6x8(...)`
2. Use 8x16 font for large numbers: `OLED_DrawString8x16(...)`
3. Clear specific rows before updating: `OLED_ClearRow(row_num)`
4. Update display after all drawing: `OLED_UpdateDisplay()`

### Modifying Vehicle Dynamics
Edit constants in the main loop:
```c
// Acceleration rates (in main loop)
if (throttle_percent >= 80) speed += 8;
else if (throttle_percent >= 60) speed += 6;
// ...

// Braking rate
if (sw2_pressed) rpm -= 100; speed -= 5;

// Coast deceleration
if (!sw1_pressed && !sw2_pressed) {
    if (speed > 0) speed -= 1;  // Adjust coast rate here
}
```

### Adding CAN Frame Types
The simulation uses simple 2-byte data frames:
```c
// Current frame structure
uint16_t can_id = 0x0C0;  // CAN identifier
uint8_t data[2] = {(rpm >> 8) & 0xFF, rpm & 0xFF};

// To add new message types, use different IDs:
// 0x0C0 = Engine RPM
// 0x120 = Vehicle Speed (example for new addition)
// 0x200 = Throttle Position (example)
```

## Troubleshooting Guide

### OLED Hangs at Initialization
**Root Cause:** PA12/PA13 configured as GPIO instead of SERCOM2_PAD0/PAD1

**Solution:**
1. Open MCC Melody
2. Pin Manager → Pin Grid View
3. Set PA12 = SERCOM2_PAD0, PA13 = SERCOM2_PAD1
4. Regenerate code
5. Rebuild and flash

### Buttons Not Responding
**Root Cause:** Input enable or pull-ups not configured

**Solution:**
1. MCC → Pins → PB10 (SW1) and PA15 (SW2)
2. Check "Input Enable" is enabled
3. Check "Pull Up Enable" is checked
4. Regenerate code

### WS2812B LEDs Stay Bright White
**Root Cause:** PB11 accidentally configured as GPIO

**Solution:**
1. MCC → Pin Manager → PB11
2. Set to "Available" (no function)
3. Regenerate code
4. Erase and reflash board

### RGB1 LED Not Changing Colors
**Root Cause:** RGB pins not configured, or wrong polarity assumption

**Verification:**
- PB09, PA04, PA05 must be GPIO Output in MCC
- RGB1 is common-cathode: HIGH = color ON
- Check `rgb_set_color()` function logic

### Serial Output Missing
**Common Issues:**
1. Wrong COM port - must use MCP2221 port
2. Wrong baud rate - must be 115200
3. UART not initialized - check `SERCOM0_USART_Write()` is called

## Testing Strategy

### Hardware Self-Test Sequence
The startup sequence tests all peripherals:
```c
1. All LEDs flash ON → OFF
2. RGB cycles: RED → GREEN → BLUE
3. Buzzer beeps (if enabled)
4. ADC reads potentiometer (shows percentage)
5. OLED displays splash screen
```
If any test fails, the system reports the failure over UART.

### Manual Testing Checklist
- [ ] SW1 held: Speed increases based on throttle position
- [ ] SW2 held: Speed decreases, LED4 ON, RGB turns RED
- [ ] Potentiometer turn: Throttle percentage changes on OLED
- [ ] LED1 blinks when CAN frame transmitted
- [ ] LED3 ON when RPM > 5500
- [ ] RGB color changes based on speed ranges
- [ ] OLED updates every 200ms with current values

## Build System

### Makefile
Located at `CAN_lab.X/Makefile` - generated by MPLAB X IDE.
**Do not** edit manually unless you know what you're doing.

### Build Commands
```bash
# From project root
cd CAN_lab.X
make build         # Compile project
make clean         # Clean build artifacts
```

### Common Build Errors
1. **"undefined reference to OLED_*"**: OLED128x64.c included incorrectly
2. **"multiple definition of OLED_*"**: OLED file compiled separately (don't do this)
3. **Missing MCC files**: Regenerate code in MCC Melody

## Version Control Notes

### Current Branch
Main branch: `main`

### Ignore Patterns
MCC Melody auto-generates files. These should be tracked:
- `src/config/default/**/*.c`
- `src/config/default/**/*.h`
- `CAN_lab.X/nbproject/Makefile-*.mk`

These change frequently and can be ignored:
- `CAN_lab.X/build/`
- `CAN_lab.X/dist/`
- `CAN_lab.X/debug/`
- `CAN_lab.X/.generated_files/`
- `*.o`, `*.d`, `*.elf`, `*.hex`

### Recent Changes
Latest commit focuses on throttle-based acceleration model where potentiometer only affects speed when SW1 (accelerate button) is held, making it behave like a real vehicle.

## Educational Context

This project is designed for:
- **Students:** Learning embedded systems, CAN protocol, vehicle dynamics
- **Instructors:** Teaching automotive networking and real-time systems
- **Engineers:** Understanding CAN frame structure and vehicle ECU behavior

The documentation in `doc/README_CAN_4_labs_arrangment.md` provides a teaching guide for instructors with hardware protocol backgrounds (chip design, verification) to teach CAN concepts.

## External Resources

- **PIC32CM3204 Datasheet:** https://www.microchip.com/PIC32CM3204
- **MCC Melody User Guide:** https://developerhelp.microchip.com/xwiki/bin/view/software-tools/harmony/mcc-harmony-intro/
- **SSD1306 OLED Datasheet:** Common 128x64 I2C OLED controller
- **CAN Protocol Specification:** ISO 11898-1 (available online)

## License & Attribution

Educational project for learning purposes. Feel free to modify and extend for teaching or personal learning.

**Co-Authored-By:** Students and instructors using this demo should understand the code thoroughly before modifications.

---

**Last Updated:** 2026-01-08
**Project Status:** Production-ready main branch
