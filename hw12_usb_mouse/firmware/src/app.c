/*******************************************************************************
  MPLAB Harmony Application Source File
  
  Company:
    Microchip Technology Inc.
  
  File Name:
    app.c

  Summary:
    This file contains the source code for the MPLAB Harmony application.

  Description:
    This file contains the source code for the MPLAB Harmony application.  It 
    implements the logic of the application's state machine and it may call 
    API routines of other MPLAB Harmony modules in the system, such as drivers,
    system services, and middleware.  However, it does not call any of the
    system interfaces (such as the "Initialize" and "Tasks" functions) of any of
    the modules in the system or make any assumptions about when those functions
    are called.  That is the responsibility of the configuration-specific system
    files.
 *******************************************************************************/

// DOM-IGNORE-BEGIN
/*******************************************************************************
Copyright (c) 2013-2014 released Microchip Technology Inc.  All rights reserved.

Microchip licenses to you the right to use, modify, copy and distribute
Software only when embedded on a Microchip microcontroller or digital signal
controller that is integrated into your product or third party product
(pursuant to the sublicense terms in the accompanying license agreement).

You should refer to the license agreement accompanying this Software for
additional information regarding your rights and obligations.

SOFTWARE AND DOCUMENTATION ARE PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF
MERCHANTABILITY, TITLE, NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE.
IN NO EVENT SHALL MICROCHIP OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER
CONTRACT, NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR
OTHER LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE OR
CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT OF
SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
(INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.
 *******************************************************************************/
// DOM-IGNORE-END


// *****************************************************************************
// *****************************************************************************
// Section: Included Files 
// *****************************************************************************
// *****************************************************************************

#include "app.h"
#include <stddef.h>                     // Defines NULL
#include <stdbool.h>                    // Defines true
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include "system/common/sys_module.h"   // SYS function prototypes
#include <xc.h>                         // processor SFR definitions
#include <stdio.h>
#include <math.h>
#include "ili9341.h"
#include "I2C_master_no_int.h"
#include "HW7_functions.h"


// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************

// *****************************************************************************
/* Application Data

  Summary:
    Holds application data

  Description:
    This structure holds the application's data.

  Remarks:
    This structure should be initialized by the APP_Initialize function.
    
    Application strings and buffers are be defined outside this structure.
 */

APP_DATA appData;

/* Mouse Report */
MOUSE_REPORT mouseReport APP_MAKE_BUFFER_DMA_READY;
MOUSE_REPORT mouseReportPrevious APP_MAKE_BUFFER_DMA_READY;


// *****************************************************************************
// *****************************************************************************
// Section: Application Callback Functions
// *****************************************************************************
// *****************************************************************************

