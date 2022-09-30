#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "stm32h7xx_hal.h"

#include "xpd_usb.h"
#include "usb_device.h"
#include "vcom_if.h"
#include "stm32_rom_dfu.h"

#include "Hal_pin.h"

// called externally to read/write to the USB CDC channel
// Expects entire buffer to be fully written
// vcom_write will copy the buffer or as much of it as it can
// so we need to stay here until all requested data has been transfered to the buffer
size_t write_cdc(uint8_t i, const char *buf, size_t len)
{
	size_t sent = 0;
	size_t tmo= 0;
	while(sent < len) {
		int n = vcom_write(i, (uint8_t *)buf + sent, len - sent);
		if(n < 0) {
			// we got an error
			printf("ERROR: write_cdc %d got an error\n", i);
			return 0;
		}
		sent += n;
		if(sent < len) {
			vTaskDelay(pdMS_TO_TICKS(10));
			// we need to timeout here if the port was not open anymore
			if(n == 0) {
				tmo += 10;
				if(tmo > 100) return sent; // arbitrary 100 ms timeout
			}else{
				tmo= 0;
			}
		}
	}

	return len;
}

// will block until data is available
size_t read_cdc(uint8_t i, char *buf, size_t len)
{
	int n= vcom_read(i, (uint8_t *)buf, len);
	if(n < 0) {
		printf("ERROR: read_cdc %d got an error\n", i);
		return 0;
	}
	return (size_t)n;
}

void HAL_USB_OTG_FS_MspInit(void* handle)
{
	GPIO_InitTypeDef GPIO_InitStruct;

	__HAL_RCC_GPIOA_CLK_ENABLE();

	/* Configure DM DP Pins */
	GPIO_InitStruct.Pin = (GPIO_PIN_11 | GPIO_PIN_12);
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF10_OTG2_FS;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/* Configure VBUS Pin */
	// GPIO_InitStruct.Pin = GPIO_PIN_9;
	// GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	// GPIO_InitStruct.Pull = GPIO_NOPULL;
	// HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/* Configure ID pin */
	GPIO_InitStruct.Pin = GPIO_PIN_10;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Alternate = GPIO_AF10_OTG2_FS;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	// allocate_hal_pin(GPIOA, GPIO_PIN_9);
	allocate_hal_pin(GPIOA, GPIO_PIN_10);
	allocate_hal_pin(GPIOA, GPIO_PIN_11);
	allocate_hal_pin(GPIOA, GPIO_PIN_12);

	/* Enable USB FS Clocks */
	__HAL_RCC_USB2_OTG_FS_CLK_ENABLE();

	__HAL_RCC_USB2_OTG_FS_CLK_SLEEP_ENABLE();
	/* Disable USB clock during CSleep mode */
	__HAL_RCC_USB2_OTG_FS_ULPI_CLK_SLEEP_DISABLE();

	/*  enable USB interrupts */
	// must be lower priority (higher number) than hal systick and sd interrupts
	NVIC_SetPriority(OTG_FS_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY+1);
	NVIC_EnableIRQ(OTG_FS_IRQn);
}

void HAL_USB_OTG_FS_MspDeInit(void* handle)
{
	/* Disable USB FS Clocks */
	__HAL_RCC_USB2_OTG_FS_CLK_DISABLE();
	HAL_NVIC_DisableIRQ(OTG_FS_IRQn);
}

void OTG_FS_IRQHandler(void)
{
	USB_vIRQHandler(UsbDevice);
}

void HAL_USBD_Setup(void)
{
	USB_INST2HANDLE(UsbDevice, USB_OTG_FS);
	UsbDevice->Callbacks.DepInit = HAL_USB_OTG_FS_MspInit;
	UsbDevice->Callbacks.DepDeinit = HAL_USB_OTG_FS_MspDeInit;
}

int setup_cdc()
{
	HAL_PWREx_EnableUSBVoltageDetector();

	HAL_USBD_Setup();
	STM32_ROM_DFU_Init();
	UsbDevice_Init();

	return 1;
}

void shutdown_cdc()
{
	UsbDevice_DeInit();
	HAL_Delay(250);
}


