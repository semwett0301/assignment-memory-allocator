#include <stdarg.h>

#define _DEFAULT_SOURCE

#include <unistd.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>

#include "mem_internals.h"
#include "mem.h"
#include "util.h"

void debug_block(struct block_header *b, const char *fmt, ...);

void debug(const char *fmt, ...);

extern inline block_size
size_from_capacity( block_capacity
cap );
extern inline block_capacity
capacity_from_size( block_size
sz );

static bool block_is_big_enough(size_t query, const struct block_header *block) { return block->capacity.bytes >= query; }

static size_t pages_count(size_t mem) { return mem / getpagesize() + ((mem % getpagesize()) > 0); }

static size_t round_pages(size_t mem) { return getpagesize() * pages_count(mem); }

static void block_init(void *restrict addr, block_size block_sz, void *restrict next) {
    *((struct block_header *) addr) = (struct block_header) {
            .next = next,
            .capacity = capacity_from_size(block_sz),
            .is_free = true
    };
}

static size_t region_actual_size(size_t query) { return size_max(round_pages(query), REGION_MIN_SIZE); }

extern inline bool

region_is_invalid(const struct region *r);


static void *map_pages(void const *addr, size_t length, int additional_flags) {
    return mmap((void *) addr, length, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | additional_flags, 0, 0);
}

/*  аллоцировать регион памяти и инициализировать его блоком */
static struct region alloc_region(void const *addr, size_t query) {
    // Пытаемся вызвать MMAP на конкретном адресе
    const size_t region_size = region_actual_size(query);
    void *address_of_allocated_blocks = map_pages(addr, region_size, MAP_FIXED_NOREPLACE);
    bool extends_region = true;

    if (address_of_allocated_blocks == (void *) MAP_FAILED) {
        address_of_allocated_blocks = map_pages(addr, region_size, NULL);
        extends_region = false;
    }

    if (address_of_allocated_blocks == (void *) MAP_FAILED) {
        address_of_allocated_blocks = NULL;
    }

    // Регистрируем блок, если получилось вызвать MMAP
    struct region allocated_region = (struct region) {
            .addr = address_of_allocated_blocks,
            .size = region_size,
            .extends = extends_region
    };

    if (region_is_invalid(&allocated_region) ) {
        return REGION_INVALID;
    }

    block_size size = (block_size) {
            .bytes = region_actual_size(query)
    };

    block_init(address_of_allocated_blocks, size, NULL);
    return allocated_region;

}

static void *block_after(struct block_header const *block);

void *heap_init(size_t initial) {
    const struct region region = alloc_region(HEAP_START, initial);
    if (region_is_invalid(&region)) return NULL;

    return region.addr;
}

#define BLOCK_MIN_CAPACITY 24

/*  --- Разделение блоков (если найденный свободный блок слишком большой )--- */

static bool block_splittable(struct block_header *restrict block, size_t query) {
    return block->is_free && query + offsetof(
    struct block_header, contents ) +BLOCK_MIN_CAPACITY <= block->capacity.bytes;
}

static bool split_if_too_big(struct block_header *block, size_t query) {
    query = size_max(query, BLOCK_MIN_CAPACITY);

    // Проверяем, можно ли разделить блок
    if (block_splittable(block, query)) {
        // Определяем размер нового блока
        block_size next_block_size = (block_size) {
                .bytes = size_from_capacity(block->capacity).bytes - query
        };

        // Переопределяем размер предыдущего блока и ищем адрес следующего
        block->capacity.bytes = query;
        void *new_next_block_address = (void*)((uint8_t*) block + offsetof(struct block_header, contents) + query);

        // Регистрируем новый блок и меняем адрес следующего блок в предыдущем
        block_init(new_next_block_address, next_block_size, block->next);

        block->next = new_next_block_address;

        return true;
    }

    return false;
}


/*  --- Слияние соседних свободных блоков --- */

static void *block_after(struct block_header const *block) {
    return (void *) (block->contents + block->capacity.bytes);
}

static bool blocks_continuous(
        struct block_header const *fst,
        struct block_header const *snd) {
    return (void *) snd == block_after(fst);
}

static bool mergeable(struct block_header const *restrict fst, struct block_header const *restrict snd) {
    return fst->is_free && snd->is_free && blocks_continuous(fst, snd);
}

