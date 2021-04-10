#include "../Unity/src/unity.h"
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <iostream>

#include "TestRegistry.h"

#include "OutputStream.h"
#include "benchmark_timer.h"

#include "FreeRTOS.h"
#include "task.h"

REGISTER_TEST(MemoryTest, stats)
{
    struct mallinfo mem= mallinfo();
    printf("             total       used       free    largest\n");
    printf("Mem:   %11d%11d%11d%11d\n", mem.arena, mem.uordblks, mem.fordblks, mem.usmblks);
}

#if 0
char test_ram2_bss[128] __attribute__ ((section (".bss.$RAM2")));
// RAM3 is used by USB CDC so it is not fully available
//char test_ram3_bss[128] __attribute__ ((section (".bss.$RAM3")));
__attribute__ ((section (".data.$RAM4"))) char test_ram4_data[8]= {1,2,3,4,5,6,7,8};
__attribute__ ((section (".data.$RAM5"))) char test_ram5_data[4]= {9,8,7,6};

REGISTER_TEST(MemoryTest, other_rams)
{
    TEST_ASSERT_EQUAL_INT(0x10080000, (unsigned int)&test_ram2_bss);
    //TEST_ASSERT_EQUAL_INT(0x20000000, (unsigned int)&test_ram3_bss);
    TEST_ASSERT_EQUAL_INT(0x20008000, (unsigned int)&test_ram4_data);
    TEST_ASSERT_EQUAL_INT(0x2000C000, (unsigned int)&test_ram5_data);

    // check bss was cleared
    for (int i = 0; i < 128; ++i) {
        TEST_ASSERT_EQUAL_INT(0, test_ram2_bss[i]);
        //TEST_ASSERT_EQUAL_INT(0, test_ram3_bss[i]);
    }

    // check data areas were copied
    for (int i = 0; i < 8; ++i) {
        TEST_ASSERT_EQUAL_INT(i+1, test_ram4_data[i]);
    }

    for (int i = 0; i < 4; ++i) {
        TEST_ASSERT_EQUAL_INT(9-i, test_ram5_data[i]);
    }
}

class DummyClass
{
public:
    DummyClass() { printf("I'm at %p\n", this); }
    ~DummyClass() { printf("I'm being dtord\n"); }
    DummyClass *getInstance() { return this; }
    int t{1234};
};

#include "MemoryPool.h"
extern uint8_t __end_bss_RAM2;
extern uint8_t __top_RAM2;

REGISTER_TEST(MemoryTest, memory_pool)
{
    MemoryPool RAM2= MemoryPool(&__end_bss_RAM2, &__top_RAM2 - &__end_bss_RAM2);
    MemoryPool *_RAM2= &RAM2;

    OutputStream os(&std::cout);
    uint32_t ef= &__top_RAM2 - &__end_bss_RAM2;
    _RAM2->debug(os);
    // check amount of memory available
    TEST_ASSERT_EQUAL_INT(ef, _RAM2->available());
    TEST_ASSERT_EQUAL_INT(0x12000-128, ef);
    uint8_t *r2= (uint8_t *)_RAM2->alloc(128);
    _RAM2->debug(os);
    TEST_ASSERT_NOT_NULL(r2);
    // check it lost expected amount + 4 byte overhead
    TEST_ASSERT_EQUAL_INT(0x10080000+128+4, (unsigned int)r2);
    TEST_ASSERT_TRUE(_RAM2->has(r2));
    TEST_ASSERT_EQUAL_INT(ef-128-4, _RAM2->available());
    _RAM2->dealloc(r2);
    // check it deallocated it
    TEST_ASSERT_EQUAL_INT(ef, _RAM2->available());

    // test placement new
    DummyClass *r3= new(*_RAM2)DummyClass();
    TEST_ASSERT_EQUAL_INT(0x10080084, (unsigned int)r3->getInstance());
    TEST_ASSERT_EQUAL_INT(ef-sizeof(DummyClass)-4, _RAM2->available());
    TEST_ASSERT_EQUAL_INT(1234, r3->t);
    delete r3;
    TEST_ASSERT_EQUAL_INT(ef, _RAM2->available());

    _RAM2->debug(os);

    // test we get correct return if no memory left
    uint8_t *r4= (uint8_t *)_RAM2->alloc(100000);
    TEST_ASSERT_NULL(r4);
    TEST_ASSERT_EQUAL_INT(ef, _RAM2->available());

    // test we get correct return if all memory used
    uint8_t *r5= (uint8_t *)_RAM2->alloc(ef-4);
    TEST_ASSERT_NOT_NULL(r5);
    _RAM2->debug(os);
    TEST_ASSERT_EQUAL_INT(0, _RAM2->available());

    uint8_t *r6= (uint8_t *)_RAM2->alloc(1);
    TEST_ASSERT_NULL(r6);

    _RAM2->dealloc(r5);
    TEST_ASSERT_EQUAL_INT(ef, _RAM2->available());
    _RAM2->debug(os);
}
#endif

