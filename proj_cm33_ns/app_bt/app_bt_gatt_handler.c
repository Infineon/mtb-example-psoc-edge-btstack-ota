/*****************************************************************************
* File Name        : app_bt_gatt_handler.c
*
* Description      : This file contains the (BLE) GATT Handler
*
* Related Document : See README.md
*
*******************************************************************************
* (c) 2024-2026, Infineon Technologies AG, or an affiliate of Infineon
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

/* *****************************************************************************
 *                              INCLUDES
 * ****************************************************************************/

#include "cy_ota_api.h"
#include "ota_context.h"
#include "cy_ota_internal.h"
#include "app_bt_gatt_handler.h"
#include "app_bt_utils.h"
#include "GeneratedSource/cycfg_gatt_db.h"
#include "GeneratedSource/cycfg_bt_settings.h"
#include "GeneratedSource/cycfg_gap.h"
#include "wiced_bt_l2c.h"
#include "cyabs_rtos.h"
#include "stdlib.h"
#include "app_bt_fw_dwnld_check.h"
#include "wiced_bt_ble.h"
#include "cy_pdl.h"
#include "cy_gpio.h"
#include "wiced_bt_ble.h"
#include "wiced_bt_l2c.h"
/* *****************************************************************************
 *                              DEFINES
 * ****************************************************************************/
/* UUID for non-secure Bluetooth® upgrade service */
static const uint8_t NON_SECURE_UUID_SERVICE_OTA_FW_UPGRADE_SERVICE[] = { 0x1F, 0x38, 0xA1, 0x38, 0xAD, 0x82, 0x35, 0x86, 0xA0, 0x43, 0x13, 0x5C, 0x47, 0x1E, 0x5D, 0xAE };
/* UUID created by Bluetooth® Configurator, supplied in "GeneratedSource/cycfg_gatt_db.h" */
static const uint8_t BLE_CONFIG_UUID_SERVICE_OTA_FW_UPGRADE_SERVICE[] = {__UUID_SERVICE_OTA_FW_UPGRADE_SERVICE };
typedef void (*pfn_free_buffer_t)(uint8_t *p_data);

/* *****************************************************************************
 *                              Data
 * ****************************************************************************/
/* The bond info structure */
static bondinfo_t  bondinfo;

/* The write req info structure to handle GATT_REQ_PREPARE_WRITE and GATT_REQ_EXECUTE_WRITE */
typedef struct gatt_write_req_buf
{
    uint8_t    value[CY_BT_RX_PDU_SIZE];
    uint16_t   written;
    uint16_t   handle;
    bool       in_use;
}gatt_write_req_buf_t;

gatt_write_req_buf_t write_buff;

wiced_bt_ble_pref_conn_params_t bt_pref_conn_params;

/* *****************************************************************************
 *                              FUNCTION DEFINITIONS
 * ****************************************************************************/
/*
 * Function Name:
 * app_bt_alloc_buffer
 *
 * Function Description:
 * @brief  Buffer allocation
 *
 * @param len     length of buffer
 *
 */
static uint8_t *app_bt_alloc_buffer(uint16_t len)
{
    uint8_t *p = (uint8_t *)malloc(len);
    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s() len %d alloc %p\n", __FUNCTION__, len, p);
    return p;
}

/*
 * Function Name:
 * app_bt_free_buffer
 *
 * Function Description:
 * @brief  Free buffer
 *
 * @param p_data     pointer to buffer
 *
 */
static void app_bt_free_buffer(uint8_t *p_data)
{
    if (p_data != NULL)
    {
        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s()        free:%p\n", __FUNCTION__, p_data);
        free(p_data);
    }
}

/*
 * Function Name:
 * app_bt_ble_send_notification
 *
 * Function Description:
 * @brief  Send Bluetooth® GATT notification
 *
 * @param bt_conn_id     Bluetooth® GATT connection ID
 * @param attr_handle     Attribute Handle
 * @param val_len        Length of value
 * @param p_val          pointer to value
 *
 * @return wiced_bt_gatt_status_t  Bluetooth® GATT status
 */
static wiced_bt_gatt_status_t app_bt_ble_send_notification(uint16_t bt_conn_id, uint16_t attr_handle, uint16_t val_len, uint8_t *p_val)
{
    wiced_bt_gatt_status_t status = (wiced_bt_gatt_status_t)WICED_BT_GATT_ERROR;

    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s() Sending Notification conn_id: 0x%x (%d) handle: 0x%x (%d) val_len: %d value:%d\n", __func__, bt_conn_id, bt_conn_id, attr_handle, attr_handle, val_len, *p_val);
    status = wiced_bt_gatt_server_send_notification(bt_conn_id, attr_handle, val_len, p_val, NULL);    /* bt_notify_buff is not allocated, no need to keep track of it w/context */
    if (status != WICED_BT_SUCCESS)
    {
        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "%s() Notification FAILED conn_id:0x%x (%d) handle: %d val_len: %d value:%d\n", __func__, bt_conn_id, bt_conn_id, attr_handle, val_len, *p_val);
    }
    return status;
}

/*
 * Function Name:
 * app_bt_ble_send_indication
 *
 * Function Description:
 * @brief  Send Bluetooth® GATT indication
 *
 * @param bt_conn_id     Bluetooth® GATT connection ID
 * @param attr_handle     Attribute Handle
 * @param val_len        Length of value
 * @param p_val          pointer to value
 *
 * @return wiced_bt_gatt_status_t  Bluetooth® GATT status
 */
static wiced_bt_gatt_status_t app_bt_ble_send_indication(uint16_t bt_conn_id, uint16_t attr_handle, uint16_t val_len, uint8_t *p_val)
{
    wiced_bt_gatt_status_t status = (wiced_bt_gatt_status_t)WICED_BT_GATT_ERROR;

    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s() Sending Indication conn_id: 0x%x (%d) handle: %d val_len: %d value:%d\n", __func__, bt_conn_id, bt_conn_id, attr_handle, val_len, *p_val);
    status = wiced_bt_gatt_server_send_indication(bt_conn_id, attr_handle, val_len, p_val, NULL);    /* bt_notify_buff is not allocated, no need to keep track of it w/context */
    if (status != WICED_BT_SUCCESS)
    {
        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "%s() Indication FAILED conn_id:0x%x (%d) handle: %d val_len: %d value:%d\n", __func__, bt_conn_id, bt_conn_id, attr_handle, val_len, *p_val);
    }
    return status;
}

/*
 * Function Name:
 * app_bt_connect_callback
 *
 * Function Description:
 * @brief  The callback function is invoked when GATT_CONNECTION_STATUS_EVT occurs
 *         in GATT Event handler function
 *
 * @param p_conn_status     Pointer to Bluetooth® GATT connection status
 *
 * @return wiced_bt_gatt_status_t  Bluetooth® GATT status
 */
static wiced_bt_gatt_status_t app_bt_connect_callback(wiced_bt_gatt_connection_status_t *p_conn_status)
{

    wiced_bt_gatt_status_t gatt_status = WICED_BT_GATT_ERROR;

    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "  %s() CONN status: %d\n", __func__, p_conn_status->connected);

    if (p_conn_status->connected)    /* If callback indicates Connected */
    {
        /* Device has connected */
        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "    CONNECTED: " BT_ADDR_FORMAT "\n",
                p_conn_status->bd_addr[0], p_conn_status->bd_addr[1], p_conn_status->bd_addr[2],
                p_conn_status->bd_addr[3], p_conn_status->bd_addr[4], p_conn_status->bd_addr[5]);

        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "         Connection ID: 0x%x (%d)\n", p_conn_status->conn_id, p_conn_status->conn_id);

        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "            addr type : (%d) %s\n", p_conn_status->addr_type,
                                        (p_conn_status->addr_type == BLE_ADDR_PUBLIC) ? "PUBLIC" :
                                        (p_conn_status->addr_type == BLE_ADDR_RANDOM) ? "RANDOM" :
                                        (p_conn_status->addr_type == BLE_ADDR_PUBLIC_ID) ? "PUBLIC_ID" :
                                        (p_conn_status->addr_type == BLE_ADDR_RANDOM_ID) ? "RANDOM_ID" : "UNKNOWN" );

        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "              ROLE    : (%d) %s\n", p_conn_status->link_role,
                                      (p_conn_status->link_role == 0) ? "Master" : "Slave");

        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "            transport : %s\n",
                                        (p_conn_status->transport == 1) ? "BR_EDR" :
                                        (p_conn_status->transport == 2) ? "Bluetooth(r)" : "UNKNOWN");

        ota_app.bt_conn_id = p_conn_status->conn_id;                        /* Save Bluetooth® connection ID in application data structure */

        memcpy(ota_app.bt_peer_addr, p_conn_status->bd_addr, BD_ADDR_LEN);  /* Save Bluetooth® peer ADDRESS in application data structure */

        gatt_status = wiced_bt_start_advertisements(BTM_BLE_ADVERT_OFF,BLE_ADDR_PUBLIC,NULL);

    }
    else
    {
        /* Device has disconnected */
        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "    Disconnected from BDA:" BT_ADDR_FORMAT "\n",
                   p_conn_status->bd_addr[0], p_conn_status->bd_addr[1], p_conn_status->bd_addr[2],
                   p_conn_status->bd_addr[3], p_conn_status->bd_addr[4], p_conn_status->bd_addr[5]);

        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Connection ID: 0x%x (%d)\n", p_conn_status->conn_id, p_conn_status->conn_id);
        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Reason for disconnection: %s \n", app_get_gatt_disconn_reason_name(p_conn_status->reason));

        /* Handle the disconnection */
         ota_app.bt_conn_id  = 0;

        gatt_status = wiced_bt_start_advertisements(BTM_BLE_ADVERT_UNDIRECTED_HIGH,BLE_ADDR_PUBLIC,NULL);
    }

    return gatt_status;
}


