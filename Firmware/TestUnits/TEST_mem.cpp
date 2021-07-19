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

    // make sure new and delete work
    uint8_t *pt= new uint8_t[1024];
    TEST_ASSERT_NOT_NULL(pt);
    struct mallinfo mem2= mallinfo();
    printf("Malloc'ed data @ %p\n", pt);
    printf("Mem:   %11d%11d%11d%11d\n", mem2.arena, mem2.uordblks, mem2.fordblks, mem2.usmblks);
    TEST_ASSERT_TRUE(mem2.uordblks > mem.uordblks);
    delete[] pt;
    struct mallinfo mem3= mallinfo();
    printf("Mem:   %11d%11d%11d%11d\n", mem3.arena, mem3.uordblks, mem3.fordblks, mem3.usmblks);
    TEST_ASSERT_TRUE(mem3.uordblks == mem.uordblks);
}

char test_dtcm_bss[128] __attribute__ ((section (".dtcm_bss")));
char test_sram1_bss[128] __attribute__ ((section (".sram_1_bss")));
 __attribute__ ((section (".dtcm_text"))) char test_dtcm_data[8]= {1,2,3,4,5,6,7,8};

REGISTER_TEST(MemoryTest, other_rams)
{
    TEST_ASSERT_EQUAL_PTR(0x20000000, (unsigned int)&test_dtcm_data);
    TEST_ASSERT_EQUAL_PTR(0x20000018, (unsigned int)&test_dtcm_bss);
    TEST_ASSERT_EQUAL_PTR(0x30000000, (unsigned int)&test_sram1_bss);

    // check bss was cleared
    for (int i = 0; i < 128; ++i) {
        TEST_ASSERT_EQUAL_INT(0, test_dtcm_bss[i]);
    }

    // check bss was cleared
    for (int i = 0; i < 128; ++i) {
        TEST_ASSERT_EQUAL_INT(0, test_sram1_bss[i]);
    }

    // check data areas were copied
    for (int i = 0; i < 8; ++i) {
        TEST_ASSERT_EQUAL_INT(i+1, test_dtcm_data[i]);
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
const size_t test_heap_size= 1024;
static uint8_t _test_heap[test_heap_size];
REGISTER_TEST(MemoryTest, memory_pool)
{
    MemoryPool RAM2= MemoryPool(_test_heap, test_heap_size);
    MemoryPool *_RAM2= &RAM2;

    OutputStream os(&std::cout);
    uint32_t ef= test_heap_size;
    _RAM2->debug(os);
    // check amount of memory available
    TEST_ASSERT_EQUAL_INT(ef, _RAM2->available());
    TEST_ASSERT_EQUAL_INT(test_heap_size, ef);
    uint8_t *r2= (uint8_t *)_RAM2->alloc(128);
    _RAM2->debug(os);
    TEST_ASSERT_NOT_NULL(r2);
    // check it lost expected amount + 4 byte overhead and is where we expect it to be
    TEST_ASSERT_TRUE(r2 >= _test_heap && r2 < &_test_heap[test_heap_size-1]);
    TEST_ASSERT_TRUE(_RAM2->has(r2));
    TEST_ASSERT_EQUAL_INT(ef-128-4, _RAM2->available());
    _RAM2->dealloc(r2);

    // check it deallocated it
    TEST_ASSERT_EQUAL_INT(ef, _RAM2->available());

    // test placement new
    DummyClass *r3= new(*_RAM2)DummyClass();
    TEST_ASSERT_TRUE((uint8_t*)r3 >= _test_heap && (uint8_t*)r3 < &_test_heap[test_heap_size-1]);
    TEST_ASSERT_EQUAL_INT(ef-sizeof(DummyClass)-4, _RAM2->available());
    TEST_ASSERT_EQUAL_INT(1234, r3->t);
    delete r3;
    TEST_ASSERT_EQUAL_INT(ef, _RAM2->available());

    _RAM2->debug(os);

    // test placement new[]
    DummyClass *r34= new(*_RAM2)DummyClass[4];
    TEST_ASSERT_TRUE((uint8_t*)r34 >= _test_heap && (uint8_t*)r34 < &_test_heap[test_heap_size-1]);
    TEST_ASSERT_EQUAL_INT(ef-(sizeof(DummyClass)+4)*4+4, _RAM2->available());
    for (int i = 0; i < 4; ++i) {
        TEST_ASSERT_EQUAL_INT(1234, r34[i].t);
    }
    delete[] r34;
    TEST_ASSERT_EQUAL_INT(ef, _RAM2->available());

    _RAM2->debug(os);

    // test we get correct return if no memory left
    uint8_t *r4= (uint8_t *)_RAM2->alloc(test_heap_size*2);
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

#define _ramfunc_ __attribute__ ((section(".ramfunctions"),long_call,noinline))
_ramfunc_ static int testramfunc() { return 123; }

REGISTER_TEST(MemoryTest, ramfunc)
{
    printf("ramfunc is at %p\n", testramfunc);
    TEST_ASSERT_TRUE((unsigned int)testramfunc >= 0x00000298 && (unsigned int)testramfunc < 0x0010000);
    TEST_ASSERT_EQUAL_INT(123, testramfunc());
}

REGISTER_TEST(MemoryTest, alloc)
{
    printf("sram_1 size=%lu, available=%lu\n", _SRAM_1->get_size(), _SRAM_1->available());
    void *tram= AllocSRAM_1(256);
    printf("sram_1 alloc is at %p, available=%lu\n", tram, _SRAM_1->available());
    // allow for the statically allocated in bss
    TEST_ASSERT_TRUE((unsigned int)tram >= 0x30000080 && (unsigned int)tram < 0x30200000);
    DeallocSRAM_1(tram);
    printf("sram_1 available=%lu\n", _SRAM_1->available());
}

#define _fast_data_ __attribute__ ((section(".dtcm_text")))
static _fast_data_ char test_array[16]= {1,2};

REGISTER_TEST(MemoryTest, fastdata)
{
    printf("fast data is at %p\n", test_array);
    TEST_ASSERT_TRUE((unsigned int)test_array >= 0x20000000 && (unsigned int)test_array < 0x20020000);
    TEST_ASSERT_EQUAL_INT(1, test_array[0]);
    TEST_ASSERT_EQUAL_INT(2, test_array[1]);
    TEST_ASSERT_EQUAL_INT(0, test_array[2]);
    TEST_ASSERT_EQUAL_INT(0, test_array[15]);
}

using systime_t= uint32_t;

// NOTE this seems to be only accurate if compiled no debug with O2 otherwise we get totally bogus results!
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
    SCB_CleanInvalidateDCache();
    benchmark_timer_reset();
    systime_t st = benchmark_timer_start();
    while(p < (uint32_t *)((uint32_t)addr+n)) {
        asm volatile ("ldm.w %[ptr]!,{%[r1],%[r2],%[r3],%[r4],%[r5],%[r6],%[r7],%[r8]}" :
                        [r1] "=r" (r1), [r2] "=r" (r2), [r3] "=r" (r3), [r4] "=r" (r4),
                        [r5] "=r" (r5), [r6] "=r" (r6),[r7] "=r" (r7), [r8] "=r" (r8),
                        [ptr] "=r" (p)                                                  :
                        "r" (p)                                                         : );
    }
    systime_t elt = benchmark_timer_elapsed(st);

    printf("elapsed time (%lu) %f us over %lu bytes %1.4f mb/sec\n", elt, benchmark_timer_as_ns(elt), n, (float)n/benchmark_timer_as_ns(elt));
}

REGISTER_TEST(MemoryTest, time_memory_cached)
{
    __disable_irq();
    printf("Timing memory at 0x08000000 (FLASH)\n");
    runMemoryTest((void*)0x08000000, 64000);

    printf("Timing memory at 0x00000000 (ITCMRAM)\n");
    runMemoryTest((void*)0x00000000, 64000);

    printf("Timing memory at 0x20000000 (DTCMRAM)\n");
    runMemoryTest((void*)0x20000000, 64000);

    printf("Timing memory at 0x24000000 (RAM_D1)\n ");
    runMemoryTest((void*)0x24000000, 64000);
    __enable_irq();
}

#if 0
REGISTER_TEST(MemoryTest, time_memory_not_cached)
{
    __disable_irq();
    /* Disable D-Cache */
    SCB_DisableDCache();
    printf("Timing memory at 0x08000000 (FLASH)\n");
    runMemoryTest((void*)0x08000000, 64000);

    printf("Timing memory at 0x00000000 (ITCMRAM)\n");
    runMemoryTest((void*)0x00000000, 64000);

    printf("Timing memory at 0x24000000 (RAM_D1)\n");
    runMemoryTest((void*)0x24000000, 64000);

    /* Enable D-Cache */
    SCB_EnableDCache();
    __enable_irq();
}
#endif

/*
cache handling macros from micropython
#define MP_HAL_CLEANINVALIDATE_DCACHE(addr, size) \
    (SCB_CleanInvalidateDCache_by_Addr((uint32_t *)((uint32_t)addr & ~0x1f), \
    ((uint32_t)((uint8_t *)addr + size + 0x1f) & ~0x1f) - ((uint32_t)addr & ~0x1f)))
#define MP_HAL_CLEAN_DCACHE(addr, size) \
    (SCB_CleanDCache_by_Addr((uint32_t *)((uint32_t)addr & ~0x1f), \
    ((uint32_t)((uint8_t *)addr + size + 0x1f) & ~0x1f) - ((uint32_t)addr & ~0x1f)))
*/
