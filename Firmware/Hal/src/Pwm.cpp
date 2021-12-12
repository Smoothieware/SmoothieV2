#include "Pwm.h"

#include <cstring>
#include <cctype>
#include <tuple>
#include <vector>
#include <cmath>

#include "StringUtils.h"

#include "stm32h7xx_hal.h"
#include "Hal_pin.h"

// allocated timers and channels
// we have two timers and each has 4 channels
// each timer can have a different frequency
// channels on the same timer have the same frequency but different duty cycles
bool Pwm::allocated[2][4];
Pwm::instance_t Pwm::instances[2];

// name is PWM1_1 and PWM2_1, first is the timer second is the channel
Pwm::Pwm(const char *nm)
{
    // decode name
    name.assign(nm);
    if(name.size() != 6 || stringutils::toUpper(name.substr(0, 3)) != "PWM" || name[4] != '_') {
        printf("ERROR: illegal PWM name: %s\n", nm);
        return;
    }

    timr= strtol(name.substr(3, 1).c_str(), nullptr, 10) - 1;
    if(timr >= 2) {
        printf("ERROR: Illegal PWM timer: %d\n", timr);
        return;
    }
    channel= strtol(name.substr(5).c_str(), nullptr, 10) - 1;
    if(channel >= 4) {
        printf("ERROR: Illegal PWM channel: %d\n", channel);
        return;
    }

    // make sure it is not already in use
    if(allocated[timr][channel]) {
        printf("ERROR: PWM channel %s already allocated\n", nm);
        return;
    }

    if(instances[timr]._htim == nullptr) {
        printf("ERROR: PWM timer %s has not been setup\n", nm);
        return;
    }

    allocated[timr][channel]= true;
    valid= true;
}

Pwm::~Pwm()
{
    if(allocated[timr][channel]) {
        set(0);
        allocated[timr][channel]= false;
        // If we stop it here we can't start it again so leave it running
        // if(instances[timr]._htim != nullptr) {
        //     uint32_t tc;
        //     switch(channel) {
        //         case 0: tc= TIM_CHANNEL_1; break;
        //         case 1: tc= TIM_CHANNEL_2; break;
        //         case 2: tc= TIM_CHANNEL_3; break;
        //         case 3: tc= TIM_CHANNEL_4; break;
        //     }
        //     HAL_TIM_PWM_Stop((TIM_HandleTypeDef*)instances[timr]._htim, tc);
        // }
    }
}

// Set the duty cycle where 0.5 is 50% duty cycle
void Pwm::set(float v)
{
    if(!valid) return;

    if(v < 0) v = 0;
    else if(v > 1) v = 1;

    value = v;

    uint32_t tc;
    switch(channel) {
        case 0: tc =  TIM_CHANNEL_1; break;
        case 1: tc =  TIM_CHANNEL_2; break;
        case 2: tc =  TIM_CHANNEL_3; break;
        case 3: tc =  TIM_CHANNEL_4; break;
        default: return;
    }
    uint32_t pulse = roundf(instances[timr].period * v);
    __HAL_TIM_SET_COMPARE((TIM_HandleTypeDef*)instances[timr]._htim, tc, pulse);
    // _htim->Instance->CCR[1234] = pulse;
}

// set duty cycle such that the pulse width is the number of given microseconds
// returns duty cycle
float Pwm::set_microseconds(float v)
{
    // determine duty cycle based on frequency
    float frequency= instances[timr].frequency; // freq in Hz
    float p= 1E6 / frequency; // get period in microseconds
    if(v < 0) v= 0;
    if(p < v) v= p;
    float dc= v / p;
    set(dc); // set duty cycle
    return dc;
}

