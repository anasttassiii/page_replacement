#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>   // Для QueryPerformanceCounter (высокоточный таймер)
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

// ============================================================================
// КОНСТАНТЫ ДЛЯ ТЕСТОВ ПРОИЗВОДИТЕЛЬНОСТИ
// ============================================================================

#define NUM_SIZES 7  // Количество различных размеров для теста масштабируемости

// ============================================================================
// ФУНКЦИЯ ДЛЯ ГЕНЕРАЦИИ СЛУЧАЙНЫХ СТРАНИЦ
// ============================================================================

// Генерирует массив случайных страниц для тестирования производительности
// Используется фиксированный seed (42) для воспроизводимости результатов
static int* generate_random_pages(int size, int max_page) {
    int* pages = (int*)malloc(size * sizeof(int));
    if (pages == NULL) {
        return NULL;  // Проверка выделения памяти
    }
    for (int i = 0; i < size; i++) {
        pages[i] = rand() % max_page;
    }
    return pages;
}

// ============================================================================
// ВСПОМОГАТЕЛЬНАЯ ФУНКЦИЯ ДЛЯ qsort
// ============================================================================

// Функция сравнения для qsort (стандартная быстрая сортировка)
static int compare_int(const void* a, const void* b) {
    return (*(int*)a - *(int*)b);
}

// ============================================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ДЛЯ ВЫВОДА
// ============================================================================

// Печатает последовательность страниц (не более 50 для читаемости)
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
// ВЫСОКОТОЧНЫЙ ЗАМЕР ВРЕМЕНИ (WINDOWS API)
// ============================================================================

// Измеряет время выполнения алгоритма с помощью QueryPerformanceCounter
// Это даёт наносекундную точность и измеряет реальное время (wall-clock time)
static double measure_time(bool (*algo)(PageCache*, int), PageCache* cache, int pages[], int size) {
    LARGE_INTEGER frequency, start, end;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);

    for (int i = 0; i < size; i++) {
        algo(cache, pages[i]);
    }

    QueryPerformanceCounter(&end);
    return (double)(end.QuadPart - start.QuadPart) / frequency.QuadPart;
}

// ============================================================================
// БАЗОВЫЕ ТЕСТЫ (БЕЗ ПОШАГОВОГО ВЫВОДА)
// ============================================================================

// Запускает все 4 алгоритма на одном тестовом наборе и выводит статистику
static void compare_algorithms(const char* test_name, int pages[], int size, int capacity) {
    print_separator(test_name);
    print_pages(pages, size, "Pages");

    run_algorithm_silent("FIFO", fifo_access_silent, pages, size, capacity);
    run_algorithm_silent("LRU", lru_access_silent, pages, size, capacity);
    run_algorithm_silent("NFU", nfu_access_silent, pages, size, capacity);
    run_algorithm_silent("CLOCK", clock_access_silent, pages, size, capacity);
}

// ============================================================================
// ТЕСТ 7: СРАВНЕНИЕ АЛГОРИТМОВ С ВЫЧИСЛЕНИЕМ КОНСТАНТ O(n)
// ============================================================================

