# CAN Bus Protocol Educational Demo

**Hardware:** PIC32CM3204GV00048 on APP-MASTERS25-1 board  
**IDE:** MPLAB X IDE + XC32 compiler + MCC Melody

---

## What You'll Learn

By the end of this lab you will be able to:

1. **Describe CAN frame structure** — SOF, Identifier, DLC, Data, CRC-15, ACK, EOF
2. **Explain CRC-15** — how polynomial error detection protects every frame
3. **Distinguish Controller vs. Transceiver** — what each chip does and why both are needed
4. **Trace data encoding** — decode RPM, speed, and throttle from raw hex bytes
5. **Build a real-time embedded UI** — peripheral integration (UART, ADC, I2C OLED, GPIO)

---

## CAN Protocol in 2 Minutes

### What Is CAN?

Controller Area Network (CAN) is a **multi-master broadcast bus** developed by Bosch in 1986 for automotive use. Every node listens to every message. There is no "master"—any node can transmit when the bus is idle.

**Physical layer:** a twisted pair of wires (CAN_H and CAN_L).  
A logical **dominant** bit drives CAN_H high and CAN_L low (differential ~2 V).  
A logical **recessive** bit leaves both wires at ~2.5 V (no drive).

### CAN Frame Structure

```
+-----+---------------+-----+-----+-----+-------+------ - ------+--------+-----+-----+
| SOF |  Identifier   | RTR | IDE |  r0 |  DLC  |     DATA      | CRC-15 | ACK | EOF |
|  1b |     11 bits   |  1b |  1b |  1b | 4 bits|  0 – 8 bytes  | 15+1 b |  2b |  7b |
+-----+---------------+-----+-----+-----+-------+---------------+--------+-----+-----+
```

| Field | Size | Meaning |
|-------|------|---------|
| **SOF** | 1 bit | Start Of Frame – dominant bit begins transmission |
| **Identifier** | 11 bits | Message ID; **lower value = higher priority** |
| **RTR** | 1 bit | 0 = data frame, 1 = remote request |
| **DLC** | 4 bits | Data Length Code – number of bytes (0–8) |
| **DATA** | 0–8 bytes | Payload |
| **CRC-15** | 15 bits | Cyclic Redundancy Check polynomial `0x4599` |
| **ACK** | 2 bits | Any receiver pulls this dominant to acknowledge |
| **EOF** | 7 bits | End Of Frame – seven recessive bits |

### What This Demo Shows

The simulation transmits three message types in rotation:

| CAN ID | Message Name | Data | Decode |
|--------|-------------|------|--------|
| `0x0C0` | `ENGINE_RPM` | 2 bytes | `RPM = (Data[0] << 8) \| Data[1]` |
| `0x0D0` | `VEHICLE_SPEED` | 1 byte | `Speed = Data[0]` (km/h) |
| `0x0F0` | `THROTTLE_BRAKE` | 2 bytes | `Throttle = Data[0]` (%), `Brake = Data[1]` (0/1) |

Each frame is printed on the terminal with the raw bytes, the CRC-15 value, and the decode formula so you can verify every number by hand.

### Why CRC-15?

The CAN CRC uses the generator polynomial **x¹⁵ + x¹⁴ + x¹⁰ + x⁸ + x⁷ + x⁴ + x³ + 1** (hex `0x4599`). It covers the ID, DLC, and all data bits. Any single-bit error or burst errors up to 15 bits are guaranteed to be detected. The code in `main.c` implements this calculation exactly:

```c
/* CAN CRC-15 – generator polynomial 0x4599 */
static uint16_t crc15(uint16_t id, uint8_t dlc, uint8_t* data) {
    uint16_t crc = 0;
    /* process ID bits (11) */
    for(int i = 10; i >= 0; i--) {
        uint8_t n = ((id >> i) & 1) ^ ((crc >> 14) & 1);
        crc = (crc << 1) & 0x7FFF;
        if(n) crc ^= 0x4599;
    }
    /* process DLC (4 bits) then each data byte */
    ...
    return crc;
}
```

---

## What Is Actually Happening (Hardware Reality)

The **PIC32CM3204GV00048 does NOT have a built-in CAN controller.** This is important to understand:

