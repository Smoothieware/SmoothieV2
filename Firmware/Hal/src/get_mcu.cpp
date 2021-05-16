#include <string>
#include <cstdint>

#include "stm32h7xx_hal.h"

static const struct stm32h7xx_rev {
    uint32_t rev_id;
    char revision;
} stm32h7xx_revisions[] = {
    { 0x1000, 'A' },
    { 0x1001, 'Z' },
    { 0x1003, 'Y' },
    { 0x2001, 'X' },
    { 0x2003, 'V' }
};

std::string get_mcu()
{
    std::string mcu;
    uint32_t rev_id = HAL_GetREVID();
    uint32_t dev_id = HAL_GetDEVID();

    if(dev_id == 0x450) {
        mcu.append("STM32H743/745");
        char rev = '?';
        for (size_t i = 0;
             i < sizeof(stm32h7xx_revisions) / sizeof(struct stm32h7xx_rev); i++) {
            /* Check for matching revision */
            if (stm32h7xx_revisions[i].rev_id == rev_id) {
                rev = stm32h7xx_revisions[i].revision;
            }
        }
        mcu.append(" Revision ").append(1, rev);
    } else {
        mcu.append("Unknown STM32H7 (%04X) may not be supported\n", dev_id);
    }

    return mcu;
}

extern "C" void get_uid(uint32_t *buf, size_t len)
{
    if(len >= 1) {
        buf[0]= HAL_GetUIDw0();
    }
    if(len >= 2) {
        buf[1]= HAL_GetUIDw1();
    }
    if(len >= 3) {
        buf[2]= HAL_GetUIDw2();
    }
}