void APP_USBDeviceHIDEventHandler(USB_DEVICE_HID_INDEX hidInstance,
        USB_DEVICE_HID_EVENT event, void * eventData, uintptr_t userData) {
    APP_DATA * appData = (APP_DATA *) userData;

    switch (event) {
        case USB_DEVICE_HID_EVENT_REPORT_SENT:

            /* This means the mouse report was sent.
             We are free to send another report */

            appData->isMouseReportSendBusy = false;
            break;

        case USB_DEVICE_HID_EVENT_REPORT_RECEIVED:

            /* Dont care for other event in this demo */
            break;

        case USB_DEVICE_HID_EVENT_SET_IDLE:

            /* Acknowledge the Control Write Transfer */
            USB_DEVICE_ControlStatus(appData->deviceHandle, USB_DEVICE_CONTROL_STATUS_OK);

            /* save Idle rate received from Host */
            appData->idleRate = ((USB_DEVICE_HID_EVENT_DATA_SET_IDLE*) eventData)->duration;
            break;

        case USB_DEVICE_HID_EVENT_GET_IDLE:

            /* Host is requesting for Idle rate. Now send the Idle rate */
            USB_DEVICE_ControlSend(appData->deviceHandle, &(appData->idleRate), 1);

            /* On successfully receiving Idle rate, the Host would acknowledge back with a
               Zero Length packet. The HID function driver returns an event
               USB_DEVICE_HID_EVENT_CONTROL_TRANSFER_DATA_SENT to the application upon
               receiving this Zero Length packet from Host.
               USB_DEVICE_HID_EVENT_CONTROL_TRANSFER_DATA_SENT event indicates this control transfer
               event is complete */

            break;

        case USB_DEVICE_HID_EVENT_SET_PROTOCOL:
            /* Host is trying set protocol. Now receive the protocol and save */
            appData->activeProtocol = *(USB_HID_PROTOCOL_CODE *) eventData;

            /* Acknowledge the Control Write Transfer */
            USB_DEVICE_ControlStatus(appData->deviceHandle, USB_DEVICE_CONTROL_STATUS_OK);
            break;

        case USB_DEVICE_HID_EVENT_GET_PROTOCOL:

            /* Host is requesting for Current Protocol. Now send the Idle rate */
            USB_DEVICE_ControlSend(appData->deviceHandle, &(appData->activeProtocol), 1);

            /* On successfully receiving Idle rate, the Host would acknowledge
              back with a Zero Length packet. The HID function driver returns
              an event USB_DEVICE_HID_EVENT_CONTROL_TRANSFER_DATA_SENT to the
              application upon receiving this Zero Length packet from Host.
              USB_DEVICE_HID_EVENT_CONTROL_TRANSFER_DATA_SENT event indicates
              this control transfer event is complete */
            break;

        case USB_DEVICE_HID_EVENT_CONTROL_TRANSFER_DATA_SENT:
            break;

        default:
            break;
    }
}

/*******************************************************************************
  Function:
    void APP_USBDeviceEventHandler (USB_DEVICE_EVENT event,
        USB_DEVICE_EVENT_DATA * eventData)

  Summary:
    Event callback generated by USB device layer.

  Description:
    This event handler will handle all device layer events.

  Parameters:
    None.

  Returns:
    None.
 */

void APP_USBDeviceEventHandler(USB_DEVICE_EVENT event, void * eventData, uintptr_t context) {
    USB_DEVICE_EVENT_DATA_CONFIGURED * configurationValue;
    switch (event) {
        case USB_DEVICE_EVENT_SOF:
            /* This event is used for switch debounce. This flag is reset
             * by the switch process routine. */
            appData.sofEventHasOccurred = true;
            appData.setIdleTimer++;
            break;
        case USB_DEVICE_EVENT_RESET:
        case USB_DEVICE_EVENT_DECONFIGURED:

            /* Device got deconfigured */

            appData.isConfigured = false;
            appData.isMouseReportSendBusy = false;
            appData.state = APP_STATE_WAIT_FOR_CONFIGURATION;
            //appData.emulateMouse = true;
            //BSP_LEDOn ( APP_USB_LED_1 );
            //BSP_LEDOn ( APP_USB_LED_2 );
            //BSP_LEDOff ( APP_USB_LED_3 );

            break;

        case USB_DEVICE_EVENT_CONFIGURED:

            /* Device is configured */
            configurationValue = (USB_DEVICE_EVENT_DATA_CONFIGURED *) eventData;
            if (configurationValue->configurationValue == 1) {
                appData.isConfigured = true;

                //BSP_LEDOff ( APP_USB_LED_1 );
                //BSP_LEDOff ( APP_USB_LED_2 );
                //BSP_LEDOn ( APP_USB_LED_3 );

                /* Register the Application HID Event Handler. */

                USB_DEVICE_HID_EventHandlerSet(appData.hidInstance,
                        APP_USBDeviceHIDEventHandler, (uintptr_t) & appData);
            }
            break;

        case USB_DEVICE_EVENT_POWER_DETECTED:

            /* VBUS was detected. We can attach the device */
            USB_DEVICE_Attach(appData.deviceHandle);
            break;

        case USB_DEVICE_EVENT_POWER_REMOVED:

            /* VBUS is not available any more. Detach the device. */
            USB_DEVICE_Detach(appData.deviceHandle);
            break;

        case USB_DEVICE_EVENT_SUSPENDED:
            //BSP_LEDOff ( APP_USB_LED_1 );
            //BSP_LEDOn ( APP_USB_LED_2 );
            //BSP_LEDOn ( APP_USB_LED_3 );
            break;

        case USB_DEVICE_EVENT_RESUMED:
        case USB_DEVICE_EVENT_ERROR:
        default:
            break;

    }
}