// Замеряет время выполнения каждого алгоритма на разных размерах данных
// и вычисляет коэффициент C = T(n) / n — «отбрасываемую константу» в O(n)
static void test_algorithm_comparison_with_constants(void) {
    print_separator("TEST 7: ALGORITHM COMPARISON WITH O(n) CONSTANTS");

    const int CAPACITY = 50;
    const int MAX_PAGE = 1000;
    const int SIZES[NUM_SIZES] = { 1000, 2000, 5000, 10000, 20000, 50000, 100000 };

    // Массивы для хранения результатов замеров (фиксированный размер)
    double fifo_times[NUM_SIZES];
    double lru_times[NUM_SIZES];
    double nfu_times[NUM_SIZES];
    double clock_times[NUM_SIZES];

    printf("\nComparing algorithms on different input sizes\n");
    printf("Cache capacity: %d\n\n", CAPACITY);

    printf("  Size    |   FIFO    |   LRU     |   NFU     |  CLOCK   \n");
    printf("----------|-----------|-----------|-----------|----------\n");

    for (int s = 0; s < NUM_SIZES; s++) {
        int size = SIZES[s];
        int* pages = generate_random_pages(size, MAX_PAGE);
        if (pages == NULL) {
            printf("  ERROR: Failed to allocate memory for size %d\n", size);
            continue;
        }

        // Замер FIFO
        PageCache* cache = cache_create(CAPACITY);
        fifo_times[s] = measure_time(fifo_access_silent, cache, pages, size);
        cache_destroy(cache);

        // Замер LRU
        cache = cache_create(CAPACITY);
        lru_times[s] = measure_time(lru_access_silent, cache, pages, size);
        cache_destroy(cache);

        // Замер NFU
        cache = cache_create(CAPACITY);
        nfu_times[s] = measure_time(nfu_access_silent, cache, pages, size);
        cache_destroy(cache);

        // Замер CLOCK
        cache = cache_create(CAPACITY);
        clock_times[s] = measure_time(clock_access_silent, cache, pages, size);
        cache_destroy(cache);

        printf("  %6d  |  %7.4f  |  %7.4f  |  %7.4f  |  %7.4f\n",
            size, fifo_times[s], lru_times[s], nfu_times[s], clock_times[s]);

        free(pages);
    }

    // ===== ВЫЧИСЛЕНИЕ КОНСТАНТ O(n) =====
    printf("\n========================================\n");
    printf("  O(n) CONSTANTS CALCULATION\n");
    printf("========================================\n");
    printf("\nT(n) = C * n + B, where C = T(n) / n\n");
    printf("\n  Algorithm |   C (avg)  |   B (avg)  |  C ratio\n");
    printf("------------|------------|------------|----------\n");

    // Используем последние 3 замера для вычисления констант (большие данные)
    int start_idx = NUM_SIZES - 3;
    double fifo_C = 0, lru_C = 0, nfu_C = 0, clock_C = 0;
    double fifo_B = 0, lru_B = 0, nfu_B = 0, clock_B = 0;
    int count = 0;

    for (int s = start_idx; s < NUM_SIZES; s++) {
        int size = SIZES[s];
        count++;

        fifo_C += fifo_times[s] / size;
        lru_C += lru_times[s] / size;
        nfu_C += nfu_times[s] / size;
        clock_C += clock_times[s] / size;
    }

    fifo_C /= count;
    lru_C /= count;
    nfu_C /= count;
    clock_C /= count;

    // Вычисляем B = T(n) - C * n для последних 3 замеров
    for (int s = start_idx; s < NUM_SIZES; s++) {
        int size = SIZES[s];
        fifo_B += fifo_times[s] - fifo_C * size;
        lru_B += lru_times[s] - lru_C * size;
        nfu_B += nfu_times[s] - nfu_C * size;
        clock_B += clock_times[s] - clock_C * size;
    }
    fifo_B /= count;
    lru_B /= count;
    nfu_B /= count;
    clock_B /= count;

    printf("  FIFO     |  %.4e  |  %.4e  |  1.00x\n", fifo_C, fifo_B);
    printf("  LRU      |  %.4e  |  %.4e  |  %.2fx\n", lru_C, lru_B, lru_C / fifo_C);
    printf("  NFU      |  %.4e  |  %.4e  |  %.2fx\n", nfu_C, nfu_B, nfu_C / fifo_C);
    printf("  CLOCK    |  %.4e  |  %.4e  |  %.2fx\n", clock_C, clock_B, clock_C / fifo_C);

    // Интерпретация результатов (на английском)
    printf("\n========== INTERPRETATION ==========\n");
    printf("C = T(n) / n is the 'dropped' constant in O(n)\n");
    printf("FIFO:  T(n) = %.4e * n + %.4e\n", fifo_C, fifo_B);
    printf("LRU:   T(n) = %.4e * n + %.4e\n", lru_C, lru_B);
    printf("NFU:   T(n) = %.4e * n + %.4e\n", nfu_C, nfu_B);
    printf("CLOCK: T(n) = %.4e * n + %.4e\n", clock_C, clock_B);

    printf("\nConclusion: LRU is %.2f times slower than FIFO due to time update operation.\n",
        lru_C / fifo_C);
}

// ============================================================================
// ТЕСТ 8: СРАВНЕНИЕ С АЛГОРИТМАМИ СОРТИРОВКИ
// ============================================================================

