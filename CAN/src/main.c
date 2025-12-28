/*******************************************************************************
 * CAN Protocol Educational Demo
 * 
 * This demo simulates a Vehicle ECU sending CAN messages.
 * The terminal shows each CAN frame being "transmitted" with full explanation.
 * 
 * Hardware: PIC32CM3204GV00048 + ATA6561 (transceiver only, no CAN controller)
 * UART: SERCOM0 (PA08=TX, PA09=RX) @ 115200 baud
 ******************************************************************************/

 #include <stddef.h>
 #include <stdbool.h>
 #include <stdlib.h>
#include <stdint.h>
 #include "definitions.h"
 
/* LED Control (Active-Low) */
#define LED1_ON()   LED1_Clear()
#define LED1_OFF()  LED1_Set()
#define LED2_ON()   LED2_Clear()
#define LED2_OFF()  LED2_Set()

/* Timing */
static void delay_ms(uint32_t ms) {
    volatile uint32_t count = ms * 800;
    while(count--);
}

/* ==========================================================================
   UART Functions
   ========================================================================== */
static void putchar_uart(char c) {
    uint32_t timeout = 50000;
    while(!(SERCOM0_REGS->USART_INT.SERCOM_INTFLAG & SERCOM_USART_INT_INTFLAG_DRE_Msk)) {
        if(--timeout == 0) return;
    }
    SERCOM0_REGS->USART_INT.SERCOM_DATA = c;
}

static void print(const char* str) {
    while(*str) {
        if(*str == '\n') putchar_uart('\r');
        putchar_uart(*str++);
    }
}

static void print_dec(uint32_t val) {
    char buf[12];
    int i = 0;
    if(val == 0) { putchar_uart('0'); return; }
    while(val > 0) { buf[i++] = '0' + (val % 10); val /= 10; }
    while(i > 0) putchar_uart(buf[--i]);
}

static void print_hex8(uint8_t val) {
    const char hex[] = "0123456789ABCDEF";
    putchar_uart('0'); putchar_uart('x');
    putchar_uart(hex[(val >> 4) & 0x0F]);
    putchar_uart(hex[val & 0x0F]);
}

static void print_hex16(uint16_t val) {
    const char hex[] = "0123456789ABCDEF";
    putchar_uart('0'); putchar_uart('x');
    putchar_uart(hex[(val >> 12) & 0x0F]);
    putchar_uart(hex[(val >> 8) & 0x0F]);
    putchar_uart(hex[(val >> 4) & 0x0F]);
    putchar_uart(hex[val & 0x0F]);
}

static void print_binary11(uint16_t val) {
    for(int i = 10; i >= 0; i--) {
        putchar_uart(((val >> i) & 1) ? '1' : '0');
    }
}

static void print_line(void) {
    print("------------------------------------------------------------\n");
}

static void print_dline(void) {
    print("============================================================\n");
}

/* ==========================================================================
   CAN Frame Structure
   ========================================================================== */
typedef struct {
    uint16_t id;
    uint8_t  dlc;
    uint8_t  data[8];
    uint16_t crc;
} CANFrame;

/* CRC-15 Calculation (Real CAN algorithm) */
static uint16_t calc_crc15(CANFrame* f) {
    uint16_t crc = 0;
    const uint16_t poly = 0x4599;
    
    for(int i = 10; i >= 0; i--) {
        uint8_t bit = (f->id >> i) & 1;
        uint8_t nxt = bit ^ ((crc >> 14) & 1);
        crc = (crc << 1) & 0x7FFF;
        if(nxt) crc ^= poly;
    }
    for(int i = 3; i >= 0; i--) {
        uint8_t bit = (f->dlc >> i) & 1;
        uint8_t nxt = bit ^ ((crc >> 14) & 1);
        crc = (crc << 1) & 0x7FFF;
        if(nxt) crc ^= poly;
    }
    for(int b = 0; b < f->dlc; b++) {
        for(int i = 7; i >= 0; i--) {
            uint8_t bit = (f->data[b] >> i) & 1;
            uint8_t nxt = bit ^ ((crc >> 14) & 1);
            crc = (crc << 1) & 0x7FFF;
            if(nxt) crc ^= poly;
        }
    }
    return crc;
}

/* ==========================================================================
   Vehicle Data
   ========================================================================== */
static uint16_t g_rpm = 850;
static uint8_t  g_speed = 0;
static uint8_t  g_coolant = 45;
static uint8_t  g_throttle = 0;
static uint8_t  g_brake = 1;

