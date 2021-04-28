#include <stdio.h>
#include <math.h>

#include "FreeRTOS.h"

#include "stm32h7xx.h"

// TODO move ramfunc define to a utils.h
#define _ramfunc_ __attribute__ ((section(".ramfunctions"),long_call,noinline))

static void (*tick_handler)();
static void (*untick_handler)();
static void (*fasttick_handler)();

// Definition for STEP_TIM and UNSTEP_TIM clock resources
#define STEP_TIM                             TIM3
#define STEP_TIM_CLK_ENABLE                  __HAL_RCC_TIM3_CLK_ENABLE
#define STEP_TIM_IRQn                        TIM3_IRQn
#define STEP_TIM_IRQHandler                  TIM3_IRQHandler
static TIM_HandleTypeDef StepTimHandle;

#define UNSTEP_TIM                             TIM4
#define UNSTEP_TIM_CLK_ENABLE                  __HAL_RCC_TIM4_CLK_ENABLE
#define UNSTEP_TIM_IRQn                        TIM4_IRQn
#define UNSTEP_TIM_IRQHandler                  TIM4_IRQHandler
static TIM_HandleTypeDef UnStepTimHandle;

#define FASTTICK_TIM                             TIM2
#define FASTTICK_TIM_CLK_ENABLE                  __HAL_RCC_TIM2_CLK_ENABLE
#define FASTTICK_TIM_IRQn                        TIM2_IRQn
#define FASTTICK_TIM_IRQHandler                  TIM2_IRQHandler
static TIM_HandleTypeDef FastTickTimHandle;

// frequency in HZ, delay in microseconds
int steptimer_setup(uint32_t frequency, uint32_t delay, void *step_handler, void *unstep_handler)
{
    /* Enable TIM clock */
    STEP_TIM_CLK_ENABLE();
    UNSTEP_TIM_CLK_ENABLE();

    // Set STEP_TIM instance
    StepTimHandle.Instance = STEP_TIM;

    // Set STEP_TIM peripheral clock rate
    uint32_t timerFreq = 20000000; // 20MHz
    printf("DEBUG: STEP_TIM input clock rate= %lu\n", timerFreq);

    /* Compute the prescaler value to have STEP_TIM counter clock equal to 20MHz */
    uint32_t uwPrescalerValue = (uint32_t) (SystemCoreClock / (2 * timerFreq)) - 1;

    // step tick period
    uint32_t period1 = timerFreq / frequency;
    StepTimHandle.Init.Period = period1 - 1; // set period to trigger interrupt
    StepTimHandle.Init.Prescaler = uwPrescalerValue;
    StepTimHandle.Init.ClockDivision = 0;
    StepTimHandle.Init.CounterMode = TIM_COUNTERMODE_UP;
    if (HAL_TIM_Base_Init(&StepTimHandle) != HAL_OK) {
        printf("ERROR: steptimer_setup failed to init steptimer\n");
        return 0;
    }

    // Start the TIM Base generation in interrupt mode
    if (HAL_TIM_Base_Start_IT(&StepTimHandle) != HAL_OK) {
        printf("ERROR: steptimer_setup failed to start steptimer\n");
        return 0;
    }
    printf("DEBUG: STEP_TIM period=%lu, interrupt rate=%lu Hz, pulse width=%f us\n", period1, timerFreq / period1, ((float)period1 * 1000000) / timerFreq);
    // the inaccuracy of the frequency if it does not exactly divide the frequency
    printf("DEBUG: innaccuracy of step timer: %lu\n", timerFreq % period1);

    // calculate ideal period for unstep interrupt
    uint32_t f = 1000000 / delay; // convert to frequency
    uint32_t delay_period = timerFreq / f; // delay is in us
    printf("DEBUG: UNSTEP_TIM period=%lu, pulse width=%f us\n", delay_period, ((float)delay_period * 1000000) / timerFreq);

    // Set UNSTEP_TIM instance
    UnStepTimHandle.Instance = UNSTEP_TIM;

    UnStepTimHandle.Init.Period = delay_period - 1;
    UnStepTimHandle.Init.Prescaler = uwPrescalerValue;
    UnStepTimHandle.Init.ClockDivision = 0;
    UnStepTimHandle.Init.CounterMode = TIM_COUNTERMODE_UP;
    if (HAL_TIM_Base_Init(&UnStepTimHandle) != HAL_OK) {
        printf("ERROR: steptimer_setup failed to init unsteptimer\n");
        return 0;
    }
    // don't start it here

    // setup the upstream handlers for each interrupt
    tick_handler = step_handler;
    untick_handler = unstep_handler;

    /* Set the priority of the STEP_TIM interrupt vector */
    NVIC_SetPriority(STEP_TIM_IRQn, 0);
    NVIC_EnableIRQ(STEP_TIM_IRQn);
    NVIC_ClearPendingIRQ(STEP_TIM_IRQn);
    NVIC_SetPriority(UNSTEP_TIM_IRQn, 0);
    NVIC_EnableIRQ(UNSTEP_TIM_IRQn);
    NVIC_ClearPendingIRQ(UNSTEP_TIM_IRQn);

    return 1;
}

/**
  * @brief  TIM period elapsed callback
  * @param  htim: TIM handle
  * @retval None
  */
extern void TIM6_PeriodElapsedCallback();

