/*******************************************************************************
  Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    This file contains the "main" function for a project.

  Description:
    This file contains the "main" function for a project.  The
    "main" function calls the "SYS_Initialize" function to initialize the state
    machines of all modules in the system
 *******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************
#include <stddef.h>                     // Defines NULL
#include <stdbool.h>                    // Defines true
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include "definitions.h"                // SYS function prototypes
#include "OLED128x64.c"                 /* includes OLED128x64.h + OLED_FONTs.c internally */

volatile bool       bFlag_200ms = 0 ;
volatile bool       bIsI2C_DONE = true ;
uint8_t             ASCII_Buffer[24];
// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************
void    I2C_EventHandler(uintptr_t  contest)
{
    if (SERCOM2_I2C_ErrorGet() == SERCOM_I2C_ERROR_NONE )
        bIsI2C_DONE = true ;
}

void SYSTICK_EventHandler(uintptr_t context)
{
    /* Toggle LED */
    LED1_Toggle();
    bFlag_200ms = 1 ;
}

int main ( void )
{
    /* Initialize all modules */
    SYS_Initialize ( NULL );
    /* Register Callback */
    SYSTICK_TimerCallbackSet(SYSTICK_EventHandler, (uintptr_t) NULL);
    /* Start the Timer */
    SYSTICK_TimerStart();
    ADC_Enable();
 
    bFlag_200ms = 0 ;
    while ( bFlag_200ms ==0){;}      // 等待 OLED RESET 完成 !
    bFlag_200ms = 0 ;
    SERCOM2_I2C_CallbackRegister(I2C_EventHandler,0) ;    
    OLED_Init();
    OLED_CLS();
    OLED_Put8x16Str(0, 0, (const unsigned char*) "NCUExMicrochip") ;    
    OLED_Put8x16Str(0, 2, (const unsigned char*) "PIC32CM3204GV048") ;    
    OLED_Put8x16Str(0, 4, (const unsigned char*) "VR =  ") ;   
    
    while ( true )
    {
        /* Maintain state machines of all polled MPLAB Harmony modules. */
        SYS_Tasks ( );
        if (bFlag_200ms)
        {
            // ADC0_ChannelSelect(ADC_POSINPUT_AIN0, ADC_NEGINPUT_GND);            
            ADC_ChannelSelect(ADC_POSINPUT_PIN1,ADC_NEGINPUT_GND);
            ADC_ConversionStart();
            while (!ADC_ConversionStatusGet()) ;            
            printf ("ADC = %4d\n\r", ADC_ConversionResultGet()) ;
            sprintf((char*)ASCII_Buffer,"%4d",ADC_ConversionResultGet());
            OLED_Put8x16Str(40, 4, ASCII_Buffer) ;   
            bFlag_200ms = 0 ;
        }
    }

    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}

/*******************************************************************************
 End of File
*/

