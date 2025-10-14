/*****************************************************************************
* File Name        : cy_ota_flash.c
*
* Description      : This file contains the OTA Flash management routines.
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

/*
 *  Flash operation callback implementation for OTA libraries.
 */

/* Header file includes */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "cy_pdl.h"
#if defined(COMPONENT_MTB_HAL)
#include "mtb_hal.h"
#include "cybsp.h"
#include "mtb_serial_memory.h"
#else
#include "cyhal.h"
#endif
#include "cybsp.h"
#include "cy_ota_flash.h"
#include "cyabs_rtos.h"

#if defined(PSE84)
#include "cy_rram.h"
#include "cycfg.h"
#if defined(COMPONENT_MTB_HAL)
#include "mtb_serial_memory.h"
#else
#include "cy_serial_flash_qspi.h"
#endif
#include "cycfg_qspi_memslot.h"
#include "cybsp_hw_config.h"
#endif

#if !(defined (CYW20829B0LKML) || defined (CYW20829B1010) || defined (CYW89829B01MKSBG) || defined (CYW89829B1232))
#include <cycfg_pins.h>
#endif

/**********************************************************************************************************************************
 * local defines
 **********************************************************************************************************************************/

#if defined(COMPONENT_PSE84)

#ifndef CY_XIP_BASE
#define CY_XIP_BASE                 0x60000000UL
#endif
#define CY_FLASH_SIZE               0x40000UL
#define CY_FLASH_BASE               0x70000000UL
#define MEM_SLOT_NUM                (0u)      /* Slot number of the memory to use */
#define SECTOR_ADDR                 0x40000U  /* Offset to the start of external memory that belongs to the sector for which size is returned. */
#define SMIF_DATA_OCTAL             (0x08U)
#define SMIF_DATA_QUAD              (0x04U)
#define DATA_WIDTH_PINS             0x04U
#define IFX_EPB_SMIF_TIMEOUT_MS     (8000U)

#if defined (OTA_USE_EXTERNAL_FLASH)
#define CY_FLASH_ERASE_SIZE         0x40000UL /* Erase Size for External Flash and XIP area */
#else
#define CY_FLASH_ERASE_SIZE         0x200UL /* Erase Size for Internal RRAM area */
#endif /*OTA_USE_EXTERNAL_FLASH */

#define CY_FLASH_SIZEOF_ROW         512UL

static mtb_serial_memory_t sm_obj;
static cy_stc_smif_mem_context_t context;
static cy_stc_smif_mem_info_t smif_mem_info;

#endif /* PSE84 */

#define CY_BOOT_TRAILER_MAX_UPDATE_SIZE             (16)

#define CY_SS0_SMIF_ID         (1U) /* Assume SlaveSelect_0 is used for External Memory */

/**********************************************************************************************************************************
 * External Functions
 **********************************************************************************************************************************/
/**
 * @brief Initializes flash, QSPI flash, or any other external memory type
 *
 * @return  CY_RSLT_SUCCESS on success
 *          CY_RSLT_TYPE_ERROR on failure
 */
cy_rslt_t cy_ota_mem_init( void )
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

#if defined(COMPONENT_PSE84)

    /* Initialize the QSPI block */
#if ((DATA_WIDTH_PINS) == (SMIF_DATA_QUAD))

    result = mtb_serial_memory_setup(&sm_obj, MTB_SERIAL_MEMORY_CHIP_SELECT_1, CYBSP_SMIF_CORE_0_XSPI_FLASH_hal_config.base, CYBSP_SMIF_CORE_0_XSPI_FLASH_hal_config.clock, &context, &smif_mem_info, &smif0BlockConfig);
    if (result != CY_RSLT_SUCCESS)
    {
        printf("\nmtb_serial_memory_setup() failed in %s : line %d", __func__, __LINE__);
    }

