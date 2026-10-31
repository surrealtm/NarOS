#include "alloc.h"
#include "alloc_kernel.h"

#define MEMORY_REGION_ALIGNMENT 16

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
    const u32 starting_address = align((unsigned int) dynamic_memory_region_pointer, MEMORY_REGION_ALIGNMENT);
    if (starting_address + capacity - 1 <= (u32) dynamic_memory_region_high) {
        dynamic_memory_region_pointer = (u8 *) (starting_address + 1);
        return (void *) starting_address;
    } else {
        return null;
    }
}

static
void release(const void *address) {
    // @Incomplete: Support releasing memory regions
    (void) address;
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

void stack_allocator_destroy(Stack_Allocator *allocator) {
    release(allocator->base);
    *allocator = (Stack_Allocator) { 0 };
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
    u32 segment_size_in_bytes;
    u32 user_size_in_bytes : 31;
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
void update_segment_metadata(Heap_Segment *segment, const u64 allocated_size_in_bytes, const u64 user_size_in_bytes, const b8 used) {
    assert(allocated_size_in_bytes < 0xffffffff, "The allocated heap segment is too large for the metadata.");
    assert(user_size_in_bytes < 0x7fffffff, "The allocated heap segment is too large for the metadata.");
    segment->segment_size_in_bytes = allocated_size_in_bytes;
    segment->user_size_in_bytes = user_size_in_bytes;
    segment->used = used;
}

static
Heap_Segment *split_segment(Heap_Segment *original, const u64 segment_size_in_bytes, const u64 user_size_in_bytes) {
    Heap_Segment *split = (Heap_Segment *) (((u8 *) original) + original->segment_size_in_bytes - segment_size_in_bytes);
    update_segment_metadata(original, original->segment_size_in_bytes - segment_size_in_bytes, original->user_size_in_bytes - user_size_in_bytes, original->used);
    update_segment_metadata(split, segment_size_in_bytes, user_size_in_bytes, true);
    return split;
}

static
s32 first_bin_index_for_size(const u64 segment_size_in_bytes) {
    const u64 size_for_highest_bin = (1 << HEAP_ALLOCATOR_BINS) - 1;
    const u64 aligned_size_in_bytes = segment_size_in_bytes >> 4;
    return highest_bit_set(min(aligned_size_in_bytes, size_for_highest_bin));
}

static
void remove_segment_from_free_list(Heap_Segment **free_list, Heap_Segment *free_segment) {
    if (free_segment->previous_free) free_segment->previous_free->next_free = free_segment->next_free;
    if (free_segment->next_free) free_segment->next_free->previous_free = free_segment->previous_free;
    if (free_list[0] == free_segment) free_list[0] = free_segment->next_free;
}

b8 heap_allocator_create(Heap_Allocator *allocator, const u64 capacity) {
    const b8 success = stack_allocator_create(&allocator->stack, capacity);
    clear_memory(allocator->bins, sizeof(allocator->bins));
    return success;
}

void heap_allocator_destroy(Heap_Allocator *allocator) {
    stack_allocator_destroy(&allocator->stack);
    clear_memory(allocator->bins, sizeof(allocator->bins));
}

void heap_allocator_reset(Heap_Allocator *allocator) {
    stack_allocator_reset(&allocator->stack, 0);
    clear_memory(allocator->bins, sizeof(allocator->bins));
}

// @Incomplete: The returned pointer should be zero-initialized?
void *heap_allocator_allocate(Heap_Allocator *allocator, const u64 user_size_in_bytes, const u64 user_alignment) {
    const u64 segment_alignment = max(HEAP_SEGMENT_ALIGNMENT, user_alignment);
    const u64 segment_size_in_bytes = user_size_in_bytes + sizeof(Heap_Segment);

    //
    // Try to find a block in the free-list that can accomodate this allocation.
    //
    for (u32 bin_idx = first_bin_index_for_size(segment_size_in_bytes); bin_idx < ARRAY_COUNT(allocator->bins); ++bin_idx) {
        Heap_Segment *free_segment = allocator->bins[bin_idx];

        while (free_segment) {
            if (free_segment->segment_size_in_bytes >= segment_size_in_bytes && free_segment->segment_size_in_bytes <= segment_size_in_bytes + sizeof(Heap_Segment)) {
                // This existing free segment is large enough to hold the new user payload, but small enough that splitting
                // doesn't make sense. Therefore, just reuse the entire block
                update_segment_metadata(free_segment, free_segment->segment_size_in_bytes, user_size_in_bytes, true);
                remove_segment_from_free_list(&allocator->bins[bin_idx], free_segment);
                return user_pointer_from_segment(free_segment);
            } else if (free_segment->segment_size_in_bytes >= segment_size_in_bytes) {
                // The existing block is large enough that splitting makes sense. Split this segment so that the free-list entry
                // remains unchanged, but the latter half of the segment can be used for this user payload.
                Heap_Segment *split = split_segment(free_segment, segment_size_in_bytes, user_size_in_bytes);
                return user_pointer_from_segment(split);
            } else {
                free_segment = free_segment->next_free;
            }
        }
    }

    //
    // No free segment that could be re-used, append a new one at the end
    //
    Heap_Segment *segment = stack_allocator_push(&allocator->stack, segment_size_in_bytes, segment_alignment);

    if (segment != null) {
        update_segment_metadata(segment, segment_size_in_bytes, user_size_in_bytes, true);
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
