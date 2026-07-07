#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>    
#include <stdlib.h>   
#include <string.h>
#include <assert.h>     
#include <stdbool.h>    
#include "page_replacement.h"  

// ============================================================================
// БАЗОВЫЕ ФУНКЦИИ ДЛЯ РАБОТЫ С КЕШЕМ
// ============================================================================

/**
 * Создает новый кеш с заданной емкостью
 * capacity - максимальное количество страниц в кеше
 * возвращает указатель на созданный кеш или NULL при ошибке
 */
PageCache* cache_create(int capacity) {
    // Проверка: емкость должна быть положительной
    if (capacity <= 0) return NULL;

    // Выделяем память под структуру кеша (calloc обнуляет память)
    PageCache* cache = (PageCache*)calloc(1, sizeof(PageCache));
    if (cache == NULL) return NULL;  // Ошибка выделения памяти

    // Выделяем память под массивы: фреймы, время и счетчики
    cache->frames = (int*)malloc(capacity * sizeof(int));
    cache->time = (int*)malloc(capacity * sizeof(int));
    cache->counter = (int*)malloc(capacity * sizeof(int));

    // Сохраняем основные параметры кеша
    cache->capacity = capacity;
    cache->faults = 0;          // Количество промахов
    cache->hits = 0;            // Количество попаданий
    cache->step = 0;            // Указатель для FIFO/CLOCK
    cache->lru_index = 0;       // Индекс самой старой страницы для LRU
    cache->access_count = 0;    // Счетчик для старения в NFU

    // Проверяем, что вся память выделена успешно
    if (cache->frames == NULL || cache->time == NULL || cache->counter == NULL) {
        // Если что-то не выделилось - освобождаем все и возвращаем NULL
        free(cache->frames);
        free(cache->time);
        free(cache->counter);
        free(cache);
        return NULL;
    }

    // Инициализируем все фреймы значением -1 (пусто)
    for (int i = 0; i < capacity; i++) {
        cache->frames[i] = -1;   // -1 означает, что фрейм пуст
        cache->time[i] = 0;      // Обнуляем время/биты
        cache->counter[i] = 0;   // Обнуляем счетчики для NFU
    }

    return cache;
}

/**
 * Удаляет кеш и освобождает всю выделенную память
 * cache - указатель на кеш для удаления
 */
void cache_destroy(PageCache* cache) {
    if (cache == NULL) return;  // Проверка на NULL
    free(cache->frames);        // Освобождаем массив фреймов
    free(cache->time);          // Освобождаем массив времени
    free(cache->counter);       // Освобождаем массив счетчиков
    free(cache);                // Освобождаем саму структуру
}

/**
 * Сбрасывает состояние кеша (очищает все фреймы, обнуляет счетчики)
 * cache - указатель на кеш для сброса
 */
void cache_reset(PageCache* cache) {
    if (cache == NULL) return;
    cache->faults = 0;
    cache->hits = 0;
    cache->step = 0;
    cache->lru_index = 0;
    cache->access_count = 0;
    for (int i = 0; i < cache->capacity; i++) {
        cache->frames[i] = -1;
        cache->time[i] = 0;
        cache->counter[i] = 0;
    }
}

/**
 * Печатает состояние кеша в консоль (для визуализации)
 * Выводит только первые 20 фреймов для читаемости
 */
void cache_print(PageCache* cache, int page, bool is_fault) {
    if (cache == NULL) return;

    // Ограничиваем вывод первыми 20 элементами (чтобы не засорять консоль)
    int max_print = cache->capacity < 20 ? cache->capacity : 20;

    printf("  Page %d -> %s  [", page, is_fault ? "FAULT" : "HIT  ");

    for (int i = 0; i < max_print; i++) {
        if (cache->frames[i] != -1) {
            printf("%d ", cache->frames[i]);  // Печатаем номер страницы
        }
        else {
            printf("_ ");  // Пустой фрейм обозначаем подчеркиванием
        }
    }
    if (cache->capacity > 20) {
        printf("... ");  // Если фреймов больше 20 - показываем многоточие
    }
    printf("]\n");
}

// ============================================================================
// FIFO (First In, First Out) - Первым пришел, первым ушел
// ============================================================================

