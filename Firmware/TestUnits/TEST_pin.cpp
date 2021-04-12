#include "../Unity/src/unity.h"
#include "TestRegistry.h"

#include "Pin.h"

#include "FreeRTOS.h"
#include "task.h"

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
	button.as_input();
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

	printf("Done\n");
}
