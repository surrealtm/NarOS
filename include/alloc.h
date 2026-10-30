#pragma once

#include "base.h"

typedef enum Memory_Unit {
    MEMORY_UNIT_Byte     = 1,
    MEMORY_UNIT_Kilobyte = 1024 * MEMORY_UNIT_Byte,
    MEMORY_UNIT_Megabyte = 1024 * MEMORY_UNIT_Kilobyte,
    MEMORY_UNIT_Gigabyte = 1024 * MEMORY_UNIT_Megabyte,
} Memory_Unit;

typedef struct Stack_Allocator {
    void *base;
    u64 capacity;
    u64 occupied;
} Stack_Allocator;

b8 stack_allocator_create(Stack_Allocator *allocator, u64 capacity);
void *stack_allocator_push(Stack_Allocator *allocator, u64 size_in_bytes, u64 alignment);
u64 stack_allocator_mark(const Stack_Allocator *allocator);
void stack_allocator_reset(Stack_Allocator *allocator, u64 mark);

#define PushStruct(stack_allocator, type) stack_allocator_push(stack_allocator, sizeof(type), STRUCT_ALIGNMENT(type))
