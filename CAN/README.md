# CAN Bus Protocol

## What This Demo Does

This demo **simulates a Vehicle ECU** (Engine Control Unit) sending CAN messages on an automotive network. You will see:

1. **Real CAN frame structure** - ID, Data bytes, CRC checksum
2. **Data encoding/decoding** - How sensor values become CAN data
3. **Live dashboard** - Decoded values displayed like a real instrument cluster

---

## Quick Start Guide

### Step 1: Connect Hardware
- Connect USB cable to the APP-MASTERS25-1 board
- Board powers on, LEDs may briefly light up

### Step 2: Open Terminal
- Open **MPLAB Data Visualizer** (or any serial terminal)
- Select COM port: **MCP2221** (e.g., `tty.usbmodem1101`)
- Settings: **115200 baud, 8-N-1**
- Click **Connect**

### Step 3: Reset the Board
- Press the **RESET** button on the board
- You should see the welcome message appear

---

## MCC Melody Configuration (For New Projects)

If you are creating this project from scratch, follow these steps to configure MCC Melody.

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

**Purpose:** Use internal 8MHz oscillator for stable operation (no external crystal needed)

1. Click **Clock Configuration** in Resource Management
2. Set the following:

| Setting | Value | Why |
|---------|-------|-----|
| **OSC8M** | Enable ✓ | Internal 8MHz oscillator |
| **GCLK Generator 0 Source** | OSC8M | Main CPU clock |
| **GCLK Generator 0 Divider** | 1 | Run at full 8MHz |

**Important:** Do NOT enable XOSC (external crystal) unless you have one on your board.

### Step 4: Configure UART (SERCOM0)

**Purpose:** Serial communication with PC via onboard MCP2221 USB-UART chip

1. In **Device Resources** panel, expand **Peripherals → SERCOM**
2. Click **+** next to **SERCOM0** to add it
3. Configure SERCOM0:

| Setting | Value | Why |
|---------|-------|-----|
| **Operating Mode** | USART Internal Clock | Async serial |
| **Baud Rate** | 115200 | Standard terminal speed |
| **Character Size** | 8 bits | Standard |
| **Parity** | None | No parity check |
| **Stop Bits** | 1 | Standard |
| **Transmit Pinout (TXPO)** | PAD[0] | TX on PA08 |
| **Receive Pinout (RXPO)** | PAD[1] | RX on PA09 |

**Why SERCOM0?** The MCP2221 chip on the board is wired to PA08/PA09, which are SERCOM0 pins.

### Step 5: Configure GPIO (LEDs)

**Purpose:** Visual indicators for CAN activity

1. Open **Pin Manager** → Grid View
2. Find PA00 and PA01, set both as **GPIO Output**
3. In **Pin Settings**:

| Pin | Custom Name | Direction | Initial State |
|-----|-------------|-----------|---------------|
| PA00 | LED1 | Output | High |
| PA01 | LED2 | Output | High |
| PA08 | (auto) | SERCOM0_PAD0 | - |
| PA09 | (auto) | SERCOM0_PAD1 | - |

**Why High initial state?** LEDs are active-low, so High = OFF at startup.

### Step 6: Add STDIO (Optional)

**Purpose:** Enable `printf()` function to work with UART

1. In **Device Resources**, find **STDIO**
2. Click **+** to add it
3. In STDIO settings, link it to **SERCOM0**

### Step 7: Generate Code

1. Click the **Generate** button
2. Wait for code generation to complete
3. Close MCC

### Step 8: Verify Clock Configuration (CRITICAL!)

After generating code, check that the clock won't hang:

1. Open `src/config/default/peripheral/clock/plib_clock.c`
2. Find the `SYSCTRL_Initialize()` function
3. Look for this dangerous line:
   ```c
   while(!(SYSCTRL_REGS->SYSCTRL_PCLKSR & SYSCTRL_PCLKSR_XOSCRDY_Msk));
   ```
4. **If present, COMMENT IT OUT** - this waits forever for external crystal:
   ```c
   // while(!(SYSCTRL_REGS->SYSCTRL_PCLKSR & SYSCTRL_PCLKSR_XOSCRDY_Msk));
   ```

### Step 9: Build and Flash

1. Click **Build** (hammer icon)
2. Verify no errors
3. Click **Run** (play icon) to flash the board
4. Open terminal and press RESET to see output

---