bool fifo_access(PageCache* cache, int page) {
    if (cache == NULL) return false;

    // ШАГ 1: Проверяем, есть ли страница уже в кеше (HIT)
    for (int i = 0; i < cache->capacity; i++) {
        if (cache->frames[i] == page) {
            cache->hits++;
            cache_print(cache, page, false);
            return false;  // Страница уже есть - попадание
        }
    }

    // ШАГ 2: Ищем свободный фрейм (если кеш еще не полон)
    for (int i = 0; i < cache->capacity; i++) {
        if (cache->frames[i] == -1) {
            cache->frames[i] = page;   // Помещаем страницу в свободный фрейм
            cache->faults++;
            cache_print(cache, page, true);
            return true;  // Промах, но есть свободное место
        }
    }

    // ШАГ 3: Кеш полон - вытесняем самую старую страницу (FIFO)
    // step указывает на самую старую страницу (циклический буфер)
    int oldest = cache->step % cache->capacity;
    cache->frames[oldest] = page;  // Заменяем самую старую страницу
    cache->step++;                 // Перемещаем указатель на следующий фрейм
    cache->faults++;
    cache_print(cache, page, true);
    return true;
}

// ============================================================================
// LRU (Least Recently Used) - Вытесняем давно не использовавшуюся
// ============================================================================

/**
 * Обновляет индекс самой старой (давно не использовавшейся) страницы
 * Для LRU - ищем страницу с минимальным временем последнего доступа
 */
static void update_lru_index(PageCache* cache) {
    if (cache == NULL) return;

    // Ищем страницу с самым маленьким временем доступа
    int oldest_time = cache->time[0];
    int oldest_idx = 0;

    for (int i = 1; i < cache->capacity; i++) {
        // Учитываем только занятые фреймы (frames[i] != -1)
        if (cache->frames[i] != -1 && cache->time[i] < oldest_time) {
            oldest_time = cache->time[i];
            oldest_idx = i;
        }
    }
    cache->lru_index = oldest_idx;  // Сохраняем индекс самой старой
}

bool lru_access(PageCache* cache, int page) {
    if (cache == NULL) return false;

    cache->step++;  // Увеличиваем глобальный счетчик времени

    // ШАГ 1: Проверяем, есть ли страница в кеше (HIT)
    for (int i = 0; i < cache->capacity; i++) {
        if (cache->frames[i] == page) {
            cache->time[i] = cache->step;  // Обновляем время доступа
            cache->hits++;

            // Если обновили самую старую - пересчитываем индекс
            if (i == cache->lru_index) {
                update_lru_index(cache);
            }

            cache_print(cache, page, false);
            return false;  // Попадание
        }
    }

    // ШАГ 2: Ищем свободный фрейм
    for (int i = 0; i < cache->capacity; i++) {
        if (cache->frames[i] == -1) {
            cache->frames[i] = page;
            cache->time[i] = cache->step;  // Запоминаем время загрузки
            cache->faults++;

            // Если это первая страница или она старее текущей - обновляем LRU
            if (cache->faults == 1 || cache->time[i] < cache->time[cache->lru_index]) {
                cache->lru_index = i;
            }

            cache_print(cache, page, true);
            return true;  // Промах, но есть свободное место
        }
    }

    // ШАГ 3: Кеш полон - вытесняем самую старую (LRU) страницу
    int lru_idx = cache->lru_index;
    cache->frames[lru_idx] = page;
    cache->time[lru_idx] = cache->step;
    cache->faults++;

    update_lru_index(cache);  // Пересчитываем индекс самой старой

    cache_print(cache, page, true);
    return true;
}

// ============================================================================
// NFU (Not Frequently Used) - Вытесняем редко используемые страницы
// ============================================================================

#define AGE_THRESHOLD 10  // Через каждые 10 обращений делаем "старение" счетчиков

bool nfu_access(PageCache* cache, int page) {
    if (cache == NULL) return false;

    cache->access_count++;  // Увеличиваем счетчик обращений

    // "Старение" - делим все счетчики на 2, чтобы старые обращения теряли вес
    if (cache->access_count % AGE_THRESHOLD == 0) {
        for (int i = 0; i < cache->capacity; i++) {
            cache->counter[i] /= 2;  // Уменьшаем все счетчики
        }
    }

    // ШАГ 1: Проверяем, есть ли страница в кеше
    for (int i = 0; i < cache->capacity; i++) {
        if (cache->frames[i] == page) {
            cache->counter[i]++;  // Увеличиваем счетчик обращений к странице
            cache->hits++;
            cache_print(cache, page, false);
            return false;  // Попадание
        }
    }

    // ШАГ 2: Ищем свободный фрейм
    for (int i = 0; i < cache->capacity; i++) {
        if (cache->frames[i] == -1) {
            cache->frames[i] = page;
            cache->counter[i] = 1;  // Начинаем счет с 1
            cache->faults++;
            cache_print(cache, page, true);
            return true;  // Промах, но есть место
        }
    }

    // ШАГ 3: Кеш полон - ищем страницу с минимальным счетчиком (NFU)
    int min_counter = cache->counter[0];
    int nfu_index = 0;
    for (int i = 1; i < cache->capacity; i++) {
        if (cache->counter[i] < min_counter) {
            min_counter = cache->counter[i];
            nfu_index = i;  // Нашли наименее частую страницу
        }
    }

    // Вытесняем наименее частую страницу
    cache->frames[nfu_index] = page;
    cache->counter[nfu_index] = 1;  // Счетчик для новой страницы = 1
    cache->faults++;
    cache_print(cache, page, true);
    return true;
}

