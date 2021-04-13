#include <stdio.h>
#include <math.h>

#include "FreeRTOS.h"

#include "stm32h7xx.h"

// TODO move ramfunc define to a utils.h
//#define _ramfunc_ __attribute__ ((section(".ramfunctions"),long_call,noinline))
#define _ramfunc_

static void (*tick_handler)();
static void (*untick_handler)();
// the period for the unstep tick
static uint32_t delay_period;

// Definition for STEP_TIM and UNSTEP_TIM clock resources
#define STEP_TIM                             TIM3
#define STEP_TIM_CLK_ENABLE                  __HAL_RCC_TIM3_CLK_ENABLE
#define STEP_TIM_IRQn                        TIM3_IRQn
#define STEP_TIM_IRQHandler                  TIM3_IRQHandler
TIM_HandleTypeDef StepTimHandle;

#define UNSTEP_TIM                             TIM4
#define UNSTEP_TIM_CLK_ENABLE                  __HAL_RCC_TIM4_CLK_ENABLE
#define UNSTEP_TIM_IRQn                        TIM4_IRQn
#define UNSTEP_TIM_IRQHandler                  TIM4_IRQHandler
TIM_HandleTypeDef UnStepTimHandle;

// frequency in HZ, delay in microseconds
int steptimer_setup(uint32_t frequency, uint32_t delay, void *step_handler, void *unstep_handler)
{
    /* Enable TIM clock */
    STEP_TIM_CLK_ENABLE();
    UNSTEP_TIM_CLK_ENABLE();

    // Set STEP_TIM instance
    StepTimHandle.Instance = STEP_TIM;

    // Get STEP_TIM peripheral clock rate
    uint32_t timerFreq = 20000000; // 20MHz
    printf("DEBUG: STEP_TIM input clock rate= %lu\n", timerFreq);

    /* Compute the prescaler value to have STEP_TIM counter clock equal to 5MHz */
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
    /* ##-2- Start the TIM Base generation in interrupt mode #################### */
    /* Start Channel1 */
    if (HAL_TIM_Base_Start_IT(&StepTimHandle) != HAL_OK) {
        printf("ERROR: steptimer_setup failed to start steptimer\n");
        return 0;
    }
    printf("DEBUG: STEP_TIM period=%lu, interrupt rate=%lu Hz, pulse width=%f us\n", period1, timerFreq / period1, ((float)period1 * 1000000) / timerFreq);
    // the inaccuracy of the frequency if it does not exactly divide the frequency
    printf("DEBUG: innaccuracy of step timer: %lu\n", timerFreq % period1);

    // calculate ideal period for unstep interrupt
    uint32_t f = 1000000 / delay; // convert to frequency
    delay_period = timerFreq / f; // delay is in us
    printf("DEBUG: UNSTEP_TIM period=%lu, pulse width=%f us\n", delay_period, ((float)delay_period * 1000000) / timerFreq);

    // Set STEP_TIM instance
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
    NVIC_SetPriority(UNSTEP_TIM_IRQn, 1);
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

#if 0
static void (*tmr1_handler)();
_ramfunc_ void TIMER1_IRQHandler(void)
{
    if (Chip_TIMER_MatchPending(LPC_TIMER1, 0)) {
        Chip_TIMER_ClearMatch(LPC_TIMER1, 0);
        tmr1_handler();
    }
}

static uint32_t tmr1_timerFreq;
// frequency in HZ
int tmr1_setup(uint32_t frequency, void *timer_handler)
{
    /* Enable timer 1 clock and reset it */
    Chip_TIMER_Init(LPC_TIMER1);
    Chip_RGU_TriggerReset(RGU_TIMER1_RST);
    while (Chip_RGU_InReset(RGU_TIMER1_RST)) {}

    /* Get timer 1 peripheral clock rate */
    tmr1_timerFreq = Chip_Clock_GetRate(CLK_MX_TIMER1);
    printf("TMR1 clock rate= %lu\n", tmr1_timerFreq);

    /* Timer setup for match and interrupt at TICKRATE_HZ */
    Chip_TIMER_Reset(LPC_TIMER1);
    Chip_TIMER_MatchEnableInt(LPC_TIMER1, 0);

    // setup step tick period
    uint32_t period = tmr1_timerFreq / frequency;
    Chip_TIMER_SetMatch(LPC_TIMER1, 0, period);
    Chip_TIMER_ResetOnMatchEnable(LPC_TIMER1, 0);
    Chip_TIMER_Enable(LPC_TIMER1);

    printf("TMR1 MR0 period=%lu cycles; interrupt rate=%lu Hz\n", period, tmr1_timerFreq / period);

    // setup the upstream handler
    tmr1_handler = timer_handler;

    /* Set the priority of the TMR1 interrupt vector */
    NVIC_SetPriority(TIMER1_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    /* Enable the timer interrupt at the NVIC and at TMR1 */
    NVIC_EnableIRQ(TIMER1_IRQn);
    NVIC_ClearPendingIRQ(TIMER1_IRQn);

    // return the inaccuracy of the frequency if it does not exactly divide the frequency
    return tmr1_timerFreq % period;
}

int tmr1_set_frequency(uint32_t frequency)
{
    Chip_TIMER_Disable(LPC_TIMER1);
    NVIC_DisableIRQ(TIMER1_IRQn);
    NVIC_ClearPendingIRQ(TIMER1_IRQn);
    Chip_TIMER_Reset(LPC_TIMER1);
    Chip_TIMER_ClearMatch(LPC_TIMER1, 0);

    // setup new tick period
    uint32_t period = tmr1_timerFreq / frequency;
    Chip_TIMER_SetMatch(LPC_TIMER1, 0, period);
    Chip_TIMER_MatchEnableInt(LPC_TIMER1, 0);
    Chip_TIMER_ResetOnMatchEnable(LPC_TIMER1, 0);
    Chip_TIMER_Enable(LPC_TIMER1);

    printf("TMR1 new MR0 period=%lu cycles; interrupt rate=%lu Hz\n", period, tmr1_timerFreq / period);
    /* Enable the timer interrupt at the NVIC and at TMR1 */
    NVIC_EnableIRQ(TIMER1_IRQn);
    return tmr1_timerFreq % period;
}

void tmr1_stop()
{
    Chip_TIMER_Disable(LPC_TIMER1);
    NVIC_DisableIRQ(TIMER1_IRQn);
}
#endif