```
  Your MCU            ATA6561           CAN Bus
 (PIC32CM)          Transceiver        (twisted pair)
+----------+       +----------+
|          |  TX   |          |  CAN_H ─────────
| Software |──────>| Converts |
| simulates|       | digital  |  CAN_L ─────────
| the CAN  |<──────| to diff. |
| protocol |  RX   | voltages |
+----------+       +----------+
     ↑
     No CAN controller here!
     Protocol logic lives entirely in main.c
```

| Component | What it does | What it does NOT do |
|-----------|-------------|---------------------|
| **MCU (PIC32CM)** | Runs application code, reads sensors, drives peripherals | Generate/receive real CAN frames |
| **CAN Transceiver (ATA6561)** | Converts digital TX/RX ↔ differential CAN_H/CAN_L voltages | Understand CAN protocol |
| **CAN Controller (MCP2515)** | Handles framing, arbitration, error handling, ACK | — (not present on this board) |

**The value of this simulation:** you observe every field of every frame in real time. The CRC is computed by the same polynomial a real CAN controller uses. The data encoding (big-endian RPM, single-byte speed) matches standard automotive practice.

> To build a real CAN node, add an **MCP2515** CAN controller via SPI to pins SERCOM1 (PA12/PA13). The ATA6561 transceiver is already wired to the bus connector.

---

## Hardware Overview

| Component | MCU Pin | Function |
|-----------|---------|----------|
| **SW1** | PB10 | Accelerate (hold to speed up) |
| **SW2** | PA15 | Brake (hold to slow down) |
| **Potentiometer** | PA03 (Nano_AN2) | Throttle control — ADC AIN1 |
| **LED1** | PA00 | CAN TX indicator (blinks each frame) |
| **LED2** | PA01 | General-purpose indicator |
| **LED3** | PA10 | General-purpose indicator |
| **LED4** | PA11 | Brake light (ON when SW2 held) |
| **RGB1 LED** | PB09 / PA04 / PA05 | Speed color — PWM1=Red, PWM2=Green, PWM3=Blue |
| **Buzzer** | PA14 | Audio (disabled by default — see note) |
| **OLED Display** | PA12 / PA13 | SERCOM2 I2C, SSD1306 128×64 |
| **UART TX / RX** | PA08 / PA09 | SERCOM0, 115200 baud |

> **Buzzer note:** `#define BUZZER_ENABLED 0` in `main.c`. Set it to `1` to enable beep feedback. With it disabled, the startup prints `Buzzer: Disabled`.

---

## MCC Melody Configuration (Step-by-Step)

### Step 1: Create New Project

1. Open **MPLAB X IDE**
2. File → New Project → Microchip Embedded → Standalone Project
3. Select Device: **PIC32CM3204GV00048**
4. Select Tool: Your programmer (PICkit, ICD4, etc.)
5. Select Compiler: **XC32**
6. Name your project (e.g., `CAN_lab`)

### Step 2: Open MCC Melody

1. Click the **MCC** button in the toolbar
2. Select **MCC Melody** when prompted
3. Wait for MCC to load completely

### Step 3: Configure System Clock

1. Click **Clock Configuration** in Resource Management
2. Set:
   - **OSC8M**: Enable ✓
   - **GCLK Generator 0 Source**: OSC8M
   - **GCLK Generator 0 Divider**: 1

### Step 4: Add UART (SERCOM0)

1. In **Device Resources**, expand **Peripherals → SERCOM**
2. Click **+** next to **SERCOM0**
3. Configure:

| Setting | Value |
|---------|-------|
| Operating Mode | USART Internal Clock |
| Baud Rate | 115200 |
| Character Size | 8 bits |
| Parity | None |
| Stop Bits | 1 |
| Transmit Pinout (TXPO) | PAD[0] |
| Receive Pinout (RXPO) | PAD[1] |

### Step 5: Add ADC (for Potentiometer)

1. In **Device Resources**, expand **Peripherals → ADC**
2. Click **+** next to **ADC**
3. Configure:

| Setting | Value |
|---------|-------|
| Select Prescaler | Peripheral clock divided by 64 |
| Select Sample Length | 4 (half ADC clock cycles) |
| Select Gain | 1x |
| Select Reference | 1/2 VDDANA (only for VDDANA > 2.0V) |
| Select Conversion Trigger | Free Run |
| **Select Positive Input** | **ADC AIN1 Pin** (PA03 = Nano_AN2 on board) |
| Select Negative Input | Internal ground |
| Select Result Resolution | 12-bit result |