// ============================================================================
// CLOCK (Second Chance / Clock Sweep) - Алгоритм "Второго шанса"
// ============================================================================

bool clock_access(PageCache* cache, int page) {
    if (cache == NULL) return false;

    // ШАГ 1: Проверяем, есть ли страница в кеше
    for (int i = 0; i < cache->capacity; i++) {
        if (cache->frames[i] == page) {
            cache->time[i] = 1;  // Устанавливаем бит использования = 1 (второй шанс)
            cache->hits++;
            cache_print(cache, page, false);
            return false;  // Попадание
        }
    }

    // ШАГ 2: Ищем свободный фрейм
    for (int i = 0; i < cache->capacity; i++) {
        if (cache->frames[i] == -1) {
            cache->frames[i] = page;
            cache->time[i] = 1;  // Устанавливаем бит использования
            cache->faults++;
            cache_print(cache, page, true);
            return true;  // Промах, но есть место
        }
    }

    // ШАГ 3: Кеш полон - идем по кругу (как стрелка часов)
    while (1) {
        int idx = cache->step % cache->capacity;  // Текущая позиция стрелки

        if (cache->time[idx] == 0) {
            // Если бит использования = 0 - вытесняем страницу
            cache->frames[idx] = page;
            cache->time[idx] = 1;  // Устанавливаем бит для новой страницы
            cache->step = (cache->step + 1) % cache->capacity;  // Двигаем стрелку
            cache->faults++;
            cache_print(cache, page, true);
            return true;
        }
        else {
            // Если бит использования = 1 - даем второй шанс
            cache->time[idx] = 0;  // Сбрасываем бит использования
            cache->step = (cache->step + 1) % cache->capacity;  // Двигаем стрелку
            // Переходим к следующему фрейму
        }
    }
}

// ============================================================================
// Используются для тестов производительности, чтобы не засорять консоль
// ============================================================================

bool fifo_access_silent(PageCache* cache, int page) {
    if (cache == NULL) return false;
    // Аналогично fifo_access, но без cache_print()
    for (int i = 0; i < cache->capacity; i++) {
        if (cache->frames[i] == page) {
            cache->hits++;
            return false;
        }
    }
    for (int i = 0; i < cache->capacity; i++) {
        if (cache->frames[i] == -1) {
            cache->frames[i] = page;
            cache->faults++;
            return true;
        }
    }
    int oldest = cache->step % cache->capacity;
    cache->frames[oldest] = page;
    cache->step++;
    cache->faults++;
    return true;
}

bool lru_access_silent(PageCache* cache, int page) {
    if (cache == NULL) return false;
    cache->step++;
    for (int i = 0; i < cache->capacity; i++) {
        if (cache->frames[i] == page) {
            cache->time[i] = cache->step;
            cache->hits++;
            if (i == cache->lru_index) {
                update_lru_index(cache);
            }
            return false;
        }
    }
    for (int i = 0; i < cache->capacity; i++) {
        if (cache->frames[i] == -1) {
            cache->frames[i] = page;
            cache->time[i] = cache->step;
            cache->faults++;
            if (cache->faults == 1 || cache->time[i] < cache->time[cache->lru_index]) {
                cache->lru_index = i;
            }
            return true;
        }
    }
    int lru_idx = cache->lru_index;
    cache->frames[lru_idx] = page;
    cache->time[lru_idx] = cache->step;
    cache->faults++;
    update_lru_index(cache);
    return true;
}

