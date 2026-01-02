/*******************************************************************************
 * INTERACTIVE CAN Protocol Educational Simulation
 * 
 * Hardware: PIC32CM3204GV00048 on APP-MASTERS25-1 board
 * UART: SERCOM0 (PA08=TX, PA09=RX) @ 115200 baud
 * OLED: SERCOM2 I2C (PA12=SDA, PA13=SCL) @ 100 kHz
 ******************************************************************************/

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "definitions.h"

/* Configuration */
#define CPU_FREQ   8000000UL

/* LED Control (Active-Low) */
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

/* Timing */
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
 * I2C INTERFACE - Using MCC-generated SERCOM2 I2C Master driver
 ******************************************************************************/
/* Legacy variable - kept for OLED library compatibility (not actively used) */
volatile bool bIsI2C_DONE __attribute__((unused)) = false;

/*******************************************************************************
 * OLED DISPLAY INTERFACE
 ******************************************************************************/
#include "OLED128x64.h"
#define OLED_STANDALONE 1
#include "OLED128x64.c"  /* Include source directly since not in Makefile */

static char oled_buf[24];  /* String buffer for OLED */

static void int_to_str(int32_t val, char* buf, int width) {
    int len = 0;
    int neg = 0;
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

/* Update OLED with current vehicle/CAN data */
static void oled_update_display(uint16_t rpm_val, uint8_t speed_val, uint8_t throttle_val, 
                                 bool brake_on, uint16_t can_id, uint8_t dlc, uint8_t* data) {
    /* Line 0 (y=0): Title */
    OLED_Put6x8Str(16, 0, (const uint8_t*)"CAN BUS MONITOR");
    
    /* Line 1 (y=1): Horizontal line */
    OLED_Put6x8Str(0, 1, (const uint8_t*)"--------------------");
    
    /* Line 2-3 (y=2): RPM - large font */
    OLED_Put6x8Str(0, 2, (const uint8_t*)"RPM:");
    int_to_str(rpm_val, oled_buf, 5);
    OLED_Put8x16Str(30, 2, (const uint8_t*)oled_buf);
    
    /* Line 4 (y=4): Speed */
    OLED_Put6x8Str(0, 4, (const uint8_t*)"SPD:");
    int_to_str(speed_val, oled_buf, 3);
    strcat(oled_buf, "km/h");
    OLED_Put6x8Str(30, 4, (const uint8_t*)oled_buf);
    
    /* Line 4: Throttle (right side) */
    OLED_Put6x8Str(78, 4, (const uint8_t*)"THR:");
    int_to_str(throttle_val, oled_buf, 3);
    strcat(oled_buf, "%");
    OLED_Put6x8Str(102, 4, (const uint8_t*)oled_buf);
    
    /* Line 5 (y=5): Brake status */
    if(brake_on)
        OLED_Put6x8Str(0, 5, (const uint8_t*)"BRAKE [ENGAGED]   ");
    else
        OLED_Put6x8Str(0, 5, (const uint8_t*)"BRAKE [Released]  ");
    
    /* Line 6 (y=6): CAN Frame Info */
    OLED_Put6x8Str(0, 6, (const uint8_t*)"CAN:");
    hex_to_str(can_id, oled_buf, 3);
    OLED_Put6x8Str(30, 6, (const uint8_t*)oled_buf);
    
    OLED_Put6x8Str(54, 6, (const uint8_t*)"D:");
    /* Show first 3 bytes of data */
    for(int i = 0; i < dlc && i < 3; i++) {
        hex_to_str(data[i], oled_buf, 2);
        OLED_Put6x8Str(66 + i * 18, 6, (const uint8_t*)oled_buf);
    }
    
    /* Line 7 (y=7): Status bar */
    OLED_Put6x8Str(0, 7, (const uint8_t*)"--------------------");
}

/* Initialize OLED with splash screen */
static void oled_init_display(void) {
    /* SERCOM2 I2C is initialized by SYS_Initialize() */
    print("Init...");
    delay_ms(100);  /* Wait for OLED power stabilization */
    
    print("OLED_Init...");
    OLED_Init();
    print("CLS...");
    OLED_CLS();
    delay_ms(10);
    
    print("Splash...");
    /* Splash screen */
    OLED_Put8x16Str(8, 1, (const uint8_t*)"MICROCHIP");
    OLED_Put8x16Str(8, 3, (const uint8_t*)"CAN DEMO");
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
 * RGB1 LED (Common Cathode)
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

static void rgb_off(void)     { RGB_RED_OFF(); RGB_GREEN_OFF(); RGB_BLUE_OFF(); }
static void rgb_red(void)     { RGB_RED_ON();  RGB_GREEN_OFF(); RGB_BLUE_OFF(); }
static void rgb_green(void)   { RGB_RED_OFF(); RGB_GREEN_ON();  RGB_BLUE_OFF(); }
static void rgb_blue(void)    { RGB_RED_OFF(); RGB_GREEN_OFF(); RGB_BLUE_ON();  }
static void rgb_yellow(void)  { RGB_RED_ON();  RGB_GREEN_ON();  RGB_BLUE_OFF(); }
static void rgb_cyan(void)    { RGB_RED_OFF(); RGB_GREEN_ON();  RGB_BLUE_ON();  }
static void rgb_white(void)   { RGB_RED_ON();  RGB_GREEN_ON();  RGB_BLUE_ON();  }

/*******************************************************************************
 * BUZZER & ADC
 ******************************************************************************/
static void beep(uint16_t freq, uint16_t ms) {
    uint32_t half = 500000/freq, cycles = (uint32_t)freq*ms/1000;
    for(uint32_t i=0; i<cycles; i++) { BUZZER_ON(); delay_us(half); BUZZER_OFF(); delay_us(half); }
}

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
    PORT_REGS->GROUP[1].PORT_DIRCLR = (1U << 10U);
    PORT_REGS->GROUP[1].PORT_PINCFG[10] = PORT_PINCFG_INEN_Msk | PORT_PINCFG_PULLEN_Msk;
    PORT_REGS->GROUP[1].PORT_OUTSET = (1U << 10U);
    PORT_REGS->GROUP[0].PORT_DIRCLR = (1U << 15U);
    PORT_REGS->GROUP[0].PORT_PINCFG[15] = PORT_PINCFG_INEN_Msk | PORT_PINCFG_PULLEN_Msk;
    PORT_REGS->GROUP[0].PORT_OUTSET = (1U << 15U);
}

static bool sw1(void) { return ((PORT_REGS->GROUP[1].PORT_IN >> 10U) & 1U) == 0; }
static bool sw2(void) { return ((PORT_REGS->GROUP[0].PORT_IN >> 15U) & 1U) == 0; }

/*******************************************************************************
 * VEHICLE STATE
 ******************************************************************************/
static uint16_t rpm = 850;
static uint8_t speed = 0, throttle = 0;
static bool brake = false, manual = false;

/* CRC-15 for CAN */
static uint16_t crc15(uint16_t id, uint8_t dlc, uint8_t* data) {
    uint16_t crc = 0;
    for(int i=10; i>=0; i--) { uint8_t n = ((id>>i)&1)^((crc>>14)&1); crc = (crc<<1)&0x7FFF; if(n) crc^=0x4599; }
    for(int i=3; i>=0; i--) { uint8_t n = ((dlc>>i)&1)^((crc>>14)&1); crc = (crc<<1)&0x7FFF; if(n) crc^=0x4599; }
    for(int b=0; b<dlc; b++) for(int i=7; i>=0; i--) { uint8_t n = ((data[b]>>i)&1)^((crc>>14)&1); crc = (crc<<1)&0x7FFF; if(n) crc^=0x4599; }
    return crc;
}

/* Update Vehicle State */
static void update(void) {
    bool s1 = sw1(), s2 = sw2();
    throttle = read_pot();
    
    if(s1 || s2) manual = true;
    
    if(s1 && !s2) {
        brake = false; LED4_OFF();
        if(rpm < 6000) rpm += 100;
        if(speed < 180) speed += 3;
    } else if(s2 && !s1) {
        brake = true; LED4_ON();
        if(rpm > 800) rpm -= 150;
        if(speed > 0) speed -= 5; else speed = 0;
    } else {
        brake = false; LED4_OFF();
        if(manual) {
            if(throttle > 20) { rpm = 800 + throttle*40; if(speed < throttle) speed++; }
            else { if(rpm > 800) rpm -= 30; if(speed > 0) speed--; }
        } else {
            static uint8_t ph=0; static uint16_t t=0; t++;
            if(ph==0) { rpm=850; speed=0; throttle=0; if(t>5) {ph=1;t=0;} }
            else if(ph==1) { if(throttle<70) throttle+=10; rpm=1000+throttle*30; if(speed<80) speed+=5; if(speed>=80) {ph=2;t=0;} }
            else if(ph==2) { rpm=2200; speed=80; throttle=30; if(t>8) {ph=3;t=0;} }
            else { throttle=0; brake=true; if(rpm>800) rpm-=150; if(speed>0) speed-=8; else {speed=0;brake=false;} if(speed==0) {ph=0;t=0;} }
        }
    }
    
    /* RGB1 LED - Speed-based colors */
    if(brake)          rgb_red();
    else if(speed>100) rgb_yellow();
    else if(speed>60)  rgb_green();
    else if(speed>20)  rgb_cyan();
    else if(speed>0)   rgb_blue();
    else               rgb_white();
}

/*******************************************************************************
 * TERMINAL DISPLAY FUNCTIONS
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
    print("  Mode:           "); println(manual ? "MANUAL (user control)" : "AUTO SIMULATION");
    println("");
    
    print("  SW1: "); print(sw1() ? "PRESSED  " : "---      ");
    print("  SW2: "); println(sw2() ? "PRESSED" : "---");
    println("");
}

static void show_can(uint16_t id, const char* name, uint8_t dlc, uint8_t* data) {
    LED1_ON();
    beep(3000, 5);
    
    println("------------------------------------------------------------");
    println("CAN FRAME TRANSMITTED:");
    println("");
    print("  Message:        "); println(name);
    print("  CAN ID:         "); print_hex(id, 3); println("");
    print("  Data Length:    "); print_int(dlc); println(" bytes");
    print("  Data Bytes:     ");
    for(int i=0; i<dlc; i++) { print_hex(data[i], 2); print(" "); }
    println("");
    print("  CRC-15:         "); print_hex(crc15(id, dlc, data), 4); println("");
    println("");
    
    delay_ms(50);
    LED1_OFF();
}

/*******************************************************************************
 * MAIN FUNCTION
 ******************************************************************************/
int main(void) {
    SYS_Initialize(NULL);
    btn_init();
    ADC_Enable();
    rgb_init();
    
    /* Startup */
    clear();
    println("");
    println("========================================");
    println("        SYSTEM INITIALIZATION");
    println("========================================");
    println("");
    
    println("Testing hardware...");
    print("  LED1-4:  "); LED1_ON(); delay_ms(100); LED1_OFF();
    LED2_ON(); delay_ms(100); LED2_OFF();
    LED3_ON(); delay_ms(100); LED3_OFF();
    LED4_ON(); delay_ms(100); LED4_OFF(); println("OK");
    
    println("  RGB1 LED test:");
    rgb_red(); delay_ms(300);
    rgb_green(); delay_ms(300);
    rgb_blue(); delay_ms(300);
    rgb_off(); println("    R->G->B OK");
    
    print("  Buzzer:  "); beep(1000, 100); println("OK");
    print("  ADC:     "); print_int(read_pot()); println("% OK");
    
    /* Initialize OLED Display */
    print("  OLED:    ");
    oled_init_display();
    println("OK");
    
    println("");
    println("Starting CAN simulation...");
    beep(1000,100); delay_ms(50); beep(1500,100); delay_ms(50); beep(2000,150);
    delay_ms(1000);
    
    /* Main Loop */
    uint8_t msg = 0;
    uint8_t data[8];
    uint16_t current_can_id = 0x0C0;
    
    while(1) {
        update();
        clear();
        show_title();
        show_controls();
        show_status();
        
        switch(msg) {
            case 0: /* RPM */
                data[0] = (rpm >> 8) & 0xFF;
                data[1] = rpm & 0xFF;
                current_can_id = 0x0C0;
                show_can(0x0C0, "ENGINE_RPM", 2, data);
                
                print("  Formula:        RPM = (Data[0] << 8) | Data[1]\n");
                print("  Calculation:    RPM = (");
                print_int(data[0]); print(" x 256) + ");
                print_int(data[1]); print(" = ");
                print_int(rpm); println("");
                println("");
                break;
                
            case 1: /* Speed */
                data[0] = speed;
                current_can_id = 0x0D0;
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
                show_can(0x0F0, "THROTTLE_BRAKE", 2, data);
                
                println("  Formula:        Throttle = Data[0], Brake = Data[1]");
                print("  Calculation:    Throttle = ");
                print_int(throttle); print("%, Brake = ");
                println(brake ? "ON (1)" : "OFF (0)");
                println("");
                break;
        }
        
        println("============================================================");
        
        /* Update OLED Display */
        oled_update_display(rpm, speed, throttle, brake, current_can_id, 
                           (msg == 1) ? 1 : 2, data);
        
        msg++;
        if(msg > 2) msg = 0;
        
        delay_ms(2000);
    }
    
    return 0;
}
