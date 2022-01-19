#include "tests.h"

#include "mem.h"
#include "mem_internals.h"
#include "util.h"

static struct block_header* get_block_from_contents(void* data) {
    return (struct block_header *) ((uint8_t *) data - offsetof(struct block_header, contents));
}

static void test_usual_malloc() {
    printf("\n%s\n", "TEST: usual malloc");

    struct block_header *heap = (struct block_header *) heap_init(15000);
    printf("\n%s\n", "Куча до аллокации:");
    debug_heap(stdout, heap);

    printf("\n%s\n", "Куча после аллокации:");
    void* block_content = _malloc(200);

    if (!block_content) {
        err("Первый тест не пройден: _malloc вернул NULL");
    }

    if (heap->is_free != false || heap->capacity.bytes != 200) {
        err("Первый тест не пройден: capacity или is_free у первого блока некорректны");
    }

    debug_heap(stdout, heap);

    printf("\n%s\n", "Тест 1 пройден");

    printf("\n%s\n", "----------------------------------------------------------------");
}

static void test_free_one_block() {
    printf("\n%s\n", "TEST 2: free one block");

    void *heap = heap_init(15000);

    void *block_сontent1 = _malloc(200);
    void *block_сontent2 = _malloc(400);

    if (!block_сontent1 || !block_сontent2) {
        err("Второй тест не пройден: _malloc вернул NULL");
    }

    printf("\n%s\n", "Куча до очищения блока:");
    debug_heap(stdout, heap);

    printf("\n%s\n", "Куча после очищения блока:");
    _free(block_сontent1);
    debug_heap(stdout, heap);

    struct block_header *block1 = get_block_from_contents(block_сontent1);
    struct block_header *block2 = get_block_from_contents(block_сontent2);

    if (!block1->is_free || block2->is_free) {
        err("Второй тест не пройден: _free отработал некорректно");
    }

    printf("\n%s\n", "Тест 2 пройден");

    printf("\n%s\n", "----------------------------------------------------------------");
}

static void test_free_several_blocks() {
    printf("\n%s\n", "TEST 3: free several blocks");

    void *heap = heap_init(15000);
    void *block1_content = _malloc(200);
    void *block2_content = _malloc(400);

    if (!block1_content || !block2_content) {
        err("Третий тест не пройден: _malloc вернул NULL");
    }

    printf("\n%s\n", "Куча до очищения блока:");
    debug_heap(stdout, heap);

    printf("\n%s\n", "Куча после очищения второго блока:");
    _free(block2_content);

    if (!get_block_from_contents(block2_content)->is_free) {
        err("Третий тест не пройден: _free отработал некорректно");
    }

    debug_heap(stdout, heap);

    printf("\n%s\n", "Куча после очищения первого блока:");
    _free(block1_content);

    if (!get_block_from_contents(block1_content)->is_free) {
        err("Третий тест не пройден: _free отработал некорректно");
    }

    debug_heap(stdout, heap);

    printf("\n%s\n", "Тест 3 пройден");
    printf("\n%s\n", "----------------------------------------------------------------");
}

static void test_memory_ended() {
    printf("\n%s\n", "TEST 4: memory ended");

    void *heap = heap_init(15000);
    void *block1_content = _malloc(500);

    if (!block1_content) {
        err("Четвертый тест не пройден: _malloc вернул NULL");
    }

    printf("\n%s\n", "Куча до выделения большого блока:");
    debug_heap(stdout, heap);

    void *block2_content = _malloc(20000);

    if (!block2_content) {
        err("Четвертый тест не пройден: _malloc вернул NULL");
    }

    if ((uint8_t *) get_block_from_contents(block1_content)->contents + get_block_from_contents(block1_content)->capacity.bytes != (uint8_t*) get_block_from_contents(block2_content)) {
        err("\"Третий тест не пройден: блок не был выделен вплотную к предыдущему блоку");
    }

    printf("\n%s\n", "Куча после выделения большого блока:");
    debug_heap(stdout, heap);

    printf("\n%s\n", "Тест 4 пройден");
    printf("\n%s\n", "----------------------------------------------------------------");
}

static void test_memory_ended_and_shifted() {
    printf("\n%s\n", "TEST 5: memory ended and shifted");

    void *heap1 = heap_init(15000);
    printf("\n%s\n", "Изначальная куча:");
    debug_heap(stdout, heap1);

    struct block_header* tmp = (struct block_header*) heap1 + 16367;

    mmap((void*) tmp, 200000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED, 0, 0);

    void *big_block_content = _malloc(50000);

    if (!big_block_content) {
        err("Пятый тест не пройден: _malloc вернул NULL");
    }

    if ((uint8_t*) get_block_from_contents(heap1) + get_block_from_contents(heap1)->capacity.bytes ==
            get_block_from_contents(big_block_content)) {
        err("Пятый тест не пройден: блок был выделен вплотную к предыдущему блоку");
    }

    printf("\n%s\n", "Итоговая куча:");
    debug_heap(stdout, heap1);

    printf("\n%s\n", "Тест 5 пройден");
    printf("\n%s\n", "----------------------------------------------------------------");
}

void test_all() {
    test_usual_malloc();
    test_free_one_block();
    test_free_several_blocks();
    test_memory_ended();
    test_memory_ended_and_shifted();
}