bool nfu_access_silent(PageCache* cache, int page) {
    if (cache == NULL) return false;
    cache->access_count++;
    if (cache->access_count % AGE_THRESHOLD == 0) {
        for (int i = 0; i < cache->capacity; i++) {
            cache->counter[i] /= 2;
        }
    }
    for (int i = 0; i < cache->capacity; i++) {
        if (cache->frames[i] == page) {
            cache->counter[i]++;
            cache->hits++;
            return false;
        }
    }
    for (int i = 0; i < cache->capacity; i++) {
        if (cache->frames[i] == -1) {
            cache->frames[i] = page;
            cache->counter[i] = 1;
            cache->faults++;
            return true;
        }
    }
    int min_counter = cache->counter[0];
    int nfu_index = 0;
    for (int i = 1; i < cache->capacity; i++) {
        if (cache->counter[i] < min_counter) {
            min_counter = cache->counter[i];
            nfu_index = i;
        }
    }
    cache->frames[nfu_index] = page;
    cache->counter[nfu_index] = 1;
    cache->faults++;
    return true;
}

bool clock_access_silent(PageCache* cache, int page) {
    if (cache == NULL) return false;
    for (int i = 0; i < cache->capacity; i++) {
        if (cache->frames[i] == page) {
            cache->time[i] = 1;
            cache->hits++;
            return false;
        }
    }
    for (int i = 0; i < cache->capacity; i++) {
        if (cache->frames[i] == -1) {
            cache->frames[i] = page;
            cache->time[i] = 1;
            cache->faults++;
            return true;
        }
    }
    while (1) {
        int idx = cache->step % cache->capacity;
        if (cache->time[idx] == 0) {
            cache->frames[idx] = page;
            cache->time[idx] = 1;
            cache->step = (cache->step + 1) % cache->capacity;
            cache->faults++;
            return true;
        }
        else {
            cache->time[idx] = 0;
            cache->step = (cache->step + 1) % cache->capacity;
        }
    }
}

// ============================================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ДЛЯ ВЫВОДА И ЗАПУСКА ТЕСТОВ
// ============================================================================

/**
 * Печатает разделитель с заголовком для тестов
 */
void print_separator(const char* title) {
    printf("\n========================================\n");
    printf("  %s\n", title);
    printf("========================================\n");
}

/**
 * Запускает алгоритм с пошаговым выводом (для отладки)
 */
void run_algorithm(const char* name,
    bool (*algo)(PageCache*, int),
    int pages[], int size, int capacity) {
    if (algo == NULL || pages == NULL) {
        printf("  ERROR: Invalid arguments\n");
        return;
    }

    printf("\n[ALGORITHM] %s (capacity=%d)\n", name, capacity);
    printf("Pages: ");
    for (int i = 0; i < size; i++) {
        printf("%d ", pages[i]);
    }
    printf("\n\n");

    PageCache* cache = cache_create(capacity);
    if (cache == NULL) {
        printf("  ERROR: Failed to create cache\n");
        return;
    }

    // Выполняем все обращения к страницам
    for (int i = 0; i < size; i++) {
        algo(cache, pages[i]);
    }

    // Выводим итоговую статистику
    printf("\n  Hits: %d, Faults: %d\n", cache->hits, cache->faults);
    printf("  Hit rate: %.2f%%\n",
        (double)cache->hits / (cache->hits + cache->faults) * 100.0);
    printf("  Fault rate: %.2f%%\n",
        (double)cache->faults / (cache->hits + cache->faults) * 100.0);

    cache_destroy(cache);
}

/**
 * Запускает алгоритм без пошагового вывода (для тестов)
 */
void run_algorithm_silent(const char* name,
    bool (*algo)(PageCache*, int),
    int pages[], int size, int capacity) {
    if (algo == NULL) {
        printf("  ERROR: Invalid arguments\n");
        return;
    }

    // Проверка: если pages == NULL, то size должен быть 0
    if (pages == NULL && size > 0) {
        printf("  ERROR: pages is NULL but size > 0\n");
        return;
    }

    PageCache* cache = cache_create(capacity);
    if (cache == NULL) {
        printf("  ERROR: Failed to create cache\n");
        return;
    }

    // Выполняем все обращения (без вывода каждого шага)
    for (int i = 0; i < size; i++) {
        algo(cache, pages[i]);
    }

    // Выводим только итоговую статистику
    printf("  %-6s: hits=%d, faults=%d, hit_rate=%.1f%%\n",
        name, cache->hits, cache->faults,
        cache->hits + cache->faults > 0 ?
        (double)cache->hits / (cache->hits + cache->faults) * 100.0 : 0.0);

    cache_destroy(cache);
}