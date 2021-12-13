#include "../Unity/src/unity.h"
#include "TestRegistry.h"

#include "Pin.h"
#include "stm32h7xx_hal.h"

#include "FreeRTOS.h"
#include "task.h"

#ifdef BOARD_PRIME

REGISTER_TEST(FETTest, turn_on)
{
    printf("defining fet pins...\n");
    Pin fets[] = {
        Pin("PE0"),
        Pin("PB8"),
        Pin("PE3"),
        Pin("PE1"),
        Pin("PI11"),
        Pin("PI4!"),
        Pin("PB7!"),
    };
    Pin button("PJ14");
    Pin fet_enable("PF14!");

    TEST_ASSERT_TRUE(button.as_input());
    if(button.connected()) {
        printf("Set input pin MSC %s\n", button.to_string().c_str());
    }else{
        printf("Button was invalid\n");
        TEST_FAIL();
    }

    TEST_ASSERT_TRUE(fet_enable.as_output());
    if(fet_enable.connected()) {
        printf("Set fet enable pin %s\n", fet_enable.to_string().c_str());
        fet_enable.set(true);
    }else{
        printf("fet enable pin was invalid\n");
        TEST_FAIL();
    }

    const char *fetname[] = {
        "HE_A", "HE_B", "BED", "FAN_A", "FAN_B", "SSR1", "SSR2"
    };

    int cnt = 0;
    bool ok= true;
    for(auto& p : fets) {
        if(!p.as_output()) {
            printf("Failed to allocate pin %s, %s\n", fetname[cnt], p.to_string().c_str());
            ok= false;
        } else {
            p.set(false); // init as off
            printf("Set pin %s, %s\n", fetname[cnt], p.to_string().c_str());
        }
        ++cnt;
    }

    if(!ok) TEST_FAIL();

    printf("Running, Press MSC button to test next FET...\n");

    uint32_t tmo = pdMS_TO_TICKS(2000);
    cnt= 0;
    for(auto& p : fets) {
        printf("Testing FET %s\n", fetname[cnt]);
        p.set(true);
        uint32_t to= pdMS_TO_TICKS(xTaskGetTickCount() + tmo);
        while(!button.get()) {
            vTaskDelay(100);
            if(pdMS_TO_TICKS(xTaskGetTickCount()) > to) break;
        }

        p.set(false);

        while(button.get()) {
            vTaskDelay(100);
        }
        ++cnt;
    }

    // printf("DeInit pins\n");
    // // deinit the pins and deallocate
    // for(auto& p : fets) {
    //     p.deinit();
    // }
    // button.deinit();

    printf("Done\n");
}

// test vfet disable PF14
// test VFET power disable and detection

#endif
