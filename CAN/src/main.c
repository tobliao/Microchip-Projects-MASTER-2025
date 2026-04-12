/*******************************************************************************
 * CAN Protocol Educational Simulation - Production Version
 * 
 * Hardware: PIC32CM3204GV00048 on APP-MASTERS25-1 board
 * 
 * Peripherals:
 *   - UART: SERCOM0 (PA08=TX, PA09=RX) @ 115200 baud
 *   - OLED: SERCOM2 I2C (PA12=SDA, PA13=SCL) @ 100 kHz, SSD1306 128x64
 *   - ADC:  PA03 (AIN1) for potentiometer
 *   - GPIO: LEDs (PA00-PA01, PA10-PA11), RGB1 (PB09, PA04, PA05)
 *   - GPIO: Buttons SW1 (PB10), SW2 (PA15), Buzzer (PA14)
 * 
 * Controls:
 *   - SW1 (ACC): Hold to accelerate (rate depends on throttle)
 *   - SW2 (BRK): Hold to brake (fixed deceleration)
 *   - Potentiometer: Set throttle level (0-100%)
 ******************************************************************************/

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "definitions.h"

/*******************************************************************************
 * CONFIGURATION
 ******************************************************************************/
#define CPU_FREQ        8000000UL
#define BUZZER_ENABLED  0           /* Set to 1 to enable buzzer feedback */
#define UPDATE_INTERVAL 200         /* Main loop update interval (ms) */

/*******************************************************************************
 * LED MACROS (Active-Low)
 ******************************************************************************/
#define LED1_ON()   LED1_Clear()
#define LED1_OFF()  LED1_Set()
#define LED2_ON()   LED2_Clear()
#define LED2_OFF()  LED2_Set()
#define LED3_ON()   LED3_Clear()
#define LED3_OFF()  LED3_Set()
#define LED4_ON()   LED4_Clear()
#define LED4_OFF()  LED4_Set()

#define BUZZER_ON()  Buzzer_Set()
#define BUZZER_OFF() Buzzer_Clear()

/*******************************************************************************
 * TIMING FUNCTIONS
 ******************************************************************************/
static void delay_us(uint32_t us) {
    volatile uint32_t count = us * (CPU_FREQ / 1000000 / 4);
    while(count--);
}

static void delay_ms(uint32_t ms) {
    for(uint32_t i = 0; i < ms; i++) delay_us(1000);
}

/*******************************************************************************
 * FORWARD DECLARATIONS
 ******************************************************************************/
static void print(const char* s);
static void println(const char* s);

/*******************************************************************************
 * OLED DISPLAY
 ******************************************************************************/
#include "OLED128x64.h"
#define OLED_STANDALONE 1
#include "OLED128x64.c"

static char oled_buf[24];

static void int_to_str(int32_t val, char* buf, int width) {
    int len = 0, neg = 0;
    char temp[12];
    
    if(val < 0) { neg = 1; val = -val; }
    if(val == 0) { temp[len++] = '0'; }
    else { while(val > 0) { temp[len++] = '0' + (val % 10); val /= 10; } }
    
    int pad = width - len - neg;
    int i = 0;
    while(pad-- > 0) buf[i++] = ' ';
    if(neg) buf[i++] = '-';
    while(len > 0) buf[i++] = temp[--len];
    buf[i] = '\0';
}

static void hex_to_str(uint16_t val, char* buf, int digits) {
    const char hex[] = "0123456789ABCDEF";
    for(int i = digits - 1; i >= 0; i--) {
        buf[digits - 1 - i] = hex[(val >> (i * 4)) & 0xF];
    }
    buf[digits] = '\0';
}

