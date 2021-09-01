/*******************************************************************************
 * File Name:   main.c
 *
 * Version 1.0
 *
 * Description:
 *
 * This project demonstrates LIN Slave functionality of PSoC 4. If the LIN slave
 * receives an unconditional frame from LIN master with Frame ID 0x10 (InFrame),
 * then the first byte of data (command) controls 3 LEDs on the kit as follows:
 *
 * Command 0x00 --> All LEDs OFF
 * Command 0x11 --> Red LED ON (LED1 on the kit)
 * Command 0x22 --> Green LED ON (LED2 on the kit)
 * Command 0x33 --> Blue LED ON (LED3 on the kit)
 *
 * The LIN Master can read the status of LEDs and previously received command
 * by sending an unconditional frame with Frame ID 0x11
 *
 * The master receives the following status from LIN Slave based on the LED
 * status as follows:
 * If Red LED ON   --> 0xAA
 * If Green LED ON --> 0xBB
 * If Blue LED ON  --> 0xCC
 * If All LEDs OFF --> 0xDD
 *
 * Note: A LIN analyzer or an external master is required to test this project.
 *
 * Hardware Dependency:
 * This project requires CY8CKIT-026 CAN and LIN Shield Kit, CY8CKIT-041-41XX
 * PSoC 4100S Pioneer Kit and a third party LIN analyzer or any LIN master.
 *
 * This project needs to be programmed to PSoC 4 of CY8CKIT-041-41XX Kit.
 *
 * CY8CKIT-026 CAN and LIN Shield Kit should be placed on to the Arduino headers
 * of the CY8CKIT-041-41XX. And connect the KIT-026 pins as follows:
 * J4_D0 to LIN1_RX (J15_1) or LIN2_RX (J6_1)
 * J4_D1 to LIN1_TX (J15_2) or LIN2_TX (J6_2)
 * J1_V3.3 to LIN1_NSLP (J15_3) or LIN2_NSLP (J6_3)
 * And connect a 12V supply to the 'Vin' pin (J11 or J12) on CY8CKIT-026.
 *
 * Related Document: See Readme.md
 *
 *******************************************************************************
 * Copyright 2021, Cypress Semiconductor Corporation (an Infineon company)
 * or an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
 *
 * This software, including source code, documentation and related
 * materials ("Software") is owned by Cypress Semiconductor Corporation
 * or one of its affiliates ("Cypress") and is protected by and subject to
 * worldwide patent protection (United States and foreign),
 * United States copyright laws and international treaty provisions.
 * Therefore, you may use this Software only as provided in the license
 * agreement accompanying the software package from which you
 * obtained this Software ("EULA").
 * If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
 * non-transferable license to copy, modify, and compile the Software
 * source code solely for use in connection with Cypress's
 * integrated circuit products.  Any reproduction, modification, translation,
 * compilation, or representation of this Software except as specified
 * above is prohibited without the express written permission of Cypress.
 *
 * Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
 * reserves the right to make changes to the Software without notice. Cypress
 * does not assume any liability arising out of the application or use of the
 * Software or any product or circuit described in the Software. Cypress does
 * not authorize its products for use in any products where a malfunction or
 * failure of the Cypress product may reasonably be expected to result in
 * significant property damage, injury or death ("High Risk Product"). By
 * including Cypress's product in a High Risk Product, the manufacturer
 * of such system or application assumes all risk of such use and in doing
 * so agrees to indemnify Cypress against all liability.
 ******************************************************************************/

/*******************************************************************************
 *               Include Files
 ******************************************************************************/
#include "cy_pdl.h"
#include "cyhal.h"

#include "mtbcfg_lin.h"
#include "resource_map.h"

/*******************************************************************************
 *               Macros
 ******************************************************************************/
/* LIN Instance number */
#define LIN_IFC_HANDLE          (0u)

/* Define start position and number of bytes for input & output signals */
/* One command byte received from Master */
#define LIN_SIGNALINPUT_START_BYTE      (0u)
#define LIN_SIGNALINPUT_NUM_OF_BYTES    (1u)

