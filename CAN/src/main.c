/*******************************************************************************
 * CAN Protocol Educational Demo
 * 
 * Target: PIC32CM3204GV00048 + ATA6561 Transceiver
 * UART: SERCOM0 (PA08=TX, PA09=RX) via MCP2221
 * 
 * This demo teaches CAN protocol concepts:
 * - Frame structure (ID, DLC, Data, CRC)
 * - Bit timing and synchronization
 * - Bus arbitration (CSMA/CD)
 * - Error detection
 * - Real automotive applications
 * 
 * NOTE: This MCU has NO built-in CAN controller.
 *       For real CAN, add MCP2515 external controller.
 ******************************************************************************/

 #include <stddef.h>
 #include <stdbool.h>
 #include <stdlib.h>
#include <stdint.h>
 #include <string.h>
 #include "definitions.h"
 
/* LED macros for ACTIVE-LOW LEDs */
#define LED1_ON()   LED1_Clear()
#define LED1_OFF()  LED1_Set()
#define LED2_ON()   LED2_Clear()
#define LED2_OFF()  LED2_Set()

/* Simple delay */
static void delay_ms(uint32_t ms) {
    volatile uint32_t count = ms * 800;
    while(count--);
}

/* ==========================================================================
   UART Functions (SERCOM0 - PA08/PA09)
   ========================================================================== */
static void UART_PutChar(char c) {
    uint32_t timeout = 50000;
    while(!(SERCOM0_REGS->USART_INT.SERCOM_INTFLAG & SERCOM_USART_INT_INTFLAG_DRE_Msk)) {
        if(--timeout == 0) return;
    }
    SERCOM0_REGS->USART_INT.SERCOM_DATA = c;
}

static void UART_Print(const char* str) {
    while(*str) {
        if(*str == '\n') UART_PutChar('\r');
        UART_PutChar(*str++);
    }
}