/*
 * Find attribute description by handle
 */
/*
* Function Name:
* app_bt_find_by_handle
*
* Function Description:
* @brief  Find attribute description by handle
*
* @param handle                 attribute handle
*/
static gatt_db_lookup_table_t *app_bt_find_by_handle(uint16_t handle)
{
    int i;
    for (i = 0; i < app_gatt_db_ext_attr_tbl_size; i++)
    {
        if (app_gatt_db_ext_attr_tbl[i].handle == handle)
        {
            return ( &app_gatt_db_ext_attr_tbl[i] );
        }
    }
    return NULL;
}
/*
* Function Name:
* app_gatt_req_read_handler
*
* Function Description:
* @brief  Process read request from peer device
*
* @param conn_id                Connection ID
* @param opcode                 opcode
* @param p_read_req             Pointer to Bluetooth® GATT read request
* @param len_requested          length of GATT read request
*
* @return wiced_bt_gatt_status_t  Bluetooth® GATT status
*/
static wiced_bt_gatt_status_t app_gatt_req_read_handler (uint16_t conn_id, wiced_bt_gatt_opcode_t opcode, wiced_bt_gatt_read_t *p_read_req, uint16_t len_requested)
{
    gatt_db_lookup_table_t *puAttribute;
    uint16_t    attr_len_to_copy, to_send;
    uint8_t     *from;


    if ( ( puAttribute = app_bt_find_by_handle(p_read_req->handle) ) == NULL)
    {
        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_WARNING, "%s()  attr not found handle: 0x%04x\n", __func__, p_read_req->handle);

        wiced_bt_gatt_server_send_error_rsp (conn_id, opcode, p_read_req->handle, WICED_BT_GATT_INVALID_HANDLE);

        return WICED_BT_GATT_INVALID_HANDLE;
    }

    attr_len_to_copy = puAttribute->cur_len;

    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "%s() conn_id: %d handle:0x%04x offset:%d len:%d\n", __func__,
               conn_id, p_read_req->handle, p_read_req->offset, attr_len_to_copy);

    if (p_read_req->offset >= puAttribute->cur_len)
    {
        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_WARNING, "%s() offset:%d larger than attribute length:%d\n", __func__,
                   p_read_req->offset, puAttribute->cur_len);

        wiced_bt_gatt_server_send_error_rsp (conn_id, opcode, p_read_req->handle, WICED_BT_GATT_INVALID_OFFSET);

        return WICED_BT_GATT_INVALID_HANDLE;
    }

    to_send = MIN(len_requested, attr_len_to_copy - p_read_req->offset);

    from = puAttribute->p_data + p_read_req->offset;

    return wiced_bt_gatt_server_send_read_handle_rsp (conn_id, opcode, to_send, from, NULL);    /* No need for context, as buff not allocated */
}

/*
 * Function Name:
 * app_gatt_req_read_by_type_handler
 *
 * Function Description:
 * @brief  Process write read-by-type request from peer device
 *
 * @param conn_id                Connection ID
 * @param opcode                 opcode
 * @param p_read_req             Pointer to Bluetooth® GATT read request
 * @param len_requested          length of GATT read request
 *
 * @return wiced_bt_gatt_status_t  Bluetooth® GATT status
 */
static wiced_bt_gatt_status_t app_gatt_req_read_by_type_handler(uint16_t conn_id, wiced_bt_gatt_opcode_t opcode,
                                                         wiced_bt_gatt_read_by_type_t *p_read_req, uint16_t len_requested)
{
    gatt_db_lookup_table_t  *puAttribute;
    uint16_t                attr_handle = p_read_req->s_handle;
    uint8_t                 *p_rsp = app_bt_alloc_buffer(len_requested);
    uint8_t                 pair_len = 0;
    int                     used = 0;

    if (p_rsp == NULL)
    {
        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_WARNING, "%s() No memory len_requested: %d!!\n", __func__, len_requested);

        wiced_bt_gatt_server_send_error_rsp (conn_id, opcode, attr_handle, WICED_BT_GATT_INSUF_RESOURCE);
        return WICED_BT_GATT_INVALID_HANDLE;
    }

    /* Read by type returns all attributes of the specified type, between the start and end handles */
    while (WICED_TRUE)
    {
        attr_handle = wiced_bt_gatt_find_handle_by_type (attr_handle, p_read_req->e_handle, &p_read_req->uuid);

        if (attr_handle == 0U)
        {
            break;
        }

        if ((puAttribute = app_bt_find_by_handle(attr_handle)) == NULL)
        {
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_WARNING, "%s()  found type but no attribute ??\n", __func__);
            wiced_bt_gatt_server_send_error_rsp (conn_id, opcode, p_read_req->s_handle, WICED_BT_GATT_ERR_UNLIKELY);
            app_bt_free_buffer(p_rsp);
            return WICED_BT_GATT_INVALID_HANDLE;
        }

        {
            int filled = wiced_bt_gatt_put_read_by_type_rsp_in_stream(p_rsp + used, len_requested - used, &pair_len,
                attr_handle, puAttribute->cur_len, puAttribute->p_data);
            if (filled == 0) {
                break;
            }
            used += filled;
        }

        /* Increment starting handle for next search to one past current */
        attr_handle++;
    }

    if (used == 0)
    {
        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_WARNING, "%s()  attr not found  start_handle: 0x%04x  end_handle: 0x%04x  Type: 0x%04x\n",
                   __func__, p_read_req->s_handle, p_read_req->e_handle, p_read_req->uuid.uu.uuid16);

        wiced_bt_gatt_server_send_error_rsp (conn_id, opcode, p_read_req->s_handle, WICED_BT_GATT_INVALID_HANDLE);
        app_bt_free_buffer (p_rsp);
        return WICED_BT_GATT_INVALID_HANDLE;
    }

    /* Send the response */
    wiced_bt_gatt_server_send_read_by_type_rsp (conn_id, opcode, pair_len, used, p_rsp, (void*)app_bt_free_buffer);

    return WICED_BT_GATT_SUCCESS;
}


/*
 * Function Name:
 * app_gatt_req_read_multi_handler
 *
 * Function Description:
 * @brief  Process write read multi request from peer device
 *
 * @param conn_id        Connection ID
 * @param opcode         opcode
 * @param p_read_req     Pointer to Bluetooth® GATT read request
 * @param len_requested  length of GATT read request
 *
 * @return wiced_bt_gatt_status_t  Bluetooth® GATT status
 */
