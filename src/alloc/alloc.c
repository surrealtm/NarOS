#include "alloc.h"
#include "alloc_kernel.h"

/**
 * The boot loader gives us a memory region that is available on the system and designated for
 * dynamic memory allocation.
 * This region is bounded by the low and high value, any dynamic memory allocator must occupy a sub-region
 * between these two points.
 */
static u8 *dynamic_memory_region_low = null, *dynamic_memory_region_high = null;

// @Temporary: We ought to have a list of used regions that can be freed again, instead of bumping this pointer.
// Otherwise we will eventually run out of memory when reserving and then releasing dynamic memory blocks
static u8 *dynamic_memory_region_pointer = null;

static
void *reserve(const u64 capacity) {
    if (dynamic_memory_region_pointer + capacity <= dynamic_memory_region_high) {
        void *result = dynamic_memory_region_pointer;
        dynamic_memory_region_pointer += capacity;
        return result;
    } else {
        return null;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Kernel API
// ---------------------------------------------------------------------------------------------------------------

void alloc_initialize_memory_region(void *low, void *high) {
    dynamic_memory_region_low = low;
    dynamic_memory_region_high = high;
    dynamic_memory_region_pointer = dynamic_memory_region_low;
}

// ---------------------------------------------------------------------------------------------------------------
// Stack Allocator
// ---------------------------------------------------------------------------------------------------------------

b8 stack_allocator_create(Stack_Allocator *allocator, const u64 capacity) {
    allocator->base = reserve(capacity);
    return allocator->base != null;
}
