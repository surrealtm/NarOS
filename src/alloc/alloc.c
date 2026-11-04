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
void *reserve(const u32 capacity) {
    const u32 starting_address = align((u32) dynamic_memory_region_pointer, MEMORY_REGION_ALIGNMENT);
    if (starting_address + capacity - 1 <= (u32) dynamic_memory_region_high) {
        dynamic_memory_region_pointer = (u8 *) (starting_address + capacity);
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
void *stack_allocator_increase(Stack_Allocator *allocator, const u32 size_in_bytes) {
    if (allocator->occupied + size_in_bytes > allocator->capacity) {
        return null;
    }

    void *pointer = (u8 *) allocator->base + allocator->occupied;
    allocator->occupied += size_in_bytes;
    return pointer;

}

b8 stack_allocator_create(Stack_Allocator *allocator, const u32 capacity) {
    allocator->base     = reserve(capacity);
    allocator->capacity = (allocator->base != null) ? capacity : 0;
    allocator->occupied = 0;
    return allocator->base != null;
}

void stack_allocator_destroy(Stack_Allocator *allocator) {
    release(allocator->base);
    *allocator = (Stack_Allocator) { 0 };
}

void *stack_allocator_push(Stack_Allocator *allocator, const u32 size_in_bytes, const u32 alignment) {
    stack_allocator_increase(allocator, align(allocator->occupied, alignment) - allocator->occupied);
    void *pointer = stack_allocator_increase(allocator, size_in_bytes);
    if (pointer != null) clear_memory(pointer, size_in_bytes);
    return pointer;
}

u32 stack_allocator_mark(const Stack_Allocator *allocator) {
    return allocator->occupied;
}

void stack_allocator_reset(Stack_Allocator *allocator, const u32 mark) {
    allocator->occupied = min(mark, allocator->capacity);
}



// ---------------------------------------------------------------------------------------------------------------
// Heap Allocator
// ---------------------------------------------------------------------------------------------------------------

typedef struct Heap_Segment {
    u32 segment_size_in_bytes : 31;
    u32 used : 1;
    struct Heap_Segment *free_list_next;
    struct Heap_Segment *free_list_prev;
    struct Heap_Segment *previous_segment; // Physical predecessor in the arena
} ALIGN_DECLARATION(16) Heap_Segment;

#define HEAP_SEGMENT_ALIGNMENT 16 // Every heap segment shall be 16 byte aligned to reduce complexity when re-allocating or re-using segments

static inline
void *user_pointer_from_segment(const Heap_Segment *segment) {
    return (segment != null) ? (u8 *) segment + sizeof(Heap_Segment) : null;
}

static inline
Heap_Segment *segment_from_user_pointer(const void *pointer) {
    return (pointer != null) ? (Heap_Segment *) ((u8 *) pointer - sizeof(Heap_Segment)) : null;
}

static inline
void update_segment_metadata(Heap_Segment *segment, const u32 segment_size_in_bytes, const b8 used) {
    assert(segment_size_in_bytes < 0x80000000ULL, "The allocated heap segment is too large for the metadata.");
    segment->segment_size_in_bytes = segment_size_in_bytes;
    segment->used = used;
}

static inline
Heap_Segment *split_segment_at(const Heap_Segment *original, const u32 offset_in_bytes) {
    return (Heap_Segment *) ((u8 *) original + offset_in_bytes);
}

static inline
Heap_Segment *merge_segment_into(Heap_Segment *target, const Heap_Segment *source) {
    update_segment_metadata(target, target->segment_size_in_bytes + source->segment_size_in_bytes + sizeof(Heap_Segment), target->used);
    return target;
}

static inline
const u8 *one_plus_end_of_segment(const Heap_Segment *segment) {
    return ((u8 *) segment) + sizeof(Heap_Segment) + segment->segment_size_in_bytes;
}

static inline
Heap_Segment *next_heap_segment(const Heap_Allocator *allocator, const Heap_Segment *segment) {
    const u8 *next = one_plus_end_of_segment(segment);
    const u8 *arena_end = (const u8 *) allocator->stack.base + allocator->stack.occupied;
    return next < arena_end ? (Heap_Segment *) next : null;
}

static inline
void update_successor_previous_segment(Heap_Allocator *allocator, Heap_Segment *segment) {
    Heap_Segment *next = next_heap_segment(allocator, segment);
    if (next != null) next->previous_segment = segment;
    else allocator->last_segment = segment;
}

static inline
s32 first_bin_index_for_size(const u32 segment_size_in_bytes) {
    const u32 size_for_highest_bin = (1 << HEAP_ALLOCATOR_BINS) - 1;
    const u32 aligned_size_in_bytes = segment_size_in_bytes >> 4;
    return highest_bit_set(min(aligned_size_in_bytes, size_for_highest_bin));
}

static
void remove_segment_from_free_list(Heap_Allocator *allocator, const Heap_Segment *segment) {
    const u32 bin_idx = first_bin_index_for_size(segment->segment_size_in_bytes);
    if (segment->free_list_prev) segment->free_list_prev->free_list_next = segment->free_list_next;
    if (segment->free_list_next) segment->free_list_next->free_list_prev = segment->free_list_prev;
    if (allocator->bins[bin_idx] == segment) allocator->bins[bin_idx] = segment->free_list_next;
}

static
void insert_segment_into_free_list(Heap_Allocator *allocator, Heap_Segment *segment) {
    const u32 bin_idx = first_bin_index_for_size(segment->segment_size_in_bytes);
    if (allocator->bins[bin_idx]) allocator->bins[bin_idx]->free_list_prev = segment;
    segment->free_list_next = allocator->bins[bin_idx];
    segment->free_list_prev = null;
    allocator->bins[bin_idx] = segment;
}

static
Heap_Segment *maybe_coalesce_free_segment(Heap_Allocator *allocator, Heap_Segment *segment) {
    Heap_Segment *next = next_heap_segment(allocator, segment);
    if (next != null && !next->used) {
        remove_segment_from_free_list(allocator, next);
        merge_segment_into(segment, next);
    }

    Heap_Segment *previous = segment->previous_segment;
    if (previous != null && !previous->used) {
        remove_segment_from_free_list(allocator, previous);
        segment = merge_segment_into(previous, segment);
    }

    update_successor_previous_segment(allocator, segment);
    return segment;
}

static
Heap_Segment *search_for_free_segment(Heap_Allocator *allocator, const u32 segment_size_in_bytes) {
    for (u32 bin_idx = first_bin_index_for_size(segment_size_in_bytes); bin_idx < ARRAY_COUNT(allocator->bins); ++bin_idx) {
        Heap_Segment *free_segment = allocator->bins[bin_idx];

        while (free_segment) {
            if (free_segment->segment_size_in_bytes >= segment_size_in_bytes && free_segment->segment_size_in_bytes <= segment_size_in_bytes + sizeof(Heap_Segment)) {
                // This existing free segment is large enough to hold the new user payload, but small enough that splitting
                // doesn't make sense. Therefore, just reuse the entire segment
                update_segment_metadata(free_segment, free_segment->segment_size_in_bytes, true);
                remove_segment_from_free_list(allocator, free_segment);
                return free_segment;
            } else if (free_segment->segment_size_in_bytes >= segment_size_in_bytes) {
                // The existing segment is large enough that splitting makes sense. Split this segment so that the free-list entry
                // remains unchanged, but the latter half of the segment can be used for this user payload.
                const u32 retained_size_in_bytes = free_segment->segment_size_in_bytes - segment_size_in_bytes - sizeof(Heap_Segment);
                Heap_Segment *split = split_segment_at(free_segment, retained_size_in_bytes + sizeof(Heap_Segment));
                remove_segment_from_free_list(allocator, free_segment);
                update_segment_metadata(free_segment, retained_size_in_bytes, false);
                update_segment_metadata(split, segment_size_in_bytes, true);
                split->previous_segment = free_segment;
                update_successor_previous_segment(allocator, split);
                insert_segment_into_free_list(allocator, free_segment); // The free segment might belong into a different bin now
                return split;
            } else {
                free_segment = free_segment->free_list_next;
            }
        }
    }

    return null;
}

b8 heap_allocator_create(Heap_Allocator *allocator, const u32 capacity) {
    const b8 success = stack_allocator_create(&allocator->stack, capacity);
    allocator->last_segment = null;
    clear_memory(allocator->bins, sizeof(allocator->bins));
    return success;
}

void heap_allocator_destroy(Heap_Allocator *allocator) {
    stack_allocator_destroy(&allocator->stack);
    allocator->last_segment = null;
    clear_memory(allocator->bins, sizeof(allocator->bins));
}

void heap_allocator_reset(Heap_Allocator *allocator) {
    stack_allocator_reset(&allocator->stack, 0);
    allocator->last_segment = null;
    clear_memory(allocator->bins, sizeof(allocator->bins));
}

void *heap_allocator_allocate(Heap_Allocator *allocator, const u32 user_size_in_bytes) {
    const u32 segment_size_in_bytes = align(user_size_in_bytes, HEAP_SEGMENT_ALIGNMENT);

    //
    // Try to find a segment in the free-list that can accomodate this allocation.
    //
    Heap_Segment *free_segment = search_for_free_segment(allocator, segment_size_in_bytes);
    if (free_segment) {
        void *user_pointer = user_pointer_from_segment(free_segment);
        clear_memory(user_pointer, free_segment->segment_size_in_bytes);
        return user_pointer;
    }

    //
    // No free segment that could be re-used, append a new one at the end
    //
    Heap_Segment *segment = stack_allocator_push(&allocator->stack, segment_size_in_bytes + sizeof(Heap_Segment), HEAP_SEGMENT_ALIGNMENT);

    if (segment != null) {
        update_segment_metadata(segment, segment_size_in_bytes, true);
        segment->previous_segment = allocator->last_segment;
        allocator->last_segment = segment;
    }

    return user_pointer_from_segment(segment);
}

void *heap_allocator_reallocate(Heap_Allocator *allocator, void *old_pointer, const u32 new_user_size_in_bytes) {
    if (new_user_size_in_bytes == 0) {
        heap_allocator_deallocate(allocator, old_pointer);
        return null;
    }

    const u32 new_segment_size_in_bytes = align(new_user_size_in_bytes, HEAP_SEGMENT_ALIGNMENT);
    Heap_Segment *old_segment = segment_from_user_pointer(old_pointer);

    if (old_segment->segment_size_in_bytes >= new_segment_size_in_bytes && old_segment->segment_size_in_bytes <= new_segment_size_in_bytes + sizeof(Heap_Segment)) {
        // The size didn't change significantly, so don't change anything about the underlying
        // block structure. Clear the unused payload so later growth within this block sees zeros.
        clear_memory((u8 *) old_pointer + new_user_size_in_bytes, old_segment->segment_size_in_bytes - new_user_size_in_bytes);
        return user_pointer_from_segment(old_segment);
    }

    if (old_segment->segment_size_in_bytes >= new_segment_size_in_bytes) {
        // The new size is smaller than the previous one, so split the existing block up.
        // This avoids having to copy the existing data.
        const u32 split_size_in_bytes = old_segment->segment_size_in_bytes - new_segment_size_in_bytes - sizeof(Heap_Segment);
        Heap_Segment *split = split_segment_at(old_segment, sizeof(Heap_Segment) + new_segment_size_in_bytes);
        update_segment_metadata(old_segment, new_segment_size_in_bytes, true);
        clear_memory((u8 *) old_pointer + new_user_size_in_bytes, new_segment_size_in_bytes - new_user_size_in_bytes);
        update_segment_metadata(split, split_size_in_bytes, false);
        split->previous_segment = old_segment;
        insert_segment_into_free_list(allocator, maybe_coalesce_free_segment(allocator, split));
        return user_pointer_from_segment(old_segment);
    }

    Heap_Segment *next_segment = next_heap_segment(allocator, old_segment);
    if (next_segment != null && !next_segment->used) {
        // The next segment is free, so we can merge with this segment to increase the size of the current segment.
        // This avoids having to copy the existing data.
        // This currently takes the *entire* next_segment, even if the next_segment is way larger than what we actually
        // need. At this moment, I don't care about that though.
        remove_segment_from_free_list(allocator, next_segment);
        merge_segment_into(old_segment, next_segment);
        update_successor_previous_segment(allocator, old_segment);
        clear_memory(next_segment, next_segment->segment_size_in_bytes + sizeof(Heap_Segment));
        if (old_segment->segment_size_in_bytes >= new_segment_size_in_bytes) {
            return user_pointer_from_segment(old_segment);
        }
    }

    Heap_Segment *free_segment = search_for_free_segment(allocator, new_segment_size_in_bytes);
    if (free_segment) {
        // Re-use a different free segment that can accomodate the complete new segment and move the memory into there.
        void *new_pointer = user_pointer_from_segment(free_segment);
        copy_memory(new_pointer, old_pointer, old_segment->segment_size_in_bytes);
        clear_memory((u8 *) new_pointer + old_segment->segment_size_in_bytes, free_segment->segment_size_in_bytes - old_segment->segment_size_in_bytes);
        heap_allocator_deallocate(allocator, old_pointer);
        return new_pointer;
    }

    if (old_segment == allocator->last_segment) {
        // The new size is larger than the previous, but this block is the very last thing in the
        // arena. In that case, we can just push the arena to get more space without having to
        // copy the existing data.
        const u32 extension_size_in_bytes = new_segment_size_in_bytes - old_segment->segment_size_in_bytes;
        const void *extension = stack_allocator_push(&allocator->stack, extension_size_in_bytes, 1);
        if (extension != null) {
            update_segment_metadata(old_segment, old_segment->segment_size_in_bytes + extension_size_in_bytes, true);
            return user_pointer_from_segment(old_segment);
        }
    }

    {
        // The new size is larger than the previous, and we cannot abuse the layout of the heap allocator
        // in the arena, so we just have to allocate a new block, and then free the old one...
        void *new_pointer = heap_allocator_allocate(allocator, new_user_size_in_bytes);
        if (new_pointer != null) {
            // The whole payload is initialized on allocation, including alignment padding.
            copy_memory(new_pointer, old_pointer, old_segment->segment_size_in_bytes);
            heap_allocator_deallocate(allocator, old_pointer);
            return new_pointer;
        } else {
            return null;
        }
    }
}

void heap_allocator_deallocate(Heap_Allocator *allocator, const void *pointer) {
    Heap_Segment *segment = segment_from_user_pointer(pointer);
    assert(segment->used, "Attempted to release a block that was already free!");

    update_segment_metadata(segment, segment->segment_size_in_bytes, false);
    insert_segment_into_free_list(allocator, maybe_coalesce_free_segment(allocator, segment));
}
