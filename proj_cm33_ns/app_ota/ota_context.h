/*****************************************************************************
* File Name        : ota_context.h
*
* Description      : Definitions and data structures for the OTA application
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
#ifndef OTA_CONTEXT_H_
#define OTA_CONTEXT_H_

#include "cy_ota_api.h"

/******************************************************
 *                     Macros
 ******************************************************/

/******************************************************
 *                    Constants
 ******************************************************/

#define CY_OTA_BLE_TEMP_BUFFER_SIZE     (512)

#define OTA_APP_TAG_VALID               (0x51EDBA15)
#define OTA_APP_TAG_INVALID             (0xDEADBEEF)

/* OTA response buffer size */
#define CY_OTA_BT_RSP_BUFFER_SIZE   (64)

/******************************************************
 *                   Enumerations
 ******************************************************/

/* Logical Start of Emulated EEPROM and location of structure elements. */
#define LOGICAL_EEPROM_START          (0u)
#define EEPROM_SLOT_DATA              (LOGICAL_EEPROM_START)
#define EEPROM_IDENTITY_KEYS_START    (EEPROM_SLOT_DATA + sizeof(bondinfo.slot_data))
#define EEPROM_LINK_KEYS_START        (EEPROM_IDENTITY_KEYS_START + sizeof(wiced_bt_local_identity_keys_t))
#define GET_ADDR_FOR_DEVICE_KEYS(x)   (EEPROM_LINK_KEYS_START + (x * sizeof(wiced_bt_device_link_keys_t)))

/* enum for slot_data structure */
enum
{
    NUM_BONDED,
    NEXT_FREE
};

/* Macro for Number of devices allowed to bond */
#define BOND_MAX    4u

/* Structure to store info that goes into EEPROM - it holds the number of bonded devices, remote keys and local keys */
#pragma pack(1)
typedef struct bondinfo_s
{
    uint16_t                           slot_data[2];
    wiced_bt_device_link_keys_t        link_keys[BOND_MAX];
    wiced_bt_local_identity_keys_t     identity_keys;
} bondinfo_t;
#pragma pack()


/******************************************************
 *            OTA example app type definitions
 ******************************************************/

typedef struct
{
    uint32_t tag;

    cy_ota_context_ptr          ota_context;
    cy_ota_connection_t         connection_type;
    uint16_t                    bt_conn_id;                 /* Host Bluetooth® Connection ID */
    uint8_t                     bt_peer_addr[BD_ADDR_LEN];  /* Host Bluetooth® address */
    wiced_bt_ble_conn_params_t  bt_conn_params;             /* Bluetooth® connection parameters */
    uint16_t                    bt_config_descriptor;       /* Bluetooth® configuration to determine if Device sends Notification/Indication */
    uint8_t                     connected;                  /* 0 = not connected, 1 = connected to AP */
    uint8_t                     reboot_at_end;              /* 0 = do NOT reboot, 1 = reboot */
} ota_app_context_t;

/******************************************************
 *               Variable Definitions
 ******************************************************/

extern ota_app_context_t ota_app;

/******************************************************
 *               Function Declarations
 ******************************************************/

cy_rslt_t init_ota(ota_app_context_t *ota);

void ota_initialize_default_values(void);

#endif /* #define OTA_CONTEXT_H_ */