// this changes the frequency of an existing, running PWM timer
// Take care in using this as it changes the frequency of all running channels for this timer
void Pwm:: set_frequency(uint32_t freq)
{
    if(!valid) return;

    uint32_t clkhz = freq * 1000;
    uint32_t uhPrescalerValue = (uint32_t)(SystemCoreClock / (2 * clkhz)) - 1;
    uint32_t period_value = (uint32_t)((clkhz / freq) - 1); // Period Value

    // reset the prescaler
    __HAL_TIM_SET_PRESCALER((TIM_HandleTypeDef*)instances[timr]._htim, uhPrescalerValue);
    // reset the timer frequency
    __HAL_TIM_SET_AUTORELOAD((TIM_HandleTypeDef*)instances[timr]._htim, period_value);
    // update the local values
    instances[timr].frequency= freq;
    instances[timr].period= period_value;
    // we also need to reset the pulse width
    set(value);
}

// define TIM1 PWM channels
#define PWM1                           TIM1
#define PWM1_CLK_ENABLE()              __HAL_RCC_TIM1_CLK_ENABLE()

/* Definition for PWM1 Channel Pins */
#define PWM1_CHANNEL_GPIO_PORT()       __HAL_RCC_GPIOE_CLK_ENABLE();
#define PWM1_GPIO_PORT_CHANNEL1        GPIOE
#define PWM1_GPIO_PORT_CHANNEL2        GPIOE
#define PWM1_GPIO_PORT_CHANNEL3        GPIOE
#define PWM1_GPIO_PORT_CHANNEL4        GPIOE
#define PWM1_GPIO_PIN_CHANNEL1         GPIO_PIN_9
#define PWM1_GPIO_PIN_CHANNEL2         GPIO_PIN_11
#define PWM1_GPIO_PIN_CHANNEL3         GPIO_PIN_13
#define PWM1_GPIO_PIN_CHANNEL4         GPIO_PIN_14
#define PWM1_GPIO_AF_CHANNEL1          GPIO_AF1_TIM1
#define PWM1_GPIO_AF_CHANNEL2          GPIO_AF1_TIM1
#define PWM1_GPIO_AF_CHANNEL3          GPIO_AF1_TIM1
#define PWM1_GPIO_AF_CHANNEL4          GPIO_AF1_TIM1

// define TIM8 PWM channels
#define PWM2                           TIM8
#define PWM2_CLK_ENABLE()              __HAL_RCC_TIM8_CLK_ENABLE()

/* Definition for PWM2 Channel Pins */
#define PWM2_CHANNEL_GPIO_PORT()       __HAL_RCC_GPIOI_CLK_ENABLE();
#define PWM2_GPIO_PORT_CHANNEL1        GPIOI
#define PWM2_GPIO_PORT_CHANNEL2        GPIOI
#define PWM2_GPIO_PORT_CHANNEL3        GPIOI
#define PWM2_GPIO_PORT_CHANNEL4        GPIOI
#define PWM2_GPIO_PIN_CHANNEL1         GPIO_PIN_5
#define PWM2_GPIO_PIN_CHANNEL2         GPIO_PIN_6
#define PWM2_GPIO_PIN_CHANNEL3         GPIO_PIN_7
#define PWM2_GPIO_PIN_CHANNEL4         GPIO_PIN_2
#define PWM2_GPIO_AF_CHANNEL1          GPIO_AF3_TIM8
#define PWM2_GPIO_AF_CHANNEL2          GPIO_AF3_TIM8
#define PWM2_GPIO_AF_CHANNEL3          GPIO_AF3_TIM8
#define PWM2_GPIO_AF_CHANNEL4          GPIO_AF3_TIM8

