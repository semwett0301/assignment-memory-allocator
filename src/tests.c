#include "tests.h"

#include "mem.h"
#include "mem_internals.h"


void test_usual_malloc() {
    printf("\n%s\n", "TEST: usual malloc");

    void *heap = heap_init(15000);
    printf("\n%s\n", "Куча до аллокации:");
    debug_heap(stdout, heap);

    printf("\n%s\n", "Куча после аллокации:");
    _malloc(200);
    debug_heap(stdout, heap);

    printf("\n%s\n", "----------------------------------------------------------------");
}

void test_free_one_block() {
    printf("\n%s\n", "TEST 2: free one block");

    void *heap = heap_init(15000);

    void *block1 = _malloc(200);
    _malloc(400);

    printf("\n%s\n", "Куча до очищения блока:");
    debug_heap(stdout, heap);

    printf("\n%s\n", "Куча после очищения блока:");
    _free(block1);
    debug_heap(stdout, heap);


    printf("\n%s\n", "----------------------------------------------------------------");
}

void test_free_several_blocks() {
    printf("\n%s\n", "TEST 3: free several blocks");

    void *heap = heap_init(15000);
    void *block1 = (struct block_header *) _malloc(200);
    void *block2 = _malloc(400);

    printf("\n%s\n", "Куча до очищения блока:");
    debug_heap(stdout, heap);

    printf("\n%s\n", "Куча после очищения второго блока:");
    _free(block2);
    debug_heap(stdout, heap);

    printf("\n%s\n", "Куча после очищения первого блока:");
    _free(block1);
    debug_heap(stdout, heap);

    printf("\n%s\n", "----------------------------------------------------------------");
}

void test_memory_ended() {
    printf("\n%s\n", "TEST 4: memory ended");

    void *heap = heap_init(15000);
    _malloc(500);
    printf("\n%s\n", "Куча до выделения большого блока:");
    debug_heap(stdout, heap);

    _malloc(20000);
    printf("\n%s\n", "Куча после выделения большого блока:");
    debug_heap(stdout, heap);

    printf("\n%s\n", "----------------------------------------------------------------");
}

void test_memory_ended_and_shifted() {
    printf("\n%s\n", "TEST 5: memory ended and shifted");

    void *heap1 = heap_init(15000);
    printf("\n%s\n", "Изначальная куча:");
    debug_heap(stdout, heap1);

    struct block_header* tmp = (struct block_header*) heap1 + 16367;

    mmap((void*) tmp, 200000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED, 0, 0);

    printf("\n%s\n", "Итоговая куча:");
    _malloc(50000);
    debug_heap(stdout, heap1);

    printf("\n%s\n", "----------------------------------------------------------------");
}