#include "stm32h7xx_hal.h"
#include <stdio.h>

/* Defines related to Clock configuration */
/* Uncomment to enable the adequate Clock Source */
#define RTC_CLOCK_SOURCE_LSE
/*#define RTC_CLOCK_SOURCE_LSI*/

#ifdef RTC_CLOCK_SOURCE_LSI
#define RTC_ASYNCH_PREDIV    0x7F
#define RTC_SYNCH_PREDIV     0x0F9
#endif

#ifdef RTC_CLOCK_SOURCE_LSE
#define RTC_ASYNCH_PREDIV  0x7F
#define RTC_SYNCH_PREDIV   0x00FF
#endif

static RTC_HandleTypeDef RtcHandle;
static int rtc_ok;

int rtc_init()
{
    /*##-1- Configure the RTC peripheral #######################################*/
    RtcHandle.Instance = RTC;

    /* Configure RTC prescaler and RTC data registers */
    /* RTC configured as follows:
        - Hour Format    = Format 24
        - Asynch Prediv  = Value according to source clock
        - Synch Prediv   = Value according to source clock
        - OutPut         = Output Disable
        - OutPutPolarity = High Polarity
        - OutPutType     = Open Drain */
    RtcHandle.Init.HourFormat     = RTC_HOURFORMAT_24;
    RtcHandle.Init.AsynchPrediv   = RTC_ASYNCH_PREDIV;
    RtcHandle.Init.SynchPrediv    = RTC_SYNCH_PREDIV;
    RtcHandle.Init.OutPut         = RTC_OUTPUT_DISABLE;
    RtcHandle.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    RtcHandle.Init.OutPutType     = RTC_OUTPUT_TYPE_OPENDRAIN;
    HAL_StatusTypeDef res= HAL_RTC_Init(&RtcHandle);
    assert_param(res == HAL_OK);
    if(res != HAL_OK) {
        return 0;
    }
    return 1;
}

// set time params are 24 hour
int rtc_setdatetime(uint8_t year, uint8_t month, uint8_t day, uint8_t weekday, uint8_t hour, uint8_t minute, uint8_t seconds)
{
    RTC_DateTypeDef  sdatestructure;
    RTC_TimeTypeDef  stimestructure;

    /*##-1- Configure the Date #################################################*/
    /* Set Date: Tuesday February 2nd 2017 */
    sdatestructure.Year = year;
    sdatestructure.Month = month;
    sdatestructure.Date = day;
    sdatestructure.WeekDay = weekday;

    HAL_StatusTypeDef res= HAL_RTC_SetDate(&RtcHandle, &sdatestructure, RTC_FORMAT_BIN);
    assert_param(res == HAL_OK);
    if(res != HAL_OK) {
        return 0;
    }

    /*##-2- Configure the Time #################################################*/
    /* Set Time: 02:20:00 */
    stimestructure.Hours = hour;
    stimestructure.Minutes = minute;
    stimestructure.Seconds = seconds;
    stimestructure.DayLightSaving = RTC_DAYLIGHTSAVING_NONE ;
    stimestructure.StoreOperation = RTC_STOREOPERATION_RESET;

    res= HAL_RTC_SetTime(&RtcHandle, &stimestructure, RTC_FORMAT_BIN);
    assert_param(res == HAL_OK);
    if(res != HAL_OK) {
        return 0;
    }

    rtc_ok= 1;
    return 1;
}

/**
  * @brief  Display the current time.
  * @param  showtime : pointer to buffer
  * @retval None
  */
void rtc_get_datetime(char *buf, size_t len)
{
    RTC_DateTypeDef sdatestructureget;
    RTC_TimeTypeDef stimestructureget;

    /* Get the RTC current Time */
    HAL_RTC_GetTime(&RtcHandle, &stimestructureget, RTC_FORMAT_BIN);
    /* Get the RTC current Date */
    HAL_RTC_GetDate(&RtcHandle, &sdatestructureget, RTC_FORMAT_BIN);
    /* Display time Format : YYYY-MM-DD hh:mm:ss */
    snprintf(buf, len, "20%02d-%02d-%02d %02d:%02d:%02d",
        sdatestructureget.Year, sdatestructureget.Month, sdatestructureget.Date,
        stimestructureget.Hours, stimestructureget.Minutes, stimestructureget.Seconds);
}