/* 2 status bytes from Slave to Master - previous command and LED status */
#define LIN_SIGNALOUTPUT_START_BYTE     (0u)
#define LIN_SIGNALOUTPUT_NUM_OF_BYTES   (2u)

/* Interrupt priority for SCB ISR */
#define LIN_SCB_INT_PRIORITY    (1u)

/* Commands sent from LIN master to set LEDs */
#define CMD_SET_LED1    (0x11u)
#define CMD_SET_LED2    (0x22u)
#define CMD_SET_LED3    (0x33u)
#define CMD_SET_OFF     (0x00u)

/* Commands sent back to LIN master by slave */
#define CMD_SENT_LED1   (0xAAu)
#define CMD_SENT_LED2   (0xBBu)
#define CMD_SENT_LED3   (0xCCu)
#define CMD_SENT_OFF    (0xDDu)

/* Turn ON LED1 - RED LED */
#define LED1_ON \
    {   \
        cyhal_gpio_write(LED1_PIN, false); \
        cyhal_gpio_write(LED2_PIN, true);  \
        cyhal_gpio_write(LED3_PIN, true);  \
    }

/* Turn ON LED2 - GREEN LED */
#define LED2_ON \
    {   \
        cyhal_gpio_write(LED1_PIN, true);  \
        cyhal_gpio_write(LED2_PIN, false); \
        cyhal_gpio_write(LED3_PIN, true);  \
    }

/* Turn ON LED3 - BLUE LED */
#define LED3_ON \
    {   \
        cyhal_gpio_write(LED1_PIN, true);  \
        cyhal_gpio_write(LED2_PIN, true);  \
        cyhal_gpio_write(LED3_PIN, false); \
    }

/* Turn OFF all LEDs */
#define ALL_LEDS_OFF \
    {   \
        cyhal_gpio_write(LED1_PIN, true); \
        cyhal_gpio_write(LED2_PIN, true); \
        cyhal_gpio_write(LED3_PIN, true); \
    }

#define CY_ASSERT_FAILED      (0u)

/* Allocate context for LIN operation */
mtb_stc_lin_context_t lin_context;

/*******************************************************************************
 * Function Name: LIN_Isr
 *******************************************************************************
 * Summary:
 *  Implement SCB ISR for LIN
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *
 ******************************************************************************/
static void LIN_Isr(void)
{
    l_ifc_rx(LIN_IFC_HANDLE, &lin_context);
}

/*******************************************************************************
 * Function Name: LIN_InactivityIsr
 *******************************************************************************
 * Summary:
 *  Implement Inactivity ISR for LIN
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *
 ******************************************************************************/
static void LIN_InactivityIsr(void)
{
    l_ifc_aux(LIN_IFC_HANDLE, &lin_context);
}

/*******************************************************************************
 * Function Name: handle_error
 *******************************************************************************
 * Summary:
 *  User defined error handling function
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *
 ******************************************************************************/
void handle_error(void)
{
    /* Disable all interrupts. */
    __disable_irq();

    /* Stop program execution if any unexpected error happened*/
    CY_ASSERT(CY_ASSERT_FAILED);
}

/*******************************************************************************
 * Function Name: main
 *******************************************************************************
 * Summary:
 *  Initialize LIN slave. If the LIN slave receives an unconditional frame from
 *  master with Frame ID 0x10, then the first byte of data (command to control
 *  the LED) is written to the other unconditional frame (OutFrame). Based on
 *  the received command slave will control three LEDs on the kit.
 *
 *  The LIN Master can read the status of the LEDs by sending the Frame ID 0x11
 *
 * Parameters:
 *  void
 *
 * Return:
 *  int
 *
 ******************************************************************************/