static wiced_bt_gatt_status_t app_gatt_req_read_multi_handler(uint16_t conn_id, wiced_bt_gatt_opcode_t opcode,
                                                       wiced_bt_gatt_read_multiple_req_t *p_read_req, uint16_t len_requested)
{
    gatt_db_lookup_table_t  *puAttribute;
    uint8_t                 *p_rsp = app_bt_alloc_buffer(len_requested);
    int                     used = 0;
    int                     xx;
    uint16_t                handle = wiced_bt_gatt_get_handle_from_stream(p_read_req->p_handle_stream, 0);

    if (p_rsp == NULL)
    {
        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_WARNING, "%s() No memory len_requested: %d!!\n", __func__, len_requested);
        wiced_bt_gatt_server_send_error_rsp(conn_id, opcode, handle, WICED_BT_GATT_INSUF_RESOURCE);
        return WICED_BT_GATT_INVALID_HANDLE;
    }

    /* Read by type returns all attributes of the specified type, between the start and end handles */
    for (xx = 0; xx < p_read_req->num_handles; xx++)
    {
        handle = wiced_bt_gatt_get_handle_from_stream(p_read_req->p_handle_stream, xx);
        if ((puAttribute = app_bt_find_by_handle(handle)) == NULL)
        {
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_WARNING, "%s()  no handle 0x%04x\n", __func__, handle);
            wiced_bt_gatt_server_send_error_rsp(conn_id, opcode, *p_read_req->p_handle_stream, WICED_BT_GATT_ERR_UNLIKELY);
            app_bt_free_buffer(p_rsp);
            return WICED_BT_GATT_INVALID_HANDLE;
        }

        {
            int filled = wiced_bt_gatt_put_read_multi_rsp_in_stream(opcode, p_rsp + used, len_requested - used,
                puAttribute->handle, puAttribute->cur_len, puAttribute->p_data);
            if (!filled) {
                break;
            }
            used += filled;
        }
    }

    if (used == 0)
    {
        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_WARNING, "%s() no attr found\n", __func__);

        wiced_bt_gatt_server_send_error_rsp(conn_id, opcode, *p_read_req->p_handle_stream, WICED_BT_GATT_INVALID_HANDLE);
        /* CID 470528 (#1 of 1): Resource leak (RESOURCE_LEAK) */
        app_bt_free_buffer (p_rsp);
        return WICED_BT_GATT_INVALID_HANDLE;
    }

    /* Send the response */
    wiced_bt_gatt_server_send_read_multiple_rsp(conn_id, opcode, used, p_rsp, (void*)app_bt_free_buffer);
    return WICED_BT_GATT_SUCCESS;
}

/*
 * Function Name:
 * app_bt_set_value
 *
 * Function Description:
 * @brief  The function is invoked by app_bt_write_handler to set a value
 *         to GATT DB.
 *
 * @param attr_handle  GATT attribute handle
 * @param p_val        Pointer to Bluetooth® GATT write request value
 * @param len          length of GATT write request
 *
 * @return wiced_bt_gatt_status_t  Bluetooth® GATT status
 */
static wiced_bt_gatt_status_t app_bt_set_value(uint16_t attr_handle,
                                               uint8_t *p_val,
                                               uint16_t len)
{
    wiced_bt_gatt_status_t result = WICED_BT_GATT_INVALID_HANDLE;

    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "%s() handle : 0x%x (%d)\n", __func__, attr_handle, attr_handle);

    for (int i = 0; i < app_gatt_db_ext_attr_tbl_size; i++)
    {
        /* Check for a matching handle entry */
        if (app_gatt_db_ext_attr_tbl[i].handle == attr_handle)
        {
            /* Detected a matching handle in the external lookup table */
            if (app_gatt_db_ext_attr_tbl[i].max_len >= len)
            {
                /* Value fits within the supplied buffer; copy over the value */
                app_gatt_db_ext_attr_tbl[i].cur_len = len;
                memset(app_gatt_db_ext_attr_tbl[i].p_data, 0x00,  app_gatt_db_ext_attr_tbl[i].max_len);
                memcpy(app_gatt_db_ext_attr_tbl[i].p_data, p_val, app_gatt_db_ext_attr_tbl[i].cur_len);

                if (memcmp(app_gatt_db_ext_attr_tbl[i].p_data, p_val, app_gatt_db_ext_attr_tbl[i].cur_len) == 0)
                {
                    result = WICED_BT_GATT_SUCCESS;
                }
            }
            else
            {
                /* Value to write will not fit within the table */
                result = WICED_BT_GATT_INVALID_ATTR_LEN;
                cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Invalid attribute length\n");
            }
            break;
        }
    }
    if (result != WICED_BT_GATT_SUCCESS)
    {
        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "%s() FAILED 0x%lx\n", __func__, result);
    }

    return result;
}

/*
 * Function Name:
 * app_bt_write_handler
 *
 * Function Description:
 * @brief  Process write request from peer device
 *
 * @param p_req        Pointer to Bluetooth® GATT write request
 *
 * @return wiced_bt_gatt_status_t  Bluetooth® GATT status
 */

