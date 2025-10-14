################################################################################
# \file Makefile
# \version 1.0
#
# \brief
# Makefile for configuring the Cortex-M33 core project
#
################################################################################
# \copyright
# Copyright 2023-2024, Cypress Semiconductor Corporation (an Infineon company)
# SPDX-License-Identifier: Apache-2.0
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
################################################################################

include ../common.mk

################################################################################
# Basic Configuration
###############################################################################

# Name of application (used to derive name of final linked file).
#
# If APPNAME is edited, ensure to update or regenerate launch
# configurations for your IDE.
APPNAME=proj_cm33_ns

# If set to "true" or "1", display full command-lines when building.
VERBOSE=


################################################################################
# Advanced Configuration
################################################################################

# Enable optional code that is ordinarily disabled by default.
#
# Available components depend on the specific targeted hardware and firmware
# in use. In general, if you have
#
#    COMPONENTS=foo bar
#
# ... then code in directories named COMPONENT_foo and COMPONENT_bar will be
# added to the build
#
#add FreeRTOS support
COMPONENTS+=FREERTOS RTOS_AWARE 
#add BLE stack support
COMPONENTS+=WICED_BLE

# Like COMPONENTS, but disable optional code that was enabled by default.
DISABLE_COMPONENTS=

CORE=CM33
CORE_NAME=CM33_0

#Configure this application to be loaded into flash/ram
APPTYPE=flash

# By default the build system automatically looks in the Makefile's directory
# tree for source code and builds it. The SOURCES variable can be used to
# manually add source code to the build process from a location not searched
# by default, or otherwise not found by the build system.
SOURCES=

# Like SOURCES, but for include directories. Value should be paths to
# directories (without a leading -I).
INCLUDES=

# Add additional defines to the build process (without a leading -D).
DEFINES= CY_RETARGET_IO_CONVERT_LF_TO_CRLF

#Add for logs
#DEFINES+=ENABLE_OTA_LOGS ENABLE_OTA_BOOTLOADER_ABSTRACTION_LOGS
 
DEFINES+=CY_MQTT_EVENT_THREAD_STACK_SIZE=5*1024

# Select softfp or hardfp floating point. Default is softfp.
VFP_SELECT=

# Additional / custom C compiler flags.
#
# NOTE: Includes and defines should use the INCLUDES and DEFINES variable
# above.
CFLAGS=

# Additional / custom C++ compiler flags.
#
# NOTE: Includes and defines should use the INCLUDES and DEFINES variable
# above.
CXXFLAGS=

# Additional / custom assembler flags.
#
# NOTE: Includes and defines should use the INCLUDES and DEFINES variable
# above.
ASFLAGS=

# Additional / custom linker flags.
LDFLAGS=

# Additional / custom libraries to link in to the application.
LDLIBS=

# Path to the linker script to use (if empty, use the default linker script).
LINKER_SCRIPT=

################################################################################
# Paths
################################################################################

# Relative path to the project directory (default is the Makefile's directory).
#
# This controls where automatic source code discovery looks for code.
CY_APP_PATH=

# Relative path to the shared repo location.
#
# All .mtb files have the format, <URI>#<COMMIT>#<LOCATION>. If the <LOCATION> field 
# begins with $$ASSET_REPO$$, then the repo is deposited in the path specified by 
# the CY_GETLIBS_SHARED_PATH variable. The default location is one directory level 
# above the current app directory.
# This is used with CY_GETLIBS_SHARED_NAME variable, which specifies the directory name.
CY_GETLIBS_SHARED_PATH=../..

# Directory name of the shared repo location.
#
CY_GETLIBS_SHARED_NAME=mtb_shared

################################################################################
# Basic OTA configuration
################################################################################
# Set to 1 to add OTA defines, sources, and libraries (must be used with MCUBoot)
OTA_SUPPORT=1

CY_BOOTLOADER?=IFX_MCUBOOT

    
# Add Boot loader support
COMPONENTS+=$(CY_BOOTLOADER)

#OTA with BTStack support
OTA_BT_SUPPORT?=1

DEFINES+= OTA_SUPPORT=1 \
          APP_VERSION_MAJOR=$(APP_VERSION_MAJOR) \
          APP_VERSION_MINOR=$(APP_VERSION_MINOR) \
          APP_VERSION_BUILD=$(APP_VERSION_BUILD) \
          APP_1_NAME=$(APP_1_NAME) \
          APP_2_NAME=$(APP_2_NAME) \
          APP_3_NAME=$(APP_3_NAME)

ifneq ($(MAKECMDGOALS),get_app_info)      
    OTA_UPDATE_MK := $(wildcard $(CY_GETLIBS_SHARED_PATH)/$(CY_GETLIBS_SHARED_NAME)/ota-update/*/makefiles/ota_update.mk)
    include $(OTA_UPDATE_MK)
endif
    
OTA_PLATFORM?=PSE84
XIP_MODE?=XIP

ifneq ($(MAKECMDGOALS),get_app_info)      
    # Path to the OTA linker script to use.
    OTA_LINKER_FILE?=$(MTB_TOOLS__TARGET_DIR)/COMPONENT_$(CORE)/TOOLCHAIN_$(TOOLCHAIN)/pse84_ns_cm33.$(BSP_LINKER_SCRIPT_EXT)
endif 

ifeq ($(UPDATE_TYPE),swap)   
OTA_FLASH_MAP?=./app_ota/flashmap/pse84_rram_swap_multi3.json
else
OTA_FLASH_MAP?=./app_ota/flashmap/pse84_rram_overwrite_multi3.json
endif

$(info OTA Linker file path = $(OTA_LINKER_FILE))
    
# Set Python path
ifeq ($(CY_PYTHON_PATH),)
   ifeq ($(OS),Windows_NT)
       CY_PYTHON_PATH=python
    else
       CY_PYTHON_PATH=python3
    endif
endif # checking for Python path
    
ifneq ($(MAKECMDGOALS),get_app_info)
    ifneq ($(MAKECMDGOALS),eclipse) 
        OTA_BOOTLOADER_ABSTRACTION_DIRS := $(wildcard $(CY_GETLIBS_SHARED_PATH)/$(CY_GETLIBS_SHARED_NAME)/ota-bootloader-abstraction/*/makefiles/mcuboot/ifx_mcuboot_support.mk)
        include $(OTA_BOOTLOADER_ABSTRACTION_DIRS)
    endif
endif

include $(CY_TOOLS_DIR)/make/start.mk