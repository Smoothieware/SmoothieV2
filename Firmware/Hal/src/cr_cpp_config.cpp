#include <stdlib.h>
#include <stdio.h>

#include "MemoryPool.h"

MemoryPool *_DTCMRAM;
MemoryPool *_SRAM_1;
// Called very early from ResetISR()
extern "C" void setup_memory_pool()
{
    // MemoryPool stuff - needs to be initialised before __libc_init_array
    // so static ctors can use them
    extern uint8_t __dtcm_bss_end;
    extern uint8_t __top_DTCMRAM;
    extern uint8_t __sram_1_end;
    extern uint8_t __top_SRAM_1;

    // alocate remaining area to memory pool
    _DTCMRAM= new MemoryPool(&__dtcm_bss_end, &__top_DTCMRAM - &__dtcm_bss_end);
    _SRAM_1= new MemoryPool(&__sram_1_end, &__top_SRAM_1 - &__sram_1_end);
}

void *operator new(size_t size)
{
    return malloc(size);
}

void *operator new[](size_t size)
{
    return malloc(size);
}

__attribute__ ((weak)) void operator delete(void *p)
{
    free(p);
}

extern "C" int __aeabi_atexit(void *object,
		void (*destructor)(void *),
		void *dso_handle)
{
	return 0;
}

#ifdef CPP_NO_HEAP
extern "C" void *malloc(size_t) {
	return (void *)0;
}

extern "C" void free(void *) {
}
#endif

#ifndef CPP_USE_CPPLIBRARY_TERMINATE_HANDLER
/******************************************************************
 * __verbose_terminate_handler()
 *
 * This is the function that is called when an uncaught C++
 * exception is encountered. The default version within the C++
 * library prints the name of the uncaught exception, but to do so
 * it must demangle its name - which causes a large amount of code
 * to be pulled in. The below minimal implementation can reduce
 * code size noticeably. Note that this function should not return.
 ******************************************************************/
namespace __gnu_cxx {
void __verbose_terminate_handler()
{
  ::puts("FATAL: Uncaught exception\n");
  __asm("bkpt #0");
  while(1) ;
}
}
extern "C" void __wrap___aeabi_unwind_cpp_pr0() {}
extern "C" void __wrap___aeabi_unwind_cpp_pr1() {}
extern "C" void __wrap___aeabi_unwind_cpp_pr2() {}
extern "C" void __cxa_pure_virtual()
{
  __asm("bkpt #0");
  for(;;);
}
#endif

