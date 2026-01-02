# CAN Bus Protocol & Intelligent Vehicle Engineering
## 業師協同教學 - 合作課程教學指南

**For Instructors with Chip Design/Verification Background**  
*Bridging Hardware Protocol Expertise to Automotive Systems*

---

## Table of Contents

1. [Introduction for Hardware Engineers](#introduction-for-hardware-engineers)
2. [CAN Protocol Deep Dive](#can-protocol-deep-dive)
3. [Vehicle Network Architecture](#vehicle-network-architecture)
4. [Understanding the Hardware Stack](#understanding-the-hardware-stack)
5. [Lab Course Plan](#lab-course-plan)
6. [Real-World Examples](#real-world-examples)

---

## Introduction for Hardware Engineers

If you're experienced in chip design, verification, or hardware protocols (SPI, I2C, UART), you already understand **90% of what CAN is**. Here's how your existing knowledge maps:

| Your Expertise | CAN Equivalent |
|----------------|----------------|
| Bus arbitration (like AXI/AHB) | CAN's CSMA/CD + Priority-based ID |
| Start/Stop conditions (I2C) | SOF (Start of Frame) / EOF |
| Parity/Checksum | CRC-15 polynomial |
| Synchronous clocking | Bit stuffing for sync recovery |
| Multi-master (SPI daisy-chain) | Multi-master (any node can transmit) |

**The Key Difference:** CAN is a **broadcast** protocol on a **differential pair** designed for **harsh automotive environments** with built-in **error handling** and **fault tolerance**.

Think of it like this:
- **I2C** = Polite conversation in a quiet room
- **SPI** = High-speed point-to-point data transfer  
- **CAN** = Shouting important messages in a noisy factory where everyone listens

---

## CAN Protocol Deep Dive

### CAN Frame Structure (The 4-bit DLC Confusion Explained)

Here's the CAN frame you'll see transmitted:

```
+-----+-------------+-----+---+----+------------+-----+------+-----+-----+-----+
| SOF | Identifier  | RTR |IDE| r0 |    DLC     |  DATA      | CRC | ACK | EOF |
| 1b  |   11/29b    | 1b  |1b | 1b |   4 bits   | 0-64 bytes |15+1b| 2b  | 7b  |
+-----+-------------+-----+---+----+------------+------------+-----+-----+-----+
```

#### ⚠️ DLC Field Explained (Common Confusion!)

**Q: DLC is only 4 bits, but it represents 0-64 bytes?**

**A: Correct!** The 4-bit DLC is an **encoded value**, not a direct byte count.

| DLC Value (4 bits) | Classical CAN | CAN FD |
|-------------------|---------------|--------|
| 0000 (0) | 0 bytes | 0 bytes |
| 0001 (1) | 1 byte | 1 byte |
| ... | ... | ... |
| 1000 (8) | 8 bytes | 8 bytes |
| 1001 (9) | ❌ Invalid | 12 bytes |
| 1010 (10) | ❌ Invalid | 16 bytes |
| 1011 (11) | ❌ Invalid | 20 bytes |
| 1100 (12) | ❌ Invalid | 24 bytes |
| 1101 (13) | ❌ Invalid | 32 bytes |
| 1110 (14) | ❌ Invalid | 48 bytes |
| 1111 (15) | ❌ Invalid | 64 bytes |

**Real-World Example:**
```
If you see DLC = 0x02 in a CAN frame:
- Classical CAN: 2 bytes of data
- CAN FD: 2 bytes of data

If you see DLC = 0x0D (13) in a CAN FD frame:
- This means 32 bytes of data, NOT 13 bytes!
```

**Why this weird encoding?** CAN FD needed to support larger payloads (up to 64 bytes) while maintaining backward compatibility with the 4-bit DLC field from Classical CAN. Since values 0-8 were already used, 9-15 were repurposed with a non-linear mapping.

---

### Classical CAN vs CAN FD: What's the Difference?

**CAN FD = CAN with Flexible Data-rate**

Think of it like upgrading from USB 2.0 to USB 3.0 - same connector shape, but faster and more capacity.

#### Side-by-Side Comparison

| Feature | Classical CAN | CAN FD |
|---------|---------------|--------|
| **Max Data Payload** | 8 bytes | 64 bytes |
| **Bit Rate (Arbitration)** | Up to 1 Mbps | Up to 1 Mbps |
| **Bit Rate (Data Phase)** | Same as arbitration | Up to 8 Mbps |
| **DLC Encoding** | 0-8 (direct) | 0-8 direct, 9-15 non-linear |
| **CRC** | CRC-15 | CRC-17 (≤16B) or CRC-21 (>16B) |
| **Error Detection** | Good | Better (stuff bit counter) |
| **Backward Compatible** | N/A | Yes (can coexist on same bus) |

#### Frame Format Comparison

**Classical CAN Frame:**
```
┌─────┬────────────┬─────┬─────┬─────┬──────────┬────────┬─────┬─────┐
│ SOF │ Identifier │ RTR │ IDE │ r0  │   DLC    │  DATA  │ CRC │ EOF │
│ 1b  │   11/29b   │ 1b  │ 1b  │ 1b  │   4b     │ 0-8B   │ 15b │ 7b  │
└─────┴────────────┴─────┴─────┴─────┴──────────┴────────┴─────┴─────┘
                   └─────────── All at same bit rate ───────────────┘
```

**CAN FD Frame:**
```
┌─────┬────────────┬─────┬─────┬─────┬─────┬─────┬──────────┬────────┬──────┬─────┐
│ SOF │ Identifier │ r1  │ IDE │ FDF │ r0  │ BRS │   DLC    │  DATA  │ CRC  │ EOF │
│ 1b  │   11/29b   │ 1b  │ 1b  │ 1b  │ 1b  │ 1b  │   4b     │ 0-64B  │17/21b│ 7b  │
└─────┴────────────┴─────┴─────┴─────┴─────┴─────┴──────────┴────────┴──────┴─────┘
                                           │                │
                   ┌───────────────────────┘                └──────────────────┐
                   │   ARBITRATION PHASE                     DATA PHASE        │
                   │   (1 Mbps max)                          (up to 8 Mbps)    │
                   └──────────────────────────────────────────────────────────-┘
```

#### New Control Bits in CAN FD

| Bit | Name | Purpose |
|-----|------|---------|
| **FDF** | Flexible Data-rate Format | 0 = Classical CAN, 1 = CAN FD |
| **BRS** | Bit Rate Switch | 0 = Same rate, 1 = Switch to faster data rate |
| **ESI** | Error State Indicator | 0 = Error Active, 1 = Error Passive |

#### The "Bit Rate Switch" Magic

This is the clever part! CAN FD can run at **two different speeds** within the same frame:

```
TIME →
        ┌────────────────────────────────────────────────────────────────────┐
        │                         CAN FD FRAME                               │
        │  ╔════════════════╗                    ╔════════════════════════╗  │
        │  ║ ARBITRATION    ║                    ║     DATA PHASE         ║  │
        │  ║ (1 Mbps)       ║                    ║     (5 Mbps example)   ║  │
        │  ╚════════════════╝                    ╚════════════════════════╝  │
        │  │← Slower here   │← BRS=1 triggers   →│   Faster here          │  │
        │     because multiple                      Only one sender now,     │
        │     nodes may be                          safe to go fast!         │
        │     competing for bus                                              │
        └────────────────────────────────────────────────────────────────────┘

WHY TWO SPEEDS?
- Arbitration must be slow (all nodes must hear each other for collision detection)
- Once arbitration is won, only ONE node is sending - can go faster!
```

#### Real-World Example: Firmware Update Over CAN

**Classical CAN (8 bytes per frame):**
```
Firmware size: 256 KB = 262,144 bytes
Bytes per frame: 8
Frames needed: 262,144 / 8 = 32,768 frames
Time @ 1 Mbps: ~5 minutes
```

**CAN FD (64 bytes per frame @ 5 Mbps data rate):**
```
Firmware size: 256 KB = 262,144 bytes
Bytes per frame: 64
Frames needed: 262,144 / 64 = 4,096 frames (8x fewer!)
Time @ 5 Mbps data: ~30 seconds (10x faster!)
```

**Result:** CAN FD firmware update is **~10x faster** than Classical CAN!

#### Why Did We Need CAN FD?

**Problem 1: Modern cars need more data**
- ADAS (Advanced Driver Assistance) sensors generate huge data
- Classical CAN's 8-byte limit was designed in 1986
- Sending a camera frame over 8-byte packets = too slow

**Problem 2: Cybersecurity**
- Adding encryption/authentication requires more bytes
- AES-128 block = 16 bytes (already exceeds Classical CAN!)
- CAN FD's 64 bytes allows room for security overhead

**Problem 3: Diagnostics**
- Modern ECU calibration data is large
- UDS (diagnostic protocol) benefits from larger payloads
- Faster firmware updates = less dealer service time

#### Backward Compatibility

CAN FD is **backward compatible** with Classical CAN:

```
┌────────────────────────────────────────────────────────────────────────┐
│                         MIXED CAN/CAN-FD NETWORK                       │
├────────────────────────────────────────────────────────────────────────┤
│                                                                        │
│    CAN BUS                                                             │
│  ══════════════════════════════════════════════════════════            │
│      │           │           │           │           │                 │
│  ┌───┴───┐   ┌───┴───┐   ┌───┴───┐   ┌───┴───┐   ┌───┴───┐             │
│  │ ECU A │   │ ECU B │   │ ECU C │   │ ECU D │   │ ECU E │             │
│  │CAN FD │   │CAN FD │   │Classic│   │CAN FD │   │Classic│             │
│  │capable│   │capable│   │ only  │   │capable│   │ only  │             │
│  └───────┘   └───────┘   └───────┘   └───────┘   └───────┘             │
│                                                                        │
│  RULES:                                                                │
│  • CAN FD nodes CAN send Classical CAN frames (everyone receives)      │
│  • CAN FD nodes CAN send CAN FD frames (only FD nodes receive)         │
│  • Classical nodes IGNORE CAN FD frames (see FDF=1, don't respond)     │
│  • Classical nodes CANNOT disrupt CAN FD frames                        │
│                                                                        │
└────────────────────────────────────────────────────────────────────────┘
```

#### When to Use Which?

| Use Case | Recommendation | Why |
|----------|----------------|-----|
| Simple sensors (temp, pressure) | Classical CAN | 8 bytes is plenty, legacy compatible |
| Safety-critical (ABS, airbag) | Classical CAN | Proven, certified, well-understood |
| ADAS (radar, lidar data) | CAN FD | Need larger payloads |
| Firmware updates | CAN FD | 10x faster updates |
| Secure communication | CAN FD | Room for encryption overhead |
| New vehicle platforms | CAN FD | Future-proof design |

---

### CRC-15 Calculation

CAN uses a 15-bit CRC with polynomial: **x¹⁵ + x¹⁴ + x¹⁰ + x⁸ + x⁷ + x⁴ + x³ + 1**

In hex: `0x4599`

```c
uint16_t crc15_calculate(uint8_t *data, uint16_t len) {
    uint16_t crc = 0;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= ((uint16_t)data[i] << 7);
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x4000) {
                crc = (crc << 1) ^ 0x4599;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc & 0x7FFF;  // 15 bits
}
```

### Bit Stuffing (Clock Recovery Mechanism)

After 5 consecutive identical bits, a complementary bit is inserted:

```
Original:   1 1 1 1 1 1 0 0 0 0 0 0
After stuff: 1 1 1 1 1 [0] 1 0 0 0 0 0 [1] 0
                       ^              ^
                   stuff bits (not counted in data)
```

**Why?** The CAN bus has no separate clock line. Receivers synchronize by detecting bit transitions. Too many consecutive identical bits = no transitions = clock drift.

---

## Vehicle Network Architecture

### Traditional In-Vehicle Network

Modern vehicles are essentially **computers on wheels** with 70-150 ECUs (Electronic Control Units) communicating via multiple bus systems:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         VEHICLE NETWORK ARCHITECTURE                        │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   ┌──────────────────────────────────────────────────────────────────────┐  │
│   │                    INFOTAINMENT / TELEMATICS                         │  │
│   │  ┌──────┐   ┌──────┐   ┌──────┐   ┌──────┐                           │  │
│   │  │ Head │   │ Radio│   │ GPS  │   │Camera│ ══════ ETHERNET (100Mbps) │  │
│   │  │ Unit │   │      │   │      │   │System│       or MOST/LVDS        │  │
│   │  └──┬───┘   └──┬───┘   └──┬───┘   └──┬───┘                           │  │
│   └─────┼──────────┼──────────┼──────────┼───────────────────────────────┘  │
│         │          │          │          │                                  │
│         └──────────┴────┬─────┴──────────┘                                  │
│                         │                                                   │
│                    ┌────┴────┐                                              │
│                    │ GATEWAY │  <── Protocol Translation Hub                │
│                    │   ECU   │      CAN ↔ LIN ↔ FlexRay ↔ Ethernet          │
│                    └────┬────┘                                              │
│              ┌──────────┼──────────┐                                        │
│              │          │          │                                        │
│   ┌──────────┴───┐ ┌────┴────┐ ┌───┴──────────┐                             │
│   │   POWERTRAIN │ │  BODY   │ │   CHASSIS    │                             │
│   │  ════════════│ │═════════│ │══════════════│                             │
│   │  CAN-HS      │ │ CAN/LIN │ │  CAN-HS      │                             │
│   │  (500kbps)   │ │(125kbps)│ │  (500kbps)   │                             │
│   │              │ │         │ │              │                             │
│   │ ┌──────┐     │ │┌──────┐ │ │ ┌─────────┐  │                             │
│   │ │Engine│     │ ││Window│ │ │ │   ABS   │  │                             │
│   │ │ ECU  │     │ ││Motor │ │ │ │ Module  │  │                             │
│   │ └──┬───┘     │ │└──┬───┘ │ │ └────┬────┘  │                             │
│   │    │         │ │   │     │ │      │       │                             │
│   │ ┌──┴──┐      │ │┌──┴───┐ │ │ ┌────┴────┐  │                             │
│   │ │Trans│      │ ││Door  │ │ │ │Steering │  │                             │
│   │ │ ECU │      │ ││Locks │ │ │ │ Assist  │  │                             │
│   │ └─────┘      │ │└──────┘ │ │ └─────────┘  │                             │
│   │              │ │         │ │              │                             │
│   │ ┌─────────┐  │ │┌──────┐ │ │ ┌─────────┐  │                             │
│   │ │Fuel Inj │  │ ││Lights│ │ │ │Airbag   │  │                             │
│   │ │ System  │  │ ││      │ │ │ │ System  │  │                             │
│   │ └─────────┘  │ │└──────┘ │ │ └─────────┘  │                             │
│   └──────────────┘ └─────────┘ └──────────────┘                             │
│                                                                             │
│   LEGEND:  ═══ High-speed CAN (500kbps)                                     │
│            ─── Low-speed CAN/LIN (125kbps or lower)                         │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### The Gateway ECU - The Central Nervous System

The **Gateway ECU** is critical because:

1. **Protocol Translation:** Converts between CAN, LIN, FlexRay, and Ethernet
2. **Security Firewall:** Isolates safety-critical networks from infotainment
3. **Message Routing:** Decides which messages cross network boundaries
4. **Diagnostic Access:** OBD-II port connects through the gateway

**Real Example - What happens when you press the brake pedal:**
```
BRAKE PEDAL PRESSED
       │
       ▼
┌──────────────┐
│ Brake Switch │ (Simple contact closure)
└──────┬───────┘
       │ LIN message
       ▼
┌──────────────┐
│  Body ECU    │ (Aggregates body signals)
└──────┬───────┘
       │ CAN message: ID=0x180, Data=[0x01]
       ▼
┌──────────────┐
│   GATEWAY    │ (Routes to relevant networks)
└──────┬───────┘
       │
   ┌───┴────┬────────────┐
   ▼        ▼            ▼
┌──────┐ ┌─────┐   ┌─────────┐
│Engine│ │ ABS │   │Brake    │
│ ECU  │ │     │   │Light ECU│
└──────┘ └─────┘   └─────────┘
  │         │           │
  │         │           │
Cut fuel   Prepare    Turn on
on coast   pressure   brake lights
```

### Single Pair Ethernet (10BASE-T1S) - The Future

**Problem with Gateway Architecture:**
- Latency (messages must traverse gateway)
- Single point of failure
- Protocol conversion overhead
- Cost and complexity

**Solution: Single Pair Ethernet (SPE)**

```
┌─────────────────────────────────────────────────────────────────────────────┐
│              FUTURE: ETHERNET-BASED VEHICLE ARCHITECTURE                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│                           ETHERNET BACKBONE                                 │
│   ═══════════════════════════════════════════════════════════════           │
│         │              │               │              │                     │
│     ┌───┴───┐     ┌────┴─────┐    ┌────┴────┐    ┌────┴───┐                 │
│     │Compute│     │Telematics│    │ Camera  │    │ Sensor │                 │
│     │ Unit  │     │   Unit   │    │ System  │    │ Fusion │                 │
│     └───────┘     └──────────┘    └─────────┘    └────────┘                 │
│                                                                             │
│   10BASE-T1S BUS (Single Twisted Pair - Multi-drop)                         │
│   ───┬──────┬──────┬──────┬──────┬──────┬──────┬───                         │
│      │      │      │      │      │      │      │                            │
│    ┌─┴─┐  ┌─┴─┐  ┌─┴─┐  ┌─┴─┐  ┌─┴─┐  ┌─┴─┐  ┌─┴─┐                          │
│    │ECU│  │ECU│  │ECU│  │ECU│  │ECU│  │ECU│  │ECU│                          │
│    └───┘  └───┘  └───┘  └───┘  └───┘  └───┘  └───┘                          │
│                                                                             │
│   ADVANTAGES:                                                               │
│   ✓ NO GATEWAY NEEDED - All nodes speak same protocol                       │
│   ✓ Reduced wiring (single pair vs 2-4 pairs)                               │
│   ✓ Multi-drop topology like CAN (up to 8 nodes per segment)                │
│   ✓ Up to 10 Mbps (vs CAN's 1 Mbps max)                                     │
│   ✓ Familiar IP/TCP/UDP stack                                               │
│   ✓ Same cable, same connectors as CAN                                      │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Why SPE is Revolutionary:**
| Feature | CAN | 10BASE-T1S |
|---------|-----|------------|
| Speed | 1 Mbps | 10 Mbps |
| Wires | 2 (differential) | 2 (single pair) |
| Protocol | CAN-specific | Standard Ethernet |
| Gateway needed | Yes | No |
| Max nodes | ~30 | 8 per segment |
| IP compatible | No | Yes |

**Real Impact:**
- Self-driving cars generate **4TB/hour** of sensor data
- CAN can handle **40KB/hour** realistically
- Ethernet backbone is **mandatory** for ADAS/autonomous vehicles
- SPE allows simple sensors to use Ethernet without gateway conversion

---

## Understanding the Hardware Stack

### The CAN Communication Stack

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        CAN HARDWARE ARCHITECTURE                            │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  YOUR APPLICATION (main.c)                                                  │
│         │                                                                   │
│         │ "Send RPM = 2200"                                                 │
│         ▼                                                                   │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │                       CAN CONTROLLER                                 │   │
│  │  ┌────────────────────────────────────────────────────────────────┐  │   │
│  │  │ • Message Filtering (which IDs to receive)                     │  │   │
│  │  │ • TX/RX Message Buffers (store pending messages)               │  │   │
│  │  │ • Frame Assembly (SOF + ID + DLC + Data + CRC + EOF)           │  │   │
│  │  │ • Bit Timing & Synchronization                                 │  │   │
│  │  │ • Error Detection & Handling (ACK, CRC, Form, Stuff errors)    │  │   │
│  │  │ • Arbitration (who wins when multiple nodes transmit)          │  │   │
│  │  │ • Retransmission (automatic retry on failure)                  │  │   │
│  │  └────────────────────────────────────────────────────────────────┘  │   │
│  │                                                                      │   │
│  │  Examples: MCP2515 (SPI), MCP2518FD (CAN FD), or built-in MCU CAN    │   │
│  └──────┬─────────────────────────────────────────────────────────┬─────┘   │
│         │                                                         │         │
│         TX (digital 0/1)                                 RX (digital 0/1)   │
│         │                                                         │         │
│         ▼                                                         │         │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │                       CAN TRANSCEIVER                                │   │
│  │  ┌────────────────────────────────────────────────────────────────┐  │   │
│  │  │ • Converts digital TX/RX ↔ differential CAN_H/CAN_L            │  │   │
│  │  │ • ESD Protection (survives automotive voltage spikes)          │  │   │
│  │  │ • Slope Control (reduces EMI)                                  │  │   │
│  │  │ • Bus Fault Protection (short to battery/ground)               │  │   │
│  │  │ • NO PROTOCOL AWARENESS - just voltage conversion              │  │   │
│  │  └────────────────────────────────────────────────────────────────┘  │   │
│  │                                                                      │   │
│  │  Examples: ATA6561 (on your board), MCP2551, TJA1050                 │   │
│  └──────┬─────────────────────────────────────────────────┬─────────────┘   │
│         │                                                 │                 │
│         │CAN_H (2.5-3.5V dominant, 2.5V recessive)        │                 │
│         │CAN_L (1.5-2.5V dominant, 2.5V recessive)        │                 │
│         ▼                                                 │                 │
│  ═══════════════════════════════════════════════════════════════════════    │
│                           CAN BUS (Twisted Pair)                            │
│              (Up to 40m @ 1Mbps, or 1km @ 50kbps)                           │
│  ═══════════════════════════════════════════════════════════════════════    │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Your Board's Situation (APP-MASTERS25-1)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    YOUR HARDWARE CONFIGURATION                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌────────────────────────────────────────────────────────────┐             │
│  │              PIC32CM3204GV00048 (Your MCU)                 │             │
│  │                                                            │             │
│  │  ┌─────────────────────────────────────────────────────┐  │             │
│  │  │                                                     │  │             │
│  │  │   ✓ SERCOM (UART/SPI/I2C)                          │  │             │
│  │  │   ✓ ADC                                             │  │             │
│  │  │   ✓ GPIO                                            │  │             │
│  │  │   ✓ Timers                                          │  │             │
│  │  │   ✗ NO CAN CONTROLLER  ← This is the limitation     │  │             │
│  │  │                                                     │  │             │
│  │  └─────────────────────────────────────────────────────┘  │             │
│  │                                                            │             │
│  │         PA13 ─────────┬──────── PA12                      │             │
│  │         (SPI_CLK)     │        (SPI_MOSI)                 │             │
│  │                       │                                    │             │
│  └───────────────────────┼────────────────────────────────────┘             │
│                          │                                                   │
│          ┌───────────────┴───────────────┐                                  │
│          │                               │                                  │
│          ▼                               ▼                                  │
│  ┌───────────────┐             ┌───────────────────┐                        │
│  │   ATA6561     │             │    MCP2515        │                        │
│  │  (ON BOARD)   │             │  (NOT PRESENT)    │                        │
│  │               │             │                   │                        │
│  │  Transceiver  │             │  CAN Controller   │                        │
│  │  ONLY         │             │  (via SPI)        │                        │
│  │               │             │                   │                        │
│  │ TXD ←── ?     │             │ CS, SCK, MOSI,    │                        │
│  │ RXD ──→ ?     │             │ MISO, INT         │                        │
│  │               │             │       │           │                        │
│  │  CAN_H ═══════╪═════════════╪═══════╪═══════════╪═══ CAN BUS            │
│  │  CAN_L ═══════╪═════════════╪═══════╪═══════════╪═══                     │
│  └───────────────┘             └───────────────────┘                        │
│                                                                              │
│  RESULT: Without MCP2515, the ATA6561 transceiver has nothing              │
│          to connect to! The MCU can't generate CAN frames.                  │
│                                                                              │
│  OUR SOLUTION: SOFTWARE SIMULATION                                          │
│  - We simulate the CAN protocol in software                                 │
│  - Display frames on UART terminal and OLED                                 │
│  - Educational value: understand frame structure, arbitration, CRC         │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### What Each Component Does

| Component | What it Does | What it Doesn't Do |
|-----------|--------------|-------------------|
| **MCU (PIC32CM)** | Runs application code, reads sensors, controls peripherals | Generate/receive CAN frames (no built-in CAN) |
| **CAN Controller (MCP2515)** | Handles all CAN protocol: framing, arbitration, error handling, ACK | Drive the physical bus voltages |
| **CAN Transceiver (ATA6561)** | Converts digital 0/1 to differential voltages | Understand CAN protocol |

**Analogy:**
- MCU = Your brain ("I want to send: Hello")
- CAN Controller = Postal service (packages, addresses, delivery confirmation)
- CAN Transceiver = Mail truck (just moves packages, doesn't read them)

Without the postal service (CAN Controller), having a mail truck (Transceiver) doesn't help you send letters!

---

## Lab Course Plan

### Overview

| Lab | Topic | Hardware Used | Learning Objectives |
|-----|-------|---------------|---------------------|
| Lab 1 | MCU Basics & GPIO | LEDs, Buttons | GPIO, Clock, Interrupts |
| Lab 2 | Serial Communication | UART, Terminal | SERCOM, Protocols |
| Lab 3 | CAN Protocol Simulation | Full Demo | CAN Frames, Vehicle Data |
| Lab 4 | OLED Dashboard Integration | OLED Display | I2C, Graphics, UI Design |

---

### Lab 1: MCU Fundamentals & GPIO

**Duration:** 2-3 hours

**Learning Objectives:**
- Understand MCU architecture (ARM Cortex-M0+)
- Configure system clock using MCC Melody
- Control GPIO pins (LEDs, buttons)
- Implement simple state machines

**Hands-On Activities:**

1. **LED Blink - The "Hello World"**
   ```c
   while(1) {
       LED1_Set();      // Turn ON (active low = set LOW)
       delay_ms(500);
       LED1_Clear();    // Turn OFF
       delay_ms(500);
   }
   ```

2. **Button-Controlled LED**
   ```c
   while(1) {
       if (SW1_Get() == 0) {  // Button pressed (active low)
           LED1_Set();
       } else {
           LED1_Clear();
       }
   }
   ```

3. **RGB LED Color Cycling**
   ```c
   // Red
   PORT_REGS->GROUP[1].PORT_OUTSET = (1 << 9);   // PB09 = RED ON
   delay_ms(1000);
   PORT_REGS->GROUP[1].PORT_OUTCLR = (1 << 9);   // RED OFF
   
   // Green
   PORT_REGS->GROUP[0].PORT_OUTSET = (1 << 4);   // PA04 = GREEN ON
   // ... etc
   ```

**Real-World Application:**
- **Automotive:** Dashboard warning lights, turn signal blinker
- **Industrial:** Status indicators on factory equipment
- **Consumer:** Power button LED on laptops/phones

**Checkpoint Questions:**
- Why are LEDs "active-low" on this board? (Trace the circuit!)
- What's the purpose of pull-up resistors on buttons?
- What happens if you forget to configure the clock?

---

### Lab 2: Serial Communication Fundamentals

**Duration:** 2-3 hours

**Learning Objectives:**
- Understand UART protocol (start bit, data bits, stop bit)
- Configure SERCOM peripheral for UART
- Implement printf-style debugging
- Compare UART vs SPI vs I2C

**Hands-On Activities:**

1. **Basic UART Output**
   ```c
   void uart_putc(char c) {
       while(!(SERCOM0_REGS->USART_INT.SERCOM_INTFLAG & 
               SERCOM_USART_INT_INTFLAG_DRE_Msk));
       SERCOM0_REGS->USART_INT.SERCOM_DATA = c;
   }
   
   void print(const char* str) {
       while(*str) uart_putc(*str++);
   }
   ```

2. **Formatted Number Output**
   ```c
   void print_int(int32_t val) {
       char buf[12];
       // Convert to string and print
   }
   
   print("RPM: ");
   print_int(2200);
   print("\r\n");
   // Output: "RPM: 2200"
   ```

3. **ADC Reading with UART Display**
   ```c
   ADC_REGS->ADC_SWTRIG = ADC_SWTRIG_START_Msk;
   while(!(ADC_REGS->ADC_INTFLAG & ADC_INTFLAG_RESRDY_Msk));
   uint16_t adc_value = ADC_REGS->ADC_RESULT;
   
   print("Potentiometer: ");
   print_int((adc_value * 100) / 4095);  // Convert to percentage
   print("%\r\n");
   ```

**Protocol Comparison:**

| Protocol | Wires | Speed | Direction | Use Case |
|----------|-------|-------|-----------|----------|
| **UART** | 2 (TX/RX) | 115200 bps | Point-to-point | Debug, GPS |
| **SPI** | 4+ (CLK/MOSI/MISO/CS) | 10+ Mbps | Master-Slave | Flash, ADC |
| **I2C** | 2 (SDA/SCL) | 100-400 kbps | Multi-master | Sensors, EEPROM |
| **CAN** | 2 (H/L) | 1 Mbps | Multi-master | Automotive |

**Real-World Application:**
- **Automotive:** OBD-II diagnostic port (ISO 9141 uses UART-like K-line)
- **Industrial:** MODBUS serial communication
- **Consumer:** GPS modules, Bluetooth serial modules

**Checkpoint Questions:**
- What happens if TX and RX baud rates don't match?
- Why does UART need a start bit but SPI doesn't?
- How does I2C addressing work vs SPI chip select?

---

### Lab 3: CAN Protocol Simulation

**Duration:** 3-4 hours

**Learning Objectives:**
- Understand CAN frame structure in detail
- Implement CRC-15 calculation
- Simulate bus arbitration
- Decode real automotive data (OBD-II PIDs)

**Hands-On Activities:**

1. **Build a CAN Frame**
   ```c
   typedef struct {
       uint16_t id;        // 11-bit identifier
       uint8_t  dlc;       // Data Length Code (0-8)
       uint8_t  data[8];   // Data bytes
       uint16_t crc;       // CRC-15
   } CAN_Frame;
   
   CAN_Frame frame;
   frame.id = 0x0C0;       // Engine RPM message ID
   frame.dlc = 2;          // 2 bytes of data
   frame.data[0] = 0x08;   // RPM high byte: 8 * 256 = 2048
   frame.data[1] = 0x98;   // RPM low byte: + 152 = 2200
   frame.crc = crc15_calculate(frame.data, frame.dlc);
   ```

2. **Simulate Vehicle Data**
   ```c
   // Real OBD-II PIDs
   #define PID_ENGINE_RPM    0x0C0
   #define PID_VEHICLE_SPEED 0x0D0
   #define PID_THROTTLE_POS  0x110
   #define PID_ENGINE_TEMP   0x050
   
   void send_rpm(uint16_t rpm) {
       frame.id = PID_ENGINE_RPM;
       frame.dlc = 2;
       frame.data[0] = (rpm >> 8) & 0xFF;  // MSB
       frame.data[1] = rpm & 0xFF;         // LSB
       display_frame(&frame);
   }
   ```

3. **Interactive Simulation**
   ```
   Controls:
     SW1 (Hold) = Accelerate  →  RPM↑, Speed↑
     SW2 (Hold) = Brake       →  RPM↓, Speed↓
     Potentiometer = Throttle Position
   
   Watch the CAN frames change in real-time!
   ```

**CAN Frame Example Output:**
```
============================================================
           CAN BUS PROTOCOL - EDUCATIONAL SIMULATION
============================================================

CAN FRAME TRANSMITTED:

  Message:        ENGINE_RPM
  CAN ID:         0x0C0
  DLC:            2 (4-bit field, value 0010)
  Data Bytes:     [0x08] [0x98]
  CRC-15:         0x1A3F

  DECODE:
    Formula:      RPM = (Data[0] << 8) | Data[1]
    Calculation:  RPM = (0x08 × 256) + 0x98
                      = 2048 + 152
                      = 2200 RPM

  BIT REPRESENTATION:
    ID (11 bits):  000 1100 0000 = 0x0C0
    DLC (4 bits):  0010 = 2 bytes
    Data (16 bits): 0000 1000 1001 1000 = 0x0898

============================================================
```

**Real-World Application:**
- **Automotive:** Every car uses CAN for engine management
- **Industrial:** CANopen protocol for factory automation
- **Medical:** CAN in medical devices (ISO 11898)

**Why RPM ≠ 0 when Speed = 0?**
- Engine **idling** maintains ~800 RPM to keep running
- Engine RPM = internal combustion rate
- Vehicle speed = wheel rotation
- Connected through transmission (gears, clutch)

---

### Lab 4: OLED Dashboard Integration

**Duration:** 2-3 hours

**Learning Objectives:**
- Understand I2C protocol in depth
- Interface with SSD1306 OLED controller
- Design professional embedded UI
- Integrate multiple peripherals into coherent system

**Hands-On Activities:**

1. **I2C Communication**
   ```c
   // SERCOM2 configured as I2C Master
   // SDA = PA12, SCL = PA13
   
   void oled_send_cmd(uint8_t cmd) {
       uint8_t buf[2] = {0x00, cmd};  // 0x00 = command mode
       SERCOM2_I2C_Write(0x3C, buf, 2);  // 0x3C = OLED address
       while(SERCOM2_I2C_IsBusy());
   }
   ```

2. **Design the Dashboard**
   ```
   ┌──────────────────────────┐
   │   CAN BUS MONITOR       │  ← Title (inverted colors)
   ├──────────────────────────┤
   │                          │
   │  RPM:  2200              │  ← Large font (8x16)
   │                          │
   ├──────────────────────────┤
   │  SPD: 80km/h  THR: 45%  │  ← Small font (6x8)
   │  BRAKE: [===    ] ON    │  ← Visual indicator
   ├──────────────────────────┤
   │  CAN ID: 0x0C0          │
   │  DATA: 08 98            │  ← Live CAN data
   └──────────────────────────┘
   ```

3. **Complete Integration**
   ```c
   void main_loop(void) {
       // Read inputs
       throttle = read_potentiometer();
       accel = !SW1_Get();
       brake = !SW2_Get();
       
       // Update vehicle model
       update_vehicle_state();
       
       // Generate CAN frame
       build_can_frame();
       
       // Update outputs
       update_leds();
       update_rgb_led();
       update_terminal_display();
       update_oled_display();  // ← New!
   }
   ```

**I2C vs SPI for Displays:**

| Feature | I2C (OLED) | SPI (TFT) |
|---------|------------|-----------|
| Wires | 2 | 4+ |
| Speed | 400 kHz | 40+ MHz |
| Display size | Small (128x64) | Large (320x240+) |
| Power | Low | Higher |
| Cost | Cheap | More expensive |

**Real-World Application:**
- **Automotive:** Instrument cluster displays
- **Industrial:** HMI (Human-Machine Interface) panels
- **Consumer:** Smartwatch displays, IoT device status

**Checkpoint Questions:**
- Why does I2C need pull-up resistors?
- What happens if two I2C devices have the same address?
- Why is the OLED address 0x3C, not 0x78?
  - (Hint: 7-bit vs 8-bit addressing)

---

## Real-World Examples

### Example 1: Reading Your Car's Data with OBD-II

When you connect an OBD-II scanner to your car:

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                          OBD-II REQUEST/RESPONSE                             │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│  YOUR OBD-II SCANNER                           CAR's ECU                      │
│         │                                          │                          │
│         │   REQUEST CAN Frame                      │                          │
│         │   ID: 0x7DF (broadcast request)         │                          │
│         │   Data: [02] [01] [0C] [00] [00] [00]   │                          │
│         │          │    │    │                     │                          │
│         │          │    │    └── PID 0x0C = RPM    │                          │
│         │          │    └── Service 01 = live data │                          │
│         │          └── 2 bytes following           │                          │
│         ├─────────────────────────────────────────>│                          │
│         │                                          │                          │
│         │   RESPONSE CAN Frame                     │                          │
│         │   ID: 0x7E8 (ECU response)              │                          │
│         │   Data: [04] [41] [0C] [1A] [F8] [00]   │                          │
│         │          │    │    │    │    │           │                          │
│         │          │    │    │    └────┴── RPM data│                          │
│         │          │    │    └── PID 0x0C          │                          │
│         │          │    └── Service 01 + 0x40      │                          │
│         │          └── 4 bytes following           │                          │
│         │<─────────────────────────────────────────┤                          │
│         │                                          │                          │
│         │   DECODE:                                │                          │
│         │   RPM = ((0x1A × 256) + 0xF8) / 4       │                          │
│         │       = (6656 + 248) / 4                 │                          │
│         │       = 6904 / 4                         │                          │
│         │       = 1726 RPM  ← Displayed on scanner│                          │
│                                                                               │
└──────────────────────────────────────────────────────────────────────────────┘
```

### Example 2: Tesla's CAN Bus Hack (2020)

Security researchers demonstrated:

```
1. Gained access to CAN bus via infotainment system
2. Sent spoofed CAN messages:
   - ID: 0x129 (Steering angle)
   - Data: [fake steering data]
3. Car's steering ECU accepted the messages!
4. Result: Could influence steering at low speeds

LESSON: CAN has NO AUTHENTICATION
- Any node can send any message ID
- Modern solution: CAN with Secure Onboard Communication (SecOC)
```

### Example 3: Why BMW Uses Different CAN Speeds

```
┌───────────────────────────────────────────────────────────────┐
│               BMW CAN NETWORK EXAMPLE                          │
├───────────────────────────────────────────────────────────────┤
│                                                                │
│  PT-CAN (Powertrain CAN) - 500 kbps                           │
│  ├── Engine ECU (needs fast response for fuel injection)      │
│  ├── Transmission (shift timing is critical)                  │
│  └── ABS/ESP (safety-critical, low latency)                  │
│                                                                │
│  K-CAN (Body CAN) - 100 kbps                                  │
│  ├── Door modules (window/lock status - not urgent)          │
│  ├── Seat controllers (memory positions)                      │
│  └── Climate control                                          │
│                                                                │
│  F-CAN (Chassis CAN) - 500 kbps                              │
│  ├── Active suspension                                        │
│  └── Dynamic stability control                                │
│                                                                │
│  MOST Ring (Fiber Optic) - 25 Mbps                           │
│  ├── Head unit                                                │
│  ├── Amplifier                                                │
│  └── CD changer                                               │
│                                                                │
│  All connected through CENTRAL GATEWAY MODULE                 │
│                                                                │
└───────────────────────────────────────────────────────────────┘
```

### Example 4: How ABS Prevents Wheel Lock

```
SCENARIO: Driver slams brakes on wet road

TIME: 0ms
├── Driver presses brake pedal
├── Brake pressure sensor → CAN message (ID: 0x180)
└── Message broadcast to all ECUs

TIME: 5ms
├── ABS ECU receives brake message
├── Starts monitoring wheel speed sensors
└── Each wheel sensor sends CAN messages @ 10ms interval

TIME: 15ms
├── Front-left wheel speed dropping faster than others
├── Wheel about to lock up!
└── ABS ECU decision time

TIME: 20ms
├── ABS sends CAN message to brake modulator (ID: 0x220)
├── Data: [Release front-left brake pressure]
└── Brake modulator receives and acts

TIME: 25ms
├── Front-left brake pressure reduced
├── Wheel regains traction
└── Process repeats 15-20 times per second

TOTAL LATENCY BUDGET: ~5ms for entire loop
WHY CAN WORKS: Guaranteed message delivery, priority-based arbitration
```

---

## Summary for Instructors

### Key Takeaways for Your Students

1. **CAN is not magic** - It's a well-designed bus protocol with clear engineering trade-offs

2. **Hardware vs Software distinction** - Understanding Controller vs Transceiver is crucial

3. **Real automotive systems are complex** - Multiple networks, gateways, security concerns

4. **Hands-on experience is irreplaceable** - Even simulation builds intuition

5. **Ethernet is the future** - But CAN will remain for decades in legacy vehicles

### Your Expertise Applied

| Your Knowledge | Direct Application |
|----------------|-------------------|
| Protocol verification | CAN bit timing, error frames, arbitration |
| Hardware design | Transceiver circuits, termination, EMI |
| Chip architecture | Peripheral configuration, register access |
| System integration | Multi-ECU communication, gateway design |

### Recommended Follow-up Topics

1. **CAN FD** - Extended data length, higher bit rates
2. **LIN** - Low-cost single-wire for simple sensors
3. **FlexRay** - Deterministic timing for drive-by-wire
4. **Automotive Ethernet** - 100BASE-T1, 1000BASE-T1
5. **SecOC** - Secure Onboard Communication
6. **UDS** - Unified Diagnostic Services (ISO 14229)

---

## Appendix: Quick Reference

### Common CAN Message IDs (OBD-II Mode 01)

| PID | Description | Formula |
|-----|-------------|---------|
| 0x0C | Engine RPM | ((A×256)+B)/4 |
| 0x0D | Vehicle Speed | A km/h |
| 0x11 | Throttle Position | A×100/255 % |
| 0x05 | Engine Coolant Temp | A-40 °C |
| 0x0F | Intake Air Temp | A-40 °C |
| 0x2F | Fuel Tank Level | A×100/255 % |

### CAN Bit Timing Quick Reference

```
Bit Time = Sync + Prop + Phase1 + Phase2

For 500 kbps with 8 MHz clock:
- Prescaler: 1
- Total TQ: 16
- Sync: 1 TQ
- Prop + Phase1: 11 TQ
- Phase2: 4 TQ
- Sample point: 75%
```

### Pin Quick Reference (APP-MASTERS25-1)

| Function | Pin | SERCOM |
|----------|-----|--------|
| UART TX | PA08 | SERCOM0_PAD0 |
| UART RX | PA09 | SERCOM0_PAD1 |
| SPI CLK | PA13 | SERCOM1_PAD1 |
| SPI MOSI | PA12 | SERCOM1_PAD0 |
| I2C SDA | PA12 | SERCOM2_PAD0 |
| I2C SCL | PA13 | SERCOM2_PAD1 |

---

*Document prepared for 業師協同教學 collaboration*  
*Target: Department of Intelligent Vehicle Engineering*  
*Hardware: PIC32CM3204GV00048 on APP-MASTERS25-1*