### Step 6: Configure All GPIO Pins

Open **Pin Manager → Grid View** and configure each pin:

#### LEDs (GPIO Output)

| Pin | Custom Name | Function | Direction | Initial State |
|-----|-------------|----------|-----------|---------------|
| PA00 | LED1 | GPIO | Output | Low |
| PA01 | LED2 | GPIO | Output | Low |
| PA10 | LED3 | GPIO | Output | Low |
| PA11 | LED4 | GPIO | Output | Low |

#### Buttons (GPIO Input)

| Pin | Custom Name | Function | Direction | Pull Up |
|-----|-------------|----------|-----------|---------|
| PB10 | SW1 | GPIO | Input | ✓ Enable |
| PA15 | SW2 | GPIO | Input | ✓ Enable |

**Important for buttons:** In the pin settings, make sure:
- Direction = **Input**
- **Input Enable (INEN):** automatically enabled when Direction = Input
- **Pull Up Enable** = Checked ✓

#### RGB1 LED (Common Cathode)

| Pin | Custom Name | Function | Direction | Initial State | Color |
|-----|-------------|----------|-----------|---------------|-------|
| PB09 | PWM1 | GPIO | Output | Low | RED |
| PA04 | PWM2 | GPIO | Output | Low | GREEN |
| PA05 | PWM3 | GPIO | Output | Low | BLUE |

**Note:** RGB1 is a common-cathode LED. `HIGH = ON`, `LOW = OFF` — opposite to the discrete LEDs which are active-low.

#### Buzzer

| Pin | Custom Name | Function | Direction | Initial State |
|-----|-------------|----------|-----------|---------------|
| PA14 | Buzzer | GPIO | Output | Low |

#### Potentiometer (ADC)

| Pin | Board Label | Custom Name | Function | Mode |
|-----|-------------|-------------|----------|------|
| PA03 | Nano_AN2 | Potentiometer | ADC_AIN1 | Analog |

**Note:** The board labels this as "Nano_AN2" (Pin 4), but PA03 maps to ADC channel **AIN1** in the MCU.

#### UART (Auto-configured with SERCOM0)

| Pin | Function |
|-----|----------|
| PA08 | SERCOM0_PAD0 (TX) |
| PA09 | SERCOM0_PAD1 (RX) |

#### OLED I2C (SERCOM2 I2C Master) ⚠️ CRITICAL

1. In **Device Resources**, expand **Peripherals → SERCOM**
2. Click **+** next to **SERCOM2**
3. Configure SERCOM2 settings:

| Setting | Value |
|---------|-------|
| Select SERCOM Operation Mode | **I2C Master** |
| I2C Speed in KHz | 100 |
| SDA Hold Time | 300–600 ns hold time |
| Enable operation in Standby mode | ☐ Unchecked |

4. **CRITICAL — Pin Assignment in Pin Manager:**

| Pin | Function | ⚠️ NOT GPIO! |
|-----|----------|--------------|
| PA12 | **SERCOM2_PAD0** | This is SDA |
| PA13 | **SERCOM2_PAD1** | This is SCL |

**⚠️ Common Mistake:** Do NOT set PA12/PA13 as "GPIO" with pull-ups. They must be assigned to **SERCOM2_PAD0** and **SERCOM2_PAD1**. If set as GPIO the I2C peripheral cannot control the pins and initialization will hang indefinitely.

### Step 7: Generate Code

1. Click the **Generate** button
2. Wait for code generation to complete
3. Close MCC

### Step 8: Verify Clock Configuration (CRITICAL!)

After generating, check `src/config/default/peripheral/clock/plib_clock.c`.

Look for this line and **comment it out** if present:

```c
// while(!(SYSCTRL_REGS->SYSCTRL_PCLKSR & SYSCTRL_PCLKSR_XOSCRDY_Msk));
```

This prevents the MCU from hanging forever if no external crystal is connected.

### Step 9: Build and Flash

1. Click **Build** (hammer icon) — verify zero errors
2. Click **Run** (play icon) to flash

---

## Pin Configuration Summary

Your MCC Pin Manager should match this table exactly:

