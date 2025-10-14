################################################################################
# \file common.mk
# \version 1.0
#
# \brief
# Settings shared across all projects.
#
################################################################################
# \copyright
# (c) 2023-2025, Infineon Technologies AG, or an affiliate of Infineon
# Technologies AG.  SPDX-License-Identifier: Apache-2.0
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

MTB_TYPE=PROJECT

# Target board/hardware (BSP).
# To change the target, it is recommended to use the Library manager
# ('make library-manager' from command line), which will also update 
# Eclipse IDE launch configurations.
TARGET=KIT_PSE84_EVAL_EPC2

# Name of toolchain to use. Options include:
#
# GCC_ARM -- GCC provided with ModusToolbox software
# ARM     -- ARM Compiler (must be installed separately)
# IAR     -- IAR Compiler (must be installed separately)
# LLVM_ARM	-- LLVM Embedded Toolchain (must be installed separately)
#
# See also: CY_COMPILER_PATH below
TOOLCHAIN=GCC_ARM
# Default build configuration. Options include:
#
# Debug -- build with minimal optimizations, focus on debugging.
# Release -- build with full optimizations
# Custom -- build with custom configuration, set the optimization flag in CFLAGS
# 
# If CONFIG is manually edited, ensure to update or regenerate launch configurations 
# for your IDE.
CONFIG?=Debug


#Image Type can be BOOT or UPDATE. Default to BOOT.
# NOTE: This flag is used only for testing with the Edge Protect Bootloader. See README.md of Edge Protect Bootloader CE
# To overwrite default values, set IMG_TYPE?=UPDATE here or pass IMG_TYPE=UPDATE in commandline
IMG_TYPE?=BOOT

#Update type can be 'overwrite' or 'swap'
UPDATE_TYPE?=overwrite

    
#Sets the names of the application for OTA 	
APP_1_NAME=proj_cm33_s
APP_2_NAME=proj_cm33_ns
APP_3_NAME=proj_cm55
    
#Image version for Boot usecase
ifeq ($(IMG_TYPE),BOOT)
# Both overwrite and swap mode can use same JSON file genearted by bootloader personality 
#COMBINE_SIGN_JSON?=./bsps/TARGET_$(TARGET)/config/GeneratedSource/boot_with_bldr.json

#Sets the version of the application for OTA 	
APP_VERSION_MAJOR?=1
APP_VERSION_MINOR?=0
APP_VERSION_BUILD?=0

#Image version for update usecase
else ifeq ($(IMG_TYPE),UPDATE)

ifeq ($(UPDATE_TYPE),overwrite)
## Following 
COMBINE_SIGN_JSON?=./configs/update_with_epb_overwrite.json
else
COMBINE_SIGN_JSON?=configs/update_with_epb_swap.json
endif

#Sets the version of the application for OTA 	
APP_VERSION_MAJOR?=1
APP_VERSION_MINOR?=1
APP_VERSION_BUILD?=0

else
$(error Invalid IMG_TYPE. Please set it to either BOOT or UPDATE)
endif

# Building ifx-mcuboot with ARM compiler requries some
# specific symbols. However, linking when ifx-mcuboot is 
# added as a library to an existing project,
# those symbols are not required. Excluding those symbols, 
#which are not needed for this application build
ifeq ($(TOOLCHAIN),ARM)
  DEFINES+=MCUBOOT_SKIP_CLEANUP_RAM=1
endif

include ../common_app.mk