static bool try_merge_with_next(struct block_header *block) {
    struct block_header *init_next_block = block->next;

    if (block->next && mergeable(block, init_next_block)) {
        block->capacity.bytes += block->next->capacity.bytes;
        block->next = init_next_block->next;
        return true;
    }
    return false;
}


/*  --- ... ecли размера кучи хватает --- */

struct block_search_result {
    enum {
        BSR_FOUND_GOOD_BLOCK, BSR_REACHED_END_NOT_FOUND, BSR_CORRUPTED
    } type;
    struct block_header *block;
};


static struct block_search_result find_good_or_last(struct block_header *restrict block, size_t sz) {
    if (!block) {
        return (struct block_search_result) {
                .type = BSR_CORRUPTED
        };
    }

    struct block_header *tmp_block = block;
    struct block_header *last_block = block;

    while (tmp_block) {
        // Если блок занят, то просто смотрим следующий
        if (tmp_block->is_free) {
            // Если блоку хватает места, то возвращаем его (при необходимости предварительно разделив)
            // Если нет - пытаемся соединить его со следующим и рассматриваем его заново
            // Если не получилось, то продолжаем поиск свободного блока
            if (block_is_big_enough(sz, tmp_block)) {
                split_if_too_big(tmp_block, sz);
                return (struct block_search_result) {
                        .type = BSR_FOUND_GOOD_BLOCK,
                        .block = tmp_block
                };
            } else {
                if (!try_merge_with_next(tmp_block)) {
                    last_block = tmp_block;
                    tmp_block = tmp_block->next;
                }
            }
        } else {
            last_block = tmp_block;
            tmp_block = tmp_block->next;
        }
    }

    return (struct block_search_result) {
            .type = BSR_REACHED_END_NOT_FOUND,
            .block = last_block
    };
}

/*  Попробовать выделить память в куче начиная с блока `block` не пытаясь расширить кучу
 Можно переиспользовать как только кучу расширили. */
static struct block_search_result try_memalloc_existing(size_t query, struct block_header *block) {
    return find_good_or_last(block, query);

}

static struct block_header *grow_heap(struct block_header *restrict last, size_t query) {
    query = size_max(query, BLOCK_MIN_CAPACITY);

    if (last->is_free) query = query - last->capacity.bytes;

    struct region grown_region = alloc_region(last + size_from_capacity(last->capacity).bytes, query);
    split_if_too_big(grown_region.addr, query);

    last->next = grown_region.addr;
    try_merge_with_next(last);
    return last;

}

/*  Реализует основную логику malloc и возвращает заголовок выделенного блока */
static struct block_header *memalloc(size_t query, struct block_header *heap_start) {
    // Определяем, есть ли подходящий блок
    struct block_search_result init_try_find_block = try_memalloc_existing(query, heap_start);
    struct block_header *new_part;

    switch (init_try_find_block.type) {
        // Если блок есть, то возвращаем адрес его заголовка
        case BSR_FOUND_GOOD_BLOCK:
            init_try_find_block.block->is_free = false;
            return init_try_find_block.block;

            // Если блока нет, то пытаемся расширить кучу и снова пробуем найти подходящий блок (если кучу расширить не получилось, то вернем NULL)
        case BSR_REACHED_END_NOT_FOUND:
            new_part = grow_heap(init_try_find_block.block, query);
            // Меняем занятость
            new_part->is_free = false;

            // Проверяем, удалось ли слияние
            if(new_part->next) {
                new_part = new_part->next;
                new_part->is_free = false;
            }
            return new_part;

        default:
            return NULL;
    }
}

void *_malloc(size_t query) {
    struct block_header *const addr = memalloc(query, (struct block_header *) HEAP_START);
    if (addr) return addr->contents;
    else return NULL;
}

static struct block_header *block_get_header(void *contents) {
    return (struct block_header *) (((uint8_t *) contents) - offsetof(
    struct block_header, contents));
}

void _free(void *mem) {
    if (!mem) return;
    struct block_header *header = block_get_header(mem);
    header->is_free = true;


    // Пока мы можем объединить текущий блок с последующими - мы это делаем
    while (try_merge_with_next(header));

}

