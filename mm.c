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
    ""
};

/* 가용 리스트 조작을 위한 기본 상수 및 매크로 정의 */
/* 기본 size 상수 정의 */
#define WSIZE 4 // 워드, 헤더, 풋터 size (bytes)
#define DSIZE 8 // 더블 워드 size (bytes)
#define CHUNKSIZE (1<<12) // 힙을 이만큼 늘린다는 의미 (bytes)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* 사이즈와 워드로 할당된 비트를 packing */
#define PACK(size, alloc) ((size) | (alloc))

/* 주소 p에 있는 워드를 읽고 쓰기. 
여기서 p는 (void*) 포인터로 직접 역참조가 불가능함 */
#define GET(p) (*(unsigned int *)(p)) // p가 가리키는 곳의 값을 가져옴
#define PUT(p, val) (*(unsigned int *)(p) = (val)) // p가 가리키는 곳에 val를 넣음

/* 주소 p의 사이즈와 여기에 할당된 필드를 읽음 */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // 헤더+데이터+풋터 - (헤더+풋터)

/* Given block ptr bp, compute address of next and previous blocks */
/* 현재 block pointer에서 WSIZE를 빼서 header를 가리키게 하고, header에서 get size를 한다.
   그럼 현재 블록 크기를 반환하고,(헤더+데이터+풋터) 그걸 현재 bp에 더하면 next_bp가 나온다.
   */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1)
	return NULL;
    else {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
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














