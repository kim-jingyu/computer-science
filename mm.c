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

#define WSIZE       4           // 워드, 헤더, 풋터 size (bytes)
#define DSIZE       8           // 더블 워드 size (bytes)
#define CHUNKSIZE   (1 << 12)   // 힙을 이만큼 늘린다는 의미 (bytes)

#define MAX(x, y)   ((x) > (y) ? (x) : (y))

#define PACK(size, alloc)   ((size) | (alloc))

#define GET(p)  (*(unsigned int *)(p))         // p가 가리키는 곳의 값을 가져옴
#define PUT(p, val) (*(unsigned int *)(p) = (val)) // p가 가리키는 곳에 val를 넣음

#define GET_SIZE(p)     (GET(p) & ~0x7)    // header와 footer의 사이즈 확인(8의 배수)
#define GET_ALLOC(p)    (GET(p) & 0x1)     // 현재 블록 가용 여부 확인(1이면 alloc, 0이면 free)

// bp. 즉, 현재 블록의 포인터로 현재 블록의 header 위치와 footer 위치를 반환
#define HDRP(bp)    ((char *)(bp) - WSIZE)
#define FTRP(bp)    ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// 다음과 이전 블록의 포인터 반환
#define NEXT_BLKP(bp)   (((char *)(bp) + GET_SIZE((char *)(bp) - WSIZE))) // bp + 현재 블록의 크기
#define PREV_BLKP(bp)   (((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))) // bp - 이전 블록의 크기

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

// 추가 정적 전역변수 선언
static char *heap_listp;
static char *last_bp;

int mm_init(void)
{
    // heap_listp가 힙의 최댓값 이상을 요청하면 실패
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;

    PUT(heap_listp, 0);                            // Alignment padding: 더블 워드 경계로 정렬된 미사용 패딩
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); // Prologue header
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); // Prologue footer
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     // Epilogue header
    heap_listp += (2 * WSIZE);                     // 정적 전역 변수는 늘 prologue block을 가리킨다.

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    last_bp = heap_listp;
    return 0;
}

void *mm_malloc(size_t size)
{
    size_t asize;   // 정렬 조건과 헤더 및 푸터를 고려하여 조정된 사이즈
    size_t extendsize;  // 메모리를 할당할 자리가 없을 때(no-fit일 때), 힙을 연장할 크기
    char *bp;

    // 가짜 요청 spurious request 무시
    if (size == 0)
        return NULL;

    if (size <= DSIZE)      // 정렬 조건 및 오버헤드를 고려하여 블록 사이즈를 조정한다.
        asize = 2 * DSIZE;  // 요청받은 크기가 2워드보다 작으면 할당 요청 블록을 16바이트로 만든다.
    else
        // 요청받은 크기가 2워드보다 크면, 8바이트의 배수 용량을 할당해준다.
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    // 가용 리스트에서 적합한 자리를 찾는다.
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize); // 가용 블록에 메모리를 할당한다.
        last_bp = bp; // last_bp를 바꿔준다.
        return bp;
    }

    // 만약 맞는 크기의 가용 블록이 없으면, 새로 힙을 늘려서 둘 중에 더 큰 값으로 사이즈를 정한다.
    extendsize = MAX(asize, CHUNKSIZE);    
    // extend_heap()은 word 단위로 인자를 받으므로 WSIZE로 나눠준다.
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    // 새 힙에 메모리를 할당한다.
    place(bp, asize);
    return bp;
}

void mm_free(void *bp)
{
    // 해당 블록의 size를 찾고, header와 footer의 정보를 수집한다.
    size_t size = GET_SIZE(HDRP(bp));
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
    
    copySize = GET_SIZE(HDRP(oldptr));
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

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE; // 워드가 홀수면 +1을 해서 공간을 할당한다.
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

    if (prev_alloc && next_alloc) {
        // case1 - 직전, 직후 블록이 모두 할당되어 있는 경우. last_bp를 갱신하고, 연결이 불가능하므로 그대로 bp를 리턴한다.
        last_bp = bp;
        return bp;
    } else if (prev_alloc && !next_alloc) {
        // case2 - 직전 블록 할당, 직후 블록이 가용인 경우.
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); // 직후 블록 사이즈를 현재 블록 사이즈와 합친다.
        PUT(HDRP(bp), PACK(size, 0));          // 현재 블록 헤더값을 갱신하고,
        PUT(FTRP(bp), PACK(size, 0));          // 현재 블록 푸터값을 갱신하다.
    } else if (!prev_alloc && next_alloc) {
        // case3 - 직전 블록 가용, 직후 블록이 할당된 경우.
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));   // 직전 블록 사이즈를 현재 블록 사이즈와 합친다.
        PUT(FTRP(bp), PACK(size, 0));            // 작전 블록 푸터값 갱신
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // 현재 블록 헤더값 갱신
        bp = PREV_BLKP(bp);                      // 블록 포인터를 직전 블록으로 옮긴다.
    } else {
        // case4 - 직전, 직후 블록이 모두 가용인 경우.
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // 직전 블록 헤더값 갱신
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0)); // 직후 블록 푸터값 갱신
        bp = PREV_BLKP(bp);                      // 블록 포인터를 직전 블록으로 옮긴다.
    }
    return bp; // 최종 가용 블록 주소를 반환한다.
}

/* next-fit */
static void *find_fit(size_t asize)
{
    // init할 때, 이미 last_bp를 설정하기 때문에 bp에 그 값을 넣어준다.
    char *bp = last_bp;

    for (bp = NEXT_BLKP(bp); GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (GET_ALLOC(HDRP(bp)) == 0 && (asize <= GET_SIZE(HDRP(bp)))) {
            // free 블록이고, asize보다 큰 블록은 last_bp에 저장한다.
            last_bp = bp;
            return bp;
        }
    }

    // 위에서 반환되지 않았으면, 마지막 last_bp부터 돌렸을 때, 끝까지 갔는데 fit을 찾을 수 없다는 뜻이고, 그럼 맨 처음부터 중간에 free된 블럭을 찾아야 한다.
    bp = heap_listp;
    while (bp < last_bp) {
        // 맨 처음부터 last_bp 전까지만 돌린다.
        bp = NEXT_BLKP(bp);
        if (GET_ALLOC(HDRP(bp)) == 0 && GET_SIZE(HDRP(bp)) >= asize) {
            last_bp = bp;
            return bp;
        }
    }
    
    return NULL; // No fit
}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2 * DSIZE)) {
        // 분할이 가능한 경우
        // 요청 용량만큼 블록을 배치
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        // 그러고 남은 블록에 header, footer 배치
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    } else {
        // 분할할 수 없다면. 즉, 할당하려는 사이즈에 비해 남은 부분이 4워드(16바이트보다 작다면) padding
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}