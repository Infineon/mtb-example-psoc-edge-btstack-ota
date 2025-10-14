[Click here](../README.md) to view the README.

## Design and implementation

The over-the-air (OTA) update feature enables devices (mobile phones, vehicles, and other IoT devices) to download and update their software/firmware using a wireless network connection. OTA updates let you deploy firmware updates to one or more devices in your fleet. This application is designed to perform an OTA firmware update over a Bluetooth&reg; interface. 

The OTA library works with an intermediate peer application on a laptop (or phone) to push the image to the device.

> **Note:**  :  *Makefile*'s `define OTA_BT_SUPPORT = 1` is set to use the Bluetooth&reg; protocol

Application image can be stored in two slots on the device:

1. Primary slot (BOOT) : stores the currently executing application
2. Secondary slot (UPDATE) : stores the new application being downloaded

Bluetooth&reg; updates use a "push" model. Upon reset/boot, the device advertises that it can handle an update, and a host (laptop, phone, etc.) is used to connect and "push" the update to the device. The main user application enables BTstack, gets BT callbacks, and calls into the appropriate OTA update APIs.

In this BTstack OTA model, the device waits for the BT host (peer) to initiate a connection and OTA update.

The host application (e.g., WsOtaUpgrade.exe) pushes the OTA image to the device via Bluetooth&reg;. The device stores OTA image to the secondary slot and marks secondary slot as ready.

Upon reset, the Edge Protect Bootloader (running on device) performs an OTA update by overwriting or swapping the update image from the secondary slot (slot 1) to the primary slot (slot 0).

Overwrite or swap is defined at compile time for the Edge Protect Bootloader and cannot be updated in the field.

![](images/ota-ble-download.png)

**Table 1. Application projects**

Project | Description
--------|------------------------
*proj_cm33_s* | Project for CM33 secure processing environment (SPE)
*proj_cm33_ns* | Project for CM33 non-secure processing environment (NSPE)
*proj_cm55* | CM55 project

<br>
In this code example, at device reset, the secure boot process starts from the ROM boot with the secure enclave (SE) as the root of trust (RoT). From the secure enclave, the boot flow is passed on to the system CPU subsystem, where the Edge Protect Bootloader is the first application to be started. The Edge Protect Bootloader then verifies and launces the secure CM33 application. After all necessary secure configurations, the flow is passed on to the non-secure CM33 application. Resource initialization for this example is performed by this CM33 non-secure project. It intializes the OTA configrations and starts the OTA firmware update task using FreeRTOS. It then enables the CM55 core using the `Cy_SysEnableCM55()` function. 


<br />