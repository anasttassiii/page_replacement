#pragma once
#include <stdbool.h>

// Структура кеша
typedef struct PageCache {
    int* frames;      // Массив фреймов
    int* time;        // Время последнего использования (для LRU) или бит ссылки (для CLOCK)
    int* counter;     // Счетчик обращений (для NFU)
    int capacity;     // Размер кеша
    int faults;       // Количество промахов
    int hits;         // Количество попаданий
    int step;         // Шаг для FIFO/CLOCK
    int lru_index;    // Индекс LRU страницы
    int access_count; // Счетчик обращений (для старения NFU)
} PageCache;

// Создание и удаление кеша
PageCache* cache_create(int capacity);
void cache_destroy(PageCache* cache);
void cache_reset(PageCache* cache);

// Алгоритмы вытеснения (с пошаговым выводом)
bool fifo_access(PageCache* cache, int page);
bool lru_access(PageCache* cache, int page);
bool nfu_access(PageCache* cache, int page);
bool clock_access(PageCache* cache, int page);

// Тихие версии (без вывода) для тестов производительности
bool fifo_access_silent(PageCache* cache, int page);
bool lru_access_silent(PageCache* cache, int page);
bool nfu_access_silent(PageCache* cache, int page);
bool clock_access_silent(PageCache* cache, int page);

// Вспомогательные функции
void cache_print(PageCache* cache, int page, bool is_fault);
void print_separator(const char* title);
void run_algorithm(const char* name, bool (*algo)(PageCache*, int), int pages[], int size, int capacity);
void run_algorithm_silent(const char* name, bool (*algo)(PageCache*, int), int pages[], int size, int capacity);