_ramfunc_ void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef * htim)
{
    if(htim->Instance == STEP_TIM) {
        tick_handler();

    }else if(htim->Instance == UNSTEP_TIM) {
        // call upstream handler
        untick_handler();
        // stop the timer as it is a oneshot
        HAL_TIM_Base_Stop_IT(&UnStepTimHandle);
        // reset the count for next time
        __HAL_TIM_SET_COUNTER(&UnStepTimHandle, 0);
        // Or this apparently will stop interrupts and reset counter
        //UnStepTickerTimHandle.Instance->CR1 |= (TIM_CR1_UDIS);
        //UnStepTickerTimHandle.Instance->EGR |= (TIM_EGR_UG);

    }else if(htim->Instance == FASTTICK_TIM) {
        fasttick_handler();

    } else if(htim->Instance == TIM6) {
        // defined in stm32h7xx_hal_timebase_tim.c
        TIM6_PeriodElapsedCallback();
    }
}

_ramfunc_ void STEP_TIM_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&StepTimHandle);
}

_ramfunc_ void UNSTEP_TIM_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&UnStepTimHandle);
}


void steptimer_stop()
{
    HAL_TIM_Base_Stop_IT(&StepTimHandle);
    NVIC_DisableIRQ(STEP_TIM_IRQn);
    HAL_TIM_Base_Stop_IT(&UnStepTimHandle);
    NVIC_DisableIRQ(UNSTEP_TIM_IRQn);
    HAL_TIM_Base_DeInit(&StepTimHandle);
    HAL_TIM_Base_DeInit(&UnStepTimHandle);
}

// called from within STEP_TIM ISR so must be in SRAM
_ramfunc_ void unsteptimer_start()
{
    __HAL_TIM_SET_COUNTER(&UnStepTimHandle, 0);
    __HAL_TIM_CLEAR_IT(&UnStepTimHandle, TIM_IT_UPDATE);
    NVIC_ClearPendingIRQ(UNSTEP_TIM_IRQn);
    HAL_StatusTypeDef stat= HAL_TIM_Base_Start_IT(&UnStepTimHandle);
    if(stat != HAL_OK) {
        // printf("ERROR: unsteptimer_start failed to start unsteptimer\n");
    }
    // Or this apparently will restart interrupts
    //UnStepTickerTimHandle.Instance->CR1 &= ~(TIM_CR1_UDIS);
}

_ramfunc_ void FASTTICK_TIM_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&FastTickTimHandle);
}

static uint32_t fasttick_timerFreq;
// frequency in HZ
int fasttick_setup(uint32_t frequency, void *timer_handler)
{
    /* Enable TIM clock */
    FASTTICK_TIM_CLK_ENABLE();

    // Set FASTTICK_TIM instance
    FastTickTimHandle.Instance = FASTTICK_TIM;

    // Get FASTTICK_TIM peripheral clock rate
    fasttick_timerFreq= 1000000; // 1MHz
    printf("DEBUG: FASTTICK_TIM input clock rate= %lu\n", fasttick_timerFreq);

    /* Compute the prescaler value to have FASTTICK_TIM counter clock equal to 1MHz */
    uint32_t uwPrescalerValue = (uint32_t) (SystemCoreClock / (2 * fasttick_timerFreq)) - 1;

    // fast tick period
    uint32_t period1 = fasttick_timerFreq / frequency;
    FastTickTimHandle.Init.Period = period1 - 1; // set period to trigger interrupt
    FastTickTimHandle.Init.Prescaler = uwPrescalerValue;
    FastTickTimHandle.Init.ClockDivision = 0;
    FastTickTimHandle.Init.CounterMode = TIM_COUNTERMODE_UP;
    if (HAL_TIM_Base_Init(&FastTickTimHandle) != HAL_OK) {
        printf("ERROR: fasttick_setup failed to init fasttick timer\n");
        return 0;
    }

    if (HAL_TIM_Base_Start_IT(&FastTickTimHandle) != HAL_OK) {
        printf("ERROR: fasttick_setup failed to start fasttick timer\n");
        return 0;
    }

    printf("DEBUG: FASTTICK_TIM period=%lu, interrupt rate=%lu Hz, pulse width=%f us\n", period1, fasttick_timerFreq / period1, ((float)period1 * 1000000) / fasttick_timerFreq);

    // setup the upstream handler
    fasttick_handler = timer_handler;

    // Set the priority of the TMR1 interrupt vector
    NVIC_SetPriority(FASTTICK_TIM_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    // Enable the timer interrupt at the NVIC
    NVIC_EnableIRQ(FASTTICK_TIM_IRQn);
    NVIC_ClearPendingIRQ(FASTTICK_TIM_IRQn);

    return 1;
}

int fasttick_set_frequency(uint32_t frequency)
{
    // setup new tick period
    uint32_t period = fasttick_timerFreq / frequency;
    HAL_TIM_Base_Stop_IT(&FastTickTimHandle);
    NVIC_DisableIRQ(FASTTICK_TIM_IRQn);
    __HAL_TIM_SET_COUNTER(&FastTickTimHandle, 0);
    __HAL_TIM_CLEAR_IT(&FastTickTimHandle, TIM_IT_UPDATE);
    NVIC_ClearPendingIRQ(FASTTICK_TIM_IRQn);
    FastTickTimHandle.Instance->ARR = period;
    // Generate an update event to reload the Prescaler
    FastTickTimHandle.Instance->EGR = TIM_EGR_UG;

    if (HAL_TIM_Base_Start_IT(&FastTickTimHandle) != HAL_OK) {
        printf("ERROR: fasttick_set_frequency failed to restart fasttick timer\n");
        return 0;
    }

    NVIC_EnableIRQ(FASTTICK_TIM_IRQn);

    printf("DEBUG: FASTTICK_TIM new period=%lu cycles; interrupt rate=%lu Hz\n", period, fasttick_timerFreq / period);
    return 1;
}

void fasttick_stop()
{
    HAL_TIM_Base_Stop_IT(&FastTickTimHandle);
    NVIC_DisableIRQ(FASTTICK_TIM_IRQn);
    HAL_TIM_Base_DeInit(&FastTickTimHandle);
}

