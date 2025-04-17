#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#ifndef MY_MALLOC_H
#define MY_MALLOC_H

typedef struct blockData{
    size_t size;
    bool free_indicator;
    struct blockData *next_block;
    struct blockData *prev_block;
} block;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

block *head = NULL;
__thread block *threadHead = NULL;

void *ts_malloc_lock(size_t size);
void ts_free_lock(void *ptr);
void *ts_malloc_nolock(size_t size);
void ts_free_nolock(void *ptr);

void freeRemove(block *bk, bool lockIndi);
void freeAdd(block *bk, bool lockIndi);
void freeMerge(block *bk);
void separate(block *bk, size_t size, bool lockIndi);
block *request_memory(size_t size, bool lockIndi);
void *bf_malloc(size_t size, bool lockIndi);
void bf_free(void *ptr, bool lockIndi);
#endif