static wiced_bt_gatt_status_t app_bt_write_handler(wiced_bt_gatt_event_data_t *p_req)
{
    wiced_bt_gatt_write_req_t       *p_write_req;
    cy_rslt_t                       result;
    wiced_bt_gatt_status_t          status = WICED_BT_GATT_SUCCESS;

    CY_ASSERT(p_req != NULL);

    p_write_req = &p_req->attribute_request.data.write_req;

    if (p_req != NULL)
    {
        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "%s() handle : 0x%x (%d)\n", __func__, p_write_req->handle, p_write_req->handle);
        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "     offset : 0x%x\n", p_write_req->offset);
        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "     p_val  : %p\n",   p_write_req->p_val);
        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "     val_len: 0x%x\n", p_write_req->val_len);
    }

    switch(p_write_req->handle)
    {
        /*
         * If write request is for the OTA FW upgrade service, pass it to the
         * library to process
         */
        case HDLD_OTA_FW_UPGRADE_SERVICE_OTA_UPGRADE_CONTROL_POINT_CLIENT_CHAR_CONFIG:
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "%s() HDLD_OTA_FW_UPGRADE_SERVICE_OTA_UPGRADE_CONTROL_POINT_CLIENT_CHAR_CONFIG\n", __func__);

            /* Save Configuration descriptor in Application data structure (Notify & Indicate flags) */
            ota_app.bt_config_descriptor = p_write_req->p_val[0];

            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "    ota_app.bt_config_descriptor: %d %s\n",
                       ota_app.bt_config_descriptor, (ota_app.bt_config_descriptor == GATT_CLIENT_CONFIG_NOTIFICATION) ? "Notify" :
                       (ota_app.bt_config_descriptor == GATT_CLIENT_CONFIG_INDICATION) ? "Indicate" : "Unknown" );

            return WICED_BT_GATT_SUCCESS;

        case HDLC_OTA_FW_UPGRADE_SERVICE_OTA_UPGRADE_CONTROL_POINT_VALUE:
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "%s() HDLC_OTA_FW_UPGRADE_SERVICE_OTA_UPGRADE_CONTROL_POINT_VALUE \n", __func__);

            switch(p_write_req->p_val[0])
            {
                case CY_OTA_UPGRADE_COMMAND_PREPARE_DOWNLOAD:
                {
                    /* We are using Bluetooth® for this connection */
                    /* Mark Connection type in Application data structure - used when calling cy_ota_agent_start() */
                    ota_app.connection_type = CY_OTA_CONNECTION_BLE;

                    /* Call application-level OTA initialization (calls cy_ota_agent_start() ) */
                    result = init_ota(&ota_app);

                    if (result != CY_RSLT_SUCCESS)
                    {
                        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "init_ota() Failed - result: 0x%lx\n", result);
                        return WICED_BT_GATT_ERROR;
                    }

                    result = cy_ota_ble_download_prepare(ota_app.ota_context);

                    if(result == CY_RSLT_SUCCESS)
                    {
                        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_ota_ble_download_prepare completed, Sending notification");

                        uint8_t bt_notify_buff = CY_OTA_UPGRADE_STATUS_OK;

                        status = app_bt_ble_send_notification(ota_app.bt_conn_id,
                                HDLC_OTA_FW_UPGRADE_SERVICE_OTA_UPGRADE_CONTROL_POINT_VALUE, 1, &bt_notify_buff);

                        if(status != WICED_BT_GATT_SUCCESS)
                        {
                            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "\nApplication BT Send notification callback failed: 0x%lx\n", result);
                            return WICED_BT_GATT_ERROR;
                        }
                    }
                    else
                    {
                        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_ota_ble_prepare_download() Failed - result: 0x%lx\n", result);
                        return WICED_BT_GATT_ERROR;
                    }
                    return WICED_BT_GATT_SUCCESS;
                }

                case CY_OTA_UPGRADE_COMMAND_DOWNLOAD:
                {
                    uint32_t total_size = 0;

                    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "%s() HDLC_OTA_FW_UPGRADE_SERVICE_OTA_UPGRADE_CONTROL_POINT_VALUE : CY_OTA_UPGRADE_COMMAND_DOWNLOAD\n", __func__);

                    if (p_write_req->val_len < 4)
                    {
                        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "CY_OTA_UPGRADE_COMMAND_DOWNLOAD len < 4\n");
                        return WICED_BT_GATT_ERROR;
                    }

                    total_size = (((uint32_t)p_write_req->p_val[4]) << 24) +
                                 (((uint32_t)p_write_req->p_val[3]) << 16) +
                                 (((uint32_t)p_write_req->p_val[2]) <<  8) +
                                 (((uint32_t)p_write_req->p_val[1]) <<  0);

                    result = cy_ota_ble_download(ota_app.ota_context, total_size);

                    if(result == CY_RSLT_SUCCESS)
                    {
                        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_ota_ble_download completed, Sending notification");

                        uint8_t bt_notify_buff = CY_OTA_UPGRADE_STATUS_OK;

                        status = app_bt_ble_send_notification(ota_app.bt_conn_id,
                                HDLC_OTA_FW_UPGRADE_SERVICE_OTA_UPGRADE_CONTROL_POINT_VALUE, 1, &bt_notify_buff);

                        if(status != WICED_BT_GATT_SUCCESS)
                        {
                            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "\nApplication BT Send notification callback failed: 0x%lx\n", result);
                            return WICED_BT_GATT_ERROR;
                        }
                    }
                    else
                    {
                        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_ota_ble_download() Failed - result: 0x%lx\n", result);
                        return WICED_BT_GATT_ERROR;
                    }

                    return WICED_BT_GATT_SUCCESS;
                }

                case CY_OTA_UPGRADE_COMMAND_VERIFY:
                {
                    bool crc_or_sig_verify = true;
                    uint32_t final_crc32 = 0;

                    if (p_write_req->val_len != 5)
                    {
                        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "CY_OTA_UPGRADE_COMMAND_VERIFY len != 5\n");
                        return WICED_BT_GATT_ERROR;
                    }

                    final_crc32 = ( ( (uint32_t)p_write_req->p_val[1]) <<  0) +
                                                ( ( (uint32_t)p_write_req->p_val[2]) <<  8) +
                                                ( ( (uint32_t)p_write_req->p_val[3]) << 16) +
                                                ( ( (uint32_t)p_write_req->p_val[4]) << 24);
                    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_NOTICE, "Final CRC from Host : 0x%lx\n", final_crc32);

                    result = cy_ota_ble_download_verify(ota_app.ota_context, final_crc32, crc_or_sig_verify);

                    if(result == CY_RSLT_SUCCESS)
                    {
                        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_ota_ble_download completed, Sending notification");

                        uint8_t bt_notify_buff = CY_OTA_UPGRADE_STATUS_OK;

                        status = app_bt_ble_send_indication(ota_app.bt_conn_id,
                                HDLC_OTA_FW_UPGRADE_SERVICE_OTA_UPGRADE_CONTROL_POINT_VALUE, 1, &bt_notify_buff);

                        if(status != WICED_BT_GATT_SUCCESS)
                        {
                            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "\nApplication BT Send Indication callback failed: 0x%lx\n", result);
                            return WICED_BT_GATT_ERROR;
                        }
                    }
                    else
                    {
                        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_ota_ble_download_verify() Failed - result: 0x%lx\n", result);

                        uint8_t bt_notify_buff = CY_OTA_UPGRADE_STATUS_BAD;

                        status = app_bt_ble_send_indication(ota_app.bt_conn_id,
                                HDLC_OTA_FW_UPGRADE_SERVICE_OTA_UPGRADE_CONTROL_POINT_VALUE, 1, &bt_notify_buff);

                        if(status != WICED_BT_GATT_SUCCESS)
                        {
                            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "\nApplication BT Send Indication callback failed: 0x%lx\n", result);
                        }

                        return WICED_BT_GATT_ERROR;
                    }

                    return WICED_BT_GATT_SUCCESS;
                }

                case CY_OTA_UPGRADE_COMMAND_ABORT:

                    result = cy_ota_ble_download_abort(&ota_app.ota_context);

                    return WICED_BT_GATT_SUCCESS;
            }
            break;

        case HDLC_OTA_FW_UPGRADE_SERVICE_OTA_UPGRADE_DATA_VALUE:
        {
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "%s() HDLC_OTA_FW_UPGRADE_SERVICE_OTA_UPGRADE_DATA_VALUE : CY_OTA_UPGRADE_COMMAND_DOWNLOAD\n", __func__);

            result = cy_ota_ble_download_write(ota_app.ota_context, p_write_req->p_val, p_write_req->val_len, p_write_req->offset);
            if(result == CY_RSLT_SUCCESS)
            {
                cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_NOTICE, "   Downloaded 0x%lx of 0x%lx (%d%%)\n",
                        ((cy_ota_context_t *)(ota_app.ota_context))->ota_storage_context.total_bytes_written,
                        ((cy_ota_context_t *)(ota_app.ota_context))->ota_storage_context.total_image_size,
                        ((cy_ota_context_t *)(ota_app.ota_context))->ble.percent);
                return WICED_BT_GATT_SUCCESS;
            }
            else
            {
                return WICED_BT_GATT_ERROR;
            }
        }

        default:
            /* Handle normal (non-OTA) indication confirmation requests here */
            /* Attempt to perform the Write Request */
            return app_bt_set_value(p_write_req->handle,
                                        p_write_req->p_val,
                                        p_write_req->val_len);
    }

    return WICED_BT_GATT_REQ_NOT_SUPPORTED;
}
/*
 * Function Name:
 * app_bt_prepare_write_handler
 *
 * Function Description:
 * @brief  Prepare for the write request from peer device
 *
 * @param conn_id        Connection ID
 * @param opcode         opcode
 * @param p_req          Pointer to Bluetooth® GATT request
 *
 * @return wiced_bt_gatt_status_t  Bluetooth® GATT status
 */
static wiced_bt_gatt_status_t app_bt_prepare_write_handler(uint16_t conn_id,
                                                           wiced_bt_gatt_opcode_t opcode,
                                                           wiced_bt_gatt_write_req_t *p_req)
{
    if(write_buff.in_use == false)
    {
        memset(&(write_buff.value[0]), 0x00, CY_BT_RX_PDU_SIZE);
        write_buff.written = 0;
        write_buff.in_use = true;
        write_buff.handle = 0;
    }

    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "%s() handle : 0x%x (%d)\n", __func__, p_req->handle, p_req->handle);
    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "     offset : 0x%x\n", p_req->offset);
    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "     p_val  : %p\n",   p_req->p_val);
    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "     val_len: 0x%x\n", p_req->val_len);
    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "val_len = %d \n", p_req->val_len);

    /** store the data  */
    if(write_buff.written == p_req->offset)
    {
        int remaining = CY_BT_RX_PDU_SIZE - write_buff.written;
        int to_write = p_req->val_len;

        if (remaining >= to_write)
        {
            memcpy( (void*)((uint32_t)(&(write_buff.value[0]) + write_buff.written)), p_req->p_val, to_write);

            /* send success response */
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "== Sending prepare write success response...\n");

            wiced_bt_gatt_server_send_prepare_write_rsp(conn_id, opcode, p_req->handle,
                                                        p_req->offset, to_write,
                                                        &(write_buff.value[write_buff.written]), NULL);

            write_buff.written += to_write;

            write_buff.handle = p_req->handle;

            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "    Total val_len: %d\n", write_buff.written);

            return WICED_BT_GATT_SUCCESS;
        }
        else
        {
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "remaining >= to_write error...\n");

            return WICED_BT_GATT_ERROR;
        }
    }
    else
    {
        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "write_buff.written != p_req->offset...\n");
    }

    return WICED_BT_GATT_ERROR;
}
/*
 * Function Name:
 * app_bt_prepare_write_handler
 *
 * Function Description:
 * @brief  Process the write request from peer device
 *
 * @param p_req     Pointer to Bluetooth® GATT request
 *
 * @return wiced_bt_gatt_status_t  Bluetooth® GATT status
 */