static void UART_PrintDec(uint32_t val) {
    char buf[12];
    int i = 0;
    if(val == 0) { UART_PutChar('0'); return; }
    while(val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    while(i > 0) UART_PutChar(buf[--i]);
}

static void UART_PrintHex8(uint8_t val) {
    const char hex[] = "0123456789ABCDEF";
    UART_PutChar(hex[(val >> 4) & 0x0F]);
    UART_PutChar(hex[val & 0x0F]);
}

static void UART_PrintHex16(uint16_t val) {
    UART_PrintHex8((val >> 8) & 0xFF);
    UART_PrintHex8(val & 0xFF);
}

static void UART_PrintBinary11(uint16_t val) {
    for(int i = 10; i >= 0; i--) {
        UART_PutChar(((val >> i) & 1) ? '1' : '0');
        if(i == 8 || i == 4) UART_PutChar(' ');
    }
}

/* ==========================================================================
   CAN Frame Structure
   ========================================================================== */
typedef struct {
    uint16_t id;        /* 11-bit Standard ID */
    uint8_t  rtr;       /* Remote Transmission Request */
    uint8_t  dlc;       /* Data Length Code (0-8) */
    uint8_t  data[8];   /* Data bytes */
    uint16_t crc;       /* 15-bit CRC */
} CAN_Frame_t;

/* Standard Automotive CAN Message IDs */
#define CAN_ID_ENGINE_RPM       0x0C0
#define CAN_ID_VEHICLE_SPEED    0x0D0
#define CAN_ID_COOLANT_TEMP     0x0E0
#define CAN_ID_THROTTLE_POS     0x0F0
#define CAN_ID_BRAKE_STATUS     0x100
#define CAN_ID_STEERING_ANGLE   0x110
#define CAN_ID_AIRBAG_STATUS    0x620
#define CAN_ID_OBD2_REQUEST     0x7DF
#define CAN_ID_OBD2_RESPONSE    0x7E8

/* ==========================================================================
   CRC-15 Calculation (Real CAN Algorithm!)
   ========================================================================== */
static uint16_t CAN_CalculateCRC(CAN_Frame_t* frame) {
    uint16_t crc = 0;
    const uint16_t polynomial = 0x4599;  /* CAN CRC polynomial */
    
    /* Process ID bits */
    for(int i = 10; i >= 0; i--) {
        uint8_t bit = (frame->id >> i) & 1;
        uint8_t crc_nxt = bit ^ ((crc >> 14) & 1);
        crc = (crc << 1) & 0x7FFF;
        if(crc_nxt) crc ^= polynomial;
    }
    
    /* Process DLC bits */
    for(int i = 3; i >= 0; i--) {
        uint8_t bit = (frame->dlc >> i) & 1;
        uint8_t crc_nxt = bit ^ ((crc >> 14) & 1);
        crc = (crc << 1) & 0x7FFF;
        if(crc_nxt) crc ^= polynomial;
    }
    
    /* Process Data bits */
    for(int b = 0; b < frame->dlc; b++) {
        for(int i = 7; i >= 0; i--) {
            uint8_t bit = (frame->data[b] >> i) & 1;
            uint8_t crc_nxt = bit ^ ((crc >> 14) & 1);
            crc = (crc << 1) & 0x7FFF;
            if(crc_nxt) crc ^= polynomial;
        }
    }
    
    return crc & 0x7FFF;
}

/* ==========================================================================
   Vehicle Simulation
   ========================================================================== */
typedef struct {
    uint16_t rpm;
    uint8_t  speed_kph;
    int8_t   coolant_c;
    uint8_t  throttle_pct;
    uint8_t  brake_on;
    int16_t  steering_deg;
    uint8_t  gear;
} Vehicle_t;

static Vehicle_t g_vehicle = {850, 0, 25, 0, 1, 0, 0};

static void SimulateVehicle(void) {
    static uint8_t state = 0;
    static uint16_t counter = 0;
    
    counter++;
    
    switch(state) {
        case 0: /* Warming up */
            if(g_vehicle.coolant_c < 90) g_vehicle.coolant_c++;
            g_vehicle.rpm = 900;
            if(counter > 30) { state = 1; counter = 0; }
            break;
        case 1: /* Start driving */
            g_vehicle.brake_on = 0;
            g_vehicle.gear = 3;
            if(g_vehicle.throttle_pct < 50) g_vehicle.throttle_pct += 5;
            g_vehicle.rpm = 1000 + g_vehicle.throttle_pct * 40;
            if(g_vehicle.speed_kph < 60) g_vehicle.speed_kph += 3;
            g_vehicle.steering_deg = (counter % 20) - 10;
            if(g_vehicle.speed_kph >= 60) { state = 2; counter = 0; }
            break;
        case 2: /* Cruising */
            g_vehicle.throttle_pct = 30;
            g_vehicle.rpm = 2500;
            g_vehicle.steering_deg = (counter % 10) - 5;
            if(counter > 40) { state = 3; counter = 0; }
            break;
        case 3: /* Braking */
            g_vehicle.throttle_pct = 0;
            g_vehicle.brake_on = 1;
            if(g_vehicle.rpm > 800) g_vehicle.rpm -= 150;
            if(g_vehicle.speed_kph > 0) g_vehicle.speed_kph -= 4;
            if(g_vehicle.speed_kph == 0) { state = 0; counter = 0; g_vehicle.gear = 0; }
            break;
    }
}

/* ==========================================================================
   Educational Display Functions
   ========================================================================== */
static void PrintBanner(void) {
    UART_Print("\n\n");
    UART_Print("################################################################\n");
    UART_Print("#                                                              #\n");
    UART_Print("#       CAN Bus Protocol - Educational Demonstration           #\n");
    UART_Print("#       PIC32CM3204GV00048 + ATA6561 Transceiver               #\n");
    UART_Print("#                                                              #\n");
    UART_Print("################################################################\n\n");
    UART_Print("NOTE: This MCU has NO built-in CAN controller.\n");
    UART_Print("      This is a SIMULATION for educational purposes.\n");
    UART_Print("      For real CAN, add MCP2515 external controller.\n\n");
}

static void PrintLesson1_FrameStructure(void) {
    UART_Print("================================================================\n");
    UART_Print("  LESSON 1: CAN Frame Structure\n");
    UART_Print("================================================================\n\n");
    
    UART_Print("A Standard CAN 2.0A frame consists of:\n\n");
    
    UART_Print("+-----+------------+---+---+---+-----+--------+-----+---+-----+\n");
    UART_Print("| SOF |  ID (11b)  |RTR|IDE|r0 | DLC |  DATA  | CRC |ACK| EOF |\n");
    UART_Print("+-----+------------+---+---+---+-----+--------+-----+---+-----+\n");
    UART_Print("|  1  |     11     | 1 | 1 | 1 |  4  |  0-64  | 15  | 2 |  7  |\n");
    UART_Print("+-----+------------+---+---+---+-----+--------+-----+---+-----+\n\n");
    
    UART_Print("Field Descriptions:\n");
    UART_Print("  SOF  - Start of Frame (1 dominant bit)\n");
    UART_Print("  ID   - Message Identifier (11 bits, lower = higher priority)\n");
    UART_Print("  RTR  - Remote Transmission Request (0=data, 1=remote)\n");
    UART_Print("  IDE  - Identifier Extension (0=standard, 1=extended)\n");
    UART_Print("  r0   - Reserved bit (must be dominant/0)\n");
    UART_Print("  DLC  - Data Length Code (0-8 bytes)\n");
    UART_Print("  DATA - Payload (0 to 8 bytes)\n");
    UART_Print("  CRC  - Cyclic Redundancy Check (15 bits + delimiter)\n");
    UART_Print("  ACK  - Acknowledge slot (2 bits)\n");
    UART_Print("  EOF  - End of Frame (7 recessive bits)\n\n");
    
    delay_ms(3000);
}

static void PrintLesson2_BitTiming(void) {
    UART_Print("================================================================\n");
    UART_Print("  LESSON 2: CAN Bit Timing\n");
    UART_Print("================================================================\n\n");
    
    UART_Print("Each CAN bit is divided into time segments:\n\n");
    
    UART_Print("  |<-------------- 1 Bit Time (tq * (1+PS1+PS2)) ------------->|\n");
    UART_Print("  +------+------------------+---------+------------------------+\n");
    UART_Print("  | SYNC |   PROP + PHASE1  | SAMPLE  |        PHASE2          |\n");
    UART_Print("  +------+------------------+---------+------------------------+\n");
    UART_Print("     1tq        1-8 tq         Point         1-8 tq\n\n");
    
    UART_Print("Common Baud Rates:\n");
    UART_Print("  - 125 kbps  : Low-speed CAN (body electronics)\n");
    UART_Print("  - 250 kbps  : J1939 trucks, agricultural equipment\n");
    UART_Print("  - 500 kbps  : High-speed CAN (powertrain, chassis)\n");
    UART_Print("  - 1 Mbps    : Maximum standard CAN speed\n\n");
    
    UART_Print("Example: 500kbps with 8MHz oscillator\n");
    UART_Print("  Bit Time = 1/500000 = 2us\n");
    UART_Print("  With 16 time quanta: tq = 125ns\n");
    UART_Print("  BRP = 8MHz / (16 * 500kHz) - 1 = 0\n\n");
    
    delay_ms(3000);
}

static void PrintLesson3_Arbitration(void) {
    UART_Print("================================================================\n");
    UART_Print("  LESSON 3: CAN Bus Arbitration (CSMA/CD)\n");
    UART_Print("================================================================\n\n");
    
    UART_Print("CAN uses non-destructive bitwise arbitration:\n\n");
    
    UART_Print("  Rule: DOMINANT (0) wins over RECESSIVE (1)\n\n");
    
    UART_Print("Example: Node A (ID=0x100) vs Node B (ID=0x200)\n\n");
    
    UART_Print("  Bit Position:  10   9   8   7   6   5   4   3   2   1   0\n");
    UART_Print("                +---+---+---+---+---+---+---+---+---+---+---+\n");
    UART_Print("  ID = 0x100:   | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | <- WINS!\n");
    UART_Print("                +---+---+---+---+---+---+---+---+---+---+---+\n");
    UART_Print("  ID = 0x200:   | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | <- loses\n");
    UART_Print("                +---+---+---+---+---+---+---+---+---+---+---+\n");
    UART_Print("                      ^ Node B sees dominant, backs off\n\n");
    
    UART_Print("  LOWER ID = HIGHER PRIORITY\n\n");
    
    UART_Print("  This is why critical messages (airbags, brakes)\n");
    UART_Print("  have LOW IDs for guaranteed bus access!\n\n");
    
    delay_ms(3000);
}

static void PrintLesson4_AutomotiveIDs(void) {
    UART_Print("================================================================\n");
    UART_Print("  LESSON 4: Common Automotive CAN Message IDs\n");
    UART_Print("================================================================\n\n");
    
    UART_Print("+--------+-------------------------+------------+-------------+\n");
    UART_Print("|   ID   |      Description        |    Rate    |  Priority   |\n");
    UART_Print("+--------+-------------------------+------------+-------------+\n");
    UART_Print("| 0x0C0  | Engine RPM              |   10 ms    |    High     |\n");
    UART_Print("| 0x0D0  | Vehicle Speed           |   20 ms    |    High     |\n");
    UART_Print("| 0x0E0  | Coolant Temperature     |  100 ms    |   Medium    |\n");
    UART_Print("| 0x0F0  | Throttle Position       |   10 ms    |    High     |\n");
    UART_Print("| 0x100  | Brake Status            |   10 ms    |    High     |\n");
    UART_Print("| 0x110  | Steering Angle          |   10 ms    |    High     |\n");
    UART_Print("| 0x620  | Airbag Status           |  100 ms    |   Medium    |\n");
    UART_Print("| 0x7DF  | OBD-II Request (all)    | On-demand  |    Low      |\n");
    UART_Print("| 0x7E8  | OBD-II Response (ECM)   | On-demand  |    Low      |\n");
    UART_Print("+--------+-------------------------+------------+-------------+\n\n");
    
    UART_Print("Note: Lower ID = Higher priority on the bus!\n\n");
    
    delay_ms(3000);
}

static void PrintCANFrame(CAN_Frame_t* frame, const char* desc) {
    UART_Print("\n+------ CAN Frame: ");
    UART_Print(desc);
    UART_Print(" ------+\n");
    
    UART_Print("| ID:   0x");
    UART_PrintHex16(frame->id);
    UART_Print("  Binary: ");
    UART_PrintBinary11(frame->id);
    UART_Print("\n");
    
    UART_Print("| DLC:  ");
    UART_PrintDec(frame->dlc);
    UART_Print(" bytes\n");
    
    UART_Print("| Data: ");
    for(int i = 0; i < frame->dlc; i++) {
        UART_PrintHex8(frame->data[i]);
        UART_PutChar(' ');
    }
    UART_Print("\n");
    
    UART_Print("| CRC:  0x");
    UART_PrintHex16(frame->crc);
    UART_Print("\n");
    
    UART_Print("+----------------------------------------+\n");
}

static void PrintDashboard(void) {
    UART_Print("\n============= VEHICLE DASHBOARD =============\n");
    
    /* RPM with bar graph */
    UART_Print("  RPM:      ");
    UART_PrintDec(g_vehicle.rpm);
    UART_Print("  [");
    int bars = g_vehicle.rpm / 500;
    for(int i = 0; i < 16; i++) {
         if(i < bars) {
            UART_PutChar(i < 12 ? '=' : '!');
         } else {
            UART_PutChar('-');
        }
    }
    UART_Print("]\n");
    
    /* Speed */
    UART_Print("  Speed:    ");
    UART_PrintDec(g_vehicle.speed_kph);
    UART_Print(" km/h\n");
    
    /* Coolant */
    UART_Print("  Coolant:  ");
    UART_PrintDec(g_vehicle.coolant_c);
    UART_Print(" C ");
    if(g_vehicle.coolant_c < 60) UART_Print("[COLD]");
    else if(g_vehicle.coolant_c > 100) UART_Print("[HOT!]");
    else UART_Print("[OK]");
    UART_Print("\n");
    
    /* Throttle */
    UART_Print("  Throttle: ");
    UART_PrintDec(g_vehicle.throttle_pct);
    UART_Print(" %\n");
    
    /* Gear and Brake */
    const char* gears[] = {"P", "R", "N", "D"};
    UART_Print("  Gear: ");
    UART_Print(gears[g_vehicle.gear]);
    UART_Print("    Brake: ");
    UART_Print(g_vehicle.brake_on ? "ON" : "OFF");
    UART_Print("\n");
    
    UART_Print("==============================================\n");
}

/* ==========================================================================
   MAIN
   ========================================================================== */
int main(void) {
    SYS_Initialize(NULL);
    
    /* Startup LED animation */
    for(int i = 0; i < 5; i++) {
        LED1_ON(); LED2_OFF(); delay_ms(100);
        LED1_OFF(); LED2_ON(); delay_ms(100);
    }
    LED1_OFF(); LED2_OFF();
    
    /* Print educational content */
    PrintBanner();
    delay_ms(2000);
    
    PrintLesson1_FrameStructure();
    PrintLesson2_BitTiming();
    PrintLesson3_Arbitration();
    PrintLesson4_AutomotiveIDs();
    
    UART_Print("\n################################################################\n");
    UART_Print("#          LIVE VEHICLE CAN BUS SIMULATION                     #\n");
    UART_Print("################################################################\n\n");
    
    /* Main simulation loop */
    CAN_Frame_t frame;
    uint32_t cycle = 0;
    
    while(1) {
        cycle++;
        SimulateVehicle();
        
        /* Send different CAN messages in rotation */
        switch(cycle % 6) {
            case 0: /* Engine RPM */
                frame.id = CAN_ID_ENGINE_RPM;
                frame.dlc = 2;
                frame.data[0] = (g_vehicle.rpm >> 8) & 0xFF;
                frame.data[1] = g_vehicle.rpm & 0xFF;
                frame.crc = CAN_CalculateCRC(&frame);
                LED1_ON();
                PrintCANFrame(&frame, "Engine RPM");
                delay_ms(50);
                LED1_OFF();
                break;
                
            case 1: /* Vehicle Speed */
                frame.id = CAN_ID_VEHICLE_SPEED;
                frame.dlc = 1;
                frame.data[0] = g_vehicle.speed_kph;
                frame.crc = CAN_CalculateCRC(&frame);
                LED1_ON();
                PrintCANFrame(&frame, "Vehicle Speed");
                delay_ms(50);
                LED1_OFF();
                break;
                
            case 2: /* Coolant Temperature */
                frame.id = CAN_ID_COOLANT_TEMP;
                frame.dlc = 1;
                frame.data[0] = (uint8_t)(g_vehicle.coolant_c + 40);
                frame.crc = CAN_CalculateCRC(&frame);
                LED1_ON();
                PrintCANFrame(&frame, "Coolant Temp");
                delay_ms(50);
                LED1_OFF();
                break;
                
            case 3: /* Throttle Position */
                frame.id = CAN_ID_THROTTLE_POS;
                frame.dlc = 2;
                frame.data[0] = g_vehicle.throttle_pct;
                frame.data[1] = g_vehicle.brake_on;
                frame.crc = CAN_CalculateCRC(&frame);
                LED1_ON();
                PrintCANFrame(&frame, "Throttle & Brake");
                delay_ms(50);
                LED1_OFF();
                break;
                
            case 4: /* Steering Angle */
                frame.id = CAN_ID_STEERING_ANGLE;
                frame.dlc = 2;
                frame.data[0] = (g_vehicle.steering_deg >> 8) & 0xFF;
                frame.data[1] = g_vehicle.steering_deg & 0xFF;
                frame.crc = CAN_CalculateCRC(&frame);
                LED1_ON();
                PrintCANFrame(&frame, "Steering Angle");
                delay_ms(50);
                LED1_OFF();
                break;
                
            case 5: /* Dashboard update */
                LED2_ON();
                PrintDashboard();
                delay_ms(50);
                LED2_OFF();
                break;
        }
        
        delay_ms(500);
    }
    
    return EXIT_FAILURE;
 }
 