// static
bool Pwm::setup(int timr, uint32_t freq)
{
    if(timr >= 2) return false;
    if(instances[timr]._htim != nullptr) {
        printf("ERROR: PWM%d instance already setup\n", timr+1);
        return false;
    }

    /* Initialize TIMx peripheral as follows:
    + Prescaler = (SystemCoreClock / (2*20000000)) - 1
    + Period = (1000 - 1)
    + ClockDivision = 0
    + Counter direction = Up
    */
    // Compute the prescaler value to have TIM1 counter clock capable of doing freq within 16 bits
    uint32_t clkhz = freq * 1000;
    uint32_t uhPrescalerValue = (uint32_t)(SystemCoreClock / (2 * clkhz)) - 1;
    uint32_t period_value = (uint32_t)((clkhz / freq) - 1); // Period Value
    printf("DEBUG: PWM%d setup frequency= %lu Hz, prsc= %lu, period= %lu\n", timr+1, freq, uhPrescalerValue, period_value);

    TIM_HandleTypeDef TimHandle{0};

    TimHandle.Instance = timr == 0 ? PWM1 : PWM2;
    TimHandle.Init.Prescaler         = uhPrescalerValue;
    TimHandle.Init.Period            = period_value;
    TimHandle.Init.ClockDivision     = 0;
    TimHandle.Init.CounterMode       = TIM_COUNTERMODE_UP;
    TimHandle.Init.RepetitionCounter = 0;

    void *htim = malloc(sizeof(TIM_HandleTypeDef));
    memcpy(htim, &TimHandle, sizeof(TIM_HandleTypeDef));
    instances[timr]._htim= htim;
    instances[timr].period= period_value;
    instances[timr].frequency= freq;

    return true;
}

// static
bool Pwm::post_config_setup()
{
    printf("DEBUG: PWM post config setup\n");

    // We need to know how many PWM timers are used and how many channels for each so we need this to be called after config

    // Common configuration for all channels
    TIM_OC_InitTypeDef sConfig{0};

    sConfig.OCMode       = TIM_OCMODE_PWM1;
    sConfig.OCPolarity   = TIM_OCPOLARITY_HIGH;
    sConfig.OCFastMode   = TIM_OCFAST_DISABLE;
    sConfig.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
    sConfig.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    sConfig.OCIdleState  = TIM_OCIDLESTATE_RESET;
    sConfig.Pulse = 0;

    for (int t = 0; t < 2; ++t) {
        if(instances[t]._htim == nullptr) continue;
        TIM_HandleTypeDef *htim= (TIM_HandleTypeDef*)instances[t]._htim;

        printf("DEBUG: PWM setting up PWM%d\n", t+1);
        if (HAL_TIM_PWM_Init(htim) != HAL_OK) {
            printf("ERROR: PWM%d Init failed\n", t+1);
            return false;
        }

        for (int ch = 0; ch < 4; ++ch) {
            if(!allocated[t][ch]) continue;
            printf("DEBUG: PWM setting up PWM%d_%d\n", t+1, ch+1);
            uint32_t tc;
            switch(ch) {
                case 0: tc= TIM_CHANNEL_1; break;
                case 1: tc= TIM_CHANNEL_2; break;
                case 2: tc= TIM_CHANNEL_3; break;
                case 3: tc= TIM_CHANNEL_4; break;
            }

            /* Set the pulse value for channel ch */
            if (HAL_TIM_PWM_ConfigChannel(htim, &sConfig, tc) != HAL_OK) {
                printf("ERROR: PWM%d_%d config failed\n", t+1, ch+1);
                return false;
            }

            /*##-3- Start PWM signals generation #######################################*/
            if (HAL_TIM_PWM_Start(htim, tc) != HAL_OK) {
                printf("ERROR: PWM%d_%d start failed\n", t+1, ch+1);
                return false;
            }
        }
    }
    return true;
}

/**
  * @brief TIM MSP Initialization
  *        This function configures the hardware resources used in this example:
  *           - Peripheral's clock enable
  *           - Peripheral's GPIO Configuration
  * @param htim: TIM handle pointer
  * @retval None
  */
