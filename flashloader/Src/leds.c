#include "stm32h7xx_hal.h"

typedef struct {GPIO_TypeDef* port; uint16_t pin; int invert; } LEDS_t;

#if defined(BOARD_PRIME)
#define NLEDS 4
static LEDS_t LEDS[NLEDS] = {
	{GPIOH, GPIO_PIN_9, 0},
	{GPIOH, GPIO_PIN_10, 0},
	{GPIOH, GPIO_PIN_11, 0},
	{GPIOH, GPIO_PIN_12, 0}
};

#define LEDS_GPIO_CLK_ENABLE()  {__HAL_RCC_GPIOH_CLK_ENABLE(); }

#elif BOARD_NUCLEO
#define NLEDS 3
static LEDS_t LEDS[NLEDS] = {
	{GPIOB, GPIO_PIN_0, 0},
	{GPIOE, GPIO_PIN_1, 0},
	{GPIOB, GPIO_PIN_14, 0}
};
#define LEDS_GPIO_CLK_ENABLE()  {__HAL_RCC_GPIOB_CLK_ENABLE(); __HAL_RCC_GPIOE_CLK_ENABLE(); }

#elif defined(BOARD_DEVEBOX)
#define NLEDS 1
static LEDS_t LEDS[NLEDS] = { {GPIOA, GPIO_PIN_1, 1} };
#define LEDS_GPIO_CLK_ENABLE()  {__HAL_RCC_GPIOA_CLK_ENABLE();}

#else
#error unrecognized board
#endif

void Board_LED_Init()
{
	GPIO_InitTypeDef  gpio_init_structure = {0};

	LEDS_GPIO_CLK_ENABLE();
	for (int i = 0; i < NLEDS; ++i) {
		gpio_init_structure.Pin   = LEDS[i].pin;
		gpio_init_structure.Mode  = GPIO_MODE_OUTPUT_PP;
		gpio_init_structure.Pull  = GPIO_NOPULL;
		gpio_init_structure.Speed = GPIO_SPEED_LOW;
		HAL_GPIO_Init(LEDS[i].port, &gpio_init_structure);
		HAL_GPIO_WritePin(LEDS[i].port, LEDS[i].pin, LEDS[i].invert ? GPIO_PIN_SET : GPIO_PIN_RESET);
	}
}


void Board_LED_Toggle(uint8_t led)
{
	if(led >= NLEDS) return;
	HAL_GPIO_TogglePin(LEDS[led].port, LEDS[led].pin);
}

void Board_LED_Set(uint8_t led, int on)
{
	if(led >= NLEDS) return;
	HAL_GPIO_WritePin(LEDS[led].port, LEDS[led].pin, (on ^ LEDS[led].invert) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

int Board_LED_Test(uint8_t led)
{
	if(led >= NLEDS) return 0;
	return (int)HAL_GPIO_ReadPin(LEDS[led].port, LEDS[led].pin) ^ LEDS[led].invert;
}

