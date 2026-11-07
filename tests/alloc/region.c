#include "test.h"

#include "base.h"
#include "alloc/alloc_kernel.h"

#include <stdlib.h>

#define MEMORY_REGION_HEADER_SIZE 16

static
void initialize_region(const u32 capacity) {
    void *region = malloc(capacity + MEMORY_REGION_HEADER_SIZE);
    ASSERT_NE(region, null);
    alloc_initialize_memory_region(region, (u8 *) region + capacity + MEMORY_REGION_HEADER_SIZE - 1);
}

TEST(Memory_Region, AllocateSplitsFreeRegion) {
    initialize_region(112);

    u8 *first = alloc_allocate_region(32);
    u8 *second = alloc_allocate_region(64);

    ASSERT_NE(first, null);
    ASSERT_EQ(second, first + 32 + MEMORY_REGION_HEADER_SIZE);
    ASSERT_EQ(alloc_allocate_region(1), null);
}

TEST(Memory_Region, DeallocateCoalescesBothNeighbors) {
    initialize_region(80);

    void *first = alloc_allocate_region(16);
    void *middle = alloc_allocate_region(16);
    void *last = alloc_allocate_region(16);
    ASSERT_NE(first, null);
    ASSERT_NE(middle, null);
    ASSERT_NE(last, null);

    alloc_deallocate_region(first);
    alloc_deallocate_region(last);
    alloc_deallocate_region(middle);

    ASSERT_EQ(alloc_allocate_region(80), first);
}