#elif ((DATA_WIDTH_PINS) == (SMIF_DATA_OCTAL))
#if !defined(MTB_HAL)
        result = cy_serial_flash_qspi_attach(smif0MemConfigs[MEM_SLOT_NUM], CYBSP_OSPI_D0, CYBSP_OSPI_D1,
                                            CYBSP_OSPI_D2, CYBSP_OSPI_D3, CYBSP_OSPI_D4, CYBSP_OSPI_D5,
                                           CYBSP_OSPI_D6, CYBSP_OSPI_D7, CYBSP_OSPI_SS);
#endif
#else
    printf("DATA_WIDTH_PINS for external memory undefined");
#endif

    if(result == CY_RSLT_SUCCESS)
    {
        printf("External Memory initialized w/ SFDP.");
    }
    else
    {
        printf("External Memory initialization w/ SFDP FAILED: 0x%" PRIx32 " \r\n", (uint32_t)result);
    }
#endif /* PSE84 */
    return result;
}

/**
 * @brief Read from flash, QSPI flash, or any other external memory type
 *
 * @param[in]   mem_type   Memory type @ref cy_ota_mem_type_t
 * @param[in]   addr       Starting address to read from.
 * @param[out]  data       Pointer to the buffer to store the data read from the memory.
 * @param[in]   len        Number of data bytes to read.
 *
 * @return  CY_RSLT_SUCCESS on success
 *          CY_RSLT_TYPE_ERROR on failure
 */
cy_rslt_t cy_ota_mem_read( cy_ota_mem_type_t mem_type, uint32_t addr, void *data, size_t len )
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    if( mem_type == CY_OTA_MEM_TYPE_EXTERNAL_FLASH )
    {
#if defined (PSE84)
        if(addr >= CY_XIP_PORT0_S_SBUS_BASE)
        {
            addr -= CY_XIP_PORT0_S_SBUS_BASE;
        }
        else if(addr >= CY_XIP_PORT0_NS_SBUS_BASE)
        {
            addr -= CY_XIP_PORT0_NS_SBUS_BASE;
        }
        else
        {
            //Nothing to do
        }
        result = mtb_serial_memory_read(&sm_obj, addr, len, data);
#endif /* PSE84 */
    }

    if(result == CY_RSLT_SUCCESS)
    {
        return 0;
    }
    else
    {
        printf("%s() READ not supported for memory type %d\n", __func__, (int)mem_type);
        return CY_RSLT_TYPE_ERROR;
    }
}

static cy_rslt_t cy_ota_mem_write_row_size( cy_ota_mem_type_t mem_type, uint32_t addr, void *data, size_t len )
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    if( mem_type == CY_OTA_MEM_TYPE_EXTERNAL_FLASH )
    {
#if defined (PSE84)

        if(addr >= CY_XIP_PORT0_S_SBUS_BASE)
        {
            addr -= CY_XIP_PORT0_S_SBUS_BASE;
        }
        else if(addr >= CY_XIP_PORT0_NS_SBUS_BASE)
        {
            addr -= CY_XIP_PORT0_NS_SBUS_BASE;
        }
        else
        {
            //Nothing to do
        }

#if defined(COMPONENT_MTB_HAL)
        result = mtb_serial_memory_write(&sm_obj, addr, len, data);
#else
        result = cy_serial_flash_qspi_write(addr, len, data);
#endif
        if(result == CY_RSLT_SUCCESS)
        {
            return 0;
        }
        else
        {
            return result;
        }

#endif /* PSE84 */
    }
    else
    {
        printf("%s() Write not supported for memory type %d\n", __func__, (int)mem_type);
        return CY_RSLT_TYPE_ERROR;
    }
}

/**
 * @brief Write to flash, QSPI flash, or any other external memory type
 *
 * @param[in]   mem_type   Memory type @ref cy_ota_mem_type_t
 * @param[in]   addr       Starting address to write to.
 * @param[in]   data       Pointer to the buffer containing the data to be written.
 * @param[in]   len        Number of bytes to write.
 *
 * @return  CY_RSLT_SUCCESS on success
 *          CY_RSLT_TYPE_ERROR on failure
 */
