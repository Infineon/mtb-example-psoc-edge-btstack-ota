/******************************************************************************
* File Name: app_bt_fw_dwnld_check.c
*
* Description:
*   This source file contains functions to verify whether the BT firmware
*   downloaded successfully in order to verify whether the connected hardware
*   is of proper version.
*
* Related Document: See README.md
*
*
*******************************************************************************
* (c) 2023-2026, Infineon Technologies AG, or an affiliate of Infineon
* Technologies AG. All rights reserved.
* This software, associated documentation and materials ("Software") is
* owned by Infineon Technologies AG or one of its affiliates ("Infineon")
* and is protected by and subject to worldwide patent protection, worldwide
* copyright laws, and international treaty provisions. Therefore, you may use
* this Software only as provided in the license agreement accompanying the
* software package from which you obtained this Software. If no license
* agreement applies, then any use, reproduction, modification, translation, or
* compilation of this Software is prohibited without the express written
* permission of Infineon.
*
* Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE
* IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF
* THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A
* SPECIFIC USE/PURPOSE OR MERCHANTABILITY.
* Infineon reserves the right to make changes to the Software without notice.
* You are responsible for properly designing, programming, and testing the
* functionality and safety of your intended application of the Software, as
* well as complying with any legal requirements related to its use. Infineon
* does not guarantee that the Software will be free from intrusion, data theft
* or loss, or other breaches ("Security Breaches"), and Infineon shall have
* no liability arising out of any Security Breaches. Unless otherwise
* explicitly approved by Infineon, the Software may not be used in any
* application where a failure of the Product or any consequences of the use
* thereof can reasonably be expected to result in personal injury.
*******************************************************************************/

/*******************************************************************************
* Header Files
*******************************************************************************/
#include "app_bt_fw_dwnld_check.h"

/******************************************************************************
 * Macros
 *****************************************************************************/

 /* A FreeRTOS time is started with this timeout value. The timer is freed if
  * the BTM_ENABLED_EVT is received before this timeout otherwise message is
  * printed in the timer callback asking the user to verify the hardware
  * version.
  */
#define BT_FW_DWNLD_TIMEOUT  (15000)

/*******************************************************************************
* Variable Definitions
*******************************************************************************/
TimerHandle_t ms_timer_app_bt_fw;


/******************************************************************************
 * Function Name: void app_bt_fw_timeout(TimerHandle_t timer_handle)
 *******************************************************************************
 * Summary:
 *           This function is for the one-shot FreeRTOS timer which handles
 *           the timeout of BT FW download
 *
 * Parameters:
 *          TimerHandle_t timer_handle
 *
 * Return:
 *          None
 *
 ******************************************************************************/

static void app_bt_fw_timeout(TimerHandle_t timer_handle)
{
    printf ("\n----------------------------------------------------------------------------------------------------------------------------------- \
             \r\nInit failed! \
             \r\nThis version of the code example only supports EVK versions REV *C and above, that contain the latest revision of CYW55513 device. \
             \r\nVerify your EVK version otherwise contact Infineon representative to get the latest hardware. \
             \r\nIt could be hardware issue if you are using the latest hardware and still see this error.\
             \n-------------------------------------------------------------------------------------------------------------------------------------\n ");

    free_app_bt_fw_download_check_timer();

     CY_ASSERT(0);

}

/******************************************************************************
 * Function Name: void app_bt_fw_download_check(void)
 *******************************************************************************
 * Summary:
 *           Creates a freertos timer
 *
 * Parameters:
 *          None
 *
 * Return:
 *          None
 *
 ******************************************************************************/

void app_bt_fw_download_check(void)
{
    BaseType_t ret;

       ms_timer_app_bt_fw = xTimerCreate("app_bt_fw_dwnld_chk_timer",
                               pdMS_TO_TICKS(BT_FW_DWNLD_TIMEOUT),
                               pdFALSE,
                               NULL,
                               app_bt_fw_timeout);

        if (ms_timer_app_bt_fw != NULL)
        {
            ret = xTimerStart(ms_timer_app_bt_fw, 0);
            if (ret == pdFALSE)
            {
                 printf("\r\nTimer Start failed\r\n");
            }
        }

}

/******************************************************************************
 * Function Name: void free_app_bt_fw_download_check_timer(void)
 *******************************************************************************
 * Summary:
 *           This function stops and deletes a freertos timer.
 *
 * Parameters:
 *          None
 *
 * Return:
 *          None
 *
 ******************************************************************************/

void free_app_bt_fw_download_check_timer(void)
{
     BaseType_t ret ;

    ret = xTimerStop(ms_timer_app_bt_fw, 0);
    if (ret == pdTRUE)
    {
        void*      cb  = pvTimerGetTimerID(ms_timer_app_bt_fw);
        ret = xTimerDelete(ms_timer_app_bt_fw, 0);
        if (ret == pdFALSE)
        {
             printf("\r\nTimer delete failed\r\n");
        }
        else
        {
            free(cb);
        }
    }
    else
    {
        printf("\r\nTimer stop failed\r\n");
    }
}