| Pin # | Pin ID | Custom Name | Function | Mode | Direction | Latch |
|-------|--------|-------------|----------|------|-----------|-------|
| 1 | PA00 | LED1 | GPIO | Digital | Out | Low |
| 2 | PA01 | LED2 | GPIO | Digital | Out | Low |
| 4 | PA03 | Potentiometer | ADC_AIN1 | Analog | — | — |
| 13 | PA08 | — | SERCOM0_PAD0 | Digital | — | — |
| 14 | PA09 | — | SERCOM0_PAD1 | Digital | — | — |
| 15 | PA10 | LED3 | GPIO | Digital | Out | Low |
| 16 | PA11 | LED4 | GPIO | Digital | Out | Low |
| 8 | PB09 | PWM1 | GPIO | Digital | Out | Low | (RGB Red)
| 9 | PA04 | PWM2 | GPIO | Digital | Out | Low | (RGB Green)
| 10 | PA05 | PWM3 | GPIO | Digital | Out | Low | (RGB Blue)
| 19 | PB10 | SW1 | GPIO | Digital | In | — |
| 20 | PB11 | — | **Available** | — | — | — | ⚠️ Do NOT configure!
| 21 | PA12 | — | SERCOM2_PAD0 | Digital | — | — | (OLED SDA)
| 22 | PA13 | — | SERCOM2_PAD1 | Digital | — | — | (OLED SCL)
| 23 | PA14 | Buzzer | GPIO | Digital | Out | Low |
| 24 | PA15 | SW2 | GPIO | Digital | In | — |

**⚠️ Important:** Leave **PB11 (Pin 20)** as "Available" — do NOT configure it as GPIO. Any configuration on PB11 causes the WS2812B addressable LEDs to receive invalid data and light solid white.

---

## How to Use the Demo

### Startup Sequence

When powered on, the firmware runs a self-test and prints:

```
========================================
        SYSTEM INITIALIZATION
========================================

  LEDs:    OK
  RGB:     OK
  Buzzer:  Disabled
  ADC:     45%
  OLED:    OK

System ready. Starting simulation...
```

During the self-test:
- LED1–LED4 each flash briefly
- RGB cycles Red → Green → Blue
- ADC reads the current potentiometer position
- OLED shows the splash screen: **NCUExMICROCHIP / CAN Simulation / PIC32CM3204GV**

If the OLED hangs at `OLED:` — see Troubleshooting below.

### Controls

| Control | Action | Effect |
|---------|--------|--------|
| **SW1 (ACC)** | Hold | Accelerate — rate depends on throttle position |
| **SW2 (BRK)** | Hold | Brake — fixed deceleration, LED4 turns ON |
| **Potentiometer** | Turn | Set throttle 0–100% (only affects rate when SW1 held) |
| **Release both** | — | Coast — speed slowly decreases |

**Key insight:** The potentiometer controls *how fast* you accelerate, not whether you accelerate. This models a real vehicle where the throttle controls engine power, not speed directly.

### Throttle-Based Acceleration (SW1 held)

| Throttle | Speed Increase per Update | Description |
|----------|--------------------------|-------------|
| 80–100% | +8 km/h | Full throttle — rapid acceleration |
| 60–79% | +6 km/h | High throttle |
| 40–59% | +4 km/h | Medium throttle |
| 10–39% | +2 km/h | Low throttle |
| 0–9% | +0 km/h | Idle — no acceleration |

Implemented in `get_throttle_increment()` in `main.c`.

### LED Indicators

| LED | Pin | Meaning |
|-----|-----|---------|
| **LED1** | PA00 | Blinks each time a CAN frame is transmitted |
| **LED2** | PA01 | Available — not driven in current firmware |
| **LED3** | PA10 | Available — not driven in current firmware |
| **LED4** | PA11 | Brake engaged (ON when SW2 held) |

All four LEDs are **active-low** on this board: `Clear() = ON`, `Set() = OFF`.

### RGB1 LED Colors (Speed Indicator)

| Color | Condition | Speed |
|-------|-----------|-------|
| **Red** | Braking | SW2 held |
| **Yellow** | High speed | > 70 km/h |
| **Green** | Cruising | 51–70 km/h |
| **Cyan** | Medium speed | 31–50 km/h |
| **Blue** | Slow | 11–30 km/h |
| **White** | Stopped / Idle | 0–10 km/h |

### Auto Demo Mode

