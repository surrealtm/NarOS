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

TEST(Heap_Allocator, ReallocateAfterReuse) {
    Heap_Allocator allocator = create(112);
    u8 *first = heap_allocator_allocate(&allocator, 16);
    void *blocker = heap_allocator_allocate(&allocator, 16);
    ASSERT_NE(first, null);
    ASSERT_NE(blocker, null);

    set_memory(first, 0xa5, 16);
    heap_allocator_deallocate(&allocator, first);

    u8 *reused = heap_allocator_allocate(&allocator, 1);
    ASSERT_EQ(reused, first);
    reused[0] = 0x5a;

    const u8 *grown = heap_allocator_reallocate(&allocator, reused, 17);
    ASSERT_NE(grown, null);
    ASSERT_EQ(grown[0], 0x5a);
    for (u32 i = 1; i < 17; ++i) ASSERT_EQ(grown[i], 0);
}

TEST(Heap_Allocator, GrowLastSegment) {
    Heap_Allocator allocator = create(80);
    u8 *first = heap_allocator_allocate(&allocator, 16);
    ASSERT_NE(first, null);
    first[0] = 0x5a;

    ASSERT_EQ(heap_allocator_reallocate(&allocator, first, 32), first);
    ASSERT_EQ(first[0], 0x5a);
    void *last = heap_allocator_allocate(&allocator, 16);
    ASSERT_NE(last, null);

    heap_allocator_deallocate(&allocator, last);
    heap_allocator_deallocate(&allocator, first);
    ASSERT_EQ(heap_allocator_allocate(&allocator, 64), first);
}

TEST(Heap_Allocator, CoalesceBothNeighbors) {
    Heap_Allocator allocator = create(96);
    void *first = heap_allocator_allocate(&allocator, 16);
    void *middle = heap_allocator_allocate(&allocator, 16);
    void *last = heap_allocator_allocate(&allocator, 16);
    ASSERT_NE(first, null);
    ASSERT_NE(middle, null);
    ASSERT_NE(last, null);

    heap_allocator_deallocate(&allocator, first);
    heap_allocator_deallocate(&allocator, last);
    heap_allocator_deallocate(&allocator, middle);

    ASSERT_EQ(heap_allocator_allocate(&allocator, 80), first);
}

TEST(Heap_Allocator, CoalesceSplitSegment) {
    Heap_Allocator allocator = create(80);
    void *original = heap_allocator_allocate(&allocator, 64);
    ASSERT_NE(original, null);
    set_memory(original, 0xa5, 64);
    heap_allocator_deallocate(&allocator, original);

    void *split = heap_allocator_allocate(&allocator, 16);
    ASSERT_NE(split, null);
    heap_allocator_deallocate(&allocator, split);

    ASSERT_EQ(heap_allocator_allocate(&allocator, 64), original);
}

TEST(Heap_Allocator, SplitUpdatesFollowingSegment) {
    Heap_Allocator allocator = create(112);
    void *first = heap_allocator_allocate(&allocator, 64);
    void *last = heap_allocator_allocate(&allocator, 16);
    ASSERT_NE(first, null);
    ASSERT_NE(last, null);

    heap_allocator_deallocate(&allocator, first);
    void *split = heap_allocator_allocate(&allocator, 16);
    ASSERT_NE(split, null);
    heap_allocator_deallocate(&allocator, last);
    heap_allocator_deallocate(&allocator, split);

    ASSERT_EQ(heap_allocator_allocate(&allocator, 96), first);
}

TEST(Heap_Allocator, ShrinkCoalescesWithFreeSuccessor) {
    Heap_Allocator allocator = create(112);
    void *first = heap_allocator_allocate(&allocator, 64);
    void *last = heap_allocator_allocate(&allocator, 16);
    ASSERT_NE(first, null);
    ASSERT_NE(last, null);

    heap_allocator_deallocate(&allocator, last);
    ASSERT_EQ(heap_allocator_reallocate(&allocator, first, 16), first);
    ASSERT_NE(heap_allocator_allocate(&allocator, 64), null);
}

TEST(Heap_Allocator, ShrinkUpdatesFollowingSegment) {
    Heap_Allocator allocator = create(112);
    void *first = heap_allocator_allocate(&allocator, 64);
    void *last = heap_allocator_allocate(&allocator, 16);
    ASSERT_NE(first, null);
    ASSERT_NE(last, null);

    ASSERT_EQ(heap_allocator_reallocate(&allocator, first, 16), first);
    heap_allocator_deallocate(&allocator, last);

    ASSERT_NE(heap_allocator_allocate(&allocator, 64), null);
}

