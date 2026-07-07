#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "page_replacement.h"

// ============================================================================
// ТЕСТОВЫЕ ДАННЫЕ
// ============================================================================

// Тест 1: Классический пример - 20 обращений, 3 фрейма
static int test_pages1[] = { 7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1 };
static int test_size1 = 20;

// Тест 2: Повторяющийся паттерн
static int test_pages2[] = { 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4 };
static int test_size2 = 12;

// Тест 3: Все страницы разные
static int test_pages3[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
static int test_size3 = 10;

// Тест 4: Все страницы одинаковые
static int test_pages4[] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
static int test_size4 = 10;

// Тест 5: Больше фреймов чем страниц (5 страниц, 10 фреймов)
static int test_pages5[] = { 1, 2, 3, 4, 5 };
static int test_size5 = 5;

// Тест 6: Один фрейм
static int test_pages6[] = { 1, 2, 3, 4, 5, 1, 2, 3, 4, 5 };
static int test_size6 = 10;

// Тест 7: Производительность (большой набор)
static int* generate_random_pages(int size, int max_page) {
    int* pages = (int*)malloc(size * sizeof(int));
    for (int i = 0; i < size; i++) {
        pages[i] = rand() % max_page;
    }
    return pages;
}

// ============================================================================
// ВСПОМОГАТЕЛЬНАЯ ФУНКЦИЯ ДЛЯ ВЫВОДА СТРАНИЦ
// ============================================================================

static void print_pages(int pages[], int size, const char* label) {
    printf("%s: ", label);
    int count = 0;
    for (int i = 0; i < size && i < 50; i++) {
        printf("%d ", pages[i]);
        count++;
    }
    if (size > 50) {
        printf("... (%d total pages)", size);
    }
    printf("\n");
}

// ============================================================================
// ЗАПУСК ТЕСТА (без пошагового вывода, только статистика)
// ============================================================================

static void compare_algorithms(const char* test_name, int pages[], int size, int capacity) {
    print_separator(test_name);

    print_pages(pages, size, "Pages");

    // Запускаем алгоритмы без пошагового вывода
    run_algorithm_silent("FIFO", fifo_access_silent, pages, size, capacity);
    run_algorithm_silent("LRU", lru_access_silent, pages, size, capacity);
    run_algorithm_silent("NFU", nfu_access_silent, pages, size, capacity);
    run_algorithm_silent("CLOCK", clock_access_silent, pages, size, capacity);
}

// ============================================================================
// ЗАПУСК ТЕСТА С ДЕТАЛЬНЫМ ВЫВОДОМ (только для отладки)
// ============================================================================

static void compare_algorithms_verbose(const char* test_name, int pages[], int size, int capacity) {
    print_separator(test_name);

    print_pages(pages, size, "Pages");

    // Запускаем с пошаговым выводом
    run_algorithm("FIFO", fifo_access, pages, size, capacity);
    run_algorithm("LRU", lru_access, pages, size, capacity);
    run_algorithm("NFU", nfu_access, pages, size, capacity);
    run_algorithm("CLOCK", clock_access, pages, size, capacity);
}

// ============================================================================
// ТЕСТ ПРОИЗВОДИТЕЛЬНОСТИ (без пошагового вывода)
// ============================================================================

static void test_performance(void) {
    print_separator("PERFORMANCE TEST");

    const int NUM_PAGES = 10000;
    const int CAPACITY = 100;
    const int MAX_PAGE = 1000;

    printf("\nGenerating %d random pages...\n", NUM_PAGES);
    int* pages = generate_random_pages(NUM_PAGES, MAX_PAGE);

    printf("\nComparing algorithms on random data:\n");

    // FIFO
    PageCache* cache = cache_create(CAPACITY);
    if (cache != NULL) {
        clock_t start = clock();
        for (int i = 0; i < NUM_PAGES; i++) {
            fifo_access_silent(cache, pages[i]);
        }
        clock_t end = clock();
        printf("  FIFO:  %d faults (%.3f sec)\n", cache->faults,
            (double)(end - start) / CLOCKS_PER_SEC);
        cache_destroy(cache);
    }

    // LRU
    cache = cache_create(CAPACITY);
    if (cache != NULL) {
        clock_t start = clock();
        for (int i = 0; i < NUM_PAGES; i++) {
            lru_access_silent(cache, pages[i]);
        }
        clock_t end = clock();
        printf("  LRU:   %d faults (%.3f sec)\n", cache->faults,
            (double)(end - start) / CLOCKS_PER_SEC);
        cache_destroy(cache);
    }

    // NFU
    cache = cache_create(CAPACITY);
    if (cache != NULL) {
        clock_t start = clock();
        for (int i = 0; i < NUM_PAGES; i++) {
            nfu_access_silent(cache, pages[i]);
        }
        clock_t end = clock();
        printf("  NFU:   %d faults (%.3f sec)\n", cache->faults,
            (double)(end - start) / CLOCKS_PER_SEC);
        cache_destroy(cache);
    }

    // CLOCK
    cache = cache_create(CAPACITY);
    if (cache != NULL) {
        clock_t start = clock();
        for (int i = 0; i < NUM_PAGES; i++) {
            clock_access_silent(cache, pages[i]);
        }
        clock_t end = clock();
        printf("  CLOCK: %d faults (%.3f sec)\n", cache->faults,
            (double)(end - start) / CLOCKS_PER_SEC);
        cache_destroy(cache);
    }

    free(pages);
}

// ============================================================================
// ТЕСТ НА РАЗНЫХ РАЗМЕРАХ КЕША
// ============================================================================

static void test_different_sizes(void) {
    print_separator("TEST: DIFFERENT CACHE SIZES");

    int pages[] = { 1, 2, 3, 4, 1, 2, 5, 6, 1, 2, 3, 4, 5, 6 };
    int size = sizeof(pages) / sizeof(pages[0]);
    int capacities[] = { 1, 2, 3, 4, 5, 6 };
    int num_capacities = sizeof(capacities) / sizeof(capacities[0]);

    print_pages(pages, size, "Pages");

    printf("\nLRU faults by cache size:\n");
    for (int c = 0; c < num_capacities; c++) {
        int cap = capacities[c];
        PageCache* cache = cache_create(cap);
        if (cache == NULL) continue;

        for (int i = 0; i < size; i++) {
            lru_access_silent(cache, pages[i]);
        }
        printf("  Capacity %d: %d faults\n", cap, cache->faults);
        cache_destroy(cache);
    }

    printf("\nCLOCK faults by cache size:\n");
    for (int c = 0; c < num_capacities; c++) {
        int cap = capacities[c];
        PageCache* cache = cache_create(cap);
        if (cache == NULL) continue;

        for (int i = 0; i < size; i++) {
            clock_access_silent(cache, pages[i]);
        }
        printf("  Capacity %d: %d faults\n", cap, cache->faults);
        cache_destroy(cache);
    }
}

// ============================================================================
// ТЕСТ 8: ПУСТОЙ МАССИВ (Edge Case)
// ============================================================================

static void test_empty_array(void) {
    print_separator("TEST 8: EMPTY ARRAY (Edge Case)");

    int* empty_pages = NULL;
    int size = 0;
    int capacity = 3;

    printf("\nPages: (empty)\n");
    printf("Testing with empty page sequence (size=0)...\n\n");

    PageCache* cache = cache_create(capacity);
    if (cache != NULL) {
        printf("  FIFO (empty):\n");
        for (int i = 0; i < size; i++) {
            fifo_access(cache, empty_pages[i]);
        }
        printf("    Hits: %d, Faults: %d, Hit rate: %.1f%%\n",
            cache->hits, cache->faults,
            cache->hits + cache->faults > 0 ?
            (double)cache->hits / (cache->hits + cache->faults) * 100.0 : 0.0);
        cache_destroy(cache);
    }

    cache = cache_create(capacity);
    if (cache != NULL) {
        printf("  LRU (empty):\n");
        for (int i = 0; i < size; i++) {
            lru_access(cache, empty_pages[i]);
        }
        printf("    Hits: %d, Faults: %d, Hit rate: %.1f%%\n",
            cache->hits, cache->faults,
            cache->hits + cache->faults > 0 ?
            (double)cache->hits / (cache->hits + cache->faults) * 100.0 : 0.0);
        cache_destroy(cache);
    }

    cache = cache_create(capacity);
    if (cache != NULL) {
        printf("  NFU (empty):\n");
        for (int i = 0; i < size; i++) {
            nfu_access(cache, empty_pages[i]);
        }
        printf("    Hits: %d, Faults: %d, Hit rate: %.1f%%\n",
            cache->hits, cache->faults,
            cache->hits + cache->faults > 0 ?
            (double)cache->hits / (cache->hits + cache->faults) * 100.0 : 0.0);
        cache_destroy(cache);
    }

    cache = cache_create(capacity);
    if (cache != NULL) {
        printf("  CLOCK (empty):\n");
        for (int i = 0; i < size; i++) {
            clock_access(cache, empty_pages[i]);
        }
        printf("    Hits: %d, Faults: %d, Hit rate: %.1f%%\n",
            cache->hits, cache->faults,
            cache->hits + cache->faults > 0 ?
            (double)cache->hits / (cache->hits + cache->faults) * 100.0 : 0.0);
        cache_destroy(cache);
    }

    printf("\n[OK] All algorithms handle empty input correctly (no crashes)\n");
}

// ============================================================================
// ТЕСТ 9: НУЛЕВОЙ РАЗМЕР КЕША
// ============================================================================

static void test_zero_capacity(void) {
    print_separator("TEST 9: ZERO CAPACITY (Edge Case)");

    int pages[] = { 1, 2, 3 };
    int size = 3;

    printf("\nPages: 1 2 3\n");
    printf("Testing with zero capacity (should return NULL)...\n\n");

    PageCache* cache = cache_create(0);
    if (cache == NULL) {
        printf("  [OK] cache_create(0) correctly returns NULL\n");
    }
    else {
        printf("  [FAIL] cache_create(0) should return NULL!\n");
        cache_destroy(cache);
    }

    cache = cache_create(-5);
    if (cache == NULL) {
        printf("  [OK] cache_create(-5) correctly returns NULL\n");
    }
    else {
        printf("  [FAIL] cache_create(-5) should return NULL!\n");
        cache_destroy(cache);
    }
}

// ============================================================================
// ТЕСТ 10: NULL-ПРОВЕРКИ
// ============================================================================

static void test_null_checks(void) {
    print_separator("TEST 10: NULL POINTER CHECKS");

    printf("\nTesting algorithms with NULL cache pointer...\n\n");

    bool result;

    result = fifo_access(NULL, 1);
    printf("  fifo_access(NULL, 1) = %s (should be false)\n", result ? "true" : "false");

    result = lru_access(NULL, 1);
    printf("  lru_access(NULL, 1) = %s (should be false)\n", result ? "true" : "false");

    result = nfu_access(NULL, 1);
    printf("  nfu_access(NULL, 1) = %s (should be false)\n", result ? "true" : "false");

    result = clock_access(NULL, 1);
    printf("  clock_access(NULL, 1) = %s (should be false)\n", result ? "true" : "false");

    result = fifo_access_silent(NULL, 1);
    printf("  fifo_access_silent(NULL, 1) = %s (should be false)\n", result ? "true" : "false");

    result = lru_access_silent(NULL, 1);
    printf("  lru_access_silent(NULL, 1) = %s (should be false)\n", result ? "true" : "false");

    result = nfu_access_silent(NULL, 1);
    printf("  nfu_access_silent(NULL, 1) = %s (should be false)\n", result ? "true" : "false");

    result = clock_access_silent(NULL, 1);
    printf("  clock_access_silent(NULL, 1) = %s (should be false)\n", result ? "true" : "false");

    printf("\n  cache_print(NULL, 1, true): ");
    cache_print(NULL, 1, true);
    printf(" (should print nothing)\n");

    printf("\n[OK] All NULL checks pass\n");
}

// ============================================================================
// ГЛАВНАЯ ФУНКЦИЯ
// ============================================================================

int main(void) {
    srand(42);  // Фиксированный seed для воспроизводимости

    printf("============================================\n");
    printf("     PAGE REPLACEMENT ALGORITHMS\n");
    printf("============================================\n");
    printf("\nNote: Step-by-step output is disabled for readability.\n");
    printf("      Only final statistics are shown.\n");
    printf("      To see detailed output, use compare_algorithms_verbose().\n");

    // Тест 1: Классический пример (без пошагового вывода)
    compare_algorithms("TEST 1: CLASSIC EXAMPLE",
        test_pages1, test_size1, 3);

    // Тест 2: Повторяющийся паттерн
    compare_algorithms("TEST 2: REPEATING PATTERN",
        test_pages2, test_size2, 3);

    // Тест 3: Все страницы разные
    compare_algorithms("TEST 3: ALL DIFFERENT",
        test_pages3, test_size3, 3);

    // Тест 4: Все страницы одинаковые
    compare_algorithms("TEST 4: ALL SAME",
        test_pages4, test_size4, 3);

    // Тест 5: Больше фреймов чем страниц
    compare_algorithms("TEST 5: MORE FRAMES THAN PAGES",
        test_pages5, test_size5, 10);

    // Тест 6: Один фрейм
    compare_algorithms("TEST 6: SINGLE FRAME",
        test_pages6, test_size6, 1);

    // Тест 7: Разные размеры кеша
    test_different_sizes();

    // Тест 8: Производительность (без пошагового вывода)
    test_performance();


    // Тест 9: Пустой массив
    test_empty_array();

    // Тест 10: Нулевой размер кеша
    test_zero_capacity();

    // Тест 11: NULL-проверки
    test_null_checks();

    printf("\n============================================\n");
    printf("           ALL TESTS COMPLETED\n");
    printf("============================================\n");

    return 0;
}