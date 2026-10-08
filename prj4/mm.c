#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "mm.h"
#include "memlib.h"

team_t team = {
    "20211605",
    "Ha Jihoon",
    "gkwlgns02@sogang.ac.kr"
};

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)
#define OVERHEAD 8

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define PACK(size, alloc) ((size) | (alloc))
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

// Basic block operations
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

// Free list navigation
#define NEXT_FREEP(bp) (*(void **)(bp))
#define PREV_FREEP(bp) (*(void **)((char *)(bp) + WSIZE))

static char *heap_listp = 0;

// Individual segregated free lists for different size classes
static void *free_list_16 = 0;
static void *free_list_32 = 0;
static void *free_list_64 = 0;
static void *free_list_128 = 0;
static void *free_list_256 = 0;
static void *free_list_512 = 0;
static void *free_list_1024 = 0;
static void *free_list_2048 = 0;
static void *free_list_4096 = 0;
static void *free_list_large = 0;

static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void remove_block(void *bp);
static void insert_block(void *bp);
static void **get_list_ptr(size_t size);

// Returns pointer to the appropriate segregated list based on size
static void **get_list_ptr(size_t size) {
    if (size <= 16) return &free_list_16;
    if (size <= 32) return &free_list_32;
    if (size <= 64) return &free_list_64;
    if (size <= 128) return &free_list_128;
    if (size <= 256) return &free_list_256;
    if (size <= 512) return &free_list_512;
    if (size <= 1024) return &free_list_1024;
    if (size <= 2048) return &free_list_2048;
    if (size <= 4096) return &free_list_4096;
    return &free_list_large;
}

// Initialize heap and segregated free lists
int mm_init(void) {
    free_list_16 = NULL;
    free_list_32 = NULL;
    free_list_64 = NULL;
    free_list_128 = NULL;
    free_list_256 = NULL;
    free_list_512 = NULL;
    free_list_1024 = NULL;
    free_list_2048 = NULL;
    free_list_4096 = NULL;
    free_list_large = NULL;
    
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;

    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));

    heap_listp += (2 * WSIZE);

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    return 0;
}

// Extend heap and return pointer to new free block
static void *extend_heap(size_t words) {
    char *bp;
    size_t size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);
}

// Allocate block using segregated free lists
void *mm_malloc(size_t size) {
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0) return NULL;

    // Calculate aligned size including overhead
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    // No suitable block found, extend heap
    extendsize = MAX(asize, CHUNKSIZE);
    if (asize > CHUNKSIZE) {
        extendsize = asize;
    }
    
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

// Free block and add to appropriate segregated list
void mm_free(void *bp) {
    if (bp == NULL) return;

    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

// Reallocate with optimization for in-place expansion
void *mm_realloc(void *ptr, size_t size) {
    if (ptr == NULL) return mm_malloc(size);
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    size_t oldsize = GET_SIZE(HDRP(ptr));
    size_t asize;
    
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    if (asize <= oldsize) {
        return ptr;
    }

    // Try expanding into next free block
    void *next_bp = NEXT_BLKP(ptr);
    size_t next_size = GET_SIZE(HDRP(next_bp));
    size_t next_alloc = GET_ALLOC(HDRP(next_bp));
    
    if (!next_alloc && (oldsize + next_size >= asize)) {
        remove_block(next_bp);
        PUT(HDRP(ptr), PACK(oldsize + next_size, 1));
        PUT(FTRP(ptr), PACK(oldsize + next_size, 1));
        return ptr;
    }

    // Allocate new block and copy data
    void *newptr = mm_malloc(size);
    if (newptr == NULL) return NULL;

    size_t copy_size = MIN(size, oldsize - DSIZE);
    memcpy(newptr, ptr, copy_size);
    mm_free(ptr);
    return newptr;
}

// Coalesce free blocks and add to segregated list
static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {
        // No coalescing needed
    } else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        remove_block(NEXT_BLKP(bp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    } else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
        remove_block(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    } else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        remove_block(PREV_BLKP(bp));
        remove_block(NEXT_BLKP(bp));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    insert_block(bp);
    return bp;
}

// Remove block from segregated free list
static void remove_block(void *bp) {
    if (bp == NULL) return;
    
    void **list_ptr = get_list_ptr(GET_SIZE(HDRP(bp)));
    
    if (PREV_FREEP(bp))
        NEXT_FREEP(PREV_FREEP(bp)) = NEXT_FREEP(bp);
    else
        *list_ptr = NEXT_FREEP(bp);

    if (NEXT_FREEP(bp))
        PREV_FREEP(NEXT_FREEP(bp)) = PREV_FREEP(bp);
}

// Insert block into segregated list maintaining size order
static void insert_block(void *bp) {
    if (bp == NULL) return;
    
    size_t size = GET_SIZE(HDRP(bp));
    void **list_ptr = get_list_ptr(size);
    void *search_ptr = *list_ptr;
    void *insert_ptr = NULL;

    // Find correct position to maintain ascending order
    while (search_ptr != NULL && GET_SIZE(HDRP(search_ptr)) < size) {
        insert_ptr = search_ptr;
        search_ptr = NEXT_FREEP(search_ptr);
    }

    if (insert_ptr != NULL) {
        NEXT_FREEP(bp) = NEXT_FREEP(insert_ptr);
        PREV_FREEP(bp) = insert_ptr;
        NEXT_FREEP(insert_ptr) = bp;
        if (NEXT_FREEP(bp) != NULL)
            PREV_FREEP(NEXT_FREEP(bp)) = bp;
    } else {
        NEXT_FREEP(bp) = *list_ptr;
        PREV_FREEP(bp) = NULL;
        if (*list_ptr != NULL)
            PREV_FREEP(*list_ptr) = bp;
        *list_ptr = bp;
    }
}

// Search segregated lists for best fit
static void *find_fit(size_t asize) {
    void *bp;
    void *best_fit = NULL;
    size_t best_size = (size_t)-1;

    // Local array for iteration convenience
    void **lists[] = {
        &free_list_16, &free_list_32, &free_list_64, &free_list_128,
        &free_list_256, &free_list_512, &free_list_1024, &free_list_2048,
        &free_list_4096, &free_list_large
    };
    
    size_t thresholds[] = {16, 32, 64, 128, 256, 512, 1024, 2048, 4096, (size_t)-1};
    
    int start_index = 0;
    while (start_index < 10 && asize > thresholds[start_index]) {
        start_index++;
    }

    // Search starting from appropriate size class
    for (int i = start_index; i < 10; i++) {
        bp = *lists[i];
        
        while (bp != NULL) {
            size_t bp_size = GET_SIZE(HDRP(bp));
            if (bp_size >= asize) {
                if (bp_size < best_size) {
                    best_fit = bp;
                    best_size = bp_size;
                }
                if (bp_size == asize) {
                    return bp;
                }
            }
            bp = NEXT_FREEP(bp);
        }
        
        if (best_fit != NULL && i == start_index) {
            return best_fit;
        }
    }
    
    return best_fit;
}

// Place allocated block and split if remainder is large enough
static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));
    remove_block(bp);

    if ((csize - asize) >= (2 * DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        
        void *next = NEXT_BLKP(bp);
        PUT(HDRP(next), PACK(csize - asize, 0));
        PUT(FTRP(next), PACK(csize - asize, 0));
        
        coalesce(next);
    } else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}