/*
    bit31:25 Year origin from the 1980 (0..127, e.g. 37 for 2017)
    bit24:21 Month (1..12)
    bit20:16 Day of the month (1..31)
    bit15:11 Hour (0..23)
    bit10:5 Minute (0..59)
    bit4:0 Second / 2 (0..29, e.g. 25 for 50)
*/
unsigned long get_fattime (void)
{
    unsigned long dttm= 0;
    RTC_DateTypeDef sdate;
    RTC_TimeTypeDef stime;
    if(rtc_ok) {
        HAL_RTC_GetTime(&RtcHandle, &stime, RTC_FORMAT_BIN);
        HAL_RTC_GetDate(&RtcHandle, &sdate, RTC_FORMAT_BIN);
    }else{
        sdate.Year= 2021-2000;
        sdate.Month= 5;
        sdate.Date= 18;
        stime.Hours= 18;
        stime.Minutes= 20;
        stime.Seconds= 1;
    }

    uint16_t yr= 2000+sdate.Year-1980;
    dttm= (yr<<25) | ((sdate.Month&0x0F)<<21) | ((sdate.Date&0x1F)<<16) |
          ((stime.Hours&0x1F)<<11) | ((stime.Minutes&0x3F)<<5) | (stime.Seconds&0x1F);
    return dttm;
}

void HAL_RTC_MspInit(RTC_HandleTypeDef *hrtc)
{
    RCC_OscInitTypeDef        RCC_OscInitStruct;
    RCC_PeriphCLKInitTypeDef  PeriphClkInitStruct;

    /*##-1- Enables access to the backup domain ######*/
    /* To enable access on RTC registers */
    HAL_PWR_EnableBkUpAccess();
    /*##-2- Configure LSE/LSI as RTC clock source ###############################*/
#ifdef RTC_CLOCK_SOURCE_LSE
    RCC_OscInitStruct.OscillatorType =  RCC_OSCILLATORTYPE_LSI | RCC_OSCILLATORTYPE_LSE;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    RCC_OscInitStruct.LSEState = RCC_LSE_ON;
    RCC_OscInitStruct.LSIState = RCC_LSI_OFF;
    HAL_StatusTypeDef res= HAL_RCC_OscConfig(&RCC_OscInitStruct);
    assert_param(res == HAL_OK);
    (void)res;

    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
    res= HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);
    assert_param(res == HAL_OK);

    /* Configures the External Low Speed oscillator (LSE) drive capability */
    __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_HIGH);
#elif defined (RTC_CLOCK_SOURCE_LSI)
    RCC_OscInitStruct.OscillatorType =  RCC_OSCILLATORTYPE_LSI | RCC_OSCILLATORTYPE_LSE;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    RCC_OscInitStruct.LSIState = RCC_LSI_ON;
    RCC_OscInitStruct.LSEState = RCC_LSE_OFF;
    HAL_StatusTypeDef res= HAL_RCC_OscConfig(&RCC_OscInitStruct);
    assert_param(res == HAL_OK);

    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
    res= HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);
    assert_param(res == HAL_OK);

#else
#error Please select the RTC Clock source inside the main.h file
#endif /*RTC_CLOCK_SOURCE_LSE*/

    /*##-3- Enable RTC peripheral Clocks #######################################*/
    /* Enable RTC Clock */
    __HAL_RCC_RTC_ENABLE();

}

/**
  * @brief RTC MSP De-Initialization
  *        This function frees the hardware resources used in this example:
  *          - Disable the Peripheral's clock
  * @param hrtc: RTC handle pointer
  * @retval None
  */
void HAL_RTC_MspDeInit(RTC_HandleTypeDef *hrtc)
{
    /*##-1- Reset peripherals ##################################################*/
    __HAL_RCC_RTC_DISABLE();
}
