#ifndef ASSIGNMENT_MEMORY_ALLOCATOR_TESTS_H
#define ASSIGNMENT_MEMORY_ALLOCATOR_TESTS_H

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

void test_usual_malloc();

void test_free_one_block();

void test_free_several_blocks();

void test_memory_ended();

void test_memory_ended_and_shifted();
#endif //ASSIGNMENT_MEMORY_ALLOCATOR_TESTS_H