TEST(Heap_Allocator, AppendAfterTailMerge) {
    Heap_Allocator allocator = create(176);
    void *first = heap_allocator_allocate(&allocator, 16);
    void *middle = heap_allocator_allocate(&allocator, 16);
    void *last = heap_allocator_allocate(&allocator, 16);
    ASSERT_NE(first, null);
    ASSERT_NE(middle, null);
    ASSERT_NE(last, null);

    heap_allocator_deallocate(&allocator, middle);
    heap_allocator_deallocate(&allocator, last);
    void *appended = heap_allocator_allocate(&allocator, 64);
    ASSERT_NE(appended, null);
    heap_allocator_deallocate(&allocator, appended);

    ASSERT_EQ(heap_allocator_allocate(&allocator, 128), middle);
}

TEST(Heap_Allocator, ReuseWithoutSplittingAtHeaderThreshold) {
    Heap_Allocator allocator = create(48);
    u8 *original = heap_allocator_allocate(&allocator, 32);
    ASSERT_NE(original, null);
    set_memory(original, 0xa5, 32);
    heap_allocator_deallocate(&allocator, original);

    u8 *reused = heap_allocator_allocate(&allocator, 16);
    ASSERT_EQ(reused, original);
    for (u32 i = 0; i < 32; ++i) ASSERT_EQ(reused[i], 0);
    ASSERT_EQ(heap_allocator_allocate(&allocator, 16), null);
}

TEST(Heap_Allocator, SplitMovesRetainedBlockToSmallerBin) {
    Heap_Allocator allocator = create(192);
    u8 *first = heap_allocator_allocate(&allocator, 64);
    u8 *guard = heap_allocator_allocate(&allocator, 16);
    u8 *last = heap_allocator_allocate(&allocator, 64);
    ASSERT_NE(first, null);
    ASSERT_NE(guard, null);
    ASSERT_NE(last, null);
    set_memory(guard, 0xa5, 16);

    heap_allocator_deallocate(&allocator, first);
    heap_allocator_deallocate(&allocator, last);

    u8 *split = heap_allocator_allocate(&allocator, 16);
    ASSERT_NE(split, null);
    ASSERT_EQ(heap_allocator_allocate(&allocator, 48), first);
    ASSERT_EQ(heap_allocator_allocate(&allocator, 32), last);
    ASSERT_NE(split, last);
    ASSERT_EQ(heap_allocator_allocate(&allocator, 16), null);
    for (u32 i = 0; i < 16; ++i) ASSERT_EQ(guard[i], 0xa5);
}

TEST(Heap_Allocator, RepeatedSplitsPreserveLiveBlocksAndMergeBack) {
    Heap_Allocator allocator = create(144);
    u8 *original = heap_allocator_allocate(&allocator, 128);
    ASSERT_NE(original, null);
    heap_allocator_deallocate(&allocator, original);

    u8 *first = heap_allocator_allocate(&allocator, 16);
    u8 *second = heap_allocator_allocate(&allocator, 16);
    u8 *third = heap_allocator_allocate(&allocator, 16);
    u8 *fourth = heap_allocator_allocate(&allocator, 32);
    ASSERT_NE(first, null);
    ASSERT_NE(second, null);
    ASSERT_NE(third, null);
    ASSERT_EQ(fourth, original);
    set_memory(first, 0x11, 16);
    set_memory(second, 0x22, 16);
    set_memory(third, 0x33, 16);
    set_memory(fourth, 0x44, 32);
    for (u32 i = 0; i < 16; ++i) {
        ASSERT_EQ(first[i], 0x11);
        ASSERT_EQ(second[i], 0x22);
        ASSERT_EQ(third[i], 0x33);
    }
    for (u32 i = 0; i < 32; ++i) ASSERT_EQ(fourth[i], 0x44);

    heap_allocator_deallocate(&allocator, first);
    heap_allocator_deallocate(&allocator, third);
    for (u32 i = 0; i < 16; ++i) ASSERT_EQ(second[i], 0x22);
    for (u32 i = 0; i < 32; ++i) ASSERT_EQ(fourth[i], 0x44);
    heap_allocator_deallocate(&allocator, fourth);
    heap_allocator_deallocate(&allocator, second);

    ASSERT_EQ(heap_allocator_allocate(&allocator, 128), original);
}