static void oled_update_display(uint16_t rpm_val, uint8_t speed_val, uint8_t thr_val,
                                bool acc_on, bool brk_on,
                                uint16_t can_id, uint8_t dlc, uint8_t* data, uint16_t crc_val) {
    /* Line 0: Title */
    OLED_Put6x8Str(16, 0, (const uint8_t*)"CAN BUS MONITOR");
    OLED_Put6x8Str(0, 1, (const uint8_t*)"---------------------");
    
    /* Line 2-3: RPM (large font) */
    OLED_Put6x8Str(0, 2, (const uint8_t*)"RPM:");
    int_to_str(rpm_val, oled_buf, 5);
    OLED_Put8x16Str(30, 2, (const uint8_t*)oled_buf);
    
    /* Line 4: Speed + Throttle */
    OLED_Put6x8Str(0, 4, (const uint8_t*)"SPD:");
    int_to_str(speed_val, oled_buf, 3);
    OLED_Put6x8Str(24, 4, (const uint8_t*)oled_buf);
    OLED_Put6x8Str(48, 4, (const uint8_t*)"km/h");
    OLED_Put6x8Str(78, 4, (const uint8_t*)"T:");
    int_to_str(thr_val, oled_buf, 3);
    strcat(oled_buf, "%");
    OLED_Put6x8Str(90, 4, (const uint8_t*)oled_buf);
    
    /* Line 5: Button status */
    OLED_Put6x8Str(0, 5, (const uint8_t*)"ACC:");
    OLED_Put6x8Str(24, 5, acc_on ? (const uint8_t*)"[*]" : (const uint8_t*)"[ ]");
    OLED_Put6x8Str(54, 5, (const uint8_t*)"BRK:");
    OLED_Put6x8Str(78, 5, brk_on ? (const uint8_t*)"[*]" : (const uint8_t*)"[ ]");
    
    /* Line 6: CAN frame data */
    hex_to_str(can_id, oled_buf, 3);
    OLED_Put6x8Str(0, 6, (const uint8_t*)oled_buf);
    OLED_Put6x8Str(18, 6, (const uint8_t*)":");
    for(int i = 0; i < dlc && i < 2; i++) {
        hex_to_str(data[i], oled_buf, 2);
        OLED_Put6x8Str(24 + i * 12, 6, (const uint8_t*)oled_buf);
    }
    OLED_Put6x8Str(54, 6, (const uint8_t*)"CRC:");
    hex_to_str(crc_val, oled_buf, 4);
    OLED_Put6x8Str(78, 6, (const uint8_t*)oled_buf);
    
    /* Line 7: Footer */
    OLED_Put6x8Str(0, 7, (const uint8_t*)"---------------------");
}

static void oled_init_display(void) {
    delay_ms(100);
    OLED_Init();
    OLED_CLS();
    delay_ms(10);
    
    /* Splash screen */
    OLED_Put8x16Str(8, 1, (const uint8_t*)"NCUExMICROCHIP");
    OLED_Put8x16Str(8, 3, (const uint8_t*)"CAN Simulation");
    OLED_Put6x8Str(22, 6, (const uint8_t*)"PIC32CM3204GV");
    delay_ms(1500);
    OLED_CLS();
}

/*******************************************************************************
 * UART FUNCTIONS
 ******************************************************************************/
static void uart_putc(char c) {
    uint32_t timeout = 50000;
    while(!(SERCOM0_REGS->USART_INT.SERCOM_INTFLAG & SERCOM_USART_INT_INTFLAG_DRE_Msk)) {
        if(--timeout == 0) return;
    }
    SERCOM0_REGS->USART_INT.SERCOM_DATA = c;
}

static void print(const char* s) {
    while(*s) { if(*s == '\n') uart_putc('\r'); uart_putc(*s++); }
}

static void println(const char* s) { print(s); print("\n"); }

static void print_int(int32_t v) {
    char buf[12]; int i = 0;
    if(v < 0) { uart_putc('-'); v = -v; }
    if(v == 0) { uart_putc('0'); return; }
    while(v > 0) { buf[i++] = '0' + (v % 10); v /= 10; }
    while(i > 0) uart_putc(buf[--i]);
}

static void print_hex(uint16_t v, int digits) {
    const char hex[] = "0123456789ABCDEF";
    print("0x");
    for(int i = (digits-1)*4; i >= 0; i -= 4) uart_putc(hex[(v >> i) & 0xF]);
}

static void print_pad(int32_t v, int width) {
    int len = 0; int32_t temp = v;
    if(temp == 0) len = 1;
    else { if(temp < 0) { len++; temp = -temp; } while(temp > 0) { len++; temp /= 10; } }
    print_int(v);
    for(int i = len; i < width; i++) uart_putc(' ');
}

static void clear(void) { print("\033[2J\033[H"); }

/*******************************************************************************
 * RGB1 LED (Common Cathode: HIGH = ON)
 ******************************************************************************/
#define RGB_RED_PIN   (1U << 9U)   /* PB09 */
#define RGB_GREEN_PIN (1U << 4U)   /* PA04 */
#define RGB_BLUE_PIN  (1U << 5U)   /* PA05 */

