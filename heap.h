//
// Created by arasi on 22.11.2021.
//
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include "custom_unistd.h"
#include <string.h>
#ifndef P1ALO_HEAP_H
#define P1ALO_HEAP_H
#define FENCE 8
#define MEMORY_CHUNK_SIZE 40
#define PAGE_SIZE 4096
struct memory_manager_t
{
    void *memory_start;
    void* brk;
    uint64_t memory_size;
    struct memory_chunk_t *first_memory_chunk;
};
struct memory_manager_t memory_manager;
struct memory_chunk_t
{
    struct memory_chunk_t* prev;
    struct memory_chunk_t* next;
    size_t size;
    int free;
    size_t control_sum;
};

enum pointer_type_t
{
    pointer_null,
    pointer_heap_corrupted,
    pointer_control_block,
    pointer_inside_fences,
    pointer_inside_data_block,
    pointer_unallocated,
    pointer_valid
};

int heap_setup(void);
void heap_clean(void);
void* heap_malloc(size_t size);
void* heap_calloc(size_t number, size_t size);
void* heap_realloc(void* memblock, size_t count);
void  heap_free(void* memblock);
size_t heap_get_largest_used_block_size(void);
enum pointer_type_t get_pointer_type(const void* const pointer);
int heap_validate(void);
void* heap_malloc_aligned(size_t count);
void* heap_calloc_aligned(size_t number, size_t size);
void* heap_realloc_aligned(void* memblock, size_t size);
size_t create_control_sum(struct memory_chunk_t* memblock);
struct memory_chunk_t* create_memory_chunk(size_t size,struct memory_chunk_t *next, struct memory_chunk_t *prev);
struct memory_chunk_t* create_first_memory_chunk(struct memory_chunk_t* new_mem_chunk, size_t size);
void set_fences(void* memblock);
struct memory_chunk_t* extend_last_chunk(struct memory_chunk_t *mem_chunk,size_t count);
struct memory_chunk_t* merge_chunks_realloc(struct memory_chunk_t *mem_chunk,size_t count,size_t true_size);
struct memory_chunk_t* extend_last_chunk_aligned(struct memory_chunk_t *mem_chunk,size_t count);
struct memory_chunk_t* merge_chunks_realloc_aligned(struct memory_chunk_t *mem_chunk,size_t count,size_t true_size);
struct memory_chunk_t* create_memory_chunk_aligned(size_t size,struct memory_chunk_t *next, struct memory_chunk_t *prev);
struct memory_chunk_t *find_free_next_block_aligned(struct memory_chunk_t *first, size_t size);
struct memory_chunk_t* create_first_memory_chunk_aligned(struct memory_chunk_t* new_mem_chunk, size_t size);
void* get_aligned(void* space);
int check_if_aligned(void* ptr);
#endif //P1ALO_HEAP_H
