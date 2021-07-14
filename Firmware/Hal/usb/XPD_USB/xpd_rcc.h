#ifndef __XPD_RCC_H_
#define __XPD_RCC_H_

#include <xpd_common.h>

#if defined(USE_HAL_DRIVER)
#define RCC_POS_USB_EN          __HAL_RCC_USB_CLK_ENABLE
#define RCC_POS_USB_DIS         __HAL_RCC_USB_CLK_DISABLE
#define RCC_POS_OTG_FS_EN       __HAL_RCC_USB_OTG_FS_CLK_ENABLE
#define RCC_POS_OTG_FS_DIS      __HAL_RCC_USB_OTG_FS_CLK_DISABLE
#define RCC_POS_OTG_HS_EN       __HAL_RCC_USB_OTG_HS_CLK_ENABLE
#define RCC_POS_OTG_HS_DIS      __HAL_RCC_USB_OTG_HS_CLK_DISABLE
#define RCC_POS_USBPHYC_EN      __HAL_RCC_OTGPHYC_CLK_ENABLE
#define RCC_POS_USBPHYC_DIS     __HAL_RCC_OTGPHYC_CLK_DISABLE
#define RCC_POS_OTG_HS_ULPI_EN  __HAL_RCC_USB_OTG_HS_ULPI_CLK_ENABLE
#define RCC_POS_OTG_HS_ULPI_DIS __HAL_RCC_USB_OTG_HS_ULPI_CLK_DISABLE

#define RCC_vClockEnable(A)     A##_EN()
#define RCC_vClockDisable(A)    A##_DIS()
#define RCC_CLKFREQ_HZ_HCLK     HAL_RCC_GetHCLKFreq

#elif defined(USE_STDPERIPH_DRIVER)
#if defined(STM32F4XX)
#define RCC_POS_OTG_FS_FN(CMD)       RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_OTG_FS, CMD)
#define RCC_POS_OTG_HS_FN(CMD)       RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_OTG_HS, CMD)
#define RCC_POS_OTG_HS_ULPI_FN(CMD)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_OTG_HS_ULPI, CMD)
#endif

#define RCC_vClockEnable(A)     A##_FN(ENABLE)
#define RCC_vClockDisable(A)    A##_FN(DISABLE)

__STATIC_INLINE uint32_t RCC_CLKFREQ_HZ_HCLK(void)
{
    RCC_ClocksTypeDef RCC_Clocks;
    RCC_GetClocksFreq(&RCC_Clocks);
    return RCC_Clocks.HCLK_Frequency;
}
#endif

#define RCC_CLKFREQ_HZ_HSE      HSE_VALUE
#define RCC_ulClockFreq_Hz(A)   RCC_CLKFREQ_HZ_##A()

#define RCC_ulOscFreq_Hz(X)     X##_VALUE

#endif /* __XPD_RCC_H_ */