#define RGB_RED_ON()    PORT_REGS->GROUP[1].PORT_OUTSET = RGB_RED_PIN
#define RGB_RED_OFF()   PORT_REGS->GROUP[1].PORT_OUTCLR = RGB_RED_PIN
#define RGB_GREEN_ON()  PORT_REGS->GROUP[0].PORT_OUTSET = RGB_GREEN_PIN
#define RGB_GREEN_OFF() PORT_REGS->GROUP[0].PORT_OUTCLR = RGB_GREEN_PIN
#define RGB_BLUE_ON()   PORT_REGS->GROUP[0].PORT_OUTSET = RGB_BLUE_PIN
#define RGB_BLUE_OFF()  PORT_REGS->GROUP[0].PORT_OUTCLR = RGB_BLUE_PIN

static void rgb_init(void) {
    PORT_REGS->GROUP[1].PORT_DIRSET = RGB_RED_PIN;
    PORT_REGS->GROUP[0].PORT_DIRSET = RGB_GREEN_PIN | RGB_BLUE_PIN;
    RGB_RED_OFF(); RGB_GREEN_OFF(); RGB_BLUE_OFF();
}

static void rgb_off(void)    { RGB_RED_OFF(); RGB_GREEN_OFF(); RGB_BLUE_OFF(); }
static void rgb_red(void)    { RGB_RED_ON();  RGB_GREEN_OFF(); RGB_BLUE_OFF(); }
static void rgb_green(void)  { RGB_RED_OFF(); RGB_GREEN_ON();  RGB_BLUE_OFF(); }
static void rgb_blue(void)   { RGB_RED_OFF(); RGB_GREEN_OFF(); RGB_BLUE_ON();  }
static void rgb_yellow(void) { RGB_RED_ON();  RGB_GREEN_ON();  RGB_BLUE_OFF(); }
static void rgb_cyan(void)   { RGB_RED_OFF(); RGB_GREEN_ON();  RGB_BLUE_ON();  }
static void rgb_white(void)  { RGB_RED_ON();  RGB_GREEN_ON();  RGB_BLUE_ON();  }

/*******************************************************************************
 * BUZZER & ADC
 ******************************************************************************/
#if BUZZER_ENABLED
static void beep(uint16_t freq, uint16_t ms) {
    uint32_t half = 500000/freq, cycles = (uint32_t)freq*ms/1000;
    for(uint32_t i = 0; i < cycles; i++) {
        BUZZER_ON(); delay_us(half); BUZZER_OFF(); delay_us(half);
    }
}
#endif

static uint8_t read_pot(void) {
    ADC_ChannelSelect(ADC_POSINPUT_PIN1, ADC_NEGINPUT_GND);
    ADC_ConversionStart();
    while(!ADC_ConversionStatusGet());
    return (uint8_t)((uint32_t)ADC_ConversionResultGet() * 100 / 4095);
}

/*******************************************************************************
 * BUTTONS - SW1=PB10, SW2=PA15
 ******************************************************************************/
static void btn_init(void) {
    /* SW1 on PB10 */
    PORT_REGS->GROUP[1].PORT_DIRCLR = (1U << 10U);
    PORT_REGS->GROUP[1].PORT_PINCFG[10] = PORT_PINCFG_INEN_Msk | PORT_PINCFG_PULLEN_Msk;
    PORT_REGS->GROUP[1].PORT_OUTSET = (1U << 10U);
    /* SW2 on PA15 */
    PORT_REGS->GROUP[0].PORT_DIRCLR = (1U << 15U);
    PORT_REGS->GROUP[0].PORT_PINCFG[15] = PORT_PINCFG_INEN_Msk | PORT_PINCFG_PULLEN_Msk;
    PORT_REGS->GROUP[0].PORT_OUTSET = (1U << 15U);
}

static bool sw1(void) { return ((PORT_REGS->GROUP[1].PORT_IN >> 10U) & 1U) == 0; }
static bool sw2(void) { return ((PORT_REGS->GROUP[0].PORT_IN >> 15U) & 1U) == 0; }

/*******************************************************************************
 * VEHICLE SIMULATION STATE
 ******************************************************************************/
static uint16_t rpm = 850;
static uint8_t speed = 0, throttle = 0;
static bool brake = false, manual = false;

