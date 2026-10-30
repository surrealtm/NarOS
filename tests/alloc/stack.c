#include "test.h"
#include "alloc.h"
#include "alloc/alloc_kernel.h"
#include <stdlib.h>

static
Stack_Allocator create(const u64 capacity) {
    Stack_Allocator allocator;
    void *region = malloc(capacity);
    alloc_initialize_memory_region(region, (u8 *) region + capacity - 1);
    ASSERT_TRUE(stack_allocator_create(&allocator, capacity));
    return allocator;
}

TEST(Stack_Allocator, Push) {
    Stack_Allocator allocator = create(16);
    int *a = stack_allocator_push(&allocator, sizeof(int), 4);
    ASSERT_NE(a, null);
    *a = 5;
    const int *b = stack_allocator_push(&allocator, sizeof(int), 4);
    ASSERT_NE(a, null);
    ASSERT_NE(a, b);
}
