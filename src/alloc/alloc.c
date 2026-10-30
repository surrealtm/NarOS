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
    dynamic_memory_region_low     = low;
    dynamic_memory_region_high    = high;
    dynamic_memory_region_pointer = dynamic_memory_region_low;
}

// ---------------------------------------------------------------------------------------------------------------
// Stack Allocator
// ---------------------------------------------------------------------------------------------------------------

static
void *stack_allocator_increase(Stack_Allocator *allocator, const u64 size_in_bytes) {
    if (allocator->occupied + size_in_bytes > allocator->capacity) {
        return null;
    }

    void *pointer = (u8 *) allocator->base + allocator->occupied;
    allocator->occupied += size_in_bytes;
    return pointer;

}

b8 stack_allocator_create(Stack_Allocator *allocator, const u64 capacity) {
    allocator->base     = reserve(capacity);
    allocator->capacity = (allocator->base != null) ? capacity : 0;
    allocator->occupied = 0;
    return allocator->base != null;
}

void *stack_allocator_push(Stack_Allocator *allocator, const u64 size_in_bytes, const u64 alignment) {
    stack_allocator_increase(allocator, align(allocator->occupied, alignment) - allocator->occupied);
    void *pointer = stack_allocator_increase(allocator, size_in_bytes);
    if (pointer != null) clear_memory(pointer, size_in_bytes);
    return pointer;
}

u64 stack_allocator_mark(const Stack_Allocator *allocator) {
    return allocator->occupied;
}

void stack_allocator_reset(Stack_Allocator *allocator, const u64 mark) {
    allocator->occupied = clamp(mark, 0, allocator->capacity);
}



// ---------------------------------------------------------------------------------------------------------------
// Heap Allocator
// ---------------------------------------------------------------------------------------------------------------

typedef struct Heap_Segment {
    // This segment might occupy more space than was actually requested by the user, either because of alignment
    // reasons or because a larger segment was re-used without splitting.
    // Store both so that we know which chunk of this segment actually is relevant for the user.
    u32 allocated_size_in_bytes;
    u32 used_size_in_bytes : 31;
    u32 used : 1;
    struct Heap_Segment *next_free;
    struct Heap_Segment *previous_free;
} ALIGN_DECLARATION(16) Heap_Segment;

#define HEAP_SEGMENT_ALIGNMENT STRUCT_ALIGNMENT(Heap_Segment)

static
void *user_pointer_from_segment(Heap_Segment *segment) {
    return (segment != null) ? ((u8 *) segment) + sizeof(Heap_Segment) : null;
}

static
void update_segment_metadata(Heap_Segment *segment, const u32 allocated_size_in_bytes, const u32 used_size_in_bytes, const b8 used) {
    segment->allocated_size_in_bytes = allocated_size_in_bytes;
    segment->used_size_in_bytes = used_size_in_bytes;
    segment->used = used;
}

b8 heap_allocator_create(Heap_Allocator *allocator, const u64 capacity) {
    return stack_allocator_create(&allocator->stack, capacity);
}

void *heap_allocator_allocate(Heap_Allocator *allocator, const u64 size_in_bytes, const u64 user_alignment) {
    const u64 segment_alignment = max(HEAP_SEGMENT_ALIGNMENT, user_alignment);

    // @Incomplete: Search free list before pushing a new segment
    Heap_Segment *segment = stack_allocator_push(&allocator->stack, size_in_bytes + sizeof(Heap_Segment), segment_alignment);

    if (segment != null) {
        update_segment_metadata(segment, size_in_bytes, size_in_bytes, true);
    }

    return user_pointer_from_segment(segment);
}

void *heap_allocator_reallocate(Heap_Allocator *allocator, const void *old_pointer, const u64 new_size_in_bytes) {
    // @Incomplete: Heap reallocation
    (void) allocator;
    (void) old_pointer;
    (void) new_size_in_bytes;
    return null;
}

void heap_allocator_deallocate(Heap_Allocator *allocator, const void *pointer) {
    // @Incomplete: Heap deallocation
    (void) allocator;
    (void) pointer;
}