// *****************************************************************************
// *****************************************************************************
// Section: Application Initialization and State Machine Functions
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    void APP_Initialize ( void )

  Remarks:
    See prototype in app.h.
 */

void APP_Initialize(void) {
    /* Place the App state machine in its initial state. */
    appData.state = APP_STATE_INIT;
    appData.deviceHandle = USB_DEVICE_HANDLE_INVALID;
    appData.isConfigured = false;
    //appData.emulateMouse = true;
    appData.hidInstance = 0;
    appData.isMouseReportSendBusy = false;
//    PIC32_Startup();
    ANSELBbits.ANSB2 = 0;
    ANSELBbits.ANSB3 = 0;  
    I2C_master_setup();  // chooses baud rate and turns I2C2 on
    initLSM6DS33();  // turns on gyroscope and accelerometer, among other things
    SPI1_init();
    LCD_init();
    LCD_clearScreen(ILI9341_BLUE);  // clear to background color
//    LCD_write_WHO_AM_I();
}

/******************************************************************************
  Function:
    void APP_Tasks ( void )

  Remarks:
    See prototype in app.h.
 */

void APP_Tasks(void) {
//    static int8_t vector = 0;
//    static uint8_t movement_length = 0;
//    int8_t dir_table[] = {-4, -4, -4, 0, 4, 4, 4, 0};
    static uint8_t inc = 0;    
    
    /* Check the application's current state. */
    switch (appData.state) {
            /* Application's initial state. */
        case APP_STATE_INIT:
        {
            /* Open the device layer */
            appData.deviceHandle = USB_DEVICE_Open(USB_DEVICE_INDEX_0,
                    DRV_IO_INTENT_READWRITE);

            if (appData.deviceHandle != USB_DEVICE_HANDLE_INVALID) {
                /* Register a callback with device layer to get event notification (for end point 0) */
                USB_DEVICE_EventHandlerSet(appData.deviceHandle,
                        APP_USBDeviceEventHandler, 0);

                appData.state = APP_STATE_WAIT_FOR_CONFIGURATION;
            } else {
                /* The Device Layer is not ready to be opened. We should try
                 * again later. */
            }
            break;
        }

        case APP_STATE_WAIT_FOR_CONFIGURATION: 
        {    
            /* Check if the device is configured. The 
             * isConfigured flag is updated in the
             * Device Event Handler */

            if (appData.isConfigured) {
                appData.state = APP_STATE_MOUSE_EMULATE;
            }
            break;
        }
        case APP_STATE_MOUSE_EMULATE:
        {            
            unsigned char register_data[14];
            I2C_read_multiple(0b1101011, 0b00100000, register_data, 14); // register address 00100000 = OUT_TEMP_L
            signed short register_data_appended[7];
            int i = 0;
            for (i = 0; i < 7; i++) {
                register_data_appended[i] = ((register_data[i*2+1] << 8) | register_data[(i*2)]);
            }
            signed short accelX = register_data_appended[4];
            signed short accelY = register_data_appended[5];
            char accelX_s[15], accelY_s[15];
            sprintf(accelX_s, " accelX: %d  ", accelX);
            sprintf(accelY_s, " accelY: %d  ", accelY);
            LCD_drawString(accelX_s, 20, 40, ILI9341_WHITE, ILI9341_BLACK);
            LCD_drawString(accelY_s, 20, 60, ILI9341_WHITE, ILI9341_BLACK);
            float accelX_scaled = accelX * 0.0000305 * 120;   // 2^16/2 = 32768, 0.0000305 = 1/32768
            float accelY_scaled = accelY * 0.0000305 * 120;   // 2^16/2 = 32768, 0.0000305 = 1/32768     
            char accelX_scaled_s[30], accelY_scaled_s[30];
            sprintf(accelX_scaled_s, " accelX scaled: %6.2f  ", accelX_scaled);
            sprintf(accelY_scaled_s, " accelY scaled: %6.2f  ", accelY_scaled);
            LCD_drawString(accelX_scaled_s, 120, 40, ILI9341_WHITE, ILI9341_BLACK);
            LCD_drawString(accelY_scaled_s, 120, 60, ILI9341_WHITE, ILI9341_BLACK);
            
            int slowness = 1;

            if (inc == slowness) {
                inc = 0;
                // every 50th loop, or 20 times per second
    //            if (movement_length > 50) {
                appData.mouseButton[0] = MOUSE_BUTTON_STATE_RELEASED;   // released = no one's clicking the button
                appData.mouseButton[1] = MOUSE_BUTTON_STATE_RELEASED;   // released = no one's clicking the button
    //            appData.xCoordinate = (int8_t) dir_table[vector & 0x07];
    //            appData.yCoordinate = (int8_t) dir_table[(vector + 2) & 0x07];
                
                if (accelX_scaled > 0) {
                    appData.xCoordinate = (int8_t) 7;
                }
                else {
                    appData.xCoordinate = (int8_t) -7;
                }                
                
                if (accelY_scaled > 0) {
                    appData.yCoordinate = (int8_t) 7;
                }
                else {
                    appData.yCoordinate = (int8_t) -7;
                }
                
    //            vector++;
    //            movement_length = 0;
    //            }

                if (!appData.isMouseReportSendBusy) {
                    /* This means we can send the mouse report. The
                       isMouseReportBusy flag is updated in the HID Event Handler. */

                    appData.isMouseReportSendBusy = true;

                    /* Create the mouse report */

                    MOUSE_ReportCreate(appData.xCoordinate, appData.yCoordinate,
                            appData.mouseButton, &mouseReport);

                    if (memcmp((const void *) &mouseReportPrevious, (const void *) &mouseReport,
                            (size_t)sizeof (mouseReport)) == 0) {
                        /* Reports are same as previous report. However mouse reports
                         * can be same as previous report as the coordinate positions are relative.
                         * In that case it needs to be sent */
                        if ((appData.xCoordinate == 0) && (appData.yCoordinate == 0)) {
                            /* If the coordinate positions are 0, that means there
                             * is no relative change */
                            if (appData.idleRate == 0) {
                                appData.isMouseReportSendBusy = false;
                            } else {
                                /* Check the idle rate here. If idle rate time elapsed
                                 * then the data will be sent. Idle rate resolution is
                                 * 4 msec as per HID specification; possible range is
                                 * between 4msec >= idlerate <= 1020 msec.
                                 */
                                if (appData.setIdleTimer
                                        >= appData.idleRate * 4) {
                                    /* Send REPORT as idle time has elapsed */
                                    appData.isMouseReportSendBusy = true;
                                } else {
                                    /* Do not send REPORT as idle time has not elapsed */
                                    appData.isMouseReportSendBusy = false;
                                }
                            }
                        }

                    }
                    if (appData.isMouseReportSendBusy == true) {
                        /* Copy the report sent to previous */
                        memcpy((void *) &mouseReportPrevious, (const void *) &mouseReport,
                                (size_t)sizeof (mouseReport));

                        /* Send the mouse report. */
                        USB_DEVICE_HID_ReportSend(appData.hidInstance,
                                &appData.reportTransferHandle, (uint8_t*) & mouseReport,
                                sizeof (MOUSE_REPORT));
                        appData.setIdleTimer = 0;
                    }
    //                movement_length++;
                }
            }
            else {
                appData.xCoordinate = (int8_t) 0;
                appData.yCoordinate = (int8_t) 0;
            }
            inc++;
            
            break;
        }   
        case APP_STATE_ERROR:
        {
            break;

        }    
            /* The default state should never be executed. */
        default:
        {
            /* TODO: Handle error in application's state machine. */
            break;
        }
    }
}


/*******************************************************************************
 End of File
 */