TEST(Heap_Allocator, LiveBlockPreventsMerge) {
    Heap_Allocator allocator = create(96);
    void *first = heap_allocator_allocate(&allocator, 16);
    u8 *middle = heap_allocator_allocate(&allocator, 16);
    void *last = heap_allocator_allocate(&allocator, 16);
    ASSERT_NE(first, null);
    ASSERT_NE(middle, null);
    ASSERT_NE(last, null);
    set_memory(middle, 0xa5, 16);

    heap_allocator_deallocate(&allocator, first);
    heap_allocator_deallocate(&allocator, last);
    ASSERT_EQ(heap_allocator_allocate(&allocator, 48), null);
    for (u32 i = 0; i < 16; ++i) ASSERT_EQ(middle[i], 0xa5);

    heap_allocator_deallocate(&allocator, middle);
    ASSERT_EQ(heap_allocator_allocate(&allocator, 80), first);
}

TEST(Heap_Allocator, ExactFitAndFailedAppend) {
    Heap_Allocator allocator = create(48);
    u8 *block = heap_allocator_allocate(&allocator, 32);
    ASSERT_NE(block, null);
    set_memory(block, 0x5a, 32);

    ASSERT_EQ(heap_allocator_allocate(&allocator, 1), null);
    for (u32 i = 0; i < 32; ++i) ASSERT_EQ(block[i], 0x5a);

    heap_allocator_deallocate(&allocator, block);
    ASSERT_EQ(heap_allocator_allocate(&allocator, 32), block);
}

TEST(Heap_Allocator, AllocationSizeAlignmentBoundaries) {
    Heap_Allocator allocator = create(144);
    u8 *one = heap_allocator_allocate(&allocator, 1);
    u8 *fifteen = heap_allocator_allocate(&allocator, 15);
    u8 *sixteen = heap_allocator_allocate(&allocator, 16);
    u8 *seventeen = heap_allocator_allocate(&allocator, 17);
    ASSERT_NE(one, null);
    ASSERT_NE(fifteen, null);
    ASSERT_NE(sixteen, null);
    ASSERT_NE(seventeen, null);
    ASSERT_EQ((u32) one % 16, 0);
    ASSERT_EQ((u32) fifteen % 16, 0);
    ASSERT_EQ((u32) sixteen % 16, 0);
    ASSERT_EQ((u32) seventeen % 16, 0);
    for (u32 i = 0; i < 32; ++i) ASSERT_EQ(seventeen[i], 0);
    ASSERT_EQ(heap_allocator_allocate(&allocator, 1), null);
}

TEST(Heap_Allocator, LargeRequestFailsWithoutChangingExistingBlock) {
    Heap_Allocator allocator = create(48);
    ASSERT_EQ(heap_allocator_allocate(&allocator, 0x7ffffff0u), null);
    u8 *block = heap_allocator_allocate(&allocator, 32);
    ASSERT_NE(block, null);
    set_memory(block, 0x5a, 32);

    ASSERT_EQ(heap_allocator_reallocate(&allocator, block, 0x7ffffff0u), null);
    for (u32 i = 0; i < 32; ++i) ASSERT_EQ(block[i], 0x5a);
}

TEST(Heap_Allocator, FreeListSkipsSmallerSegmentInSameBin) {
    Heap_Allocator allocator = create(176);
    void *larger = heap_allocator_allocate(&allocator, 48);
    u8 *first_guard = heap_allocator_allocate(&allocator, 16);
    void *smaller = heap_allocator_allocate(&allocator, 32);
    u8 *second_guard = heap_allocator_allocate(&allocator, 16);
    ASSERT_NE(larger, null);
    ASSERT_NE(first_guard, null);
    ASSERT_NE(smaller, null);
    ASSERT_NE(second_guard, null);
    set_memory(first_guard, 0x11, 16);
    set_memory(second_guard, 0x22, 16);

    heap_allocator_deallocate(&allocator, larger);
    heap_allocator_deallocate(&allocator, smaller);
    ASSERT_EQ(heap_allocator_allocate(&allocator, 48), larger);
    ASSERT_EQ(heap_allocator_allocate(&allocator, 32), smaller);
    for (u32 i = 0; i < 16; ++i) {
        ASSERT_EQ(first_guard[i], 0x11);
        ASSERT_EQ(second_guard[i], 0x22);
    }
}