cy_rslt_t cy_ota_mem_write( cy_ota_mem_type_t mem_type, uint32_t addr, void *data, size_t len )
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    /**
     * This is used if a block is < Block size to satisfy requirements
     * of flash_area_write(). "static" so it is not on the stack.
     */
    static uint8_t block_buffer[CY_FLASH_SIZEOF_ROW];
    uint32_t chunk_size = 0;

    uint32_t bytes_to_write = len;
    uint32_t curr_addr = addr;
    uint8_t *curr_src = data;

#ifdef ENABLE_ON_THE_FLY_ENCRYPTION
#ifndef CY_OTA_DIRECT_XIP
    cy_en_smif_status_t cy_smif_result = CY_SMIF_SUCCESS;
    uint32_t cbus_addr = 0;
#endif
#endif

    while(bytes_to_write > 0x0U)
    {
        chunk_size = bytes_to_write;
        if(chunk_size > CY_FLASH_SIZEOF_ROW)
        {
            chunk_size = CY_FLASH_SIZEOF_ROW;
        }

        /* Is the chunk_size smaller than a flash row? */
        if((chunk_size % CY_FLASH_SIZEOF_ROW) != 0x0U)
        {
            uint32_t row_offset = 0;
            uint32_t row_base = 0;

            row_base   = (curr_addr / CY_FLASH_SIZEOF_ROW) * CY_FLASH_SIZEOF_ROW;
            row_offset = curr_addr - row_base;

            if((row_offset + chunk_size) > CY_FLASH_SIZEOF_ROW)
            {
                chunk_size = (CY_FLASH_SIZEOF_ROW - row_offset);
            }

            /* we will read a CY_FLASH_SIZEOF_ROW byte block, write the new data into the block, then write the whole block */
            result = cy_ota_mem_read( mem_type, row_base, (void *)(&block_buffer[0]), sizeof(block_buffer));
            if(result != CY_RSLT_SUCCESS)
            {
                 return CY_RSLT_TYPE_ERROR;
            }

#ifdef ENABLE_ON_THE_FLY_ENCRYPTION
#ifndef CY_OTA_DIRECT_XIP
            cbus_addr = cy_flash_addr_to_cbus_addr(row_base);

            /* pre-access to SMIF */
            PRE_SMIF_ACCESS_TURN_OFF_XIP;

            /* Encrypt again block_buffer to get plain txBuffer */
            cy_smif_result = Cy_SMIF_Encrypt(SMIF0, cbus_addr, &(block_buffer[0]), sizeof(block_buffer), &ota_QSPI_context);

            /* post-access to SMIF */
            POST_SMIF_ACCESS_TURN_ON_XIP;

            if(cy_smif_result != CY_SMIF_SUCCESS)
            {
                printf("[Error] Data encryption failed with error %d\r\n\r\n", cy_smif_result);
            }
#endif
#endif
            memcpy (&block_buffer[row_offset], curr_src, chunk_size);

#ifdef ENABLE_ON_THE_FLY_ENCRYPTION
#ifndef CY_OTA_DIRECT_XIP
            if(mem_type == CY_OTA_MEM_TYPE_EXTERNAL_FLASH)
            {
                /* Erase while updating Image trailers */
                if(len <= CY_BOOT_TRAILER_MAX_UPDATE_SIZE)
                {
                    result = cy_ota_mem_erase(mem_type, curr_addr, bytes_to_write);
                    if(result != CY_RSLT_SUCCESS)
                    {
                        printf("%s() Erase failed for memory type %d\n", __func__, (int)mem_type);
                        return CY_RSLT_TYPE_ERROR;
                    }
                }
            }
#endif
#endif
            result = cy_ota_mem_write_row_size(mem_type, row_base, (void *)(&block_buffer[0]), sizeof(block_buffer));
            if(result != CY_RSLT_SUCCESS)
            {
                return CY_RSLT_TYPE_ERROR;
            }
        }
        else
        {
            result = cy_ota_mem_write_row_size(mem_type, curr_addr, curr_src, chunk_size);
            if(result != CY_RSLT_SUCCESS)
            {
                return CY_RSLT_TYPE_ERROR;
            }
        }

        curr_addr += chunk_size;
        curr_src += chunk_size;
        bytes_to_write -= chunk_size;
    }

    return CY_RSLT_SUCCESS;
}

