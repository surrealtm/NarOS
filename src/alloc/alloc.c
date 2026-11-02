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
    struct Heap_Segment *previous_free; // @Cleanup: Is this ever even used anywhere?
} ALIGN_DECLARATION(16) Heap_Segment;

#define HEAP_SEGMENT_ALIGNMENT STRUCT_ALIGNMENT(Heap_Segment)

static inline
void *user_pointer_from_segment(const Heap_Segment *segment) {
    return (segment != null) ? ((u8 *) segment) + sizeof(Heap_Segment) : null;
}

static inline
Heap_Segment *segment_from_user_pointer(const void *pointer) {
    return (pointer != null) ? (Heap_Segment *) ((u8 *) pointer) - sizeof(Heap_Segment) : null;
}

static inline
Heap_Segment *split_segment_at(const Heap_Segment *original, const u64 offset_in_bytes) {
    return (Heap_Segment *) ((u8 *) original + offset_in_bytes);
}

static inline
b8 segments_are_continuous(const Heap_Segment *lhs, const Heap_Segment *rhs) {
    return ((u8 *) lhs) + lhs->segment_size_in_bytes + sizeof(Heap_Segment) == (u8 *) rhs;
}

static inline
void update_segment_metadata(Heap_Segment *segment, const u64 allocated_size_in_bytes, const u64 user_size_in_bytes, const b8 used) {
    assert(allocated_size_in_bytes < 0xffffffff, "The allocated heap segment is too large for the metadata.");
    assert(user_size_in_bytes < 0x7fffffff, "The allocated heap segment is too large for the metadata.");
    segment->segment_size_in_bytes = allocated_size_in_bytes;
    segment->user_size_in_bytes = user_size_in_bytes;
    segment->used = used;
}

static inline
s32 first_bin_index_for_size(const u64 segment_size_in_bytes) {
    const u64 size_for_highest_bin = (1 << HEAP_ALLOCATOR_BINS) - 1;
    const u64 aligned_size_in_bytes = segment_size_in_bytes >> 4;
    return highest_bit_set(min(aligned_size_in_bytes, size_for_highest_bin));
}

static
void remove_segment_from_free_list(Heap_Allocator *allocator, Heap_Segment *segment) {
    const u64 bin_idx = first_bin_index_for_size(segment->segment_size_in_bytes);
    if (segment->previous_free) segment->previous_free->next_free = segment->next_free;
    if (segment->next_free) segment->next_free->previous_free = segment->previous_free;
    if (allocator->bins[bin_idx] == segment) allocator->bins[bin_idx] = segment->next_free;
}

static
void insert_segment_into_free_list(Heap_Allocator *allocator, Heap_Segment *segment) {
    const u64 bin_idx = first_bin_index_for_size(segment->segment_size_in_bytes);
    if (allocator->bins[bin_idx]) allocator->bins[bin_idx]->previous_free = segment;
    segment->next_free = allocator->bins[bin_idx];
    segment->previous_free = null;
    allocator->bins[bin_idx] = segment;
}

static
Heap_Segment *maybe_coalesce_free_segment(Heap_Allocator *allocator, Heap_Segment *segment) {
    //
    // Find adjacent blocks that are free and can be coalesced with this new free block.
    // If they can be merged together, remove the existing blocks from the free list, as the size
    // has changed and they may therefore need to be moved into a different bin. This new freed
    // block hasn't yet been added to the free list, so it does not have to be removed, but
    // it also doesn't have valid free-list pointers yet.
    //
    Heap_Segment *returned = segment;

    if (segment->next_free && segments_are_continuous(segment, segment->next_free)) {
        remove_segment_from_free_list(allocator, segment->next_free);
        update_segment_metadata(segment, segment->segment_size_in_bytes + segment->next_free->segment_size_in_bytes + sizeof(Heap_Segment), 0, false);
        segment->next_free = segment->next_free->next_free;
    }

    if (segment->previous_free && segments_are_continuous(segment->previous_free, segment)) {
        remove_segment_from_free_list(allocator, segment->previous_free);
        update_segment_metadata(segment->previous_free, segment->previous_free->segment_size_in_bytes + segment->segment_size_in_bytes + sizeof(Heap_Segment), 0, false);
        returned = segment->previous_free;
    }

    return returned;

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

void *heap_allocator_allocate(Heap_Allocator *allocator, const u64 user_size_in_bytes, const u64 user_alignment) {
    const u64 segment_alignment = max(HEAP_SEGMENT_ALIGNMENT, user_alignment);
    const u64 segment_size_in_bytes = align(user_size_in_bytes, STRUCT_ALIGNMENT(Heap_Segment));

    //
    // Try to find a segment in the free-list that can accomodate this allocation.
    //
    for (u32 bin_idx = first_bin_index_for_size(segment_size_in_bytes); bin_idx < ARRAY_COUNT(allocator->bins); ++bin_idx) {
        Heap_Segment *free_segment = allocator->bins[bin_idx];

        while (free_segment) {
            if (free_segment->segment_size_in_bytes >= segment_size_in_bytes && free_segment->segment_size_in_bytes <= segment_size_in_bytes + sizeof(Heap_Segment)) {
                // This existing free segment is large enough to hold the new user payload, but small enough that splitting
                // doesn't make sense. Therefore, just reuse the entire segment
                update_segment_metadata(free_segment, free_segment->segment_size_in_bytes, user_size_in_bytes, true);
                remove_segment_from_free_list(allocator, free_segment);
                void *user_pointer = user_pointer_from_segment(free_segment);
                clear_memory(user_pointer, user_size_in_bytes);
                return user_pointer;
            } else if (free_segment->segment_size_in_bytes >= segment_size_in_bytes) {
                // The existing segment is large enough that splitting makes sense. Split this segment so that the free-list entry
                // remains unchanged, but the latter half of the segment can be used for this user payload.
                const u64 retained_size_in_bytes = free_segment->segment_size_in_bytes - segment_size_in_bytes;
                Heap_Segment *split = split_segment_at(free_segment, retained_size_in_bytes);
                split->previous_free = null;
                split->next_free     = null;
                update_segment_metadata(free_segment, retained_size_in_bytes, 0, false);
                update_segment_metadata(split, segment_size_in_bytes, user_size_in_bytes, true);
                void *user_pointer = user_pointer_from_segment(split);
                clear_memory(user_pointer, user_size_in_bytes);
                return user_pointer;
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

void *heap_allocator_reallocate(Heap_Allocator *allocator, const void *old_pointer, const u64 new_user_size_in_bytes, const u64 new_user_alignment) {
    // @Incomplete: Heap reallocation
    (void) allocator;
    (void) old_pointer;
    (void) new_user_size_in_bytes;
    (void) new_user_alignment;
    (void) insert_segment_into_free_list;
    (void) maybe_coalesce_free_segment;
    return null;
}

void heap_allocator_deallocate(Heap_Allocator *allocator, const void *pointer) {
    Heap_Segment *segment = segment_from_user_pointer(pointer);
    assert(segment->used, "Attempted to release a block that was already free!");

    update_segment_metadata(segment, segment->segment_size_in_bytes, 0, false);
    insert_segment_into_free_list(allocator, maybe_coalesce_free_segment(allocator, segment));
}
