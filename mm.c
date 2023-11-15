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

#define WSIZE       4           // 워드 사이즈
#define DSIZE       8           // 더블 워드 사이즈
#define CHUNKSIZE   (1 << 12)   // 초기 가용 블록과 힙 확장을 위한 기본 크기

#define MAX(x, y)   ((x) > (y) ? (x) : (y))

// 사이즈와 할당 비트를 합쳐서 header, footer에 저장할 수 있는 값 반환
#define PACK(size, alloc)   ((size) | (alloc))

// 특정 주소 p에 워드 읽고, 쓰기
#define GET(p)  (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

// 특정 주소 p에 해당하는 블록의 사이즈와 가용 여부 반환
#define GET_SIZE(p)     (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)

// 특정 주소 p에 해당하는 블록의 header, footer의 포인터 주소 반환
#define HDRP(bp)    ((char *)(bp) - WSIZE)
#define FTRP(bp)    ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// 다음, 이전 블록의 헤더 이후의 시작 위치의 포인터 주소 반환
#define NEXT_BLKP(bp)   (((char *)(bp) + GET_SIZE((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)   (((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE)))

// 기본 함수 선언
int mm_init(void);
void *mm_malloc(size_t size);
void mm_free(void *ptr);
void *mm_realloc(void *ptr, size_t size);

// 추가 함수 선언
static void *extend_heap(size_t words);
static void *coalesce(void *ptr);
static void *find_fit(size_t adjust_size);
static void place(void *ptr, size_t adjust_size);

// 추가 정적 전역변수 선언
static char *heap_listp;
static char *last_blockp;

int mm_init(void)
{
    // 초기 힙 영역의 크기 할당
    if((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) {
        return -1;
    }

    PUT(heap_listp, 0);                             // padding
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));  // prologue header
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));  // prologue footer
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));      // epilogue header
    heap_listp += (2 * WSIZE);

    // 4096 비트만큼 힙 영역 확장
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {
        return -1;
    }
    last_blockp = heap_listp;
    return 0;
}

void *mm_malloc(size_t size)
{
    size_t adjust_size;
    size_t extend_size;
    char *ptr;

    // 의미없는 요청은 처리 안함
    if (size == 0) {
        return NULL;
    }

    // 더블 워드 정렬 제한 조건을 만족하기 위해, 할당할 공간의 크기 계산
    if (size <= DSIZE) {
        adjust_size = 2 * DSIZE;    // header(4byte) + footer(4byte) + 나머지(8byte)
    } else {
        adjust_size = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);
    }

    // 가용 블록을 가용 리스트에서 검색하고, 할당기는 요청한 블록을 배치
    if ((ptr = find_fit(adjust_size)) != NULL) {
        place(ptr, adjust_size);
        last_blockp = ptr;
        return ptr;
    }

    // 리스트에 들어갈 수 있는 free 리스트가 없으면, 메모리를 확장하고 블록을 배치
    extend_size = MAX(adjust_size, CHUNKSIZE);
    if ((ptr = extend_heap(extend_size / WSIZE)) == NULL) {
        return NULL;
    }
    place(ptr, adjust_size);
    last_blockp = ptr;
    return ptr;
}

void mm_free(void *ptr) {
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));

    coalesce(ptr);
}

void *mm_realloc(void *bp, size_t size) {
    // if (ptr == NULL) {
    //     return mm_malloc(size);
    // }

    // if (size == 0) {
    //     mm_free(ptr);
    //     return;
    // }

    // void *new_ptr = mm_malloc(size);
    // if (new_ptr == NULL) {
    //     return NULL;
    // }

    // size_t entire_size = GET_SIZE(HDRP(ptr));
    // if (size < entire_size) {
    //     entire_size = size;
    // }
    // memcpy(new_ptr, ptr, entire_size);  // ptr 위치에서 entire_size 만큼의 크기를 new_ptr에 복사
    // mm_free(ptr);   // 기존 메모리는 할당 해제
    // return new_ptr;
    size_t old_size = GET_SIZE(HDRP(bp));
    size_t new_size = size + (2 * WSIZE);

    // new_size가 old_size보다 작거나 같으면 기존 bp 그대로 사용
    if (new_size <= old_size) {
        return bp;
    }
    // new_size가 old_size보다 크면 사이즈 변경
    else {
        size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
        size_t current_size = old_size + GET_SIZE(HDRP(NEXT_BLKP(bp)));

        // next block이 free 상태이고 old, next block의 사이즈 합이 new_size보다 크면, next block에 합쳐서 사용
        if (!next_alloc && current_size >= new_size) {
            PUT(HDRP(bp), PACK(current_size, 1));
            PUT(FTRP(bp), PACK(current_size, 1));
            return bp;
        }
        // 아니면 새로 block 만들어서 거기로 옮기기
        else {
            void *new_bp = mm_malloc(new_size);
            place(new_bp, new_size);
            memcpy(new_bp, bp, new_size);  // old_bp로부터 new_size만큼의 문자를 new_bp로 복사하는 함수
            mm_free(bp);
            return new_bp;
        }
    }
}

