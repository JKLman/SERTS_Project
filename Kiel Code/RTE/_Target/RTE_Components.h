
/*
 * Auto generated Run-Time-Environment Component Configuration File
 *      *** Do not modify ! ***
 *
 * Project: 'BaseProject' 
 * Target:  'Target' 
 */

#ifndef RTE_COMPONENTS_H
#define RTE_COMPONENTS_H


/*
 * Define the Device Header File: 
 */
#define CMSIS_device_header "stm32f4xx.h"

#define RTE_CMSIS_RTOS                  /* CMSIS-RTOS */
        #define RTE_CMSIS_RTOS_RTX              /* CMSIS-RTOS Keil RTX */
#define RTE_Compiler_IO_File            /* Compiler I/O: File */
          #define RTE_Compiler_IO_File_FS         /* Compiler I/O: File (File System) */
#define RTE_DEVICE_FRAMEWORK_CLASSIC
#define RTE_DEVICE_HAL_COMMON
#define RTE_DEVICE_HAL_CORTEX
#define RTE_DEVICE_HAL_DMA
#define RTE_DEVICE_HAL_GPIO
#define RTE_DEVICE_HAL_I2C
#define RTE_DEVICE_HAL_I2S
#define RTE_DEVICE_HAL_PWR
#define RTE_DEVICE_HAL_RCC
#define RTE_DEVICE_HAL_SPI
#define RTE_DEVICE_HAL_USART
#define RTE_DEVICE_STARTUP_STM32F4XX    /* Device Startup for STM32F4 */
#define RTE_Drivers_USART1              /* Driver USART1 */
        #define RTE_Drivers_USART2              /* Driver USART2 */
        #define RTE_Drivers_USART3              /* Driver USART3 */
        #define RTE_Drivers_USART4              /* Driver USART4 */
        #define RTE_Drivers_USART5              /* Driver USART5 */
        #define RTE_Drivers_USART6              /* Driver USART6 */
        #define RTE_Drivers_USART7              /* Driver USART7 */
        #define RTE_Drivers_USART8              /* Driver USART8 */
        #define RTE_Drivers_USART9              /* Driver USART9 */
        #define RTE_Drivers_USART10             /* Driver USART10 */
#define RTE_Drivers_USBH0               /* Driver USBH0 */
#define RTE_FileSystem_Core             /* File System Core */
          #define RTE_FileSystem_LFN              /* File System with Long Filename support */
          #define RTE_FileSystem_Release          /* File System Release Version */
#define RTE_FileSystem_Drive_USB_0      /* File System USB Drive 0 */
#define RTE_USB_Core                    /* USB Core */
          #define RTE_USB_Core_Release            /* USB Core Release Version */
#define RTE_USB_Host_0                  /* USB Host 0 */
#define RTE_USB_Host_MSC                /* USB Host MSC */

#endif /* RTE_COMPONENTS_H */