TEST(Heap_Allocator, ShrinkAtHeaderThresholdDoesNotSplit) {
    Heap_Allocator allocator = create(96);
    u8 *first = heap_allocator_allocate(&allocator, 48);
    u8 *guard = heap_allocator_allocate(&allocator, 16);
    ASSERT_NE(first, null);
    ASSERT_NE(guard, null);
    set_memory(first, 0x5a, 48);
    set_memory(guard, 0xa5, 16);

    ASSERT_EQ(heap_allocator_reallocate(&allocator, first, 32), first);
    ASSERT_EQ(heap_allocator_allocate(&allocator, 16), null);
    for (u32 i = 0; i < 32; ++i) ASSERT_EQ(first[i], 0x5a);
    ASSERT_EQ(heap_allocator_reallocate(&allocator, first, 48), first);
    for (u32 i = 32; i < 48; ++i) ASSERT_EQ(first[i], 0);
    for (u32 i = 0; i < 16; ++i) ASSERT_EQ(guard[i], 0xa5);
}

TEST(Heap_Allocator, ReallocateWithinSameSegmentKeepsBlock) {
    Heap_Allocator allocator = create(80);
    u8 *first = heap_allocator_allocate(&allocator, 32);
    u8 *guard = heap_allocator_allocate(&allocator, 16);
    ASSERT_NE(first, null);
    ASSERT_NE(guard, null);
    set_memory(first, 0x5a, 32);
    set_memory(guard, 0xa5, 16);

    ASSERT_EQ(heap_allocator_reallocate(&allocator, first, 17), first);
    ASSERT_EQ(heap_allocator_reallocate(&allocator, first, 32), first);
    ASSERT_EQ(heap_allocator_allocate(&allocator, 16), null);
    for (u32 i = 0; i < 17; ++i) ASSERT_EQ(first[i], 0x5a);
    for (u32 i = 17; i < 32; ++i) ASSERT_EQ(first[i], 0);
    for (u32 i = 0; i < 16; ++i) ASSERT_EQ(guard[i], 0xa5);
}

TEST(Heap_Allocator, ReallocateToZeroReleasesBlock) {
    Heap_Allocator allocator = create(32);
    void *block = heap_allocator_allocate(&allocator, 16);
    ASSERT_NE(block, null);

    ASSERT_EQ(heap_allocator_reallocate(&allocator, block, 0), null);
    ASSERT_EQ(heap_allocator_allocate(&allocator, 16), block);
}

TEST(Heap_Allocator, ShrinkOneAlignmentBeyondHeaderSplits) {
    Heap_Allocator allocator = create(112);
    u8 *first = heap_allocator_allocate(&allocator, 64);
    u8 *guard = heap_allocator_allocate(&allocator, 16);
    ASSERT_NE(first, null);
    ASSERT_NE(guard, null);
    set_memory(guard, 0xa5, 16);

    ASSERT_EQ(heap_allocator_reallocate(&allocator, first, 32), first);
    void *split = heap_allocator_allocate(&allocator, 16);
    ASSERT_EQ(split, first + 48);
    for (u32 i = 0; i < 16; ++i) ASSERT_EQ(guard[i], 0xa5);

    heap_allocator_deallocate(&allocator, split);
    heap_allocator_deallocate(&allocator, first);
    ASSERT_EQ(heap_allocator_allocate(&allocator, 64), first);
}

TEST(Heap_Allocator, GrowIntoFreeSuccessorExactFit) {
    Heap_Allocator allocator = create(80);
    u8 *first = heap_allocator_allocate(&allocator, 16);
    void *last = heap_allocator_allocate(&allocator, 32);
    ASSERT_NE(first, null);
    ASSERT_NE(last, null);
    set_memory(first, 0x5a, 16);
    set_memory(last, 0xa5, 32);
    heap_allocator_deallocate(&allocator, last);

    ASSERT_EQ(heap_allocator_reallocate(&allocator, first, 64), first);
    for (u32 i = 0; i < 16; ++i) ASSERT_EQ(first[i], 0x5a);
    for (u32 i = 16; i < 64; ++i) ASSERT_EQ(first[i], 0);
    ASSERT_EQ(heap_allocator_allocate(&allocator, 16), null);

    heap_allocator_deallocate(&allocator, first);
    ASSERT_EQ(heap_allocator_allocate(&allocator, 64), first);
}