/* CAN CRC-15 calculation */
static uint16_t crc15(uint16_t id, uint8_t dlc, uint8_t* data) {
    uint16_t crc = 0;
    for(int i = 10; i >= 0; i--) {
        uint8_t n = ((id >> i) & 1) ^ ((crc >> 14) & 1);
        crc = (crc << 1) & 0x7FFF;
        if(n) crc ^= 0x4599;
    }
    for(int i = 3; i >= 0; i--) {
        uint8_t n = ((dlc >> i) & 1) ^ ((crc >> 14) & 1);
        crc = (crc << 1) & 0x7FFF;
        if(n) crc ^= 0x4599;
    }
    for(int b = 0; b < dlc; b++) {
        for(int i = 7; i >= 0; i--) {
            uint8_t n = ((data[b] >> i) & 1) ^ ((crc >> 14) & 1);
            crc = (crc << 1) & 0x7FFF;
            if(n) crc ^= 0x4599;
        }
    }
    return crc;
}

/* Throttle-based acceleration rate */
static uint8_t get_throttle_increment(uint8_t thr) {
    if(thr >= 80) return 8;       /* 80-100%: +8 km/h */
    else if(thr >= 60) return 6;  /* 60-79%:  +6 km/h */
    else if(thr >= 40) return 4;  /* 40-59%:  +4 km/h */
    else if(thr >= 10) return 2;  /* 10-39%:  +2 km/h */
    else return 0;                /* 0-9%:    +0 km/h */
}

/* Update vehicle state based on inputs */
static void update(void) {
    bool s1 = sw1(), s2 = sw2();
    throttle = read_pot();
    
    if(s1 || s2) manual = true;
    
    if(s1 && !s2) {
        /* Accelerating */
        brake = false; LED4_OFF();
        uint8_t inc = get_throttle_increment(throttle);
        if(rpm < 6000) rpm += 50 + inc * 10;
        if(speed < 180) speed += inc;
    } else if(s2 && !s1) {
        /* Braking */
        brake = true; LED4_ON();
        if(rpm > 800) rpm -= 150;
        if(speed > 0) speed -= 5; else speed = 0;
    } else {
        /* Coasting */
        brake = false; LED4_OFF();
        if(manual) {
            if(rpm > 850) rpm -= 20;
            if(speed > 0) speed--;
        } else {
            /* Auto demo mode */
            static uint8_t phase = 0;
            static uint16_t tick = 0;
            tick++;
            switch(phase) {
                case 0: rpm = 850; speed = 0; if(tick > 5) { phase = 1; tick = 0; } break;
                case 1: rpm = 1000 + speed * 20; if(speed < 80) speed += 3; if(speed >= 80) { phase = 2; tick = 0; } break;
                case 2: rpm = 2200; speed = 80; if(tick > 8) { phase = 3; tick = 0; } break;
                case 3: brake = true; if(rpm > 800) rpm -= 150; if(speed > 0) speed -= 5; else { speed = 0; brake = false; phase = 0; tick = 0; } break;
            }
        }
    }
    
    /* RGB LED color based on speed/brake */
    if(brake)         rgb_red();
    else if(speed>70) rgb_yellow();
    else if(speed>50) rgb_green();
    else if(speed>30) rgb_cyan();
    else if(speed>10) rgb_blue();
    else              rgb_white();
}

/*******************************************************************************
 * TERMINAL DISPLAY
 ******************************************************************************/
static void show_title(void) {
    println("");
    println("============================================================");
    println("           CAN BUS PROTOCOL - EDUCATIONAL SIMULATION");
    println("============================================================");
    println("");
}

static void show_controls(void) {
    println("CONTROLS:");
    println("  [SW1] Hold = ACCELERATE    [SW2] Hold = BRAKE");
    println("  [POT] Turn = Set throttle level (0-100%)");
    println("");
}

static void show_status(void) {
    println("------------------------------------------------------------");
    println("VEHICLE STATUS:");
    println("");
    print("  Engine RPM:     "); print_pad(rpm, 6); println("");
    print("  Speed:          "); print_pad(speed, 3); println(" km/h");
    print("  Throttle:       "); print_pad(throttle, 3); println(" %");
    print("  Brake:          "); println(brake ? "ENGAGED" : "Released");
    print("  Mode:           "); println(manual ? "MANUAL" : "AUTO DEMO");
    println("");
    print("  SW1: "); print(sw1() ? "PRESSED  " : "---      ");
    print("  SW2: "); println(sw2() ? "PRESSED" : "---");
    println("");
}

