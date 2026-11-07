#include "test.h"
#include "alloc.h"
#include "alloc/alloc_kernel.h"

#include <stdlib.h>

#define STACK_SIZE 32

static
Stack_Allocator create(const u64 stack_capacity) {
    const u64 memory_region_size = stack_capacity + 16;
    Stack_Allocator allocator;
    void *region = malloc(memory_region_size);
    alloc_initialize_memory_region(region, (u8 *) region + memory_region_size - 1);
    ASSERT_TRUE(stack_allocator_create(&allocator, stack_capacity));
    return allocator;
}

TEST(Stack_Allocator, Push) {
    Stack_Allocator allocator = create(STACK_SIZE);
    int *a = stack_allocator_push(&allocator, sizeof(int), 4);
    ASSERT_NE(a, null);
    *a = 5;
    const int *b = stack_allocator_push(&allocator, sizeof(int), 8);
    ASSERT_NE(b, null);
    ASSERT_NE(a, b);
    ASSERT_TRUE(((int) b % 8) == 0);
}

TEST(Stack_Allocator, Release) {
    Stack_Allocator allocator = create(STACK_SIZE);
    int *a = stack_allocator_push(&allocator, sizeof(int), 4);
    int *b = stack_allocator_push(&allocator, sizeof(int), 4);
    stack_allocator_reset(&allocator, 4);
    int *c = stack_allocator_push(&allocator, sizeof(int), 4);
    ASSERT_NE(a, b);
    ASSERT_EQ(b, c);
    ASSERT_EQ(allocator.occupied, 8);
}

TEST(Stack_Allocator, Destroy) {
    Stack_Allocator allocator = create(STACK_SIZE);
    stack_allocator_destroy(&allocator);
    ASSERT_EQ(allocator.base, 0);
    ASSERT_EQ(allocator.occupied, 0);
    ASSERT_EQ(allocator.capacity, 0);
}