static void update_vehicle(void) {
    static uint8_t phase = 0;
    static uint16_t tick = 0;
    tick++;
    
    switch(phase) {
        case 0: /* Idle */
            g_rpm = 850;
            g_throttle = 0;
            g_brake = 1;
            if(tick > 10) { phase = 1; tick = 0; }
            break;
        case 1: /* Accelerate */
            g_brake = 0;
            if(g_throttle < 70) g_throttle += 10;
            g_rpm = 1000 + g_throttle * 30;
            if(g_speed < 80) g_speed += 5;
            if(g_speed >= 80) { phase = 2; tick = 0; }
            break;
        case 2: /* Cruise */
            g_throttle = 30;
            g_rpm = 2200;
            if(tick > 15) { phase = 3; tick = 0; }
            break;
        case 3: /* Brake */
            g_throttle = 0;
            g_brake = 1;
            if(g_rpm > 800) g_rpm -= 200;
            if(g_speed > 0) g_speed -= 10;
            if(g_speed == 0) { phase = 0; tick = 0; }
            break;
    }
    
    if(g_coolant < 90) g_coolant++;
}

/* ==========================================================================
   Display Functions
   ========================================================================== */
static void show_can_frame(CANFrame* f, const char* name, const char* meaning) {
    LED1_ON();
    
    print("\n");
    print_dline();
    print("  TRANSMITTING: ");
    print(name);
    print("\n");
    print_dline();
    print("\n");
    
    print("  CAN ID:     ");
    print_hex16(f->id);
    print("  (decimal: ");
    print_dec(f->id);
    print(")\n");
    
    print("  Binary:     ");
    print_binary11(f->id);
    print("  (11-bit standard ID)\n");
    
    print("  DLC:        ");
    print_dec(f->dlc);
    print(" byte(s)\n");
    
    print("  Data:       ");
    for(int i = 0; i < f->dlc; i++) {
        print_hex8(f->data[i]);
        if(i < f->dlc - 1) print(" ");
    }
    print("\n");
    
    print("  CRC-15:     ");
    print_hex16(f->crc);
    print("\n\n");
    
    print("  MEANING:    ");
    print(meaning);
    print("\n");
    
    print_line();
    
    delay_ms(100);
    LED1_OFF();
}

static void show_dashboard(void) {
    LED2_ON();
    
    print("\n");
    print("+------------------[ DASHBOARD ]------------------+\n");
    print("|                                                 |\n");
    
    /* RPM */
    print("|  RPM:      ");
    print_dec(g_rpm);
    int spaces = 5 - (g_rpm >= 1000 ? 4 : (g_rpm >= 100 ? 3 : (g_rpm >= 10 ? 2 : 1)));
    for(int i = 0; i < spaces; i++) putchar_uart(' ');
    print("  [");
    int bars = g_rpm / 400;
    for(int i = 0; i < 20; i++) {
        putchar_uart(i < bars ? (i < 15 ? '#' : '!') : '-');
    }
    print("]  |\n");
    
    /* Speed */
    print("|  SPEED:    ");
    print_dec(g_speed);
    spaces = 5 - (g_speed >= 100 ? 3 : (g_speed >= 10 ? 2 : 1));
    for(int i = 0; i < spaces; i++) putchar_uart(' ');
    print(" km/h                       |\n");
    
    /* Coolant */
    print("|  COOLANT:  ");
    print_dec(g_coolant);
    spaces = 5 - (g_coolant >= 100 ? 3 : (g_coolant >= 10 ? 2 : 1));
    for(int i = 0; i < spaces; i++) putchar_uart(' ');
    print(" C   ");
    if(g_coolant < 60) print("[WARMING UP]          ");
    else if(g_coolant > 100) print("[OVERHEAT!]           ");
    else print("[NORMAL]              ");
    print("|\n");
    
    /* Throttle & Brake */
    print("|  THROTTLE: ");
    print_dec(g_throttle);
    spaces = 5 - (g_throttle >= 100 ? 3 : (g_throttle >= 10 ? 2 : 1));
    for(int i = 0; i < spaces; i++) putchar_uart(' ');
    print(" %    BRAKE: ");
    print(g_brake ? "ON " : "OFF");
    print("                |\n");
    
    print("|                                                 |\n");
    print("+-------------------------------------------------+\n");
    
    delay_ms(100);
    LED2_OFF();
}

/* ==========================================================================
   MAIN
   ========================================================================== */