static void *extend_heap(size_t words)
{
    char *ptr;
    size_t size;

    // 더블 워드 정렬 위해 짝수 개수의 워드 할당
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(ptr = mem_sbrk(size)) == -1) {
        return NULL;
    }

    PUT(HDRP(ptr), PACK(size, 0));  // 기존 epilogue header 초기화(free 블록의 header)
    PUT(FTRP(ptr), PACK(size, 0));  // free 블록의 footer
    PUT(HDRP(NEXT_BLKP(ptr)), PACK(0, 1));  // new epilogue header

    // 만약 이전 블록이 free 였다면, 통합
    return coalesce(ptr);
}

static void *coalesce(void *ptr)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(ptr))); // 이전 블록의 할당 여부
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr))); // 다음 블록의 할당 여부
    size_t size = GET_SIZE(HDRP(ptr));   // 현재 블록의 사이즈

    if (prev_alloc && next_alloc) {
        // case1 - 직전, 직후 블록이 모두 할당되어 있는 경우.
        last_blockp = ptr;
        return ptr;
    } else if (prev_alloc && !next_alloc) {
        // case2 - 직전 블록 할당, 직후 블록이 가용인 경우.
        size += GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        PUT(HDRP(ptr), PACK(size, 0));
        PUT(FTRP(ptr), PACK(size, 0));
    } else if (!prev_alloc && next_alloc) {
        // case3 - 직전 블록 가용, 직후 블록이 할당된 경우.
        size += GET_SIZE(HDRP(PREV_BLKP(ptr)));
        PUT(FTRP(ptr), PACK(size, 0)); 
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
        ptr = PREV_BLKP(ptr);   // 블록 포인터를 직전 블록으로 옮긴다.
    } else {
        // case4 - 직전, 직후 블록이 모두 가용인 경우.
        size += GET_SIZE(HDRP(PREV_BLKP(ptr))) + GET_SIZE(FTRP(NEXT_BLKP(ptr)));
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(size, 0));
        ptr = PREV_BLKP(ptr);   // 블록 포인터를 직전 블록으로 옮긴다.
    }
    last_blockp = ptr;
    return ptr; // 최종 가용 블록 주소를 반환한다.
}

/* next-fit */
static void *find_fit(size_t adjust_size)
{
    char *ptr = last_blockp;

    for (ptr = NEXT_BLKP(ptr); GET_SIZE(HDRP(ptr)) > 0; ptr = NEXT_BLKP(ptr)) {
        if (!GET_ALLOC(HDRP(ptr)) && (adjust_size <= GET_SIZE(HDRP(ptr)))) {
            last_blockp = ptr;
            return ptr;
        }
    }

    // 끝까지 갔는데 fit을 찾을 수 없는 경우, 맨 처음부터 중간에 free된 블럭을 찾아야 한다.
    ptr = heap_listp;
    while (ptr < last_blockp) {
        ptr = NEXT_BLKP(ptr);
        if (!GET_ALLOC(HDRP(ptr)) && GET_SIZE(HDRP(ptr)) >= adjust_size) {
            last_blockp = ptr;
            return ptr;
        }
    }
    
    return NULL; // No fit
}

static void place(void *ptr, size_t adjust_size)
{
    size_t entire_size = GET_SIZE(HDRP(ptr));

    // 블록 공간 분할
    if ((entire_size - adjust_size) >= (2 * DSIZE)) {
        // 블록 할당
        PUT(HDRP(ptr), PACK(adjust_size, 1));
        PUT(FTRP(ptr), PACK(adjust_size, 1));
        ptr = NEXT_BLKP(ptr);
        // 가용 블록
        PUT(HDRP(ptr), PACK(entire_size - adjust_size, 0));
        PUT(FTRP(ptr), PACK(entire_size - adjust_size, 0));
    } else {
        // 분할할 수 없으면 그냥 해당 블록 사용
        PUT(HDRP(ptr), PACK(entire_size, 1));
        PUT(FTRP(ptr), PACK(entire_size, 1));
    }
}