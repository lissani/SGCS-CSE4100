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
 * provide your information in the following struct.
 ********************************************************/
team_t team = {
    /* Your student ID */
    "20231609",
    /* Your full name*/
    "HuiSeon Jeong",
    /* Your email address */
    "jhs040617@sogang.ac.kr",
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
#define WSIZE 4
#define DSIZE (WSIZE*2) // block size는 항상 8의 배수이므로, 하위 3비트는 flag로 활용 가능
#define MIN_BLOCK (DSIZE*2)
#define CHUNKSIZE (1<<13) // 초기 블록 & 힙 확장 기본 크기

#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define MAX(x,y) ((x)>(y)? (x):(y))
#define PACK(size, alloc) ((size) | (alloc)) // 크기와 할당 비트 통합
#define GET(p) (*(unsigned int *)(p)) // p가 참조하는 워드 리턴
#define PUT(p,val) (*(unsigned int *)(p) = (val)) // p가 참조하는 워드에 val 저장
#define GET_SIZE(p) (GET(p) & ~0x7) // 헤더와 풋터에서 size만 리턴
#define GET_ALLOC(p) (GET(p) & 0x1) // 할당 비트만 리턴

#define HDRP(bp) ((char *)(bp)-WSIZE) // 블록 포인터 -> 블록 헤더 포인터 리턴
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // 블록 포인터 -> 블록 풋터 포인터
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) // 다음 블록 bp 리턴
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) // 이전 블록 bp 리턴

void *extend_heap(size_t words); // 힙 부족 시, (words*4) 만큼 힙 확장
static void *coalesce(void *bp); // free된 블록과 인접한 블록들을 병합
static void *first_fit(size_t asize); // free block 탐색 방식 - first fit
static void *next_fit(size_t asize);
static void allocate(void *bp, size_t asize); // 블록을 할당하고, 필요시 분할
int mm_check(void); // heap consistency 점검

static char *heap_listp; // 첫 블럭 가리키는 포인터
static void *last_fit = NULL;

// ---- explicit free list 구현을 위한 변수, 매크로, 함수 ------
static void *free_listp;
#define NEXT(bp) (*(void **)(bp)) // free block 내부 next 포인터
#define PREV(bp) (*(void **)((char *)(bp) +WSIZE)) // free block 내부 prev 포인터
#define SET_PTR(p, val) (*(void **)(p) = (val)) // p 위치에 val 포인터 값 지정

static void put_free_block(void *bp); // 리스트에 free block 삽입
static void remove_free_block(void *bp); // 리스트에서 free block 제거

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    free_listp = NULL;
    // 4 워드 짜리 새로운 힙 리스트를 생성
    // 실패시 -1 반환
    if((heap_listp = mem_sbrk(4 * WSIZE)) == (void*)-1) return -1;

    PUT(heap_listp, 0); // 정렬 용 패딩
    PUT(heap_listp+(1*WSIZE), PACK(DSIZE, 1)); //prologue 블럭의 헤더
    PUT(heap_listp+(2*WSIZE), PACK(DSIZE, 1)); //prologue 블럭의 풋터
    PUT(heap_listp+(3*WSIZE), PACK(0, 1)); //epilogue 블럭의 헤더
    heap_listp += (2*WSIZE); //prologue의 payload

    // 힙 확장, 실패시 -1 리턴
    if(extend_heap(CHUNKSIZE / WSIZE) == NULL) return -1;

    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
   size_t asize; // 블록 사이즈 조정
   size_t extendsize; // 힙 확징 사이즈
   char *bp;

   if (size == 0) return NULL;
   if (size <= DSIZE) asize = MIN_BLOCK; // 블럭의 최소 크기
   else asize = DSIZE * ((size + (DSIZE) + (DSIZE -1)) / DSIZE); // 블럭 크기 8의 배수로 정렬

   if (free_listp != NULL && (unsigned long)free_listp < 0x1000) {
       printf("Corrupted free_listp: %p\n", free_listp);
       free_listp = NULL;
   }

   if ((bp = first_fit(asize)) != NULL) {
    allocate(bp, asize);
    //assert(mm_check());
    return bp;
   }

   extendsize = MAX(asize, CHUNKSIZE);
   if((bp = extend_heap(extendsize/WSIZE)) == NULL) return NULL;

   allocate(bp, asize);
   //assert(mm_check());
   return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    if(ptr == NULL) return;
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size,0));
    PUT(FTRP(ptr), PACK(size,0));

    coalesce(ptr);
    //assert(mm_check());
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    if (ptr == NULL) return mm_malloc(size);
    if (size == 0){
        mm_free(ptr);
        return NULL;
    }

    size_t old_size = GET_SIZE(HDRP(ptr));
    size_t new_size = ALIGN(size+DSIZE); // 새로 필요한 크기 올림 정렬
    if(new_size < MIN_BLOCK) new_size = MIN_BLOCK;

    if (old_size >= new_size) {
        return ptr;
    }

    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
    size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(ptr)));

    if(!next_alloc && (old_size + next_size) >= new_size){
        remove_free_block(NEXT_BLKP(ptr));
        size_t total_size = old_size+next_size;
        PUT(HDRP(ptr), PACK(total_size, 1));
        PUT(FTRP(ptr), PACK(total_size, 1));
        return ptr;
    }

    void *newptr = mm_malloc(size);
    if(newptr == NULL) return NULL;

    size_t copySize = old_size - DSIZE; // 실제 payload 크기만
    if (size < copySize) copySize = size;
    memcpy(newptr, ptr, copySize);
    mm_free(ptr);
    //assert(mm_check());
    return newptr;
}

