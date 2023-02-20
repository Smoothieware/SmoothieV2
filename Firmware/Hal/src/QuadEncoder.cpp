// Handle a H/W Timer Quadrature Encoder (Mainly for Lathe spindle sync)

#include "stm32h7xx.h"

#include <stdio.h>

#include "Hal_pin.h"

#define USE_ENCODER_HW

#ifdef USE_ENCODER_HW
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
    sConfig.EncoderMode = TIM_ENCODERMODE_TI1; //TIM_ENCODERMODE_TI12;
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

uint32_t get_quadrature_encoder_max_count()
{
    return 65535;
}

uint32_t get_quadrature_encoder_div()
{
    return 2;
}

uint32_t read_quadrature_encoder()
{
    return QE_TIM->CNT;
}

#else

// Enable this to emit codes twice per step.
//#define HALF_STEP

#define R_START 0x0

#ifdef HALF_STEP
// Use the half-step state table (emits a code at 00 and 11)
#define R_CCW_BEGIN 0x1
#define R_CW_BEGIN 0x2
#define R_START_M 0x3
#define R_CW_BEGIN_M 0x4
#define R_CCW_BEGIN_M 0x5
const unsigned char ttable[6][4] = {
  // R_START (00)
  {R_START_M,            R_CW_BEGIN,     R_CCW_BEGIN,  R_START},
  // R_CCW_BEGIN
  {R_START_M | DIR_CCW, R_START,        R_CCW_BEGIN,  R_START},
  // R_CW_BEGIN
  {R_START_M | DIR_CW,  R_CW_BEGIN,     R_START,      R_START},
  // R_START_M (11)
  {R_START_M,            R_CCW_BEGIN_M,  R_CW_BEGIN_M, R_START},
  // R_CW_BEGIN_M
  {R_START_M,            R_START_M,      R_CW_BEGIN_M, R_START | DIR_CW},
  // R_CCW_BEGIN_M
  {R_START_M,            R_CCW_BEGIN_M,  R_START_M,    R_START | DIR_CCW},
};

#else

// Use the full-step state table (emits a code at 00 only)
#define R_CW_FINAL 0x1
#define R_CW_BEGIN 0x2
#define R_CW_NEXT 0x3
#define R_CCW_BEGIN 0x4
#define R_CCW_FINAL 0x5
#define R_CCW_NEXT 0x6

// Values returned by 'process'
// No complete step yet.
#define DIR_NONE 0x0
// Clockwise step.
#define DIR_CW 0x10
// Anti-clockwise step.
#define DIR_CCW 0x20

const uint8_t ttable[7][4] = {
  // R_START
  {R_START,    R_CW_BEGIN,  R_CCW_BEGIN, R_START},
  // R_CW_FINAL
  {R_CW_NEXT,  R_START,     R_CW_FINAL,  R_START | DIR_CW},
  // R_CW_BEGIN
  {R_CW_NEXT,  R_CW_BEGIN,  R_START,     R_START},
  // R_CW_NEXT
  {R_CW_NEXT,  R_CW_BEGIN,  R_CW_FINAL,  R_START},
  // R_CCW_BEGIN
  {R_CCW_NEXT, R_START,     R_CCW_BEGIN, R_START},
  // R_CCW_FINAL
  {R_CCW_NEXT, R_CCW_FINAL, R_START,     R_START | DIR_CCW},
  // R_CCW_NEXT
  {R_CCW_NEXT, R_CCW_FINAL, R_CCW_BEGIN, R_START},
};
#endif

#include "Pin.h"

class Rotary
{
  public:
    Rotary();
    uint8_t process();
    uint8_t state;
    Pin *pin1;
    Pin *pin2;
};

Rotary::Rotary() {
  // Initialise state.
  state = R_START;
}

uint8_t Rotary::process() {
  // Grab state of input pins.
  uint8_t pinstate = (pin2->get()?2:0) | (pin1->get()?1:0);
  // Determine new state from the pins and state table.
  state = ttable[state & 0xf][pinstate];
  // Return emit bits, ie the generated event.
  return state & 0x30;
}

static Rotary enc;
static uint16_t qe_cnt;

static void handle_en_irq()
{
    uint8_t result = enc.process();
    if (result == DIR_CW) {
        ++qe_cnt;
    } else if (result == DIR_CCW) {
        --qe_cnt;
    }
}

bool setup_quadrature_encoder()
{
    enc.pin1= new Pin("PJ8^");
    enc.pin1->as_interrupt(handle_en_irq, Pin::CHANGE);
    if(!enc.pin1->connected()) {
        printf("Cannot set interrupt pin %s\n", enc.pin1->to_string().c_str());
        delete enc.pin1;
        return false;
    }
    enc.pin2= new Pin("PJ10^");
    enc.pin2->as_interrupt(handle_en_irq, Pin::CHANGE);
    if(!enc.pin2->connected()) {
        printf("Cannot set interrupt pin %s\n", enc.pin1->to_string().c_str());
        delete enc.pin1;
        delete enc.pin2;
        return false;
    }
    qe_cnt= 0;
    return true;
}

uint32_t get_quadrature_encoder_max_count()
{
    //return 0xFFFFFFFF;
    return 0xFFFFUL;
}

uint32_t get_quadrature_encoder_div()
{
    return 1;
}

uint32_t read_quadrature_encoder()
{
    return qe_cnt;
}

#endif