/**
 * @brief Erase flash, QSPI flash, or any other external memory type
 *
 * @param[in]   mem_type   Memory type @ref cy_ota_mem_type_t
 * @param[in]   addr       Starting address to begin erasing.
 * @param[in]   len        Number of bytes to erase.
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_TYPE_ERROR
 */
cy_rslt_t cy_ota_mem_erase( cy_ota_mem_type_t mem_type, uint32_t addr, size_t len )
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    if( mem_type == CY_OTA_MEM_TYPE_EXTERNAL_FLASH )
    {
        uint32_t offset=0;

        if(addr >= CY_XIP_PORT0_S_SBUS_BASE)
        {
            offset= addr - CY_XIP_PORT0_S_SBUS_BASE;
        }
        else if(addr >= CY_XIP_PORT0_NS_SBUS_BASE)
        {
            offset= addr - CY_XIP_PORT0_NS_SBUS_BASE;
        }
        else
        {
            //Nothing to do
        }

#if defined(COMPONENT_MTB_HAL)
        result = mtb_serial_memory_erase(&sm_obj, offset, len);
#else
        result = cy_serial_flash_qspi_erase(offset, len);
#endif
        if(result == CY_RSLT_SUCCESS)
        {
            return 0;
        }
        else
        {
            return -1;
        }
    }
    else
    {
        printf("%s() Erase not supported for memory type %d\n", __func__, (int)mem_type);
        return CY_RSLT_TYPE_ERROR;
    }
}

/**
 * @brief To get page size for programming flash, QSPI flash, or any other external memory type
 *
 * @param[in]   mem_type   Memory type @ref cy_ota_mem_type_t
 * @param[in]   addr       Address that belongs to the sector for which programming page size needs to be returned.
 *
 * @return    Page size in bytes.
 */
size_t cy_ota_mem_get_prog_size ( cy_ota_mem_type_t mem_type, uint32_t addr )
{
    if( mem_type == CY_OTA_MEM_TYPE_EXTERNAL_FLASH )
    {
        uint32_t    program_size = 0;
        (void)addr; /* Hybrid parts not yet supported */
#if defined (PSE84)
#if defined(COMPONENT_MTB_HAL)
        program_size = mtb_serial_memory_get_prog_size(&sm_obj, addr);
#else
        program_size = cy_serial_flash_qspi_get_prog_size(addr);
#endif
        return program_size;
#endif /* !PSE84 */
    }
    else
    {
        return 0;
    }
}

/**
 * @brief To get sector size of flash, QSPI flash, or any other external memory type
 *
 * @param[in]   mem_type   Memory type @ref cy_ota_mem_type_t
 * @param[in]   addr       Address that belongs to the sector for which sector erase size needs to be returned.
 *
 * @return    Sector size in bytes.
 */
size_t cy_ota_mem_get_erase_size ( cy_ota_mem_type_t mem_type, uint32_t addr )
{
    if( mem_type == CY_OTA_MEM_TYPE_EXTERNAL_FLASH )
    {
#if defined (PSE84)
#if defined(COMPONENT_MTB_HAL)
        return mtb_serial_memory_get_erase_size(&sm_obj, SECTOR_ADDR);
#else
        return cy_serial_flash_qspi_get_erase_size(SECTOR_ADDR);
#endif

#endif /* PSE84 */
    }
    else
    {
        return 0;
    }
}