#define _ramfunc_ __attribute__ ((section(".ramfunctions"),long_call,noinline))
_ramfunc_ int testramfunc() { return 123; }

REGISTER_TEST(MemoryTest, ramfunc)
{
    printf("ramfunc is at %p\n", testramfunc);
    TEST_ASSERT_TRUE((unsigned int)testramfunc >= 0x00000000 && (unsigned int)testramfunc < 0x0000200);
    TEST_ASSERT_EQUAL_INT(123, testramfunc());
}

using systime_t= uint32_t;

void runMemoryTest(void *addr, uint32_t n)
{
    register uint32_t* p = (uint32_t *)addr;
    register uint32_t r1;
    register uint32_t r2;
    register uint32_t r3;
    register uint32_t r4;
    register uint32_t r5;
    register uint32_t r6;
    register uint32_t r7;
    register uint32_t r8;

    systime_t st = benchmark_timer_start();
    while(p < (uint32_t *)((uint32_t)addr+n)) {
        asm volatile ("ldm.w %[ptr]!,{%[r1],%[r2],%[r3],%[r4],%[r5],%[r6],%[r7],%[r8]}" :
                        [r1] "=r" (r1), [r2] "=r" (r2), [r3] "=r" (r3), [r4] "=r" (r4),
                        [r5] "=r" (r5), [r6] "=r" (r6),[r7] "=r" (r7), [r8] "=r" (r8),
                        [ptr] "=r" (p)                                                  :
                        "r" (p)                                                         : );
    }
    systime_t elt = benchmark_timer_elapsed(st);

    printf("elapsed time %lu us over %lu bytes %1.4f mb/sec\n", benchmark_timer_as_us(elt), n, (float)n/benchmark_timer_as_us(elt));
}

REGISTER_TEST(MemoryTest, time_memory_cached)
{
    taskENTER_CRITICAL();
    printf("Timing memory at 0x08000000 (FLASH)\n");
    runMemoryTest((void*)0x08000000, 2000000);

    printf("Timing memory at 0x00000000 (ITCMRAM)\n");
    runMemoryTest((void*)0x00000000, 64000);

    printf("Timing memory at 0x24000000 (RAM_D1)\n");
    runMemoryTest((void*)0x24000000, 512000);
    taskEXIT_CRITICAL();
}

REGISTER_TEST(MemoryTest, time_memory_not_cached)
{
    taskENTER_CRITICAL();
    /* Disable D-Cache */
    SCB_DisableDCache();
    printf("Timing memory at 0x08000000 (FLASH)\n");
    runMemoryTest((void*)0x08000000, 2000000);

    printf("Timing memory at 0x00000000 (ITCMRAM)\n");
    runMemoryTest((void*)0x00000000, 64000);

    printf("Timing memory at 0x24000000 (RAM_D1)\n");
    runMemoryTest((void*)0x24000000, 512000);

    /* Enable D-Cache */
    SCB_EnableDCache();
    taskEXIT_CRITICAL();
}