TEST(Heap_Allocator, GrowIntoFreeSuccessorUpdatesFollowingSegment) {
    Heap_Allocator allocator = create(112);
    u8 *first = heap_allocator_allocate(&allocator, 16);
    void *middle = heap_allocator_allocate(&allocator, 16);
    u8 *last = heap_allocator_allocate(&allocator, 32);
    ASSERT_NE(first, null);
    ASSERT_NE(middle, null);
    ASSERT_NE(last, null);
    set_memory(last, 0xa5, 32);
    heap_allocator_deallocate(&allocator, middle);

    ASSERT_EQ(heap_allocator_reallocate(&allocator, first, 48), first);
    for (u32 i = 0; i < 32; ++i) ASSERT_EQ(last[i], 0xa5);
    heap_allocator_deallocate(&allocator, last);
    heap_allocator_deallocate(&allocator, first);
    ASSERT_EQ(heap_allocator_allocate(&allocator, 96), first);
}

TEST(Heap_Allocator, GrowAcrossFreeTailAndUnusedArena) {
    Heap_Allocator allocator = create(96);
    u8 *first = heap_allocator_allocate(&allocator, 16);
    void *last = heap_allocator_allocate(&allocator, 16);
    ASSERT_NE(first, null);
    ASSERT_NE(last, null);
    set_memory(first, 0x5a, 16);
    set_memory(last, 0xa5, 16);
    heap_allocator_deallocate(&allocator, last);

    ASSERT_EQ(heap_allocator_reallocate(&allocator, first, 64), first);
    for (u32 i = 0; i < 16; ++i) ASSERT_EQ(first[i], 0x5a);
    for (u32 i = 16; i < 64; ++i) ASSERT_EQ(first[i], 0);
    ASSERT_EQ(heap_allocator_allocate(&allocator, 16), null);

    heap_allocator_deallocate(&allocator, first);
    ASSERT_EQ(heap_allocator_allocate(&allocator, 64), first);
}

TEST(Heap_Allocator, GrowTailToExactArenaCapacity) {
    Heap_Allocator allocator = create(64);
    u8 *block = heap_allocator_allocate(&allocator, 16);
    ASSERT_NE(block, null);
    set_memory(block, 0x5a, 16);

    ASSERT_EQ(heap_allocator_reallocate(&allocator, block, 48), block);
    for (u32 i = 0; i < 16; ++i) ASSERT_EQ(block[i], 0x5a);
    for (u32 i = 16; i < 48; ++i) ASSERT_EQ(block[i], 0);
    ASSERT_EQ(heap_allocator_allocate(&allocator, 16), null);
}

TEST(Heap_Allocator, FullArenaMovesTailIntoEarlierFreeSegment) {
    Heap_Allocator allocator = create(144);
    void *first = heap_allocator_allocate(&allocator, 64);
    u8 *guard = heap_allocator_allocate(&allocator, 16);
    u8 *last = heap_allocator_allocate(&allocator, 16);
    ASSERT_NE(first, null);
    ASSERT_NE(guard, null);
    ASSERT_NE(last, null);
    set_memory(guard, 0xa5, 16);
    set_memory(last, 0x5a, 16);
    set_memory(first, 0xcc, 64);
    heap_allocator_deallocate(&allocator, first);

    u8 *grown = heap_allocator_reallocate(&allocator, last, 32);
    ASSERT_NE(grown, null);
    ASSERT_NE(grown, last);
    for (u32 i = 0; i < 16; ++i) {
        ASSERT_EQ(grown[i], 0x5a);
        ASSERT_EQ(guard[i], 0xa5);
    }
    for (u32 i = 16; i < 32; ++i) ASSERT_EQ(grown[i], 0);
}

TEST(Heap_Allocator, InsufficientSuccessorCanMoveToNewTail) {
    Heap_Allocator allocator = create(176);
    u8 *first = heap_allocator_allocate(&allocator, 16);
    void *neighbor = heap_allocator_allocate(&allocator, 16);
    u8 *guard = heap_allocator_allocate(&allocator, 16);
    ASSERT_NE(first, null);
    ASSERT_NE(neighbor, null);
    ASSERT_NE(guard, null);
    set_memory(first, 0x5a, 16);
    set_memory(guard, 0xa5, 16);
    heap_allocator_deallocate(&allocator, neighbor);

    u8 *grown = heap_allocator_reallocate(&allocator, first, 64);
    ASSERT_NE(grown, null);
    ASSERT_NE(grown, first);
    for (u32 i = 0; i < 16; ++i) {
        ASSERT_EQ(grown[i], 0x5a);
        ASSERT_EQ(guard[i], 0xa5);
    }
    ASSERT_EQ(heap_allocator_allocate(&allocator, 48), first);
}