## What You Will See

### Phase 1: Welcome Screen (appears immediately after reset)

```
============================================================
       CAN BUS PROTOCOL - EDUCATIONAL DEMO
============================================================

  This demo simulates a Vehicle ECU transmitting
  CAN messages on an automotive network.

  WHAT YOU WILL SEE:
  - Each CAN frame with ID, Data, and CRC
  - The meaning of the data being sent
  - A dashboard showing decoded values

  VEHICLE SIMULATION CYCLE:
  1. Idle      -> Engine at 850 RPM, stopped
  2. Accelerate-> Throttle up, speed increases
  3. Cruise    -> Steady 80 km/h at 2200 RPM
  4. Brake     -> Slowing down to stop
  (Cycle repeats)

  LED INDICATORS:
  - LED1 blinks = CAN frame transmitted
  - LED2 blinks = Dashboard updated

============================================================
  Starting in 3 seconds...
============================================================
```

**What's happening:** The MCU initializes and explains what the demo will show.

---

### Phase 2: CAN Frame Transmission (repeats every 2 seconds)

Each cycle shows a different CAN message:

#### Message 1: Engine RPM
```
>>> Sending ENGINE RPM data...

============================================================
  TRANSMITTING: ENGINE RPM (ID: 0x0C0)
============================================================

  CAN ID:     0x00C0  (decimal: 192)
  Binary:     00011000000  (11-bit standard ID)
  DLC:        2 byte(s)
  Data:       0x03 0x52
  CRC-15:     0x1A3F

  MEANING:    RPM = (Data[0] << 8) | Data[1]

------------------------------------------------------------
  DECODED:    RPM = 850
```

**What's happening:** 
- The "ECU" is sending engine RPM as a 2-byte value
- `0x03 0x52` = (3 × 256) + 82 = 850 RPM
- LED1 blinks when this frame is "transmitted"

#### Message 2: Vehicle Speed
```
>>> Sending VEHICLE SPEED data...

============================================================
  TRANSMITTING: VEHICLE SPEED (ID: 0x0D0)
============================================================

  CAN ID:     0x00D0  (decimal: 208)
  Binary:     00011010000  (11-bit standard ID)
  DLC:        1 byte(s)
  Data:       0x00
  CRC-15:     0x2B1C

  MEANING:    Speed in km/h = Data[0]

------------------------------------------------------------
  DECODED:    Speed = 0 km/h
```

**What's happening:**
- Speed is sent as a single byte (0-255 km/h range)
- Data `0x00` = 0 km/h (vehicle stopped)

#### Message 3: Throttle & Brake
```
>>> Sending THROTTLE & BRAKE data...

============================================================
  TRANSMITTING: THROTTLE & BRAKE (ID: 0x0F0)
============================================================

  CAN ID:     0x00F0  (decimal: 240)
  Binary:     00011110000  (11-bit standard ID)
  DLC:        2 byte(s)
  Data:       0x00 0x01
  CRC-15:     0x3D7E

  MEANING:    Throttle% = Data[0], Brake = Data[1]

------------------------------------------------------------
  DECODED:    Throttle = 0%, Brake = ON
```

**What's happening:**
- Two values packed into one CAN message
- Data[0] = Throttle percentage (0-100)
- Data[1] = Brake status (0=OFF, 1=ON)

---

### Phase 3: Dashboard Update

```
>>> Updating DASHBOARD with received data...

+------------------[ DASHBOARD ]------------------+
|                                                 |
|  RPM:      850    [##------------------]        |
|  SPEED:    0      km/h                          |
|  COOLANT:  45     C   [WARMING UP]              |
|  THROTTLE: 0      %    BRAKE: ON                |
|                                                 |
+-------------------------------------------------+

  The dashboard above shows data DECODED from
  the CAN messages sent by the Engine ECU.

  In a real car, multiple ECUs communicate
  this way over the CAN bus network.
```

**What's happening:**
- LED2 blinks when dashboard updates
- All previously sent CAN data is displayed as human-readable values
- This simulates how an instrument cluster decodes CAN messages

---

## Vehicle Simulation Cycle

The simulated vehicle goes through these states automatically:

