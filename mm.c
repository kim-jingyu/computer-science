/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "team 5",
    /* First member's full name */
    "kim-jingyu",
    /* First member's email address */
    "jingyu@jungle.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* 기본 size 상수 정의 */
#define WSIZE 4             // 워드, 헤더, 풋터 size (bytes)
#define DSIZE 8             // 더블 워드 size (bytes)
#define CHUNKSIZE (1 << 12) // 힙을 이만큼 늘린다는 의미 (bytes)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *)(p))              // p가 가리키는 곳의 값을 가져옴
#define PUT(p, val) (*(unsigned int *)(p) = (val)) // p가 가리키는 곳에 val를 넣음

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp)-WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // 헤더+데이터+풋터 - (헤더+데이터)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))

#define ALIGNMENT 8

#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

static char *heap_listp;

// 기본 함수 선언
int mm_init(void);
void *mm_malloc(size_t size);
void mm_free(void *ptr);
void *mm_realloc(void *ptr, size_t size);

// 추가 함수 선언
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t newsize);

int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;

    PUT(heap_listp, 0);                            // Alignment padding: 더블 워드 경계로 정렬된 미사용 패딩
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); // Prologue header
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); // Prologue footer
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     // Epilogue header
    heap_listp += (2 * WSIZE);                     // 정적 전역 변수는 늘 prologue block을 가리킨다.

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    return 0;
}

void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0)
        return NULL;

    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

void mm_free(void *bp)
{
    // 해당 블록의 size를 찾고, header와 footer의 정보를 수집한다.
    size_t size = GET_SIZE(bp);
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    // 만약 앞뒤 블록이 가용 상태면 연결한다.
    coalesce(bp);
}

void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

static void *extend_heap(size_t words)
{
    char *bp;    // 블록 포인터 선언
    size_t size; // 힙 영역의 크기를 담을 변수 선언

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE; // size는 힙의 총 바이트 수이다. words가 홀수면 1을 더한 후에 4바이트를 곱하고, 짝수라면 그대로 4바이트를 곱해서 size에 저장한다.
    if ((long)(bp = mem_sbrk(size)) == -1)                    // 새 메모리의 첫 부분을 bp로 한다. 이때, 주소값은 long으로 캐스팅한다.
        return NULL;

    // 새 가용 블록의 header와 footer를 정해주고, epilogue block을 가용 블록 맨 끝으로 옮긴다.
    PUT(HDRP(bp), PACK(size, 0));         // free block header
    PUT(FTRP(bp), PACK(size, 0));         // free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // new epilogue header. 항상 size는 0, alloc은 1

    // 만약 이전 블록이 가용 블록이라면 연결하고, 통합된 블록의 블록 포인터를 반환한다.
    return coalesce(bp);
}

/*
 * coalesce - 해당 가용 블록을 앞뒤 가용 블록과 연결하고, 가용 블록의 주소를 리턴한다.
 * bp 인자는 free 상태 블록의 payload를 가리키는 포인터이다.
 */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 직전 블록의 헤더에서의 가용 여부
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 직후 블록의 헤더에서의 가용 여부
    size_t size = GET_SIZE(HDRP(bp));

    // case1 - 직전, 직후 블록이 모두 할당되어 있는 경우. 연결이 불가능하므로 그대로 bp를 리턴한다.
    if (prev_alloc && next_alloc)
    {
        return bp;
    }

    // case2 - 직전 블록 할당, 직후 블록이 가용인 경우.
    else if (prev_alloc && !next_alloc)
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); // 직후 블록 사이즈를 현재 블록 사이즈와 합친다.
        PUT(HDRP(bp), PACK(size, 0));          // 현재 블록 헤더값을 갱신하고,
        PUT(FTRP(bp), PACK(size, 0));          // 현재 블록 푸터값을 갱신하다.
    }

    // case3 - 직전 블록 가용, 직후 블록이 할당된 경우.
    else if (!prev_alloc && next_alloc)
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));   // 직전 블록 사이즈를 현재 블록 사이즈와 합친다.
        PUT(FTRP(bp), PACK(size, 0));            // 작전 블록 헤더값 갱신
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // 현재 블록 푸터값 갱신
        bp = PREV_BLKP(bp);                      // 블록 포인터를 직전 블록으로 옮긴다.
    }

    // case4 - 직전, 직후 블록이 모두 가용인 경우.
    else
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)) + GET_SIZE(FTRP(NEXT_BLKP(bp))));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // 직전 블록 헤더값 갱신
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0)); // 직후 블록 푸터값 갱신
        bp = PREV_BLKP(bp);                      // 블록 포인터를 직전 블록으로 옮긴다.
    }
    return bp; // 최종 가용 블록 주소를 반환한다.
}

/* first-fit */
static void *find_fit(size_t asize)
{
    void *bp;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
        {
            return bp;
        }
    }
    return NULL; // No fit
}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    // 분할이 가능한 경우
    if ((csize - asize) >= (2 * DSIZE))
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    }

    // 분할할 수 없다면 남은 부분은 padding
    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}