static wiced_bt_gatt_status_t app_bt_execute_write_handler(wiced_bt_gatt_event_data_t *p_req)
{
    wiced_bt_gatt_write_req_t       *p_write_req;
    wiced_bt_gatt_status_t          status = WICED_BT_GATT_SUCCESS;

    CY_ASSERT(p_req != NULL);

    p_write_req = &p_req->attribute_request.data.write_req;

    CY_ASSERT(p_req != NULL);

    if(write_buff.in_use == false)
    {
        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "write_buff.inuse is false returning error...\n");
        return WICED_BT_GATT_ERROR;
    }

    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Execute Write with %d bytes\n", write_buff.written);

    p_write_req->handle = write_buff.handle;
    p_write_req->offset = 0;
    p_write_req->p_val = &(write_buff.value[0]);
    p_write_req->val_len = write_buff.written;

    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "%s() handle : 0x%x (%d)\n", __func__, p_write_req->handle, p_write_req->handle);
    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "     offset : 0x%x\n", p_write_req->offset);
    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "     p_val  : %p\n",   p_write_req->p_val);
    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "     val_len: 0x%x\n", p_write_req->val_len);

    status = app_bt_write_handler(p_req);
    if (status != WICED_BT_GATT_SUCCESS)
    {
        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "app_bt_write_handler() failed....\n");
    }

    write_buff.in_use = false;

    return status;
}

/*
 * Function Name:
 * app_bt_server_callback
 *
 * Function Description:
 * @brief  The callback function is invoked when GATT_ATTRIBUTE_REQUEST_EVT occurs
 *         in GATT Event handler function. GATT Server Event Callback function.
 *
 * @param p_data   Pointer to Bluetooth® GATT request data
 *
 * @return wiced_bt_gatt_status_t  Bluetooth® GATT status
 */
static wiced_bt_gatt_status_t app_bt_server_callback(wiced_bt_gatt_event_data_t *p_data)
{
    wiced_bt_gatt_status_t status = WICED_BT_GATT_ERROR;
    wiced_bt_gatt_attribute_request_t   *p_att_req = &p_data->attribute_request;

    switch (p_att_req->opcode)
    {
        case GATT_REQ_READ:           /* Attribute read notification (attribute value internally read from GATT database) */
        case GATT_REQ_READ_BLOB:
             // structure used wiced_bt_gatt_attribute_request_t.data.read_req
             // Application calls wiced_bt_gatt_server_send_read_by_rsp(), with a pointer at a offset
             // to the value of the requested attribute handle.
             // Ideally application does not need to allocate memory for this rsp since as a server
             // typically the handle has a value which can be read as a binary array.
             // The context in this case may be set to NULL, to avoid freeing the memory in the
             // GATT_APP_BUFFER_TRANSMITTED_EVT
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "  %s() GATTS_REQ_TYPE_READ\n", __func__);

            status = app_gatt_req_read_handler( p_att_req->conn_id, p_att_req->opcode, &p_att_req->data.read_req, p_att_req->len_requested );
            break;

        case GATT_REQ_READ_BY_TYPE:
            // structure used wiced_bt_gatt_attribute_request_t.data.read_by_type                           // TODO requires new code?
            // Application allocates a
            //   a) buffer of the size wiced_bt_gatt_attribute_request_t.len_requested
            //   b) Set the s_handle to wiced_bt_gatt_read_by_type_t.s_handle
            //   c) Use wiced_bt_gatt_find_by_type API to find the attr_handle for the
            //      wiced_bt_gatt_read_by_type_t.uuid which lies between s_handle and
            //      wiced_bt_gatt_read_by_type_t.e_handle
            //    d) Call wiced_bt_gatt_put_read_by_type_rsp_in_stream to form the response
            //    e) Set s_handle = attr_handle + 1
            //    f) Repeat  (c) - (e) till the wiced_bt_gatt_put_read_by_type_rsp_in_stream returns 0.
            //    g) Send the response with wiced_bt_gatt_server_send_read_by_type_rsp with valid length in
            //       buffer else send an error response.
            //    h) To free the allocated buffer sent in the success case, you can set the context to the
            //        applicable free function, which are returned in the GATT_APP_BUFFER_TRANSMITTED_EVT
            status = app_gatt_req_read_by_type_handler(p_att_req->conn_id, p_att_req->opcode, &p_att_req->data.read_by_type, p_att_req->len_requested);
            break;

        case GATT_REQ_READ_MULTI:
        case GATT_REQ_READ_MULTI_VAR_LENGTH:
             // structure used wiced_bt_gatt_attribute_request_t.data.read_multiple_req                           // TODO requires new code?
             // Application allocates a
             //   a) buffer of the size wiced_bt_gatt_attribute_request_t.len_requested
             //   b) Application loops through the handles received with wiced_bt_gatt_get_handle_from_stream
             //       for upto wiced_bt_gatt_read_multiple_req_t.num_handles
             //   c) for each handle, application gets the value of the handle and uses
             //      wiced_bt_gatt_put_read_multi_rsp_in_stream  to write response into allocated buffer
             //   d) Once done, use wiced_bt_gatt_server_send_read_multi_rsp
             //   e) To free the allocated buffer sent in the success case, you can set the context to the
             //        applicable free function, which are returned in the GATT_APP_BUFFER_TRANSMITTED_EVT
            status = app_gatt_req_read_multi_handler(p_att_req->conn_id, p_att_req->opcode, &p_att_req->data.read_multiple_req, p_att_req->len_requested);
            break;

        case GATT_REQ_WRITE:
        case GATT_CMD_WRITE:
        case GATT_CMD_SIGNED_WRITE:
            // Application writes the data in wiced_bt_gatt_write_t.p_val of length                           // TODO requires new code?
            // wiced_bt_gatt_write_t.val_len into the attribute handle and
            // calls the wiced_bt_gatt_server_send_write_rsp in case of success else sends an error rsp
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "  %s() GATTS_REQ_WRITE\n", __func__);
            status = app_bt_write_handler(p_data);
            if((p_att_req->opcode == GATT_REQ_WRITE) &&  (status == WICED_BT_GATT_SUCCESS))
            {
                wiced_bt_gatt_write_req_t   *p_write_request = &p_att_req->data.write_req;
                wiced_bt_gatt_server_send_write_rsp(p_att_req->conn_id, p_att_req->opcode, p_write_request->handle);
            }
            break;

        case GATT_REQ_PREPARE_WRITE:
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "  %s() GATT_REQ_PREPARE_WRITE\n", __func__);

            status = app_bt_prepare_write_handler(p_att_req->conn_id,
                                                  p_att_req->opcode,
                                                  &p_att_req->data.write_req);

            if((p_att_req->opcode == GATT_REQ_PREPARE_WRITE) &&  (status != WICED_BT_GATT_SUCCESS))
            {
                cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "\n\n== Sending Prepare write error response...\n");

                wiced_bt_gatt_server_send_error_rsp(p_att_req->conn_id,
                                                    p_att_req->opcode,
                                                    p_att_req->data.write_req.handle, status);
            }
            break;

        case GATT_REQ_EXECUTE_WRITE:
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "  %s() GATTS_REQ_TYPE_WRITE_EXEC\n", __func__);

            status = app_bt_execute_write_handler(p_data);

            if((p_att_req->opcode == GATT_REQ_EXECUTE_WRITE) &&  (status == WICED_BT_GATT_SUCCESS))
            {
                cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "== Sending execute write success response...\n");
                wiced_bt_gatt_server_send_execute_write_rsp(p_att_req->conn_id, p_att_req->opcode);
                status = WICED_BT_GATT_SUCCESS;
            }
            else
            {
                cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "== Sending execute write error response...\n");
                wiced_bt_gatt_server_send_error_rsp(p_att_req->conn_id,
                                                    p_att_req->opcode,
                                                    p_att_req->data.write_req.handle, status);
            }

            break;

        case GATT_REQ_MTU:
            // Application calls wiced_bt_gatt_server_send_mtu_rsp()
            // with the desired mtu.
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "  %s() GATTS_REQ_TYPE_MTU\n", __func__);

            status = wiced_bt_gatt_server_send_mtu_rsp(p_att_req->conn_id,
                                                       p_att_req->data.remote_mtu,
                                                       CY_BT_RX_PDU_SIZE);
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_NOTICE, "    Set MTU size to : %d  status: 0x%lx\r\n", CY_BT_RX_PDU_SIZE, status);
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_NOTICE, "     RX PDU Size    : %d  status: 0x%lx\r\n", p_att_req->data.remote_mtu, status);

            break;

        case GATT_HANDLE_VALUE_CONF:           /* Value confirmation */

            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "  %s() GATTS_REQ_TYPE_CONF\n", __func__);

            cy_ota_agent_state_t ota_lib_state;

            cy_ota_get_state(ota_app.ota_context, &ota_lib_state);

            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "  %s() ota_lib_state : %d \n", __func__, (int)ota_lib_state);

            /* Check if the download has completed before rebooting */
            if ( (ota_lib_state == CY_OTA_STATE_OTA_COMPLETE) && (ota_app.reboot_at_end != 0) )
            {
                cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_WARNING, "%s()   RESETTING NOW !!!!\n", __func__);

                cy_rtos_delay_milliseconds(1000);

                NVIC_SystemReset();
            }
            else
            {
                cy_ota_agent_stop(&ota_app.ota_context);    /* Stop OTA */
            }
            break;

        case GATT_HANDLE_VALUE_NOTIF:
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "  %s() GATT_HANDLE_VALUE_NOTIF - Client received our notification\n", __func__);
            break;

        default:
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_WARNING, "  %s() Unhandled Event opcode: %d\n", __func__, p_att_req->opcode);
            break;
    }

    return status;
}

