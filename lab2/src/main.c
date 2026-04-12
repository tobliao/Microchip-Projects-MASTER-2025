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

volatile bool bFlag_200ms = 0 ;
// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************
void SYSTICK_EventHandler(uintptr_t context)
{
    /* Toggle LED */
    LED1_Toggle();
    LED2_Toggle();
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
            bFlag_200ms = 0 ;
        }
    }

    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}


/*******************************************************************************
 End of File
*/

