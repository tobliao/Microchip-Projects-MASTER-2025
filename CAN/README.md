# CAN Protocol Hands-On Course
## Complete Guide for PIC32CM3204GV00048 + ATA6561

---

## Table of Contents
1. [Course Overview](#course-overview)
2. [Hardware Setup](#hardware-setup)
3. [MCC Melody Configuration](#mcc-melody-configuration)
4. [Demo Functionality](#demo-functionality)
5. [CAN Protocol Lessons](#can-protocol-lessons)
6. [Code Walkthrough](#code-walkthrough)
7. [Troubleshooting](#troubleshooting)
8. [Future Enhancement: Real CAN](#future-enhancement-real-can)

---

## Course Overview

### Learning Objectives
By completing this course, students will understand:
- CAN (Controller Area Network) frame structure
- Bit timing and synchronization concepts
- Bus arbitration mechanism (CSMA/CD with non-destructive bitwise arbitration)
- Error detection methods (CRC-15)
- Real-world automotive CAN applications and message IDs

### Hardware Limitations
**Important:** The PIC32CM3204GV00048 MCU does **NOT** have a built-in CAN controller. The board includes:
- **ATA6561-GAQW**: CAN Transceiver only (converts TX/RX ↔ CANH/CANL differential signals)
- **No CAN Controller**: Cannot generate/parse CAN frames without external hardware

This demo provides an **educational simulation** that teaches CAN protocol concepts without actual CAN hardware.

### What This Demo Does
1. Displays educational lessons about CAN protocol
2. Simulates a vehicle ECU generating CAN messages
3. Shows real CAN frame structure with calculated CRC
4. Displays a live vehicle dashboard updated via simulated CAN data

---

## Hardware Setup

### Development Board
- **Board**: APP-MASTERS25-1
- **MCU**: PIC32CM3204GV00048 (ARM Cortex-M0+, no built-in CAN)
- **CAN Transceiver**: ATA6561-GAQW (on-board)
- **USB-UART**: MCP2221 (on-board)

### Pin Connections

| Function | MCU Pin | Peripheral | Notes |
|----------|---------|------------|-------|
| **UART TX** | PA08 | SERCOM0_PAD0 | To MCP2221 URX |
| **UART RX** | PA09 | SERCOM0_PAD1 | From MCP2221 UTX |
| **LED1** | PA00 | GPIO Output | TX activity indicator |
| **LED2** | PA01 | GPIO Output | Dashboard update indicator |
| **SPI MOSI** | PA16 | SERCOM1_PAD0 | For future MCP2515 |
| **SPI SCK** | PA17 | SERCOM1_PAD1 | For future MCP2515 |
| **SPI CS** | PA18 | GPIO Output | For future MCP2515 |
| **SPI MISO** | PA19 | SERCOM1_PAD3 | For future MCP2515 |
| **CAN TX** | Via P4 jumper | To ATA6561 TXD | Short P4 jumper |
| **CAN RX** | Via P5 jumper | From ATA6561 RXD | Short P5 jumper |

### Jumper Settings
- **P4**: Short pins 1-2 to connect MCU CAN TX → ATA6561 TXD
- **P5**: Short pins 1-2 to connect MCU CAN RX ← ATA6561 RXD

### Terminal Setup
1. Connect USB cable to board
2. Open MPLAB Data Visualizer or any serial terminal
3. Select the MCP2221 COM port (e.g., `tty.usbmodem1101`)
4. Configure: **115200 baud, 8N1**
5. Enable **DTR** if available

---

## MCC Melody Configuration

### Step 1: Create New Project
1. Open MPLAB X IDE
2. File → New Project → Microchip Embedded → Standalone Project
3. Select Device: **PIC32CM3204GV00048**
4. Select Tool: Your programmer (e.g., PICkit, ICD4)
5. Select Compiler: **XC32**
6. Name project (e.g., `CAN_lab`)

### Step 2: Open MCC Melody
1. Click **MCC** button in toolbar (or Tools → Embedded → MPLAB Code Configurator)
2. Select **MCC Melody** when prompted
3. Wait for MCC to load

### Step 3: Configure Clock
**Purpose**: Set up the system clock for stable operation

1. In **Resource Management**, click **Clock Configuration**
2. Configure as follows:
   - **OSC8M**: Enable ✓ (8MHz Internal Oscillator)
   - **GCLK Generator 0**:
     - Source: **OSC8M**
     - Divider: **1**
     - Output: 8 MHz
   - **GCLK Generator 1** (for peripherals):
     - Source: **OSC8M**
     - Divider: **1**

**Why OSC8M?** The internal 8MHz oscillator is stable and doesn't require external crystal, avoiding clock lock issues.

### Step 4: Configure UART (SERCOM0)
**Purpose**: Enable serial communication with PC via MCP2221 USB-UART chip

1. In **Device Resources**, expand **Peripherals → SERCOM**
2. Click **+** next to **SERCOM0** to add it
3. In SERCOM0 configuration:
   - **Operating Mode**: USART with internal clock
   - **Baud Rate**: **115200**
   - **Character Size**: 8 bits
   - **Parity**: None
   - **Stop Bits**: 1
   - **Transmit Pinout (TXPO)**: **PAD[0]** (PA08)
   - **Receive Pinout (RXPO)**: **PAD[1]** (PA09)

**Why SERCOM0?** The MCP2221 USB-UART chip is physically connected to PA08/PA09, which are SERCOM0 pins.

### Step 5: Configure GPIO (LEDs)
**Purpose**: Visual feedback for CAN message transmission

1. Open **Pin Manager** (Grid View)
2. Configure pins:
   
   | Pin | Function | Direction | Initial State |
   |-----|----------|-----------|---------------|
   | PA00 | GPIO | Output | High (LED off) |
   | PA01 | GPIO | Output | High (LED off) |
   | PA08 | SERCOM0_PAD0 | - | - |
   | PA09 | SERCOM0_PAD1 | - | - |

3. In **Pin Settings**, set custom names:
   - PA00 → `LED1`
   - PA01 → `LED2`

**Why High initial state?** LEDs are active-low (LED ON when pin is LOW).

### Step 6: Configure STDIO (Optional)
**Purpose**: Enable printf() functionality

1. In **Device Resources**, find **STDIO**
2. Add **STDIO** to project
3. Link STDIO to **SERCOM0**

### Step 7: Generate Code
1. Click **Generate** button
2. Review generated files
3. Close MCC

### Step 8: Verify Clock Configuration
**Critical Step**: Check `plib_clock.c` to ensure no XOSC wait loops

1. Open `src/config/default/peripheral/clock/plib_clock.c`
2. Find `SYSCTRL_Initialize()` function
3. **Verify** there is NO infinite wait for external crystal:
   ```c
   // This should NOT exist or should be commented out:
   // while(!(SYSCTRL_REGS->SYSCTRL_PCLKSR & SYSCTRL_PCLKSR_XOSCRDY_Msk));
   ```
4. If present, comment it out to prevent hang

---

## Demo Functionality

### Startup Sequence

| Stage | LED Pattern | Duration | What Happens |
|-------|-------------|----------|--------------|
| 1 | 5x alternating LED1/LED2 | ~1 sec | System initialization complete |
| 2 | Banner printed | - | Course title and hardware info |
| 3 | Lesson 1 displayed | 3 sec | CAN Frame Structure |
| 4 | Lesson 2 displayed | 3 sec | Bit Timing |
| 5 | Lesson 3 displayed | 3 sec | Arbitration |
| 6 | Lesson 4 displayed | 3 sec | Automotive CAN IDs |
| 7 | Simulation starts | Continuous | Live CAN frames + Dashboard |

### Main Loop Behavior

The main loop cycles through 6 states, each ~500ms:

| Cycle | CAN Message | ID | Data Content |
|-------|-------------|-----|--------------|
| 0 | Engine RPM | 0x0C0 | 2 bytes: RPM value |
| 1 | Vehicle Speed | 0x0D0 | 1 byte: km/h |
| 2 | Coolant Temp | 0x0E0 | 1 byte: °C + 40 offset |
| 3 | Throttle & Brake | 0x0F0 | 2 bytes: throttle%, brake |
| 4 | Steering Angle | 0x110 | 2 bytes: degrees |
| 5 | Dashboard Update | - | Display all values |

### Vehicle Simulation States

The simulated vehicle goes through realistic driving states:

| State | Description | Duration | Vehicle Behavior |
|-------|-------------|----------|------------------|
| 0 | Warming Up | ~15 sec | Coolant rises, idle RPM |
| 1 | Accelerating | ~10 sec | Throttle up, speed increases |
| 2 | Cruising | ~20 sec | Steady speed, slight steering |
| 3 | Braking | ~5 sec | Throttle 0, speed decreases |
| (repeat) | Back to idle | - | Gear to P, cycle restarts |

---

## CAN Protocol Lessons

### Lesson 1: CAN Frame Structure

```
+-----+------------+---+---+---+-----+--------+-----+---+-----+
| SOF |  ID (11b)  |RTR|IDE|r0 | DLC |  DATA  | CRC |ACK| EOF |
+-----+------------+---+---+---+-----+--------+-----+---+-----+
|  1  |     11     | 1 | 1 | 1 |  4  |  0-64  | 15  | 2 |  7  |
+-----+------------+---+---+---+-----+--------+-----+---+-----+
```

| Field | Bits | Description |
|-------|------|-------------|
| SOF | 1 | Start of Frame - dominant bit signals start |
| ID | 11 | Message identifier - lower value = higher priority |
| RTR | 1 | Remote Transmission Request - 0=data frame, 1=remote frame |
| IDE | 1 | Identifier Extension - 0=standard (11-bit), 1=extended (29-bit) |
| r0 | 1 | Reserved bit - must be dominant (0) |
| DLC | 4 | Data Length Code - number of data bytes (0-8) |
| DATA | 0-64 | Payload - actual message content |
| CRC | 15 | Cyclic Redundancy Check - error detection |
| ACK | 2 | Acknowledge - receiver confirms valid frame |
| EOF | 7 | End of Frame - 7 recessive bits |

### Lesson 2: Bit Timing

Each CAN bit is divided into time segments:

```
|<-------------- 1 Bit Time --------------->|
+------+------------------+--------+--------+
| SYNC |   PROP + PHASE1  | SAMPLE | PHASE2 |
+------+------------------+--------+--------+
  1tq        1-8 tq          ↑       1-8 tq
                        Sample Point
```

**Common Baud Rates**:
- **125 kbps**: Body electronics, comfort systems
- **250 kbps**: J1939 trucks, agricultural equipment
- **500 kbps**: Powertrain, chassis, safety systems
- **1 Mbps**: Maximum standard CAN speed

### Lesson 3: Arbitration (CSMA/CD)

CAN uses **non-destructive bitwise arbitration**:

**Rule**: Dominant (0) always wins over Recessive (1)

```
Example: Node A (ID=0x100) vs Node B (ID=0x200)

Bit Position:  10   9   8   7   6   5   4   3   2   1   0
              +---+---+---+---+---+---+---+---+---+---+---+
ID = 0x100:   | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | ← WINS!
              +---+---+---+---+---+---+---+---+---+---+---+
ID = 0x200:   | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | ← loses
              +---+---+---+---+---+---+---+---+---+---+---+
                    ↑
              Node B sends recessive (1)
              but sees dominant (0) on bus
              → backs off, waits for bus idle
```

**Key Insight**: LOWER ID = HIGHER PRIORITY

This is why safety-critical messages (airbags, brakes) use low IDs!

### Lesson 4: Automotive CAN IDs

| ID | Message | Update Rate | Priority |
|----|---------|-------------|----------|
| 0x0C0 | Engine RPM | 10 ms | High |
| 0x0D0 | Vehicle Speed | 20 ms | High |
| 0x0E0 | Coolant Temperature | 100 ms | Medium |
| 0x0F0 | Throttle Position | 10 ms | High |
| 0x100 | Brake Status | 10 ms | High |
| 0x110 | Steering Angle | 10 ms | High |
| 0x620 | Airbag Status | 100 ms | Medium |
| 0x7DF | OBD-II Request (broadcast) | On-demand | Low |
| 0x7E8 | OBD-II Response (ECM) | On-demand | Low |

---

## Code Walkthrough

### File Structure
```
CAN/
├── src/
│   ├── main.c                 ← Main application code
│   └── config/default/
│       ├── peripheral/
│       │   ├── clock/plib_clock.c
│       │   ├── port/plib_port.c
│       │   └── sercom/usart/plib_sercom0_usart.c
│       └── definitions.h
└── CAN_lab.X/
    └── nbproject/
        └── configurations.xml
```

### Key Code Sections

#### 1. UART Functions (Lines 38-78)
```c
static void UART_PutChar(char c) {
    // Wait for Data Register Empty
    while(!(SERCOM0_REGS->USART_INT.SERCOM_INTFLAG & 
            SERCOM_USART_INT_INTFLAG_DRE_Msk));
    SERCOM0_REGS->USART_INT.SERCOM_DATA = c;
}
```
**Purpose**: Direct register access for reliable UART output

#### 2. CRC-15 Calculation (Lines 103-131)
```c
static uint16_t CAN_CalculateCRC(CAN_Frame_t* frame) {
    const uint16_t polynomial = 0x4599;  // Real CAN polynomial
    // Process ID, DLC, and Data bits
    // Returns 15-bit CRC
}
```
**Purpose**: Implements actual CAN CRC algorithm for education

#### 3. Vehicle Simulation (Lines 148-175)
```c
static void SimulateVehicle(void) {
    // State machine: Warming → Accelerating → Cruising → Braking
}
```
**Purpose**: Generates realistic vehicle data changes

#### 4. Educational Lessons (Lines 182-297)
```c
static void PrintLesson1_FrameStructure(void) { ... }
static void PrintLesson2_BitTiming(void) { ... }
static void PrintLesson3_Arbitration(void) { ... }
static void PrintLesson4_AutomotiveIDs(void) { ... }
```
**Purpose**: Displays formatted CAN protocol education

#### 5. Main Loop (Lines 325-400)
```c
while(1) {
    SimulateVehicle();
    switch(cycle % 6) {
        case 0: /* Engine RPM frame */ ...
        case 1: /* Vehicle Speed frame */ ...
        // ... etc
    }
}
```
**Purpose**: Cycles through CAN messages and updates dashboard

---

## Troubleshooting

### Problem: No Terminal Output

**Symptoms**: LEDs blink but no text in terminal

**Solutions**:
1. **Check COM Port**: Use MCP2221 port, not debugger port
2. **Check Baud Rate**: Must be 115200
3. **Check Pin Config**: PA08=TX, PA09=RX (SERCOM0)
4. **Enable DTR**: Some terminals require DTR signal

### Problem: LEDs Always On, Not Blinking

**Symptoms**: Both LEDs lit, no activity

**Cause**: Clock initialization hanging (waiting for external crystal)

**Solution**:
1. Open `plib_clock.c`
2. Find and comment out XOSC wait loop:
   ```c
   // while(!(SYSCTRL_REGS->SYSCTRL_PCLKSR & SYSCTRL_PCLKSR_XOSCRDY_Msk));
   ```
3. Ensure GCLK0 uses OSC8M (not XOSC)

### Problem: Build Errors - "No such file"

**Symptoms**: `fatal error: device.h: No such file or directory`

**Solution**:
1. Right-click project → Properties
2. XC32 (Global Options) → xc32-gcc → Preprocessing and Messages
3. Add include paths:
   - `../src`
   - `../src/config/default`
   - `../src/packs/CMSIS/CMSIS/Core/Include`
   - `../src/packs/PIC32CM3204GV00048_DFP`

### Problem: Garbled Terminal Output

**Symptoms**: Random characters or symbols

**Cause**: Baud rate mismatch

**Solution**: Verify both terminal and code use 115200 baud

---

## Future Enhancement: Real CAN

To upgrade to actual CAN communication, add an **MCP2515 CAN Controller Module**:

### Hardware Addition

```
MCU (SPI) → MCP2515 (CAN Controller) → ATA6561 (Transceiver) → CAN Bus
```

### MCP2515 Wiring

| MCP2515 Pin | MCU Pin | Function |
|-------------|---------|----------|
| VCC | 3.3V | Power |
| GND | GND | Ground |
| CS | PA18 | SPI Chip Select |
| SCK | PA17 | SPI Clock |
| MOSI (SI) | PA16 | SPI Data Out |
| MISO (SO) | PA19 | SPI Data In |
| INT | (optional) | Interrupt |

### Code Changes Required

1. Add MCP2515 driver functions:
   - `MCP_WriteReg()`, `MCP_ReadReg()`
   - `MCP_Reset()`, `MCP_BitModify()`

2. Initialize CAN controller:
   - Set bit timing for desired baud rate
   - Configure filters/masks
   - Enter Normal or Loopback mode

3. Implement TX/RX:
   - `CAN_Send()` - write to TX buffer, request send
   - `CAN_Receive()` - read from RX buffer, clear flag

---

## Appendix: Microchip Contact (Traditional Chinese)

如需詢問支援 CAN 的替代晶片方案：

> **主旨**：關於 PIC32CM3204GV00048 與 APP-MASTERS25-1 開發板的 CAN 功能詢問
>
> 您好，我是大學教授車輛工程相關課程的教師。這幾天深度研究了 PIC32CM3204GV00048 這顆晶片，並使用 APP-MASTERS25-1 開發板進行實作教學準備。
>
> 發現此 MCU 沒有內建 CAN 控制器，想請教是否有 PIN 腳相容且具備 CAN 的替代方案？

---

*Course Version: 2025.01*
*Hardware: PIC32CM3204GV00048 + ATA6561*
*UART: SERCOM0 (PA08/PA09) via MCP2221*
