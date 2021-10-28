#pragma once
#include <cstdint>

class Pin;

bool Board_LED_Init();
bool Board_LED_Assign(uint8_t, const char *pin);
void Board_LED_Toggle(uint8_t);
void Board_LED_Set(uint8_t, bool);
bool Board_LED_Test(uint8_t);