When neither button is pressed at startup, the firmware enters **AUTO DEMO** mode. It runs a four-phase cycle automatically:

| Phase | Action |
|-------|--------|
| 0 | Idle at 850 RPM, speed = 0 |
| 1 | Accelerate to 80 km/h |
| 2 | Cruise at 80 km/h, 2200 RPM |
| 3 | Brake to stop, then repeat |

Pressing either button immediately switches to **MANUAL** mode.

---

## Terminal Output

Connect at **115200 baud** (use the MCP2221 COM port). The screen refreshes every 200 ms.

The firmware cycles through three CAN messages in order. Each update shows one message with its full decode:

### Message 1 — ENGINE_RPM (0x0C0)

```
============================================================
           CAN BUS PROTOCOL - EDUCATIONAL SIMULATION
============================================================

CONTROLS:
  [SW1] Hold = ACCELERATE    [SW2] Hold = BRAKE
  [POT] Turn = Set throttle level (0-100%)

------------------------------------------------------------
VEHICLE STATUS:

  Engine RPM:     2200
  Speed:            80  km/h
  Throttle:         45  %
  Brake:          Released
  Mode:           AUTO DEMO

  SW1: ---        SW2: ---

------------------------------------------------------------
CAN FRAME TRANSMITTED:

  Message:        ENGINE_RPM
  CAN ID:         0x0C0
  Data Length:    2 bytes
  Data Bytes:     0x08 0x98
  CRC-15:         0x1A3F

  Formula:        RPM = (Data[0] << 8) | Data[1]
  Calculation:    RPM = (8 x 256) + 152 = 2200

============================================================
```

### Message 2 — VEHICLE_SPEED (0x0D0)

```
  Message:        VEHICLE_SPEED
  CAN ID:         0x0D0
  Data Length:    1 bytes
  Data Bytes:     0x50
  CRC-15:         0x0F22

  Formula:        Speed = Data[0]
  Calculation:    Speed = 80 km/h
```

### Message 3 — THROTTLE_BRAKE (0x0F0)

```
  Message:        THROTTLE_BRAKE
  CAN ID:         0x0F0
  Data Length:    2 bytes
  Data Bytes:     0x2D 0x00
  CRC-15:         0x3C11

  Formula:        Throttle = Data[0], Brake = Data[1]
  Calculation:    Throttle = 45%, Brake = OFF (0)
```

After every three-message cycle the OLED also updates.

---

## OLED Display (128×64)

The OLED shows a real-time dashboard updated every 200 ms:

```
+---------------------+
|   CAN BUS MONITOR   |  <- Title (row 0)
|---------------------|
| RPM:                |  <- Label (row 2, 6×8 font)
|      2200           |  <- Value (row 2, 8×16 large font)
| SPD: 80km/h  T: 45% |  <- Speed + throttle (row 4)
| ACC:[*] BRK:[ ]     |  <- Button status (row 5)
| 0C0:08 98  CRC:1A3F |  <- CAN ID:Data + CRC (row 6)
|---------------------|
+---------------------+
```

| Field | Description |
|-------|-------------|
| **SPD** | Vehicle speed in km/h |
| **T:** | Throttle percentage (0–100%) |
| **ACC:[*]** | SW1 is pressed (accelerating) |
| **BRK:[*]** | SW2 is pressed (braking) |
| **[ ]** | Button is released |
| **CRC:xxxx** | CAN frame CRC-15 value (hex) |

**Splash screen** on startup: `NCUExMICROCHIP` (row 1) / `CAN Simulation` (row 3) / `PIC32CM3204GV` (row 6), displayed for ~1.5 s before the dashboard appears.

---

## Troubleshooting

### "Buttons don't work"

1. In MCC, check SW1 (PB10) and SW2 (PA15):
   - Direction = **Input**
   - **Input Enable (INEN)** = ✓ Checked
   - **Pull Up Enable** = ✓ Checked
2. Regenerate code and reflash
3. Watch the startup terminal output — button state is printed before the main loop

### "RGB1 LED doesn't change color"

1. Verify PB09 (PWM1), PA04 (PWM2), PA05 (PWM3) are GPIO Output in MCC
2. RGB1 is common-cathode: `HIGH = color ON` — the opposite of the discrete LEDs
3. Check the RGB pin group assignments in `main.c`: Red is on PORT Group 1 (PB), Green/Blue are on Group 0 (PA)