int main(void) {
    SYS_Initialize(NULL);
    
    /* Startup blink */
    for(int i = 0; i < 3; i++) {
        LED1_ON(); LED2_OFF(); delay_ms(150);
        LED1_OFF(); LED2_ON(); delay_ms(150);
    }
    LED1_OFF(); LED2_OFF();
    
    /* Welcome message */
    print("\n\n\n");
    print_dline();
    print("       CAN BUS PROTOCOL - EDUCATIONAL DEMO\n");
    print_dline();
    print("\n");
    print("  This demo simulates a Vehicle ECU transmitting\n");
    print("  CAN messages on an automotive network.\n");
    print("\n");
    print("  WHAT YOU WILL SEE:\n");
    print("  - Each CAN frame with ID, Data, and CRC\n");
    print("  - The meaning of the data being sent\n");
    print("  - A dashboard showing decoded values\n");
    print("\n");
    print("  VEHICLE SIMULATION CYCLE:\n");
    print("  1. Idle      -> Engine at 850 RPM, car stopped\n");
    print("  2. Accelerate-> Throttle up, speed increases\n");
    print("  3. Cruise    -> Steady 80 km/h at 2200 RPM\n");
    print("  4. Brake     -> Slowing down to stop\n");
    print("  (Cycle repeats)\n");
    print("\n");
    print("  NOTE: RPM = Engine revolutions, not wheel speed!\n");
    print("        At idle, engine runs but car doesn't move.\n");
    print("\n");
    print("  LED INDICATORS:\n");
    print("  - LED1 blinks = CAN frame transmitted\n");
    print("  - LED2 blinks = Dashboard updated\n");
    print("\n");
    print_dline();
    print("  Starting in 3 seconds...\n");
    print_dline();
    
    delay_ms(3000);
    
    /* Main loop */
    CANFrame frame;
    uint32_t cycle = 0;
    char meaning[80];
    
    while(1) {
        update_vehicle();
        
        switch(cycle % 4) {
            case 0: /* Engine RPM */
                frame.id = 0x0C0;
                frame.dlc = 2;
                frame.data[0] = (g_rpm >> 8) & 0xFF;
                frame.data[1] = g_rpm & 0xFF;
                frame.crc = calc_crc15(&frame);
                
                print("\n\n");
                print(">>> Sending ENGINE RPM data...\n");
                
                /* Build meaning string */
                print("\n");
                show_can_frame(&frame, "ENGINE RPM (ID: 0x0C0)", 
                    "RPM = (Data[0] << 8) | Data[1]");
                
                print("  DECODED:    RPM = ");
                print_dec(g_rpm);
                print("\n");
                break;
                
            case 1: /* Vehicle Speed */
                frame.id = 0x0D0;
                frame.dlc = 1;
                frame.data[0] = g_speed;
                frame.crc = calc_crc15(&frame);
                
                print("\n\n");
                print(">>> Sending VEHICLE SPEED data...\n");
                
                show_can_frame(&frame, "VEHICLE SPEED (ID: 0x0D0)",
                    "Speed in km/h = Data[0]");
                
                print("  DECODED:    Speed = ");
                print_dec(g_speed);
                print(" km/h\n");
                break;
                
            case 2: /* Throttle & Brake */
                frame.id = 0x0F0;
                frame.dlc = 2;
                frame.data[0] = g_throttle;
                frame.data[1] = g_brake;
                frame.crc = calc_crc15(&frame);
                
                print("\n\n");
                print(">>> Sending THROTTLE & BRAKE data...\n");
                
                show_can_frame(&frame, "THROTTLE & BRAKE (ID: 0x0F0)",
                    "Throttle% = Data[0], Brake = Data[1]");
                
                print("  DECODED:    Throttle = ");
                print_dec(g_throttle);
                print("%, Brake = ");
                print(g_brake ? "ON" : "OFF");
                print("\n");
                break;
                
            case 3: /* Dashboard */
                print("\n\n");
                print(">>> Updating DASHBOARD with received data...\n");
                show_dashboard();
                
                print("\n");
                print("  The dashboard above shows data DECODED from\n");
                print("  the CAN messages sent by the Engine ECU.\n");
                print("\n");
                print("  In a real car, multiple ECUs communicate\n");
                print("  this way over the CAN bus network.\n");
                break;
        }
        
        cycle++;
        delay_ms(2000);  /* 2 second pause between messages */
    }
    
    return EXIT_FAILURE;
 }
 