// Сравнивает время выполнения алгоритмов вытеснения страниц
// с алгоритмами сортировки на том же объёме данных
static void compare_with_sorting(void) {
    print_separator("TEST 8: COMPARISON WITH SORTING ALGORITHMS");

    const int SIZE = 50000;
    const int CAPACITY = 50;
    const int MAX_PAGE = 500;

    printf("\nComparing page replacement algorithms with sorting\n");
    printf("Data size: %d elements\n\n", SIZE);

    // Генерируем данные
    int* pages = generate_random_pages(SIZE, MAX_PAGE);
    if (pages == NULL) {
        printf("  ERROR: Failed to allocate memory\n");
        return;
    }

    int* array_for_sort = (int*)malloc(SIZE * sizeof(int));
    if (array_for_sort == NULL) {
        printf("  ERROR: Failed to allocate memory for sorting array\n");
        free(pages);
        return;
    }

    // === 1. Замер LRU ===
    PageCache* cache = cache_create(CAPACITY);
    double lru_time = measure_time(lru_access_silent, cache, pages, SIZE);
    int lru_faults = cache->faults;
    cache_destroy(cache);

    // === 2. Замер сортировки вставками (O(n²)) ===
    for (int i = 0; i < SIZE; i++) {
        array_for_sort[i] = pages[i];
    }
    double insertion_time = 0.0;
    LARGE_INTEGER frequency, start, end;
    QueryPerformanceFrequency(&frequency);

    // Сортировка вставками (очень медленная для больших массивов)
    QueryPerformanceCounter(&start);
    for (int i = 1; i < SIZE; i++) {
        int key = array_for_sort[i];
        int j = i - 1;
        while (j >= 0 && array_for_sort[j] > key) {
            array_for_sort[j + 1] = array_for_sort[j];
            j--;
        }
        array_for_sort[j + 1] = key;
    }
    QueryPerformanceCounter(&end);
    insertion_time = (double)(end.QuadPart - start.QuadPart) / frequency.QuadPart;

    // === 3. Замер быстрой сортировки (qsort) ===
    for (int i = 0; i < SIZE; i++) {
        array_for_sort[i] = pages[i];
    }
    double qsort_time = 0.0;
    QueryPerformanceCounter(&start);
    qsort(array_for_sort, SIZE, sizeof(int), compare_int);
    QueryPerformanceCounter(&end);
    qsort_time = (double)(end.QuadPart - start.QuadPart) / frequency.QuadPart;

    // === 4. Вывод результатов ===
    printf("========== PERFORMANCE COMPARISON ==========\n");
    printf("  LRU (page replacement):   %.6f sec (faults=%d)\n", lru_time, lru_faults);
    printf("  Insertion Sort:           %.6f sec\n", insertion_time);
    printf("  Quicksort (qsort):        %.6f sec\n", qsort_time);
    printf("\n========== RATIO ==========\n");
    printf("  LRU vs Insertion Sort:    %.2fx faster\n", insertion_time / lru_time);
    printf("  LRU vs Quicksort:         %.2fx slower\n", lru_time / qsort_time);

    printf("\nConclusion: Page replacement algorithms are slower than qsort,\n");
    printf("but faster than insertion sort on large datasets.\n");

    free(pages);
    free(array_for_sort);
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
// ТЕСТ ПРОИЗВОДИТЕЛЬНОСТИ С ИСПОЛЬЗОВАНИЕМ clock()
// ============================================================================

// Дополнительный тест с использованием стандартной функции clock()
static void test_performance_with_clock(void) {
    print_separator("TEST: PERFORMANCE WITH clock()");

    const int NUM_PAGES = 50000;
    const int CAPACITY = 50;
    const int MAX_PAGE = 500;

    printf("\nGenerating %d random pages...\n", NUM_PAGES);
    int* pages = generate_random_pages(NUM_PAGES, MAX_PAGE);
    if (pages == NULL) {
        printf("  ERROR: Failed to allocate memory\n");
        return;
    }

    printf("Comparing algorithms using clock() (CPU time):\n\n");

    // FIFO
    PageCache* cache = cache_create(CAPACITY);
    clock_t start = clock();
    for (int i = 0; i < NUM_PAGES; i++) {
        fifo_access_silent(cache, pages[i]);
    }
    clock_t end = clock();
    double fifo_time = (double)(end - start) / CLOCKS_PER_SEC;
    int fifo_faults = cache->faults;
    cache_destroy(cache);

    // LRU
    cache = cache_create(CAPACITY);
    start = clock();
    for (int i = 0; i < NUM_PAGES; i++) {
        lru_access_silent(cache, pages[i]);
    }
    end = clock();
    double lru_time = (double)(end - start) / CLOCKS_PER_SEC;
    int lru_faults = cache->faults;
    cache_destroy(cache);

    printf("  FIFO:  %.6f sec (faults=%d)\n", fifo_time, fifo_faults);
    printf("  LRU:   %.6f sec (faults=%d)\n", lru_time, lru_faults);

    free(pages);
}

// ============================================================================
// ГЛАВНАЯ ФУНКЦИЯ
// ============================================================================

int main(void) {
    srand(42);  // Fixed seed for reproducibility

    printf("============================================\n");
    printf("     PAGE REPLACEMENT ALGORITHMS\n");
    printf("     Performance Comparison\n");
    printf("============================================\n");
    printf("\nNote: Step-by-step output is disabled for readability.\n");
    printf("      Only final statistics are shown.\n");

    // === BASIC TESTS (without step-by-step output) ===
    compare_algorithms("TEST 1: CLASSIC EXAMPLE",
        test_pages1, test_size1, 3);
    compare_algorithms("TEST 2: REPEATING PATTERN",
        test_pages2, test_size2, 3);
    compare_algorithms("TEST 3: ALL DIFFERENT",
        test_pages3, test_size3, 3);
    compare_algorithms("TEST 4: ALL SAME",
        test_pages4, test_size4, 3);
    compare_algorithms("TEST 5: MORE FRAMES THAN PAGES",
        test_pages5, test_size5, 10);
    compare_algorithms("TEST 6: SINGLE FRAME",
        test_pages6, test_size6, 1);

    // === PERFORMANCE COMPARISON TESTS ===
    test_different_sizes();

    // === TEST 7: ALGORITHM COMPARISON WITH O(n) CONSTANTS ===
    test_algorithm_comparison_with_constants();

    // === TEST 8: COMPARISON WITH SORTING ===
    compare_with_sorting();

    // === TEST WITH clock() ===
    test_performance_with_clock();

    printf("\n============================================\n");
    printf("           ALL TESTS COMPLETED\n");
    printf("============================================\n");

    return 0;
}