/*
 * Function Name:
 * app_bt_set_advertisement_data
 *
 * Function Description:
 * @brief  Set Advertisement Data
 *
 * @param void
 *
 * @return wiced_result_t WICED_SUCCESS or WICED_failure
 */
static wiced_result_t app_bt_set_advertisement_data(void)
{

    wiced_result_t result = WICED_SUCCESS;
    wiced_bt_ble_advert_elem_t adv_elem[3] = { 0 };

    uint8_t num_elem        = 0;
    uint8_t flag            = (BTM_BLE_GENERAL_DISCOVERABLE_FLAG | BTM_BLE_BREDR_NOT_SUPPORTED);

    /* Advertisement Element for Advertisement Flags */
    adv_elem[num_elem].advert_type = BTM_BLE_ADVERT_TYPE_FLAG;
    adv_elem[num_elem].len         = sizeof(uint8_t);
    adv_elem[num_elem].p_data      = &flag;
    num_elem++;

    /* Advertisement Element for Name */
    adv_elem[num_elem].advert_type = BTM_BLE_ADVERT_TYPE_NAME_COMPLETE;
    adv_elem[num_elem].len         = app_gap_device_name_len;
    adv_elem[num_elem].p_data      = app_gap_device_name;
    num_elem++;

    result = wiced_bt_ble_set_raw_advertisement_data(num_elem, adv_elem);

    if(result != WICED_SUCCESS)
    {
        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "   wiced_bt_ble_set_raw_advertisement_data Failed 0x%x\n", result);
    }

    return (result);
}

/*
 * Function Name:
 * app_bt_gatt_event_handler
 *
 * Function Description:
 * @brief  This Function handles the all the GATT events - GATT Event Handler
 *
 * @param event            Bluetooth® GATT event type
 * @param p_event_data     Pointer to Bluetooth® GATT event data
 *
 * @return wiced_bt_gatt_status_t  Bluetooth® GATT status
 */
static wiced_bt_gatt_status_t app_bt_gatt_event_handler(wiced_bt_gatt_evt_t event, wiced_bt_gatt_event_data_t *p_event_data)
{

    wiced_bt_gatt_status_t status = WICED_BT_GATT_SUCCESS;
    wiced_bt_gatt_attribute_request_t *p_attr_req = &p_event_data->attribute_request;

    switch (event)
    {
        /* GATT connection status change. Event data: #wiced_bt_gatt_connection_status_t */
        case GATT_CONNECTION_STATUS_EVT:
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "\n\n%s() GATT_CONNECTION_STATUS_EVT:  %d\n", __func__, event);

            status = app_bt_connect_callback(&p_event_data->connection_status);
            break;

        /* GATT attribute request (from remote client). Event data: #wiced_bt_gatt_attribute_request_t */
        case GATT_ATTRIBUTE_REQUEST_EVT:
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "\n\n%s() GATT_ATTRIBUTE_REQUEST_EVT:  %d type:%d\n", __func__, event, p_attr_req->opcode);

            status = app_bt_server_callback(p_event_data);
            break;

        case GATT_GET_RESPONSE_BUFFER_EVT:      /* GATT buffer request, typically sized to max of bearer mtu - 1 */
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "\n\n%s() GATT_GET_RESPONSE_BUFFER_EVT\n", __func__);

            p_event_data->buffer_request.buffer.p_app_rsp_buffer = app_bt_alloc_buffer (p_event_data->buffer_request.len_requested);
            p_event_data->buffer_request.buffer.p_app_ctxt       = (void*)app_bt_free_buffer;

            status = WICED_BT_GATT_SUCCESS;
            break;

        case GATT_APP_BUFFER_TRANSMITTED_EVT:    /* GATT buffer transmitted event,  check \ref wiced_bt_gatt_buffer_transmitted_t*/
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "\n\n%s() GATT_APP_BUFFER_TRANSMITTED_EVT.\n", __func__);
            {
                pfn_free_buffer_t pfn_free = (pfn_free_buffer_t)p_event_data->buffer_xmitted.p_app_ctxt;

                /* If the buffer is dynamic, the context will point to a function to free it. */
                if (pfn_free != 0U)
                {
                    pfn_free(p_event_data->buffer_xmitted.p_app_data);
                }

                status = WICED_BT_GATT_SUCCESS;
            }
            break;

        case GATT_OPERATION_CPLT_EVT:       /* GATT operation complete. Event data: #wiced_bt_gatt_event_data_t */
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "\n\n%s() GATT_OPERATION_CPLT_EVT:  We are a server, nothing to do.\n", __func__);
            break;

        default:
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_WARNING, "\n\n%s()------------------> Unhandled GATT event: %d\n\n", __func__, event);
            status = WICED_BT_GATT_SUCCESS;
            break;
    }

    return status;
}

/*
 * Function name:
 * bt_app_init
 *
 * Function Description:
 * @brief    This function is executed if BTM_ENABLED_EVT event occurs in
 *           Bluetooth® management callback.
 *
 * @param    void
 *
 * @return    void
 */
static void bt_app_init(void)
{
    wiced_bt_gatt_status_t status = WICED_BT_GATT_BUSY;

    memset(&write_buff, 0x00, sizeof(gatt_write_req_buf_t));

    /* Register with stack to receive GATT callback */
    status = wiced_bt_gatt_register(app_bt_gatt_event_handler);
    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wiced_bt_gatt_register() status (0x%lx) %s\n", status, app_get_gatt_status_name(status));

    /* Initialize GATT Database */
    status = wiced_bt_gatt_db_init(gatt_database, gatt_database_len, NULL);
    if(status != WICED_BT_GATT_SUCCESS)
    {
        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "%s() wiced_bt_gatt_db_init() FAILED 0x%lx !\n", __func__, status);
    }

    /* Allow peer to pair */
    wiced_bt_set_pairable_mode(WICED_TRUE, 0);

    /* Set Advertisement Data */
    status = app_bt_set_advertisement_data();
    if(status != WICED_SUCCESS)
    {
        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "%s() app_bt_set_advertisement_data() FAILED 0x%lx !\n", __func__, status);
    }

    /* Start Undirected LE Advertisements on device startup. */
    status = wiced_bt_start_advertisements(BTM_BLE_ADVERT_UNDIRECTED_HIGH, BLE_ADDR_PUBLIC, NULL);
    if(status != WICED_SUCCESS)
    {
        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "%s() wiced_bt_start_advertisements()  FAILED 0x%lx\n", __func__, status);
    }
}

/*
 * Function Name: app_bt_management_callback()
 *
 * @brief This is a Bluetooth® stack event handler function to receive management events
 *        from the Bluetooth® stack and process as per the application.
 *
 * @param wiced_bt_management_evt_t  Bluetooth® event code of one byte length
 * @param wiced_bt_management_evt_data_t  Pointer to Bluetooth® management event
 *                                        structures
 *
 * @return wiced_result_t Error code from WICED_RESULT_LIST or BT_RESULT_LIST
 *
 */
