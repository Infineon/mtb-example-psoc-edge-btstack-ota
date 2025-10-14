/*****************************************************************************
* File Name        : ota_task.c
*
* Description      : This file contains the OTA task and callback
*
* Related Document : See README.md
*
*******************************************************************************
* (c) 2024-2025, Infineon Technologies AG, or an affiliate of Infineon
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
/* Header file includes */

#include "stdio.h"
#include "ota_context.h"
#include "cy_ota_storage_api.h"
#include "app_bt_gatt_handler.h"
#include "wiced_bt_stack.h"
#include "cycfg_bt_settings.h"
#include "app_bt_fw_dwnld_check.h"
#include "wiced_bt_ble.h"
/******************************************************
 *               Variable Definitions
 ******************************************************/
/**
 * @brief network parameters for OTA
 */
cy_ota_network_params_t     ota_network_params = { CY_OTA_CONNECTION_UNKNOWN };

/**
 * @brief aAgent parameters for OTA
 */
cy_ota_agent_params_t     ota_agent_params = { 0 };

/**
 * @brief Storage interface APIs for storage operations.
 */
cy_ota_storage_interface_t ota_interfaces =
{
   .ota_file_open            = cy_ota_storage_open,
   .ota_file_read            = cy_ota_storage_read,
   .ota_file_write           = cy_ota_storage_write,
   .ota_file_close           = cy_ota_storage_close,
   .ota_file_verify          = cy_ota_storage_verify,
   .ota_file_validate        = cy_ota_storage_image_validate,
   .ota_file_get_app_info    = cy_ota_storage_get_app_info
};
/**
 * @brief OTA app context.
 */
ota_app_context_t ota_app;
/******************************************************
 *               Function Definitions
 ******************************************************/

/*******************************************************************************
 * Function Name: ota_task
 *******************************************************************************
 * Summary:
 *  Task used to establish a connection to a remote TCP client.
 *
 * Parameters:
 *  void *args : Task parameter defined during task creation (unused)
 *
 * Return:
 *  void
 *
 *******************************************************************************/
void ota_task(void * arg)
{
    cy_rslt_t result;
    uint32_t image_ID;

    wiced_result_t  wiced_result;

    // Disable watchdog to prevent wakeup from deep sleep mode
    Cy_WDT_Unlock();
    Cy_WDT_Disable();
    Cy_WDT_Lock();

    /* Initialize external flash */
    result = cy_ota_storage_init();

    /* External flash init failed. Stop program execution */
    if (result!=CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Validate all three images so that swap is permanent */
    for ( image_ID = 1; image_ID < 4; image_ID++)
    {
        cy_ota_storage_image_validate(image_ID);
    }

    /* set default values for OTA support */
    ota_initialize_default_values();

    if ( CY_RSLT_SUCCESS != cy_ota_ble_check_build_vs_configurator() )
    {
        printf("Failed configurator check\n");
        CY_ASSERT(0);
    }

    /* Check if the BT firmware download is successful by creating and
     * starting a FreeRTOS timer.*/
    app_bt_fw_download_check();

    printf("===============================================================\n");

    /* Register call back and configuration with stack */
    wiced_result = wiced_bt_stack_init(app_bt_management_callback , &cy_bt_cfg_settings);

    if( WICED_BT_SUCCESS == wiced_result)
    {
        printf("Bluetooth(r) Stack Initialization Successful\n");
    }
    else
    {
        printf("Bluetooth(r) Stack Initialization failed!! wiced_result 0x%x\n", wiced_result);
    }

    printf("===============================================================\n");

    while(true)
    {

    }

 }
/*******************************************************************************
 * Function Name: init_ota()
 *******************************************************************************
 * Summary:
 *  Initialize and start the OTA update
 *
 * Parameters:
 *  pointer to Application context
 *
 * Return:
 *  cy_rslt_t
 *
 *******************************************************************************/
cy_rslt_t init_ota(ota_app_context_t *ota)
{
    cy_rslt_t               result;

    if (ota == NULL || ota->tag != OTA_APP_TAG_VALID)
    {
        return CY_RSLT_OTA_ERROR_BADARG;
    }

    memset(&ota_network_params, 0, sizeof(ota_network_params));
    memset(&ota_agent_params,   0, sizeof(ota_agent_params));

    /* Ota Network Parameters */
    ota_network_params.initial_connection = ota->connection_type;

    /* OTA Agent parameters*/
    /* set validate_after_reboot = 0 so we don't have to call cy_ota_validated() on reboot */
    ota_agent_params.validate_after_reboot  = 1;

    /* OTA Agent start*/
    result = cy_ota_agent_start(&ota_network_params, &ota_agent_params, &ota_interfaces, &ota_app.ota_context);

    if (result != CY_RSLT_SUCCESS)
    {
        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_ota_agent_start() Failed - result: 0x%lx\n", result);
    }
    else
    {
        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "cy_ota_agent_start() Result: 0x%lx\n", result);
    }

    return result;
}
/*******************************************************************************
 * Function Name: ota_initialize_default_values()
 *******************************************************************************
 * Summary:
 *  Initialize default values for OTA
 *
 * Parameters:
 *  None
 *
 * Return:
 *  void
 *
 *******************************************************************************/
void ota_initialize_default_values(void)
{
    ota_app.tag               = OTA_APP_TAG_VALID;
    ota_app.connection_type   = CY_OTA_CONNECTION_BLE;
    ota_app.reboot_at_end     = 1;

};
