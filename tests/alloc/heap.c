#include "alloc.h"
#include "alloc/alloc_kernel.h"
#include "test.h"

#include <stdlib.h>

static
Heap_Allocator create(const u64 capacity) {
    Heap_Allocator allocator;
    void *region = malloc(capacity);
    alloc_initialize_memory_region(region, (u8 *) region + capacity - 1);
    ASSERT_TRUE(heap_allocator_create(&allocator, capacity));
    return allocator;
}

TEST(Heap_Allocator, Allocate) {
    Heap_Allocator allocator = create(64);
    int *a = heap_allocator_allocate(&allocator, sizeof(int));
    ASSERT_NE(a, null);
    ASSERT_EQ(((int) a) % 16, 0);
    *a = 5;
    const int *b = heap_allocator_allocate(&allocator, sizeof(int));
    ASSERT_NE(b, null);
    ASSERT_NE(a, b);
    ASSERT_EQ(((int) b) % 16, 0);
}