| State | Duration | RPM | Speed | Throttle | Brake | What's Simulated |
|-------|----------|-----|-------|----------|-------|------------------|
| **Idle** | ~20 sec | 850 | 0 | 0% | ON | Car stopped, engine running |
| **Accelerate** | ~16 sec | 850→3100 | 0→80 | 0%→70% | OFF | Driver pressing gas pedal |
| **Cruise** | ~30 sec | 2200 | 80 | 30% | OFF | Steady highway driving |
| **Brake** | ~16 sec | 2200→800 | 80→0 | 0% | ON | Slowing to a stop |

The cycle then repeats from Idle.

### Understanding RPM vs Speed

**RPM = Engine Revolutions Per Minute** (not wheel/tire rotations!)

| Question | Answer |
|----------|--------|
| Why RPM=850 but Speed=0? | Engine is **idling** - running but not moving the car |
| When does this happen? | Car in Park, or stopped at a red light |
| How does engine power reach wheels? | Through the **transmission** (gearbox) |

```
Engine (RPM) ──→ Transmission ──→ Wheels ──→ Speed (km/h)
                     │
            In Park/Neutral: disconnected
            In Drive: connected via gears
```

**Real-world example:** Your car in the driveway with engine running and gear in Park:
- Engine RPM: ~800 (you hear it running)
- Speed: 0 km/h (car doesn't move)
- Fuel is being consumed, but no motion is produced

---

## LED Behavior

| LED | Trigger | Meaning |
|-----|---------|---------|
| **LED1** | Brief flash | A CAN frame was "transmitted" |
| **LED2** | Brief flash | Dashboard display updated |
| **Both alternate** | At startup | System initializing |

---

## Understanding CAN Frame Fields

Each transmitted frame shows these fields:

| Field | Example | Explanation |
|-------|---------|-------------|
| **CAN ID** | `0x00C0` | 11-bit message identifier. Lower ID = higher priority |
| **Binary** | `00011000000` | The ID shown as bits (sent on the bus this way) |
| **DLC** | `2` | Data Length Code - how many data bytes follow |
| **Data** | `0x03 0x52` | The actual payload (sensor values encoded) |
| **CRC-15** | `0x1A3F` | Error detection checksum (calculated automatically) |

---

## Standard Automotive CAN IDs Used

| ID | Message | Priority | Why This ID? |
|----|---------|----------|--------------|
| `0x0C0` | Engine RPM | High | Low ID = critical engine data |
| `0x0D0` | Vehicle Speed | High | Safety-related (ABS, stability) |
| `0x0F0` | Throttle/Brake | High | Safety-related (collision avoidance) |

**Key Concept:** Lower CAN ID = Higher Priority

In a real car with multiple ECUs transmitting simultaneously, the message with the **lowest ID wins** the bus arbitration and transmits first.

---

## Hardware Notes

### Why This is a Simulation

The **PIC32CM3204GV00048** MCU does **NOT** have a built-in CAN controller. The board has:

- ✅ **ATA6561** - CAN Transceiver (converts digital ↔ differential signals)
- ❌ **No CAN Controller** - Cannot generate/parse CAN frames in hardware

To add real CAN functionality, you would need an external **MCP2515** CAN controller module connected via SPI.

### Pin Assignments

| Function | MCU Pin | Notes |
|----------|---------|-------|
| UART TX | PA08 | To MCP2221 (USB-Serial) |
| UART RX | PA09 | From MCP2221 |
| LED1 | PA00 | TX indicator (active-low) |
| LED2 | PA01 | Dashboard indicator (active-low) |

---

## Troubleshooting

### "I don't see any output"

1. **Check COM port** - Must be the MCP2221 port, not the debugger
2. **Check baud rate** - Must be exactly **115200**
3. **Press RESET** - The welcome message only appears once at startup

### "LEDs are always ON"

1. **Clock issue** - The MCU may be stuck waiting for an external crystal
2. **Fix**: Open `src/config/default/peripheral/clock/plib_clock.c` and comment out any XOSC wait loops

### "Output looks garbled"

1. **Baud rate mismatch** - Verify terminal is set to 115200
2. **Try a different terminal** - Some terminals handle line endings differently

---

## Next Steps

After understanding this simulation, you can:

1. **Modify the code** - Change CAN IDs, add new messages
2. **Add MCP2515** - Connect external CAN controller for real CAN
3. **Connect to real vehicle** - Use OBD-II adapter to read actual car data

---

*Hardware: PIC32CM3204GV00048 + ATA6561 | UART: SERCOM0 @ 115200 baud*
