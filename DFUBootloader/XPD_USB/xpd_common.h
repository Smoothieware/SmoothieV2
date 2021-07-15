/**
  ******************************************************************************
  * @file    xpd_common.h
  * @author  Benedek Kupper
  * @version 0.2
  * @date    2018-01-28
  * @brief   STM32 eXtensible Peripheral Drivers Common Module
  *
  * Copyright (c) 2018 Benedek Kupper
  *
  * Licensed under the Apache License, Version 2.0 (the "License");
  * you may not use this file except in compliance with the License.
  * You may obtain a copy of the License at
  *
  *     http://www.apache.org/licenses/LICENSE-2.0
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  */
#ifndef __XPD_COMMON_H_
#define __XPD_COMMON_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <xpd_config.h>

/** @defgroup Common
 * @{ */

/** @defgroup Common_Exported_Types Common Exported Types
 * @{ */

/** @brief XPD functions return type */
typedef enum
{
    XPD_OK       = 0, /*!< Operation finished successfully */
    XPD_ERROR    = 1, /*!< Operation encountered an error */
    XPD_BUSY     = 2, /*!< Operation exited because it was already in progress */
    XPD_TIMEOUT  = 3  /*!< Operation timed out */
}XPD_ReturnType;

/**
 * @brief Function pointer type for binary control function reference
 * @param NewState: the state to set
 */
typedef void ( *XPD_CtrlCallbackType )      ( FunctionalState NewState );

/**
 * @brief Callback function pointer type with no parameters
 */
typedef void ( *XPD_SimpleCallbackType )    ( void );

/**
 * @brief Callback function pointer type with numeric parameter
 * @param Value: provided data for the callback
 */
typedef void ( *XPD_ValueCallbackType )    ( uint32_t Value );

/**
 * @brief Callback function pointer type with handle pointer parameter
 * @param Handle: pointer of the callback sender handle
 */
typedef void ( *XPD_HandleCallbackType )    ( void * Handle );

/** @} */

/** @defgroup Common_Exported_Macros Common Exported Macros
 * @{ */

/**
 * @brief  Safe function pointer caller that checks it against NULL and calls it with parameters.
 * @param  CALLBACK: the function pointer
 * @param  PARAMETERS: the required parameters of the function
 */
#define XPD_SAFE_CALLBACK(CALLBACK, ...)        \
    do{ if ((CALLBACK) != NULL) (void) CALLBACK(__VA_ARGS__); }while(0)

/** @} */

/** @} */

/* Inherited macros */

#define SET_BIT(REG, BIT)     ((REG) |= (BIT))

#define CLEAR_BIT(REG, BIT)   ((REG) &= ~(BIT))

#ifdef MODIFY_REG
#undef MODIFY_REG
#endif
#define MODIFY_REG(REG, CLEARMASK, SETMASK)  \
    ((REG) = (((REG) & (~(CLEARMASK))) | ((SETMASK) & (CLEARMASK))))

#define POSITION_VAL(VAL)     (__CLZ(__RBIT(VAL)))

#define ARRAY_SIZE(arr)       (sizeof(arr) / sizeof((arr)[0]))

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type,member)))

#if  defined ( __GNUC__ )
#ifndef __weak
#define __weak   __attribute__((weak))
#endif /* __weak */
#ifndef __packed
#define __packed __attribute__((__packed__))
#endif /* __packed */
#endif /* __GNUC__ */

/* Macro to get variable aligned on 4-bytes,
 * for __ICCARM__ the directive "#pragma data_alignment=4" must be used instead */
#if defined   (__GNUC__)        /* GNU Compiler */
#ifndef __ALIGN_END
#define __ALIGN_END    __attribute__ ((aligned (4)))
#endif /* __ALIGN_END */
#ifndef __ALIGN_BEGIN
#define __ALIGN_BEGIN
#endif /* __ALIGN_BEGIN */
#else
#ifndef __ALIGN_END
#define __ALIGN_END
#endif /* __ALIGN_END */
#ifndef __ALIGN_BEGIN
#if defined   (__CC_ARM)      /* ARM Compiler */
#define __ALIGN_BEGIN    __align(4)
#elif defined (__ICCARM__)    /* IAR Compiler */
#define __ALIGN_BEGIN
#endif /* __CC_ARM */
#endif /* __ALIGN_BEGIN */
#endif /* __GNUC__ */

#ifndef PACKED
#if defined (__ICCARM__)
#define PACKED(X) __packed X
#else
#define PACKED(X) X __packed
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* __XPD_COMMON_H_ */
