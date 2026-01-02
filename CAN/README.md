# Interactive CAN Bus Protocol - Educational Demo

## What This Demo Does

This demo **simulates a Vehicle ECU** with interactive controls:
- **Press buttons** to accelerate (SW1) and brake (SW2)
- **Turn potentiometer** to control throttle
- **Watch RGB LEDs** change color based on vehicle speed
- **See CAN frames** being transmitted with full explanation

---

## Hardware Features

| Component | MCU Pin | Function |
|-----------|---------|----------|
| **SW1** | PB10 | Accelerate button (hold to speed up) |
| **SW2** | PA15 | Brake button (hold to slow down) |
| **Potentiometer** | PA03 | Throttle control (ADC input) |
| **LED1** | PA00 | CAN TX indicator |
| **LED2** | PA01 | Dashboard indicator |
| **LED3** | PA10 | Engine warning |
| **LED4** | PA11 | Brake light |
| **RGB1 LED** | PB09/PA04/PA05 | RGB status (PWM1=Red, PWM2=Green, PWM3=Blue) |
| **Buzzer** | PA14 | Audio feedback |
| **UART TX** | PA08 | Serial output |
| **UART RX** | PA09 | Serial input |

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
1. Click the **MCC** button in toolbar
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
| Reference | VDDANA |
| Prescaler | DIV64 |
| Resolution | 12-bit |

### Step 6: Configure All GPIO Pins

Open **Pin Manager** → **Grid View** and configure each pin:

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

**Important for Buttons:** In the pin settings, make sure:
- Direction = **Input**
- **Input Enable (INEN):** when you set Direction to "In", the Input Enable (INEN) is automatically enabled.
- **Pull Up Enable** = Checked ✓

#### RGB1 LED (Common Cathode)
| Pin | Custom Name | Function | Direction | Initial State | Color |
|-----|-------------|----------|-----------|---------------|-------|
| PB09 | PWM1 | GPIO | Output | Low | RED |
| PA04 | PWM2 | GPIO | Output | Low | GREEN |
| PA05 | PWM3 | GPIO | Output | Low | BLUE |

**Note:** RGB1 is a common-cathode LED. HIGH = ON, LOW = OFF.

#### Buzzer
| Pin | Custom Name | Function | Direction | Initial State |
|-----|-------------|----------|-----------|---------------|
| PA14 | Buzzer | GPIO | Output | Low |

#### Potentiometer (ADC)
| Pin | Custom Name | Function | Mode |
|-----|-------------|----------|------|
| PA03 | Potentiometer | ADC_AIN1 | Analog |

#### UART (Auto-configured with SERCOM0)
| Pin | Function |
|-----|----------|
| PA08 | SERCOM0_PAD0 (TX) |
| PA09 | SERCOM0_PAD1 (RX) |

### Step 7: Generate Code
1. Click the **Generate** button
2. Wait for code generation to complete
3. Close MCC

### Step 8: Verify Clock Configuration (CRITICAL!)
After generating, check `src/config/default/peripheral/clock/plib_clock.c`:

Look for this line and **comment it out** if present:
   ```c
   // while(!(SYSCTRL_REGS->SYSCTRL_PCLKSR & SYSCTRL_PCLKSR_XOSCRDY_Msk));
   ```

This prevents the MCU from hanging if no external crystal is present.

### Step 9: Build and Flash
1. Click **Build** (hammer icon)
2. Verify no errors
3. Click **Run** (play icon) to flash

---

## Pin Configuration Summary (Screenshot Reference)

Your MCC Pin Manager should look like this:

| Pin # | Pin ID | Custom Name | Function | Mode | Direction | Latch |
|-------|--------|-------------|----------|------|-----------|-------|
| 1 | PA00 | LED1 | GPIO | Digital | Out | Low |
| 2 | PA01 | LED2 | GPIO | Digital | Out | Low |
| 4 | PA03 | Potentiometer | ADC_AIN1 | Analog | - | - |
| 13 | PA08 | - | SERCOM0_PAD0 | Digital | - | - |
| 14 | PA09 | - | SERCOM0_PAD1 | Digital | - | - |
| 15 | PA10 | LED3 | GPIO | Digital | Out | Low |
| 16 | PA11 | LED4 | GPIO | Digital | Out | Low |
| 8  | PB09 | PWM1 | GPIO | Digital | Out | Low | (RGB Red)
| 9  | PA04 | PWM2 | GPIO | Digital | Out | Low | (RGB Green)
| 10 | PA05 | PWM3 | GPIO | Digital | Out | Low | (RGB Blue)
| 19 | PB10 | SW1 | GPIO | Digital | In | - |
| 20 | PB11 | - | **Available** | - | - | - | ⚠️ Do NOT configure!
| 23 | PA14 | Buzzer | GPIO | Digital | Out | Low |
| 24 | PA15 | SW2 | GPIO | Digital | In | - |

**⚠️ Important:** Leave **PB11 (Pin 20)** as "Available" - do NOT configure it as GPIO. Any configuration on PB11 will cause the WS2812B LEDs to light up bright white.

---

## How to Use the Demo

### Startup Sequence
When you power on, the demo runs a **hardware self-test**:
```
========================================
        SYSTEM INITIALIZATION
========================================

Testing hardware...
  LED1-4:  OK
  RGB LED: RED
           GREEN
           BLUE
           OK
  Buzzer:  OK
  ADC:     45% OK

Starting CAN simulation...
```