extern "C" void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
{
    GPIO_InitTypeDef   GPIO_InitStruct;

    if(htim->Instance == PWM1) {
        PWM1_CLK_ENABLE();
        PWM1_CHANNEL_GPIO_PORT();

        /* Common configuration for all channels */
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

        if(Pwm::is_allocated(0, 0)) {
            GPIO_InitStruct.Alternate = PWM1_GPIO_AF_CHANNEL1;
            GPIO_InitStruct.Pin = PWM1_GPIO_PIN_CHANNEL1;
            HAL_GPIO_Init(PWM1_GPIO_PORT_CHANNEL1, &GPIO_InitStruct);
            allocate_hal_pin(PWM1_GPIO_PORT_CHANNEL1, PWM1_GPIO_PIN_CHANNEL1);
        }

        if(Pwm::is_allocated(0, 1)) {
            GPIO_InitStruct.Alternate = PWM1_GPIO_AF_CHANNEL2;
            GPIO_InitStruct.Pin = PWM1_GPIO_PIN_CHANNEL2;
            HAL_GPIO_Init(PWM1_GPIO_PORT_CHANNEL2, &GPIO_InitStruct);
            allocate_hal_pin(PWM1_GPIO_PORT_CHANNEL2, PWM1_GPIO_PIN_CHANNEL2);
        }

        if(Pwm::is_allocated(0, 2)) {
            GPIO_InitStruct.Alternate = PWM1_GPIO_AF_CHANNEL3;
            GPIO_InitStruct.Pin = PWM1_GPIO_PIN_CHANNEL3;
            HAL_GPIO_Init(PWM1_GPIO_PORT_CHANNEL3, &GPIO_InitStruct);
            allocate_hal_pin(PWM1_GPIO_PORT_CHANNEL3, PWM1_GPIO_PIN_CHANNEL3);
        }

        if(Pwm::is_allocated(0, 3)) {
            GPIO_InitStruct.Alternate = PWM1_GPIO_AF_CHANNEL4;
            GPIO_InitStruct.Pin = PWM1_GPIO_PIN_CHANNEL4;
            HAL_GPIO_Init(PWM1_GPIO_PORT_CHANNEL4, &GPIO_InitStruct);
            allocate_hal_pin(PWM1_GPIO_PORT_CHANNEL4, PWM1_GPIO_PIN_CHANNEL4);
        }

    } else if(htim->Instance == PWM2) {
        PWM2_CLK_ENABLE();
        PWM2_CHANNEL_GPIO_PORT();

        /* Common configuration for all channels */
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

        if(Pwm::is_allocated(1, 0)) {
            GPIO_InitStruct.Alternate = PWM2_GPIO_AF_CHANNEL1;
            GPIO_InitStruct.Pin = PWM2_GPIO_PIN_CHANNEL1;
            HAL_GPIO_Init(PWM2_GPIO_PORT_CHANNEL1, &GPIO_InitStruct);
            allocate_hal_pin(PWM2_GPIO_PORT_CHANNEL1, PWM2_GPIO_PIN_CHANNEL1);
        }

        if(Pwm::is_allocated(1, 1)) {
            GPIO_InitStruct.Alternate = PWM2_GPIO_AF_CHANNEL2;
            GPIO_InitStruct.Pin = PWM2_GPIO_PIN_CHANNEL2;
            HAL_GPIO_Init(PWM2_GPIO_PORT_CHANNEL2, &GPIO_InitStruct);
            allocate_hal_pin(PWM2_GPIO_PORT_CHANNEL2, PWM2_GPIO_PIN_CHANNEL2);
        }

        if(Pwm::is_allocated(1, 2)) {
            GPIO_InitStruct.Alternate = PWM2_GPIO_AF_CHANNEL3;
            GPIO_InitStruct.Pin = PWM2_GPIO_PIN_CHANNEL3;
            HAL_GPIO_Init(PWM2_GPIO_PORT_CHANNEL3, &GPIO_InitStruct);
            allocate_hal_pin(PWM2_GPIO_PORT_CHANNEL3, PWM2_GPIO_PIN_CHANNEL3);
        }

        if(Pwm::is_allocated(1, 3)) {
            GPIO_InitStruct.Alternate = PWM2_GPIO_AF_CHANNEL4;
            GPIO_InitStruct.Pin = PWM2_GPIO_PIN_CHANNEL4;
            HAL_GPIO_Init(PWM2_GPIO_PORT_CHANNEL4, &GPIO_InitStruct);
            allocate_hal_pin(PWM2_GPIO_PORT_CHANNEL4, PWM2_GPIO_PIN_CHANNEL4);
        }
    }
}
