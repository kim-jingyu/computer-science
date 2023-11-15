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
#define PUT(p, val) (*(unsigned int *)(p) = (unsigned int)(val))

// 특정 주소 p에 해당하는 블록의 사이즈와 가용 여부 반환
#define GET_SIZE(p)     (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)

// 특정 주소 p에 해당하는 블록의 header, footer의 포인터 주소 반환
#define HDRP(p)    ((char *)(p) - WSIZE)
#define FTRP(p)    ((char *)(p) + GET_SIZE(HDRP(p)) - DSIZE)

// 다음, 이전 블록의 헤더 이후의 시작 위치의 포인터 주소 반환
#define NEXT_BLKP(ptr)   (((char *)(ptr) + GET_SIZE((char *)(ptr) - WSIZE)))
#define PREV_BLKP(ptr)   (((char *)(ptr) - GET_SIZE((char *)(ptr) - DSIZE)))

#define GET_PRED(ptr)  (*(void**)(ptr))         // 이전 가용 블록의 주소
#define GET_SUCC(ptr)  (*(void**)((char*)(ptr) + WSIZE)) // 다음 가용 블록의 주소

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

// explicit free list 방식에서 추가
static void remove_free_block(void *bp);  // 가용 리스트에서 제거
static void add_free_block(void *bp);     // 가용 리스트에 추가

// 추가 정적 전역변수 선언
static char *free_listp;

int mm_init(void)
{
    // 초기 힙 영역의 크기 할당
    if((free_listp = mem_sbrk(8 * WSIZE)) == (void *)-1) {
        return -1;
    }

    PUT(free_listp, 0);                             // padding
    PUT(free_listp + (1 * WSIZE), PACK(DSIZE, 1));  // prologue header
    PUT(free_listp + (2 * WSIZE), PACK(DSIZE, 1));  // prologue footer
    PUT(free_listp + (3 * WSIZE), PACK(2 * DSIZE, 0));  // 첫 가용 블록의 헤더
    PUT(free_listp + (4 * WSIZE), NULL);            // 이전 가용 블록의 주소
    PUT(free_listp + (5 * WSIZE), NULL);            // 다음 가용 블록의 주소
    PUT(free_listp + (6 * WSIZE), PACK(2 * DSIZE, 0));  // 첫 가용 블록의 푸터
    PUT(free_listp + (7 * WSIZE), PACK(0, 1));      // epilogue header
    
    free_listp += (4 * WSIZE);   // 첫번째 가용 블록의 block pointer

    // 4096 비트만큼 힙 영역 확장
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {
        return -1;
    }
    
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
        return ptr;
    }

    // 리스트에 들어갈 수 있는 free 리스트가 없으면, 메모리를 확장하고 블록을 배치
    extend_size = MAX(adjust_size, CHUNKSIZE);
    if ((ptr = extend_heap(extend_size / WSIZE)) == NULL) {
        return NULL;
    }
    place(ptr, adjust_size);
    return ptr;
}

void mm_free(void *ptr) {
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));

    coalesce(ptr);
}

void *mm_realloc(void *ptr, size_t size)
{
    /* 예외 처리 */
    if (ptr == NULL) // 포인터가 NULL인 경우 할당만 수행
        return mm_malloc(size);

    if (size <= 0) // size가 0인 경우 메모리 반환만 수행
    {
        mm_free(ptr);
        return 0;
    }

    /* 새 블록에 할당 */
    void *newptr = mm_malloc(size); // 새로 할당한 블록의 포인터
    if (newptr == NULL)
        return NULL; // 할당 실패

    /* 데이터 복사 */
    size_t copySize = GET_SIZE(HDRP(ptr)) - DSIZE; // payload만큼 복사
    if (size < copySize)                           // 기존 사이즈가 새 크기보다 더 크면
        copySize = size;                           // size로 크기 변경 (기존 메모리 블록보다 작은 크기에 할당하면, 일부 데이터만 복사)

    memcpy(newptr, ptr, copySize); // 새 블록으로 데이터 복사

    /* 기존 블록 반환 */
    mm_free(ptr);
    
    return newptr;
}

static void remove_free_block(void *ptr) {
    if (ptr == free_listp) {
        free_listp = GET_SUCC(free_listp);
        return;
    }
    GET_SUCC(GET_PRED(ptr)) = GET_SUCC(ptr);
    if (GET_SUCC(ptr) != NULL) {
        GET_PRED(GET_SUCC(ptr)) = GET_PRED(ptr);
    }
}

static void add_free_block(void *ptr) {
    GET_SUCC(ptr) = free_listp;
    if (free_listp != NULL) {
        GET_PRED(free_listp) = ptr;
    }
    free_listp = ptr;
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
        add_free_block(ptr);
        return ptr;
    } else if (prev_alloc && !next_alloc) {
        // case2 - 직전 블록 할당, 직후 블록이 가용인 경우.
        remove_free_block(NEXT_BLKP(ptr));
        size += GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        PUT(HDRP(ptr), PACK(size, 0));
        PUT(FTRP(ptr), PACK(size, 0));
    } else if (!prev_alloc && next_alloc) {
        // case3 - 직전 블록 가용, 직후 블록이 할당된 경우.
        remove_free_block(PREV_BLKP(ptr));
        size += GET_SIZE(HDRP(PREV_BLKP(ptr)));
        PUT(FTRP(ptr), PACK(size, 0)); 
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
        ptr = PREV_BLKP(ptr);   // 블록 포인터를 직전 블록으로 옮긴다.
    } else {
        // case4 - 직전, 직후 블록이 모두 가용인 경우.
        remove_free_block(PREV_BLKP(ptr));
        remove_free_block(NEXT_BLKP(ptr));
        size += GET_SIZE(HDRP(PREV_BLKP(ptr))) + GET_SIZE(FTRP(NEXT_BLKP(ptr)));
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(size, 0));
        ptr = PREV_BLKP(ptr);   // 블록 포인터를 직전 블록으로 옮긴다.
    }
    add_free_block(ptr);
    return ptr; // 최종 가용 블록 주소를 반환한다.
}

static void *find_fit(size_t adjust_size)
{
    void *bp = free_listp;

    while (bp != NULL) {
        if ((adjust_size <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
        bp = GET_SUCC(bp);
    }
    return NULL;
}

static void place(void *ptr, size_t adjust_size)
{
    remove_free_block(ptr);

    size_t current_size = GET_SIZE(HDRP(ptr));

    if ((current_size - adjust_size) >= (2 * DSIZE)) {
        PUT(HDRP(ptr), PACK(adjust_size, 1));
        PUT(FTRP(ptr), PACK(adjust_size, 1));

        PUT(HDRP(NEXT_BLKP(ptr)), PACK((current_size - adjust_size), 0));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK((current_size - adjust_size), 0));
        add_free_block(NEXT_BLKP(ptr));
    } else {
        PUT(HDRP(ptr), PACK(current_size, 1));
        PUT(FTRP(ptr), PACK(current_size, 1));
    }
}