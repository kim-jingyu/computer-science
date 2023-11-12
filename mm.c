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

/* 크기와 할당 비트를 통합해서(또는 연산자 비트를 활용해서) header와 footer에 저장할 수 있는 값을 리턴
   header 및 footer 값 (size + allocated) 리턴
   더블워드 정렬로 인해서 size의 오른쪽 3-4자리는 비어있다.
   이곳에 0(free), 1(allocated) flag를 삽입한다.
*/
#define PACK(size, alloc) ((size) | (alloc))

/* GET 매크로: 주소 p가 참조하는 워드를 읽는 함수
   PUT 매크로: 주소 p가 가리키는 워드에 val을 저장하는 함수
*/
#define GET(p) (*(unsigned int *)(p))              // p가 가리키는 곳의 값을 가져옴
#define PUT(p, val) (*(unsigned int *)(p) = (val)) // p가 가리키는 곳에 val를 넣음

/* GET_SIZE 매크로: GET으로 읽어온 p 워드의 사이즈 정보를 읽어오는 함수
   작동 원리 -> 비트연산 활용.
   여기서 p는 (size + allocated) 값을 가지고있는 헤더이기 때문에 4바이트 중 29비트까지는 size 정보, 나머지 3비트는 allocated 정보, & 연산을 활용하여 블럭의 사이즈 값만 리턴이 가능하다.
   & ~0x7 을 해석하면, 0x7: 0000 0111 이고, ~0x7: 1111 1000이므로, 1011 0111 & 1111 1000 = 1011 0000 이고, 이때 사이즈는 176바이트이다.

   GET_ALLOC 매크로: 사이즈 정보를 무시한 채, 할당 여부를 나타내는 맨 뒷자리만 확인하는 함수
   & 0x1 => 1011 0111 & 0000 0001 = 0000 0001의 결과값인 1은 Allocated이다.
*/
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer
   HDRP 매크로: bp가 속한 블록의 헤더를 알려주는 함수
   FTRP 매크로: bp가 속한 블록의 풋터를 알려주는 함수
   블록 포인터 bp를 인자로 받아 블록의 header와 footer의 주소를 반환한다.
   포인터가 char* 형이므로, 숫자를 더하거나 빼면 그만큼의 바이트를 뺀 것과 같다.
   WSIZE 4를 뺀다는 것은 주소가 4byte(1워드) 뒤로 간다는 뜻이다. bp의 1 워드 뒤는 헤더이다.
*/
#define HDRP(bp) ((char *)(bp)-WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // 헤더+데이터+풋터 - (헤더+데이터)

/* Given block ptr bp, compute address of next and previous blocks */
/* 현재 block pointer에서 WSIZE를 빼서 header를 가리키게 하고, header에서 get size를 한다.
   그럼 현재 블록 크기를 반환하고,(헤더+데이터+풋터) 그걸 현재 bp에 더하면 next_bp가 나온다.
   */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))

/* 가용 리스트 조작을 위한 기본 상수 및 매크로 정의 */
#define ALIGNMENT 8

/* size보다 큰 가장 가까운 ALIGNMENT의 배수로 만들어준다. -> 정렬
   size 1 ~ 7 bytes: 8 bytes
   size 8 ~ 16 bytes: 16 bytes
   size 17 ~ 24 bytes: 24 bytes
 */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* global variable & functions
   항상 prologue block을 가리키는 정적 전역 변수를 설정한다.
*/
static char *heap_listp;

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;
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
    else
    {
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
