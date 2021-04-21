#include "../Unity/src/unity.h"
#include "TestRegistry.h"

#include "Pin.h"
#include "Hal_pin.h"
#include "stm32h7xx_hal.h"

#include "FreeRTOS.h"
#include "task.h"

REGISTER_TEST(PinTest, test_allocation)
{
	Pin p("PB2");
	TEST_ASSERT_TRUE(allocate_hal_pin(GPIOB, GPIO_PIN_3));
	TEST_ASSERT_FALSE(allocate_hal_pin(GPIOB, GPIO_PIN_2));
	TEST_ASSERT_FALSE(allocate_hal_pin(GPIOB, GPIO_PIN_3));
	Pin::set_allocated('B', 2, false);
	Pin::set_allocated('B', 3, false);
}

REGISTER_TEST(PinTest, flashleds)
{
	int cnt = 0;
	printf("defining pins...\n");
#ifdef BOARD_NUCLEO
	Pin myleds[] = {
		Pin("PB0"),
		Pin("PE.1"),
		Pin("PB_14"),
	};
	Pin button("PC13v");
	TEST_ASSERT_NOT_NULL(button.as_input());
	if(button.connected()) {
 		printf("Set input pin %s\n", button.to_string().c_str());
	}else{
		printf("Button was invalid\n");
	}
#else
	#error unrecognized board
#endif

	printf("set as outputs... \n");
	for(auto& p : myleds) {
		cnt++;
		if(p.as_output() == nullptr) {
			printf("Failed to allocate pin %d, %s\n", cnt, p.to_string().c_str());
			TEST_FAIL();
		} else {
			p.set(false);
			printf("Set output pin %d, %s\n", cnt, p.to_string().c_str());
		}
	}

	printf("Running...\n");

	TickType_t delayms= pdMS_TO_TICKS(100);
	cnt = 0;
	while(!button.get() && cnt < 100) {
		uint8_t m = 1;
		for(auto& p : myleds) {
			p.set(cnt & m);
			m <<= 1;
		}
		cnt++;
		vTaskDelay(delayms);
		if(button.get()) {
			printf("Button pressed\n");
		}
	}
	if(cnt < 100) printf("user stopped\n");

	// deinit the pins and deallocate
	for(auto& p : myleds) {
		p.deinit();
	}
	button.deinit();

	printf("Done\n");
}

static volatile int button_pressed= 0;
static void test_button_int()
{
	button_pressed= 1;
}

REGISTER_TEST(PinTest, interrupt_pin)
{
#ifdef BOARD_NUCLEO
	Pin button("PC13v");
#else
	#error unrecognized board
#endif

	TEST_ASSERT_NOT_NULL(button.as_interrupt(test_button_int));
	if(button.connected()) {
 		printf("Set input pin %s\n", button.to_string().c_str());
	}else{
		printf("Button was invalid\n");
	}

	{
		// Test that we cannot have another interrupt on a pin with the same pin number
		Pin dummy("PD13");
		TEST_ASSERT_TRUE(dummy.connected());
		TEST_ASSERT_NULL(dummy.as_interrupt(test_button_int));
		TEST_ASSERT_FALSE(dummy.connected());
	}

	printf("Press button to test...\n");

	int cnt = 0;
	while(!button_pressed && ++cnt < 100) {
		vTaskDelay(pdMS_TO_TICKS(100));
	}
	if(cnt < 100) printf("test passed\n");
	else printf("test timed out\n");

}