int main(void)
{
    /* Local variables */
    uint8_t dataReceived = 0u;
    uint8_t dataArray[2] = { 0u, 0u };

    cy_rslt_t result = CY_RSLT_SUCCESS;

    /* Initialize the device and board peripherals */
    result = cybsp_init();

    /* Board init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(CY_ASSERT_FAILED);
    }

    /* Enable global interrupts */
    __enable_irq();

    /* Initialize Board LEDs */
    /* LED 1 - Red LED */
    if (CY_RSLT_SUCCESS != cyhal_gpio_init(LED1_PIN, CYHAL_GPIO_DIR_OUTPUT, \
            CYHAL_GPIO_DRIVE_STRONG, true))
    {
        handle_error();
    }

    /* LED 2 - GREEN LED */
    if (CY_RSLT_SUCCESS != cyhal_gpio_init(LED2_PIN, CYHAL_GPIO_DIR_OUTPUT, \
            CYHAL_GPIO_DRIVE_STRONG, true))
    {
        handle_error();
    }

    /* LED 3 - BLUE LED */
    if (CY_RSLT_SUCCESS != cyhal_gpio_init(LED3_PIN, CYHAL_GPIO_DIR_OUTPUT, \
            CYHAL_GPIO_DRIVE_STRONG, true))
    {
        handle_error();
    }

    /* Initialize the LIN core that is specified by the context structure */
    if (0u == l_sys_init(&mtb_lin_0_config, &lin_context, &LIN_Isr, \
            LIN_SCB_INT_PRIORITY, &LIN_InactivityIsr))
    {
        handle_error();
    }

    /* Initialize the LIN instance that is specified by the context structure.
     * Choose appropriate pins for tx and rx direction from HAL library.
     */
    if (CY_RSLT_SUCCESS != l_ifc_init(LIN_IFC_HANDLE, &lin_context, \
            LIN_TX_PIN, LIN_RX_PIN))
    {
        handle_error();
    }

    while (1)
    {
        /***********************************************************************
         * Check if "InFrame" frame is received from LIN Master
         **********************************************************************/
        if (true == l_flg_tst(MTB_LIN_0_FLAG_HANDLE_InFrame, &lin_context))
        {
            /* Read the 1st byte command received from the LIN Master */
            l_bytes_rd(MTB_LIN_0_SIGNAL_HANDLE_SignalInput, \
                    LIN_SIGNALINPUT_START_BYTE, \
                    LIN_SIGNALINPUT_NUM_OF_BYTES, \
                    &dataReceived, &lin_context);

            /* Clear frame flag */
            l_flg_clr(MTB_LIN_0_FLAG_HANDLE_InFrame, &lin_context);
            
            /* Store the received command in dataArray */
            dataArray[0] = dataReceived;

            /* Turn on the LED corresponding to the command received */
            if (CMD_SET_LED1 == dataReceived)
            {
                LED1_ON;
                dataArray[1] = CMD_SENT_LED1;
            }
            else if (CMD_SET_LED2 == dataReceived)
            {
                LED2_ON;
                dataArray[1] = CMD_SENT_LED2;
            }
            else if (CMD_SET_LED3 == dataReceived)
            {
                LED3_ON;
                dataArray[1] = CMD_SENT_LED3;
            }
            else if (CMD_SET_OFF == dataReceived)
            {
                ALL_LEDS_OFF;
                dataArray[1] = CMD_SENT_OFF;
            }

            /* Send the previous command and the status of LEDs to LIN Master */
            l_bytes_wr(MTB_LIN_0_SIGNAL_HANDLE_SignalOutput, \
                    LIN_SIGNALOUTPUT_START_BYTE, \
                    LIN_SIGNALOUTPUT_NUM_OF_BYTES, \
                    dataArray, &lin_context);
        }

        /***********************************************************************
         * Check if the data in "OutFrame" frame is sent to LIN Master
         **********************************************************************/
        if (true == l_flg_tst(MTB_LIN_0_FLAG_HANDLE_OutFrame, &lin_context))
        {
            /* Clear frame flag */
            l_flg_clr(MTB_LIN_0_FLAG_HANDLE_OutFrame, &lin_context);
        }
    }
}

/* [] END OF FILE */