TEST(Heap_Allocator, GrowNonTailByAppending) {
    Heap_Allocator allocator = create(144);
    u8 *first = heap_allocator_allocate(&allocator, 16);
    u8 *guard = heap_allocator_allocate(&allocator, 16);
    ASSERT_NE(first, null);
    ASSERT_NE(guard, null);
    set_memory(first, 0x5a, 16);
    set_memory(guard, 0xa5, 16);

    u8 *grown = heap_allocator_reallocate(&allocator, first, 32);
    ASSERT_NE(grown, null);
    ASSERT_NE(grown, first);
    for (u32 i = 0; i < 16; ++i) {
        ASSERT_EQ(grown[i], 0x5a);
        ASSERT_EQ(guard[i], 0xa5);
    }
    for (u32 i = 16; i < 32; ++i) ASSERT_EQ(grown[i], 0);
    ASSERT_EQ(heap_allocator_allocate(&allocator, 16), first);
}

TEST(Heap_Allocator, InsufficientSuccessorMovesToDifferentFreeSegment) {
    Heap_Allocator allocator = create(208);
    void *larger = heap_allocator_allocate(&allocator, 64);
    u8 *first_guard = heap_allocator_allocate(&allocator, 16);
    u8 *first = heap_allocator_allocate(&allocator, 16);
    void *small_neighbor = heap_allocator_allocate(&allocator, 16);
    u8 *last_guard = heap_allocator_allocate(&allocator, 16);
    ASSERT_NE(larger, null);
    ASSERT_NE(first_guard, null);
    ASSERT_NE(first, null);
    ASSERT_NE(small_neighbor, null);
    ASSERT_NE(last_guard, null);
    set_memory(first, 0x5a, 16);
    set_memory(first_guard, 0x11, 16);
    set_memory(last_guard, 0x22, 16);
    set_memory(larger, 0xcc, 64);
    set_memory(small_neighbor, 0xa5, 16);
    heap_allocator_deallocate(&allocator, larger);
    heap_allocator_deallocate(&allocator, small_neighbor);

    u8 *grown = heap_allocator_reallocate(&allocator, first, 64);
    ASSERT_NE(grown, null);
    ASSERT_NE(grown, first);
    for (u32 i = 0; i < 16; ++i) {
        ASSERT_EQ(grown[i], 0x5a);
        ASSERT_EQ(first_guard[i], 0x11);
        ASSERT_EQ(last_guard[i], 0x22);
    }
    for (u32 i = 16; i < 64; ++i) ASSERT_EQ(grown[i], 0);
    ASSERT_EQ(heap_allocator_allocate(&allocator, 48), first);
}

TEST(Heap_Allocator, FailedGrowthPreservesLiveBlocksAndCanBeFreed) {
    Heap_Allocator allocator = create(96);
    u8 *first = heap_allocator_allocate(&allocator, 16);
    void *middle = heap_allocator_allocate(&allocator, 16);
    u8 *last = heap_allocator_allocate(&allocator, 16);
    ASSERT_NE(first, null);
    ASSERT_NE(middle, null);
    ASSERT_NE(last, null);
    set_memory(first, 0x5a, 16);
    set_memory(last, 0xa5, 16);
    heap_allocator_deallocate(&allocator, middle);

    ASSERT_EQ(heap_allocator_reallocate(&allocator, first, 80), null);
    for (u32 i = 0; i < 16; ++i) {
        ASSERT_EQ(first[i], 0x5a);
        ASSERT_EQ(last[i], 0xa5);
    }
    heap_allocator_deallocate(&allocator, last);
    heap_allocator_deallocate(&allocator, first);
    ASSERT_EQ(heap_allocator_allocate(&allocator, 80), first);
}

TEST(Heap_Allocator, InsufficientFreeTailAndNoArenaSpace) {
    Heap_Allocator allocator = create(64);
    u8 *first = heap_allocator_allocate(&allocator, 16);
    void *last = heap_allocator_allocate(&allocator, 16);
    ASSERT_NE(first, null);
    ASSERT_NE(last, null);
    set_memory(first, 0x5a, 16);
    heap_allocator_deallocate(&allocator, last);

    ASSERT_EQ(heap_allocator_reallocate(&allocator, first, 64), null);
    for (u32 i = 0; i < 16; ++i) ASSERT_EQ(first[i], 0x5a);
    heap_allocator_deallocate(&allocator, first);
    ASSERT_EQ(heap_allocator_allocate(&allocator, 48), first);
}