### "WS2812B LEDs are bright white"

⚠️ Do NOT configure PB11 (Pin 20) in MCC. Leave it as **Available**.

Any GPIO configuration on PB11 sends invalid data to the WS2812B LED chain. Fix:

1. MCC → Pin Manager → set PB11 to **Available**
2. Regenerate code
3. **Erase chip** and reflash (the WS2812B may latch the bad state until power-cycled)

### "OLED display not working / initialization hangs"

**Most common cause — wrong pin configuration:**

1. PA12 must be **SERCOM2_PAD0** (SDA), NOT GPIO
2. PA13 must be **SERCOM2_PAD1** (SCL), NOT GPIO

OLED wiring:

| OLED Pin | Board Connection |
|----------|-----------------|
| GND | GND |
| VDD | 3.3 V |
| SCL | PA13 (Pin 22) |
| SDA | PA12 (Pin 21) |

Other checks:
- OLED I2C address must be **0x3C** (standard 4-pin module)
- Terminal debug sequence: `OLED: OK` — if it hangs before printing `OK`, the issue is the pin configuration above

### "No serial output"

1. Open the correct COM port — must be the **MCP2221** port
2. Baud rate must be **115200**
3. Press **RESET** on the board to trigger the startup message again

### "LEDs always ON"

LEDs are **active-low** on this board. `Low = ON`, `High = OFF`. The macros in `main.c` handle this:

```c
#define LED1_ON()   LED1_Clear()   /* drive LOW = turn ON  */
#define LED1_OFF()  LED1_Set()     /* drive HIGH = turn OFF */
```

If your LED is always on, check the **Initial State** in MCC pin configuration.

---

## Hands-On Extensions

These exercises encourage modifying `main.c` directly. They require no additional hardware.

### Exercise 1 — Widen the Throttle Bands

Find `get_throttle_increment()` in `main.c`:

```c
static uint8_t get_throttle_increment(uint8_t thr) {
    if(thr >= 80) return 8;
    else if(thr >= 60) return 6;
    else if(thr >= 40) return 4;
    else if(thr >= 10) return 2;
    else return 0;
}
```

**Task:** Make full throttle (100%) produce +12 km/h instead of +8. Observe the change on the OLED and terminal. What happens to the maximum speed (capped at 180 in `update()`)?

### Exercise 2 — Add a Fourth CAN Message

The main loop rotates through `msg = 0, 1, 2`. Add a new case for engine temperature:

```c
case 3: /* Engine Temperature */
    data[0] = 80 + (rpm / 100);   /* simple temperature model */
    current_can_id = 0x050;
    current_dlc = 1;
    ...
```

Change the modulo from `% 3` to `% 4` and watch the new message appear. What CAN ID does OBD-II use for coolant temperature? (Hint: check the `doc/` folder.)

### Exercise 3 — Decode a CRC by Hand

With the demo running, note a frame where `ENGINE_RPM = 850`:

- `Data[0] = 0x03`, `Data[1] = 0x52`
- `CAN ID = 0x0C0`, `DLC = 2`

Using the `crc15()` function logic (or a Python script), compute the expected CRC-15. Compare it to what the terminal prints. If they match, your understanding of the algorithm is correct.

---

## Quick Reference Card

```
+------------------------------------------------+
|           INTERACTIVE CAN DEMO                 |
+------------------------------------------------+
|  SW1 (PB10)  | HOLD = Accelerate               |
|  SW2 (PA15)  | HOLD = Brake                    |
|  POT (PA03)  | TURN = Throttle 0-100%          |
+------------------------------------------------+
|  LED1 (PA00) | CAN TX blink                    |
|  LED4 (PA11) | Brake engaged                   |
|  RGB1 LED    | Speed color (PB09/PA04/PA05)    |
|  OLED        | 128x64 Dashboard (PA12/PA13)    |
+------------------------------------------------+
|  UART: PA08/PA09 @ 115200 baud                 |
|  CAN messages: 0x0C0 / 0x0D0 / 0x0F0          |
|  Update interval: 200 ms                       |
+------------------------------------------------+
```

---

*Hardware: PIC32CM3204GV00048 on APP-MASTERS25-1 board*  
*For instructor reference, see `doc/README_CAN_4_labs_arrangment.md`*
