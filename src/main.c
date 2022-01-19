#include "tests.h"

int main() {
    test_usual_malloc();
    test_free_one_block();
    test_free_several_blocks();
    test_memory_ended();
    test_memory_ended_and_shifted();
    return 0;
}
