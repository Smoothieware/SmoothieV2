#ifndef __XPD_UTILS_H_
#define __XPD_UTILS_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <xpd_common.h>

#if defined(USE_HAL_DRIVER)
__STATIC_INLINE void XPD_vDelay_ms(uint32_t ulMilliseconds)
{
    HAL_Delay(ulMilliseconds);
}

__STATIC_INLINE XPD_ReturnType XPD_eWaitForDiff(volatile uint32_t * pulVarAddress, uint32_t ulBitSelector,
        uint32_t ulMatch, uint32_t * pulTimeout)
{
    uint32_t tickstart = HAL_GetTick();

    while ((*pulVarAddress & ulBitSelector) == ulMatch)
    {
        if ((HAL_GetTick() - tickstart) > *pulTimeout)
        {
            return XPD_TIMEOUT;
        }
    }
    *pulTimeout -= HAL_GetTick() - tickstart;
    return XPD_OK;
}

__STATIC_INLINE XPD_ReturnType XPD_eWaitForMatch(volatile uint32_t * pulVarAddress, uint32_t ulBitSelector,
        uint32_t ulMatch, uint32_t * pulTimeout)
{
    uint32_t tickstart = HAL_GetTick();

    while ((*pulVarAddress & ulBitSelector) != ulMatch)
    {
        if ((HAL_GetTick() - tickstart) > *pulTimeout)
        {
            return XPD_TIMEOUT;
        }
    }
    *pulTimeout -= HAL_GetTick() - tickstart;
    return XPD_OK;
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* __XPD_UTILS_H_ */