All LEDs will flash, RGB cycles through colors, and buzzer beeps. Then the simulation begins.

### Controls

| Control | Action | Effect |
|---------|--------|--------|
| **SW1** | **HOLD** | Accelerate - RPM↑, Speed↑ |
| **SW2** | **HOLD** | Brake - RPM↓, Speed↓ |
| **Potentiometer** | **TURN** | Set throttle level (0-100%) |
| **Release both** | - | Coast (speed gradually decreases) |

### LED Indicators

| LED | Color | Meaning |
|-----|-------|---------|
| **LED1** | Red | CAN frame transmitted (blinks) |
| **LED2** | Red | Dashboard updated (blinks) |
| **LED3** | Red | Engine warning (if RPM > 5500) |
| **LED4** | Red | Brake engaged |

### RGB1 LED Colors

| Color | Vehicle State |
|-------|---------------|
| 🔴 **Red** | Braking (SW2 held) |
| 🟡 **Yellow** | High speed (>100 km/h) |
| 🟢 **Green** | Cruising (60-100 km/h) |
| 🩵 **Cyan** | Medium speed (20-60 km/h) |
| 🔵 **Blue** | Slow (1-20 km/h) |
| ⚪ **White** | Stopped/Idle (0 km/h) |

### Buzzer Feedback

| Sound | Meaning |
|-------|---------|
| Short beep | Button pressed |
| Triple ascending | Startup complete |

---

## Terminal Output

Connect to serial port at **115200 baud** to see:

```
╔══════════════════════════════════════════════════════════╗
║        CAN BUS EDUCATIONAL DEMO - VEHICLE SIMULATOR      ║
╠══════════════════════════════════════════════════════════╣
║                                                          ║
║  HOW TO USE:                                             ║
║  ┌─────────┬────────────────────────────────────────┐    ║
║  │ SW1     │ HOLD to ACCELERATE (increase speed)    │    ║
║  │ SW2     │ HOLD to BRAKE (decrease speed)         │    ║
║  │ POT     │ TURN to set THROTTLE level             │    ║
║  └─────────┴────────────────────────────────────────┘    ║
║                                                          ║
╠══════════════════════════════════════════════════════════╣
║  CURRENT STATUS:                                         ║
║                                                          ║
║  SW1: □ released     SW2: □ released                     ║
║  POT Throttle: 45%                                       ║
║                                                          ║
║  ┌────────────────────────────────────────────────────┐  ║
║  │  ENGINE RPM:    2200                               │  ║
║  │  SPEED:         80  km/h                           │  ║
║  │  BRAKE:         OFF                                │  ║
║  │  MODE:          [AUTO DEMO]                        │  ║
║  └────────────────────────────────────────────────────┘  ║
╚══════════════════════════════════════════════════════════╝
```

---

## Troubleshooting

### "Buttons don't work"
1. Check MCC Pin Settings for SW1 (PB10) and SW2 (PA15):
   - Direction = **Input**
   - **Input Enable** = ✓ Checked
   - **Pull Up** = ✓ Checked
2. Regenerate code after changes
3. Watch the startup button test - it shows real-time button state

### "RGB1 LED doesn't change color"
1. Verify PB09 (PWM1), PA04 (PWM2), PA05 (PWM3) are configured as GPIO Output in MCC
2. Check that the RGB1 LED (CLX6F-FKC) is properly connected
3. Each color pin should show HIGH when active (common cathode LED)

### "WS2812B LEDs are bright white"
⚠️ **Do NOT configure PB11 (Pin 20)** - leave it as "Available" in MCC!

Any GPIO configuration on PB11 causes the WS2812B to receive invalid data and light up white. The solution:
1. In MCC, set PB11 to **Available** (not GPIO)
2. Regenerate code
3. Erase and reflash the board

### "No serial output"
1. Check COM port - must be **MCP2221** port
2. Baud rate must be **115200**
3. Press RESET button on board

### "LEDs always ON or not working"
1. LEDs are **active-low** on this board
2. Low = LED ON, High = LED OFF
3. Check Initial State in pin configuration

---

## Hardware Notes

### Why This is a Simulation
The **PIC32CM3204GV00048** does **NOT** have a built-in CAN controller.

The board has:
- ✅ **ATA6561** - CAN Transceiver (converts digital ↔ differential)
- ❌ **No CAN Controller** - Cannot generate real CAN frames

For real CAN, add an external **MCP2515** CAN controller via SPI.

---

## Quick Reference Card

```
┌────────────────────────────────────────────────┐
│           INTERACTIVE CAN DEMO                 │
├────────────────────────────────────────────────┤
│  SW1 (PB10)  │ HOLD = Accelerate               │
│  SW2 (PA15)  │ HOLD = Brake                    │
│  POT (PA03)  │ TURN = Throttle 0-100%          │
├────────────────────────────────────────────────┤
│  LED1 (PA00) │ CAN TX indicator                │
│  LED2 (PA01) │ Dashboard indicator             │
│  LED3 (PA10) │ Engine warning                  │
│  LED4 (PA11) │ Brake light                     │
├────────────────────────────────────────────────┤
│  RGB1 LED    │ Speed-based color (PWM1/2/3)    │
│  Buzzer(PA14)│ Audio feedback                  │
├────────────────────────────────────────────────┤
│  UART: PA08/PA09 @ 115200 baud                 │
└────────────────────────────────────────────────┘
```

---

*Hardware: PIC32CM3204GV00048 on APP-MASTERS25-1 board*