static void show_can(uint16_t id, const char* name, uint8_t dlc, uint8_t* data) {
    LED1_ON();
#if BUZZER_ENABLED
    beep(3000, 5);
#endif
    
    println("------------------------------------------------------------");
    println("CAN FRAME TRANSMITTED:");
    println("");
    print("  Message:        "); println(name);
    print("  CAN ID:         "); print_hex(id, 3); println("");
    print("  Data Length:    "); print_int(dlc); println(" bytes");
    print("  Data Bytes:     ");
    for(int i = 0; i < dlc; i++) { print_hex(data[i], 2); print(" "); }
    println("");
    print("  CRC-15:         "); print_hex(crc15(id, dlc, data), 4); println("");
    println("");
    
    delay_ms(50);
    LED1_OFF();
}

/*******************************************************************************
 * MAIN
 ******************************************************************************/
int main(void) {
    SYS_Initialize(NULL);
    btn_init();
    ADC_Enable();
    rgb_init();
    
    /* Startup sequence */
    clear();
    println("");
    println("========================================");
    println("        SYSTEM INITIALIZATION");
    println("========================================");
    println("");
    
    /* Hardware test */
    print("  LEDs:    ");
    LED1_ON(); delay_ms(100); LED1_OFF();
    LED2_ON(); delay_ms(100); LED2_OFF();
    LED3_ON(); delay_ms(100); LED3_OFF();
    LED4_ON(); delay_ms(100); LED4_OFF();
    println("OK");
    
    print("  RGB:     ");
    rgb_red(); delay_ms(200);
    rgb_green(); delay_ms(200);
    rgb_blue(); delay_ms(200);
    rgb_off();
    println("OK");
    
#if BUZZER_ENABLED
    print("  Buzzer:  "); beep(1000, 100); println("OK");
#else
    println("  Buzzer:  Disabled");
#endif
    
    print("  ADC:     "); print_int(read_pot()); println("%");
    
    print("  OLED:    ");
    oled_init_display();
    println("OK");
    
    println("");
    println("System ready. Starting simulation...");
    
#if BUZZER_ENABLED
    beep(1000, 100); delay_ms(50);
    beep(1500, 100); delay_ms(50);
    beep(2000, 150);
#endif
    delay_ms(1000);
    
    /* Main loop variables */
    uint8_t msg = 0;
    uint8_t data[8];
    uint16_t current_can_id = 0x0C0;
    uint8_t current_dlc = 2;
    uint16_t current_crc = 0;
    
    while(1) {
        update();
        clear();
        show_title();
        show_controls();
        show_status();
        
        /* Cycle through CAN messages */
        switch(msg) {
            case 0: /* Engine RPM */
                data[0] = (rpm >> 8) & 0xFF;
                data[1] = rpm & 0xFF;
                current_can_id = 0x0C0;
                current_dlc = 2;
                current_crc = crc15(current_can_id, current_dlc, data);
                show_can(0x0C0, "ENGINE_RPM", 2, data);
                print("  Formula:        RPM = (Data[0] << 8) | Data[1]\n");
                print("  Calculation:    RPM = (");
                print_int(data[0]); print(" x 256) + ");
                print_int(data[1]); print(" = ");
                print_int(rpm); println("");
                println("");
                break;
                
            case 1: /* Vehicle Speed */
                data[0] = speed;
                current_can_id = 0x0D0;
                current_dlc = 1;
                current_crc = crc15(current_can_id, current_dlc, data);
                show_can(0x0D0, "VEHICLE_SPEED", 1, data);
                print("  Formula:        Speed = Data[0]\n");
                print("  Calculation:    Speed = ");
                print_int(speed); println(" km/h");
                println("");
                break;
                
            case 2: /* Throttle/Brake */
                data[0] = throttle;
                data[1] = brake ? 1 : 0;
                current_can_id = 0x0F0;
                current_dlc = 2;
                current_crc = crc15(current_can_id, current_dlc, data);
                show_can(0x0F0, "THROTTLE_BRAKE", 2, data);
                println("  Formula:        Throttle = Data[0], Brake = Data[1]");
                print("  Calculation:    Throttle = ");
                print_int(throttle); print("%, Brake = ");
                println(brake ? "ON (1)" : "OFF (0)");
                println("");
                break;
        }
        
        println("============================================================");
        
        /* Update OLED */
        oled_update_display(rpm, speed, throttle, sw1(), sw2(),
                           current_can_id, current_dlc, data, current_crc);
        
        msg = (msg + 1) % 3;
        delay_ms(UPDATE_INTERVAL);
    }
    
    return 0;
}
