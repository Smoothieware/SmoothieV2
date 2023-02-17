// Handle a H/W Timer Quadrature Encoder (Mainly for Lathe spindle sync)

#include "stm32h7xx.h"

#include <stdio.h>

#include "Hal_pin.h"

// Definition for QE_TIM resources
#define QE_TIM                TIM8
#define QE_TIM_CLK_ENABLE     __HAL_RCC_TIM8_CLK_ENABLE
#define QE_TIM_CLK_DISABLE    __HAL_RCC_TIM8_CLK_DISABLE

#define QE_CH1_GPIO_PIN       GPIO_PIN_8
#define QE_CH1_GPIO_PORT      GPIOJ
#define QE_CH1_GPIO_AF        GPIO_AF3_TIM8
#define QE_CH1_CLK_ENABLE     __HAL_RCC_GPIOJ_CLK_ENABLE

#define QE_CH2_GPIO_PIN       GPIO_PIN_10
#define QE_CH2_GPIO_PORT      GPIOJ
#define QE_CH2_GPIO_AF        GPIO_AF3_TIM8
#define QE_CH2_CLK_ENABLE     __HAL_RCC_GPIOJ_CLK_ENABLE

/**
* @brief TIM_Encoder MSP Initialization
* This function configures the hardware resources used in this example
* @param htim_encoder: TIM_Encoder handle pointer
* @retval None
*/
extern "C"
void HAL_TIM_Encoder_MspInit(TIM_HandleTypeDef* htim_encoder)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if(htim_encoder->Instance == QE_TIM) {
        /* Peripheral clock enable */
        QE_TIM_CLK_ENABLE();
        QE_CH1_CLK_ENABLE();
        QE_CH2_CLK_ENABLE();

        /** QE_TIM GPIO Configuration
            PJ8 and PJ10 for TIM8
            NOTE if different ports then need to do both explicitly
        */
        GPIO_InitStruct.Pin = QE_CH1_GPIO_PIN | QE_CH2_GPIO_PIN;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Alternate = QE_CH1_GPIO_AF;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        HAL_GPIO_Init(QE_CH1_GPIO_PORT, &GPIO_InitStruct);

        allocate_hal_pin(QE_CH1_GPIO_PORT, QE_CH1_GPIO_PIN);
        allocate_hal_pin(QE_CH2_GPIO_PORT, QE_CH2_GPIO_PIN);
    }

}

/**
* @brief TIM_Encoder MSP De-Initialization
* This function freeze the hardware resources used in this example
* @param htim_encoder: TIM_Encoder handle pointer
* @retval None
*/
extern "C"
void HAL_TIM_Encoder_MspDeInit(TIM_HandleTypeDef* htim_encoder)
{
    if(htim_encoder->Instance == QE_TIM) {
        /* Peripheral clock disable */
        QE_TIM_CLK_DISABLE();
        HAL_GPIO_DeInit(QE_CH1_GPIO_PORT, QE_CH1_GPIO_PIN | QE_CH2_GPIO_PIN);
    }

}

static TIM_HandleTypeDef htimqe;

static bool MX_QE_TIM_Init(void)
{
    TIM_Encoder_InitTypeDef sConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};

    htimqe.Instance = QE_TIM;
    htimqe.Init.Prescaler = 0;
    htimqe.Init.CounterMode = TIM_COUNTERMODE_UP;
    htimqe.Init.Period = 65535;
    htimqe.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htimqe.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
    sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
    sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
    sConfig.IC1Filter = 10;
    sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
    sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
    sConfig.IC2Filter = 10;

    if (HAL_TIM_Encoder_Init(&htimqe, &sConfig) != HAL_OK) {
        return false;
    }

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htimqe, &sMasterConfig) != HAL_OK) {
        return false;
    }

    return true;
}

bool setup_quadrature_encoder()
{
    if(!MX_QE_TIM_Init()) {
        printf("ERROR: QuadEncoder - unable to setup quadrature timer\n");
        return false;
    }

    if(HAL_TIM_Encoder_Start(&htimqe, TIM_CHANNEL_ALL) != HAL_OK) {
        printf("ERROR: QuadEncoder - unable to start quadrature timer\n");
        return false;
    }

    return true;
}

int32_t get_quadrature_encoder_max_count()
{
    return 65535;
}

int32_t read_quadrature_encoder()
{
    static uint16_t last= 0;
    uint16_t cnt= QE_TIM->CNT;
    uint16_t c= cnt;

    // handle wrap around as cnt is 16 bits
    if(cnt < last) c= (65536 - last) + cnt;
    last= cnt;
    return (int16_t)c >> 2;

/*
        Uint16 Encoder :: getRPM(void)
        {
            if(ENCODER_REGS.QFLG.bit.UTO==1)       // If unit timeout (one 10Hz period)
            {
                Uint32 current = ENCODER_REGS.QPOSLAT;
                Uint32 count = (current > previous) ? current - previous : previous - current;

                // deal with over/underflow
                if( count > _ENCODER_MAX_COUNT/2 ) {
                    count = _ENCODER_MAX_COUNT - count; // just subtract from max value
                }

                rpm = count * 60 * RPM_CALC_RATE_HZ / ENCODER_RESOLUTION;

                previous = current;
                ENCODER_REGS.QCLR.bit.UTO=1;       // Clear interrupt flag
            }

            return rpm;
        }

        // if the feed or direction changed, reset sync to avoid a big step
        if( feed != previousFeed || feedDirection != previousFeedDirection) {
            stepperDrive->setCurrentPosition(desiredSteps);
        }

*/

}