void *extend_heap(size_t words){
    char *bp;
    size_t size;

    // 정렬 보장을 위해 항상 짝수개의 워드로
    size = (words%2) ? (words+1)*WSIZE : words*WSIZE;

    if((long)(bp = mem_sbrk(size)) == -1) return NULL; // 힙 확장

    // 새로 할당한 블럭을 free block으로
    PUT(HDRP(bp), PACK(size, 0)); // 헤더
    PUT(FTRP(bp), PACK(size, 0)); // 풋터
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // 새 epilogue 헤더

    // 이전 블럭이 free였다면 병합
    return coalesce(bp);
}

static void *coalesce(void *bp){
    size_t prev_alloc = 1;
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // prologue 블록 이후부터 검사
    if ((char*)bp > (char*)heap_listp + DSIZE) {
        prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    }

    if (prev_alloc && next_alloc) { // 앞 뒤 모두 할당
        put_free_block(bp);
        return bp;
    }
    else if (prev_alloc && !next_alloc){ // 뒤만 free
        remove_free_block(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    }
    else if (!prev_alloc && next_alloc){ // 앞만 free
        remove_free_block(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
    }
    else{
        remove_free_block(PREV_BLKP(bp));
        remove_free_block(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        bp = PREV_BLKP(bp);
    }
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    put_free_block(bp);

    return bp;
}

static void *first_fit(size_t asize){
    void *bp;

    if(free_listp == NULL) return NULL;

    for (bp = free_listp; bp!= NULL; bp = NEXT(bp)){
        if ((unsigned long)bp < 0x1000 || (unsigned long)bp > 0xffffffff) {
            printf("Invalid free block pointer: %p\n", bp);
            return NULL;
        }

        if(GET_SIZE(HDRP(bp)) >= asize) return bp;
    }
    return NULL;
}

static void *next_fit(size_t asize){
    void *bp;

    if(last_fit == NULL || GET_ALLOC(HDRP(last_fit))) {
        last_fit = free_listp;
    }

    for (bp = last_fit; bp!= NULL; bp = NEXT(bp)){
        if(!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize) {
            last_fit = bp;
            return bp;
        }
    }
    for (bp = free_listp; bp != last_fit && bp!= NULL; bp = NEXT(bp)){
        if(!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize) {
            last_fit = bp;
            return bp;
        }
    }
    return NULL;
}

static void allocate(void *bp, size_t asize){
    size_t cur_size = GET_SIZE(HDRP(bp));
    remove_free_block(bp);

    if (last_fit == bp) {
        last_fit = NEXT(bp);
    }
    // 할당 후 남은 크기가 최소 블럭 사이즈보다 크다면 split
    if((cur_size - asize) >= (MIN_BLOCK)){
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(cur_size-asize, 0));
        PUT(FTRP(bp), PACK(cur_size-asize, 0));
        put_free_block(bp);
    }
    else{
        PUT(HDRP(bp), PACK(cur_size, 1));
        PUT(FTRP(bp), PACK(cur_size, 1));
    }
}

int mm_check(void){
    void *bp = NEXT_BLKP(heap_listp);
    int free_count_heap = 0;
    int free_count_list = 0;

    while (GET_SIZE(HDRP(bp))>0){
        size_t header = GET(HDRP(bp));
        size_t footer = GET(FTRP(bp));
        size_t alloc = GET_ALLOC(HDRP(bp));
        size_t size = GET_SIZE(HDRP(bp));

        if ((size_t)bp % 8 != 0) {
            printf("%p not alligned\n", bp);
            return 0;
        }
        if (header != footer) {
            printf("header %#x and footer %#x mismatch at %p\n", (unsigned)header, (unsigned)footer, bp);
            return 0;
        }
        if (!alloc) {
            free_count_heap++;
            if (GET_SIZE(HDRP(NEXT_BLKP(bp))) > 0 && !GET_ALLOC(HDRP(NEXT_BLKP(bp)))) {
                printf("ERROR: consecutive free blocks at %p and %p\n", bp, NEXT_BLKP(bp));
                return 0;
            }
        }
        bp = NEXT_BLKP(bp);
    }

    for (bp = free_listp; bp != NULL; bp = NEXT(bp)) {
        if ((unsigned long)bp < 0x1000) {
            printf("ERROR: invalid free list pointer %p\n", bp);
            return 0;
        }
        if (GET_ALLOC(HDRP(bp))) {
            printf("ERROR: allocated block %p in free list\n", bp);
            return 0;
        }
        free_count_list++;
    }

    if (free_count_heap != free_count_list) {
        printf("ERROR: free count mismatch - heap:%d, list:%d\n", 
               free_count_heap, free_count_list);
        return 0;
    }
    return 1;
}

static void put_free_block(void *bp){
    // LIFO 방식을 사용하여 새로운 블럭을 가장 앞에 붙인다
    NEXT(bp) = free_listp;
    PREV(bp) = NULL;

    if (free_listp != NULL) PREV(free_listp) = bp;

    free_listp = bp;
}

static void remove_free_block(void *bp){
    if(bp == NULL) return;

    void *next = NEXT(bp);
    void *prev = PREV(bp);

    if (prev != NULL) NEXT(prev) = next;
    else free_listp = next;

    if (next != NULL) PREV(next) = prev;

    NEXT(bp) = NULL;
    PREV(bp) = NULL;
}