wiced_result_t app_bt_management_callback(wiced_bt_management_evt_t event,
                                          wiced_bt_management_evt_data_t *p_event_data)
{
    wiced_bt_dev_status_t               status               = WICED_SUCCESS;
    wiced_bt_dev_encryption_status_t    *p_status        = NULL;
    wiced_bt_dev_ble_pairing_info_t     *p_info              = NULL;
    wiced_bt_ble_advert_mode_t          *p_adv_mode          = NULL;
    wiced_bt_device_address_t           local_device_bd_addr = {0};
    uint16_t                            min_interval = 6;
    uint16_t                            max_interval = 6;

    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\n%s() Event: (%d) %s\n", __func__, event, app_get_bt_event_name(event));

    switch (event)
    {
        case BTM_ENABLED_EVT:
            {
                /* Verified that the BT firmware was downloaded. Now, stop and delete
                 * freeRTOS timer.*/
                free_app_bt_fw_download_check_timer();

                /* Bluetooth® Controller and Host Stack Enabled */
                cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "  BTM_ENABLED_EVT\n");
                if (WICED_BT_SUCCESS == p_event_data->enabled.status)
                {
                    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_NOTICE, "  Bluetooth(r) ENABLED\n");
                    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_NOTICE, "  This application supports Bluetooth(r) OTA updates.\n");
                    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_NOTICE, "  Device name: '%s'  addr: " BT_ADDR_FORMAT "\n",
                               app_gap_device_name,
                               local_device_bd_addr[0], local_device_bd_addr[1], local_device_bd_addr[2],
                               local_device_bd_addr[3], local_device_bd_addr[4], local_device_bd_addr[5]);

                    /* Perform application-specific Bluetooth® initialization */
                    bt_app_init();
                }
                else
                {
                    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "  Bluetooth(r) Enable FAILED \n" );
                }
            }
           break;

        case BTM_DISABLED_EVT:
            /* Bluetooth® Controller and Host Stack Disabled */
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "  BTM_DISABLED_EVT\n");
            break;

        case BTM_USER_CONFIRMATION_REQUEST_EVT:
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "  BTM_USER_CONFIRMATION_REQUEST_EVT: Numeric_value: %d \n", p_event_data->user_confirmation_request.numeric_value);

            wiced_bt_dev_confirm_req_reply(WICED_BT_SUCCESS, p_event_data->user_confirmation_request.bd_addr);
            break;

        case BTM_PASSKEY_NOTIFICATION_EVT:
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "  PassKey Notification. BDA 0x%2x:0x%2x:0x%2x:0x%2x:0x%2x:0x%2x, Key %d \n",
                   p_event_data->user_passkey_notification.bd_addr[0], p_event_data->user_passkey_notification.bd_addr[1],
                   p_event_data->user_passkey_notification.bd_addr[2], p_event_data->user_passkey_notification.bd_addr[3],
                   p_event_data->user_passkey_notification.bd_addr[4], p_event_data->user_passkey_notification.bd_addr[5],
                   p_event_data->user_passkey_notification.passkey );

            wiced_bt_dev_confirm_req_reply(WICED_BT_SUCCESS, p_event_data->user_passkey_notification.bd_addr);
            break;

        case BTM_PAIRING_IO_CAPABILITIES_BLE_REQUEST_EVT:
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "  BTM_PAIRING_IO_CAPABILITIES_BLE_REQUEST_EVT\n");

            p_event_data->pairing_io_capabilities_ble_request.local_io_cap  = BTM_IO_CAPABILITIES_NONE;
            p_event_data->pairing_io_capabilities_ble_request.oob_data      = BTM_OOB_NONE;
            p_event_data->pairing_io_capabilities_ble_request.auth_req      = BTM_LE_AUTH_REQ_BOND | BTM_LE_AUTH_REQ_MITM;
            p_event_data->pairing_io_capabilities_ble_request.max_key_size  = 0x10;                                         // TODO: Why 10?
            p_event_data->pairing_io_capabilities_ble_request.init_keys     = BTM_LE_KEY_PENC | BTM_LE_KEY_PID;
            p_event_data->pairing_io_capabilities_ble_request.resp_keys     = BTM_LE_KEY_PENC | BTM_LE_KEY_PID;

            break;

        case BTM_PAIRING_COMPLETE_EVT:
            p_info =  &p_event_data->pairing_complete.pairing_complete_info.ble;

            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "  Pairing Complete: %d ", p_info->reason);
            break;

        case BTM_PAIRED_DEVICE_LINK_KEYS_UPDATE_EVT:
            /* save device keys*/
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "  BTM_PAIRED_DEVICE_LINK_KEYS_UPDATE_EVT\n");

            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "bondinfo.link_keys[bondinfo.slot_data[NEXT_FREE] = " BT_ADDR_FORMAT "\n",
                    bondinfo.link_keys[bondinfo.slot_data[NEXT_FREE]].bd_addr[0], bondinfo.link_keys[bondinfo.slot_data[NEXT_FREE]].bd_addr[1], bondinfo.link_keys[bondinfo.slot_data[NEXT_FREE]].bd_addr[2],
                    bondinfo.link_keys[bondinfo.slot_data[NEXT_FREE]].bd_addr[3], bondinfo.link_keys[bondinfo.slot_data[NEXT_FREE]].bd_addr[4], bondinfo.link_keys[bondinfo.slot_data[NEXT_FREE]].bd_addr[5]);

            bondinfo.link_keys[bondinfo.slot_data[NEXT_FREE]] = p_event_data->paired_device_link_keys_update;

            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "GET_ADDR_FOR_DEVICE_KEYS(bondinfo.slot_data[NEXT_FREE]) = %d\n", GET_ADDR_FOR_DEVICE_KEYS(bondinfo.slot_data[NEXT_FREE]));

            status = wiced_bt_dev_add_device_to_address_resolution_db(&p_event_data->paired_device_link_keys_update);

            if (status != WICED_BT_SUCCESS)
            {
                cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "  wiced_bt_dev_add_device_to_address_resolution_db() failed: 0x%lx\n", status);
            }

            break;

        case BTM_PAIRED_DEVICE_LINK_KEYS_REQUEST_EVT:
            /* Paired Device Link Keys Request */
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "  BTM_PAIRED_DEVICE_LINK_KEYS_REQUEST_EVT for " BT_ADDR_FORMAT " \n",
                    p_event_data->paired_device_link_keys_request.bd_addr[0],
                    p_event_data->paired_device_link_keys_request.bd_addr[1],
                    p_event_data->paired_device_link_keys_request.bd_addr[2],
                    p_event_data->paired_device_link_keys_request.bd_addr[3],
                    p_event_data->paired_device_link_keys_request.bd_addr[4],
                    p_event_data->paired_device_link_keys_request.bd_addr[5] );

            /* search if the BD_ADDR is in EEPROM. If not, return WICED_BT_ERROR and the stack */
            /* generate keys and call BTM_PAIRED_DEVICE_LINK_KEYS_UPDATE_EVT so that they can be stored */
            status = WICED_BT_ERROR;
            for(uint8_t count = 0; count < bondinfo.slot_data[NUM_BONDED]; count++)
            {
                cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "    comparing with - BDA = " BT_ADDR_FORMAT "\n",
                        bondinfo.link_keys[count].bd_addr[0], bondinfo.link_keys[count].bd_addr[1],
                        bondinfo.link_keys[count].bd_addr[2],bondinfo.link_keys[count].bd_addr[3],
                        bondinfo.link_keys[count].bd_addr[4], bondinfo.link_keys[count].bd_addr[5] );

                cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "    count = %d\n", count);

                if( (bondinfo.link_keys[count].bd_addr[0] == p_event_data->paired_device_link_keys_request.bd_addr[0]) &&
                    (bondinfo.link_keys[count].bd_addr[1] == p_event_data->paired_device_link_keys_request.bd_addr[1]) &&
                    (bondinfo.link_keys[count].bd_addr[2] == p_event_data->paired_device_link_keys_request.bd_addr[2]) &&
                    (bondinfo.link_keys[count].bd_addr[3] == p_event_data->paired_device_link_keys_request.bd_addr[3]) &&
                    (bondinfo.link_keys[count].bd_addr[4] == p_event_data->paired_device_link_keys_request.bd_addr[4]) &&
                    (bondinfo.link_keys[count].bd_addr[5] == p_event_data->paired_device_link_keys_request.bd_addr[5]) )
                {
                    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "  Matching Device Key Found \n");

                    p_event_data->paired_device_link_keys_request = bondinfo.link_keys[count];

                    status = WICED_BT_SUCCESS;
                    break;
                }
            }
            if (WICED_BT_ERROR == status)
            {
                /* We return WICED_BT_ERROR and the stack will generate keys */
                cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "  Device not found in the database \n");
            }
            break;

        case BTM_LOCAL_IDENTITY_KEYS_UPDATE_EVT:
            /* Update of local privacy keys - save to EEPROM */
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "BTM_LOCAL_IDENTITY_KEYS_UPDATE_EVT\n");

            memcpy(&bondinfo.identity_keys , (uint8_t*)&(p_event_data->local_identity_keys_update), sizeof(wiced_bt_local_identity_keys_t));

            break;

        case BTM_LOCAL_IDENTITY_KEYS_REQUEST_EVT:
            /* Request for local privacy keys - read from EEPROM */
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "BTM_LOCAL_IDENTITY_KEYS_REQUEST_EVT\n" );
            /* If the key type is 0, return an error to cause the stack to generate keys and then call
             * BTM_PAIRED_DEVICE_LINK_KEYS_UPDATE_EVT so that the keys can be stored */

            if(0 == bondinfo.identity_keys.key_type_mask)
            {
                status = WICED_ERROR;

                cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "  New identity keys need to be generated by the stack.\n");
            }
            else
            {
                memcpy(&(p_event_data->local_identity_keys_request), &(bondinfo.identity_keys), sizeof(wiced_bt_local_identity_keys_t));

                cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "  Identity keys are available in the database.\n");

                cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "  Local identity keys read from EEPROM: \n" );
            }
            break;

        case BTM_ENCRYPTION_STATUS_EVT:
            p_status = &p_event_data->encryption_status;

            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "  Encryption Status Event: res %d", p_status->result);

            break;

        case BTM_SECURITY_REQUEST_EVT:
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "  BTM_SECURITY_REQUEST_EVT\n");

            wiced_bt_ble_security_grant( p_event_data->security_request.bd_addr, WICED_BT_SUCCESS );

            break;

        case BTM_BLE_ADVERT_STATE_CHANGED_EVT:
            /* Advertisement State Changed */
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "BTM_BLE_ADVERT_STATE_CHANGED_EVT\n" );

            p_adv_mode = &p_event_data->ble_advert_state_changed;

            if (p_adv_mode != BTM_BLE_ADVERT_OFF)
            {
                cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "  New Adv state (%d) %s\n", *p_adv_mode, app_get_bt_advert_mode_name(*p_adv_mode) );
            }
            else
            {
                cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "  Advertise OFF\n");
            }
            break;

        case BTM_BLE_CONNECTION_PARAM_UPDATE:
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "  BTM_BLE_CONNECTION_PARAM_UPDATE\n");

            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "    ble_connection_param_update.bd_addr             : " BT_ADDR_FORMAT "\n",
                    p_event_data->ble_connection_param_update.bd_addr[0], p_event_data->ble_connection_param_update.bd_addr[1], p_event_data->ble_connection_param_update.bd_addr[2],
                    p_event_data->ble_connection_param_update.bd_addr[3], p_event_data->ble_connection_param_update.bd_addr[4], p_event_data->ble_connection_param_update.bd_addr[5] );

            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "    ble_connection_param_update.conn_interval       : %d\n", p_event_data->ble_connection_param_update.conn_interval);

            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "    ble_connection_param_update.conn_latency        : %d\n", p_event_data->ble_connection_param_update.conn_latency);

            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "    ble_connection_param_update.status              : %d\n", p_event_data->ble_connection_param_update.status);

            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "    ble_connection_param_update.supervision_timeout : %d\n", p_event_data->ble_connection_param_update.supervision_timeout);

            status = wiced_bt_ble_get_connection_parameters(ota_app.bt_peer_addr, &ota_app.bt_conn_params );

            if (status != WICED_BT_SUCCESS)
            {
                cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "  wiced_bt_ble_get_connection_parameters() failed: 0x%lx\n", status);
                status = WICED_ERROR;
                break;
            }

            status = WICED_SUCCESS;
            bt_pref_conn_params.conn_interval_min=min_interval;
            bt_pref_conn_params.conn_interval_max=max_interval;
            bt_pref_conn_params.min_ce_length=0;
            bt_pref_conn_params.max_ce_length=0;
            bt_pref_conn_params.conn_latency=p_event_data->ble_connection_param_update.conn_latency;
            bt_pref_conn_params.conn_supervision_timeout=2500;

            if (wiced_bt_l2cap_update_ble_conn_params(p_event_data->ble_connection_param_update.bd_addr, &bt_pref_conn_params) == 0)
            {
                cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "          wiced_bt_l2cap_update_ble_conn_params() failed\n");
                status = WICED_ERROR;
            }
            else
            {
                memcpy(ota_app.bt_peer_addr, p_event_data->ble_connection_param_update.bd_addr, sizeof(ota_app.bt_peer_addr));  /* Save peer Bluetooth® addr in Application data structure */

                cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "  NEW SETTINGS\n");
                cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "    min_interval       : %d\n", min_interval);
                cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "    max_interval       : %d\n", max_interval);
                cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "    conn_latency       : %d\n", p_event_data->ble_connection_param_update.conn_latency);
                cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "    supervision_timeout: %d\n", p_event_data->ble_connection_param_update.supervision_timeout);
                status = WICED_SUCCESS;
            }
            break;

        case BTM_BLE_DATA_LENGTH_UPDATE_EVENT:
            /* Print the updated BLE physical link*/
            printf("\nCase = %d\nSelected TX PHY - %dM\n Selected RX PHY - %dM\n",
                    event,
                    p_event_data->ble_phy_update_event.tx_phy,
                    p_event_data->ble_phy_update_event.rx_phy);
            break;


        case BTM_BLE_PHY_UPDATE_EVT:
            /* Print the updated BLE physical link*/
            printf("\nCase = %d\nSelected TX PHY - %dM\n Selected RX PHY - %dM\n",
                    event,
                    p_event_data->ble_phy_update_event.tx_phy,
                    p_event_data->ble_phy_update_event.rx_phy);
            break;

        case BTM_BLE_DEVICE_ADDRESS_UPDATE_EVENT:
            /* Event to notify change in the device address */
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_NOTICE, "  Device name: '%s'  addr: " BT_ADDR_FORMAT "\n",
                   app_gap_device_name,
                   local_device_bd_addr[0], local_device_bd_addr[1], local_device_bd_addr[2],
                   local_device_bd_addr[3], local_device_bd_addr[4], local_device_bd_addr[5]);
            break;

        case BTM_BLE_CHANNEL_SELECTION_ALGO_EVENT:
            /* Print the updated BLE physical link*/
            printf("\nCase = %d\nSelected TX PHY - %dM\n Selected RX PHY - %dM\n",
                    event,
                    p_event_data->ble_phy_update_event.tx_phy,
                    p_event_data->ble_phy_update_event.rx_phy);
            break;

        default:
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_WARNING, "%s()  UNHANDLED Bluetooth(r) Management Event: (%d) %s\n", __func__, event, app_get_bt_event_name(event));
            break;
    }

    return (status);
}

cy_rslt_t cy_ota_ble_check_build_vs_configurator( void )
{
    /* verify "make built" with proper Bluetooth® SECURE setting, compare to output on what Bluetooth® Configurator output for us */
    /* Check UUID for non-secure Bluetooth® upgrade service */
    if (0 != memcmp(NON_SECURE_UUID_SERVICE_OTA_FW_UPGRADE_SERVICE, BLE_CONFIG_UUID_SERVICE_OTA_FW_UPGRADE_SERVICE, sizeof(NON_SECURE_UUID_SERVICE_OTA_FW_UPGRADE_SERVICE)))
    {
        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "\n");
        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "    SECURE <appname>.cybt File does not match NON-SECURE APP build!\n");
        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "      Change the <appname>.cybt File to use NON-SECURE OTA UUID.\n");
        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "        (Set 'GATT->Server->OTA FW UPGRADE SERVICE' to 'ae5d1e47-5c13-43a0-8635-82ad38a1381f')\n");

        return CY_RSLT_OTA_ERROR_GENERAL;
    }

    return CY_RSLT_SUCCESS;
}


/* [] END OF FILE */
