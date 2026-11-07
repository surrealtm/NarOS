#pragma once

/**
 * Initializes the memory region that can be used for dynamic memory allocation.
 */
void alloc_initialize_memory_region(void *low, void *high);

/**
 * Attempts to reserve a memory region containing at least the specified size in bytes.
 * Returns a pointer to the region, or null if the region could not be allocated.
 */
void *alloc_allocate_region(u32 size_in_bytes);

/**
 * Returns the pointer to a previously allocated memory region back to the system, so
 * that it may be allocated again in the future.
 */
void alloc_deallocate_region(const void *pointer);
