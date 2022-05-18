#include "heap.h"
#include "tested_declarations.h"
#include "rdebug.h"




int heap_setup(void){
    memory_manager.memory_start = custom_sbrk(0);
    if(memory_manager.memory_start == (void*) -1)
        return -1;
    memory_manager.brk = custom_sbrk(0);
    memory_manager.memory_size = 0;
    memory_manager.first_memory_chunk = NULL;
    return 0;
}
void heap_clean(void){
    memory_manager.memory_start = custom_sbrk((int64_t)-memory_manager.memory_size);
    memory_manager.brk = NULL;
    memory_manager.memory_size = 0;
    memory_manager.memory_start = NULL;
    memory_manager.first_memory_chunk = NULL;
}
void set_fences (void* memblock){
    if (memblock == NULL)
        return;
    memset(memblock, '#', FENCE);
    memset(((char*)memblock + ((struct memory_chunk_t*)((char*)memblock - sizeof(struct memory_chunk_t)))->size + FENCE), '#', FENCE);
}
size_t create_control_sum(struct memory_chunk_t* memory_chunk){
    unsigned char* mem_to_calculate = (unsigned char *)memory_chunk;
    size_t control_sum = 0;
    for (unsigned int i = 0; i < MEMORY_CHUNK_SIZE - sizeof(size_t); ++i) {
        control_sum += mem_to_calculate[i];
    }
    return control_sum;
}

void* request_space(uint32_t space){
    void* request = custom_sbrk(space);
    if(request == (void *)-1)
        return NULL;
    memory_manager.memory_size += space;
    memory_manager.brk = custom_sbrk(0);
    return request;
}

struct memory_chunk_t* create_first_memory_chunk(struct memory_chunk_t* new_mem_chunk, size_t size){
    if(memory_manager.memory_size < size)
        if(!request_space(size + 2*FENCE + MEMORY_CHUNK_SIZE))
            return NULL;

    new_mem_chunk = (struct memory_chunk_t*)memory_manager.memory_start;
    new_mem_chunk->next = NULL;
    new_mem_chunk->prev = NULL;
    new_mem_chunk->size = size;
    new_mem_chunk->free = 0;
    new_mem_chunk->control_sum = create_control_sum(new_mem_chunk);
    set_fences((char*)new_mem_chunk + MEMORY_CHUNK_SIZE);
    memory_manager.first_memory_chunk = new_mem_chunk;
    return new_mem_chunk;
}
//if it would be too slow uncomment
struct memory_chunk_t *find_free_next_block(struct memory_chunk_t *first, size_t size) { //, size_t* mem_size_to_put
    // size_t calculate = memory_manager.memory_size;
    while(first->next && !(first->free && first->size >= size + 2*FENCE)){
        first = first->next;
        //   calculate -= (char*)first->next - (char*)first;
    }
    /*   if(mem_size_to_put != NULL) {
           *mem_size_to_put = calculate;
       }*/
    return first;
}

struct memory_chunk_t* create_memory_chunk(size_t size,struct memory_chunk_t *next, struct memory_chunk_t *prev){
    //, size_t* mem_size_to_put
    struct memory_chunk_t* mem_chunk = request_space(size + 2*FENCE + MEMORY_CHUNK_SIZE);
    if(mem_chunk == NULL)
        return NULL;
    mem_chunk->prev = prev;
    mem_chunk->next = next;
    mem_chunk->free = 0;
    mem_chunk->size = size;
    prev->next = mem_chunk;
    set_fences((char*)mem_chunk + MEMORY_CHUNK_SIZE);
    mem_chunk->control_sum = create_control_sum(mem_chunk);
    prev->control_sum = create_control_sum(prev);
    return mem_chunk;
}
void* heap_malloc(size_t size){
    if(size == 0 || heap_validate())
        return NULL;

    struct memory_chunk_t* mem_chunk = memory_manager.first_memory_chunk;

    if(mem_chunk == NULL) {
        if((mem_chunk = create_first_memory_chunk(mem_chunk, size)) == NULL)
            return NULL;
    }else {
        //if it would be too slow
        //size_t mem_size;
        struct memory_chunk_t *first = memory_manager.first_memory_chunk;
        mem_chunk = find_free_next_block(first, size); //, &mem_size
        if(mem_chunk->next == NULL){
            //    mem_size -= MEMORY_CHUNK_SIZE - mem_chunk->size - 2*FENCE;
            if((mem_chunk->next = create_memory_chunk(size,NULL,mem_chunk)) == NULL)
                return NULL;
            mem_chunk = mem_chunk->next;
        }else{
            mem_chunk->size = size;
            set_fences((char*)mem_chunk + MEMORY_CHUNK_SIZE);
            mem_chunk->free = 0;
            mem_chunk->control_sum = create_control_sum(mem_chunk);
        }
    }
    return (void*)((char*)mem_chunk + MEMORY_CHUNK_SIZE + FENCE);
}
void* heap_calloc(size_t number, size_t size){
    if(number == 0 || size == 0)
        return NULL;
    size_t size_for_malloc = number * size;
    void* ptr = heap_malloc(size_for_malloc);
    if(ptr != NULL)
        memset(ptr,0,size_for_malloc);
    return ptr;
}

struct memory_chunk_t* merge_left(struct memory_chunk_t* mem_chunk){
    if(mem_chunk->prev != NULL) {
        if (mem_chunk->prev->free == 1) {
            struct memory_chunk_t *pchunk = mem_chunk->prev;
            pchunk->size += mem_chunk->size + sizeof(struct memory_chunk_t);
            if (mem_chunk->next != NULL) {
                mem_chunk->next->prev = pchunk;
                mem_chunk->next->control_sum = create_control_sum(mem_chunk->next);
            }
            pchunk->next = mem_chunk->next;
            mem_chunk = pchunk;
            //   pchunk->next->control_sum = create_control_sum(pchunk->next);

        }else if(mem_chunk->next == NULL){
            mem_chunk->prev->next = NULL;
            mem_chunk->prev->control_sum = create_control_sum(mem_chunk->prev);
        }
    }
    return mem_chunk;
}
struct memory_chunk_t* merge_right(struct memory_chunk_t*mem_chunk){
    if (mem_chunk->next != NULL) {
        mem_chunk->size = (char *) mem_chunk->next - (char *) mem_chunk - sizeof(struct memory_chunk_t);
        if (mem_chunk->next->free == 1) {
            struct memory_chunk_t *nchunk = mem_chunk->next;
            mem_chunk->size += nchunk->size + sizeof(struct memory_chunk_t);
            mem_chunk->next = nchunk->next;
            if (nchunk->next != NULL) {
                nchunk->next->prev = mem_chunk;
                nchunk->next->control_sum = create_control_sum(nchunk->next);
            }

        }
    }

    return mem_chunk;
}

void  heap_free(void* memblock){
    if(memblock == NULL || heap_validate() || get_pointer_type(memblock) != pointer_valid)
        return;
    struct memory_chunk_t* mem_chunk = (struct memory_chunk_t*)((char*)memblock - MEMORY_CHUNK_SIZE - FENCE);

    mem_chunk = merge_right(mem_chunk);
    mem_chunk = merge_left(mem_chunk);
    mem_chunk->free = 1;
    if(mem_chunk->prev == NULL && mem_chunk->next == NULL){
        memory_manager.first_memory_chunk = NULL;
    }
    mem_chunk->control_sum = create_control_sum(mem_chunk);

}
int heap_validate(void){
    if (memory_manager.brk == NULL && memory_manager.memory_start == NULL && memory_manager.first_memory_chunk == NULL && memory_manager.memory_size == 0)
        return 2;
    for (struct memory_chunk_t* temp = memory_manager.first_memory_chunk; temp!=NULL; temp = temp->next){
        if(temp->control_sum != create_control_sum(temp))
            return 3;
        if((memcmp((char*)temp + MEMORY_CHUNK_SIZE,"########",FENCE) != 0 || memcmp((char*)temp + temp->size + MEMORY_CHUNK_SIZE + FENCE ,"########",FENCE) != 0)
           && temp->free == 0)
            return 1;

    }
    return 0;
}
enum pointer_type_t get_pointer_type(const void* const pointer){
    if(pointer == NULL)
        return pointer_null;
    int check = heap_validate();
    if(check == 3 || check == 1)
        return pointer_heap_corrupted;
    for (struct memory_chunk_t* temp = memory_manager.first_memory_chunk; temp != NULL; temp = temp->next) {
        int64_t cmp = (char *) temp - (char *) pointer;
        if (temp->free) {
            if ((uint64_t )(-cmp) < temp->size + MEMORY_CHUNK_SIZE) {
                return pointer_unallocated;
            }
        } else {
            if ((uint64_t )(-cmp) < temp->size + MEMORY_CHUNK_SIZE + 2 * FENCE) {
                if ((uint64_t )(-cmp) < MEMORY_CHUNK_SIZE)
                    return pointer_control_block;
                else if ((uint64_t )(-cmp) == MEMORY_CHUNK_SIZE + FENCE)
                    return pointer_valid;
                else if ((uint64_t )(-cmp) - MEMORY_CHUNK_SIZE < FENCE ||
                         (uint64_t )(-cmp) - MEMORY_CHUNK_SIZE - temp->size - FENCE < FENCE)
                    return pointer_inside_fences;
                else
                    return pointer_inside_data_block;
            }
            // jezeli obszarem nie zaalokowanym tez bedzie pozostalosc po alligned zmienic trzeba elsa

        }
    }
    return pointer_unallocated;
}
struct memory_chunk_t* merge_chunks_realloc(struct memory_chunk_t *mem_chunk,size_t count,size_t true_size){
    //jezeli drugi blok ma doklnie tyle pamieci to usuwamy
    //fix if too much time taken
    if(true_size + mem_chunk->next->size >= count){
        size_t diff = count - true_size;
        struct  memory_chunk_t* temp = (struct  memory_chunk_t*)((char *)mem_chunk->next + diff);
        if(mem_chunk->next->next != NULL) {
            mem_chunk->next->next->prev = temp;
            mem_chunk->next->next->control_sum = create_control_sum(mem_chunk->next->next);
        }
        struct memory_chunk_t*next = mem_chunk->next->next,*prev = mem_chunk;
        size_t temp_size = mem_chunk->next->size - diff;
        int temp_free = 1;
        temp->next = next;
        temp->size = temp_size;
        temp->prev = prev;
        temp->free = temp_free;
        temp->control_sum = create_control_sum(temp);
        mem_chunk->size = count;
        set_fences((char*)mem_chunk + MEMORY_CHUNK_SIZE);
        mem_chunk->next = temp;
        mem_chunk->control_sum = create_control_sum(mem_chunk);
    }else if(true_size + mem_chunk->next->size + MEMORY_CHUNK_SIZE>= count){
        if(mem_chunk->next->next != NULL) {
            mem_chunk->next->next->prev = mem_chunk;
            mem_chunk->next->next->control_sum = create_control_sum(mem_chunk->next->next);
        }
        mem_chunk->size = count;
        mem_chunk->next = mem_chunk->next->next;
        set_fences((char*)mem_chunk + MEMORY_CHUNK_SIZE);
        mem_chunk->control_sum = create_control_sum(mem_chunk);
    }


    return mem_chunk;
}
struct memory_chunk_t* extend_last_chunk(struct memory_chunk_t *mem_chunk,size_t count){
    if(!request_space(count))
        return NULL;

    mem_chunk->size += count;
    mem_chunk->control_sum = create_control_sum(mem_chunk);
    set_fences((char*)mem_chunk + MEMORY_CHUNK_SIZE);
    return mem_chunk;
}
void* heap_realloc(void* memblock, size_t count){
    if((memblock == NULL && count == 0) || heap_validate())
        return NULL;
    if(memblock == NULL)
        return heap_malloc(count);
    else if(count == 0) {
        heap_free(memblock);
        return NULL;
    }
    if(get_pointer_type(memblock) != pointer_valid)
        return NULL;

    struct memory_chunk_t* mem_chunk = (struct memory_chunk_t*)((char*)memblock - MEMORY_CHUNK_SIZE - FENCE);
    if(count < mem_chunk->size){
        mem_chunk->size = count;
        set_fences((char*)memblock - FENCE);
        mem_chunk->control_sum = create_control_sum(mem_chunk);
    }else if(count > mem_chunk->size){
        if(mem_chunk->next != NULL){
            size_t true_size = (char *) mem_chunk->next - (char *) mem_chunk - MEMORY_CHUNK_SIZE- 2*FENCE;
            if(mem_chunk->next->free && mem_chunk->next->size + true_size + MEMORY_CHUNK_SIZE > count){
                return (char*)merge_chunks_realloc(mem_chunk,count,true_size) + MEMORY_CHUNK_SIZE + FENCE;
            }
        }

        if((char*)memory_manager.brk - (char*)memblock <  (int64_t)(count - mem_chunk->size + FENCE) && !mem_chunk->next ){
            mem_chunk = extend_last_chunk(mem_chunk,count - mem_chunk->size);
        }else{
            char* ptr = heap_malloc(count);
            if(ptr == NULL)
                return NULL;
            memcpy(ptr,memblock, mem_chunk->size);
            heap_free(memblock);
            mem_chunk = (struct memory_chunk_t*)((char*)ptr - MEMORY_CHUNK_SIZE - FENCE);
        }
        if (mem_chunk == NULL)
            return NULL;
        memblock = (char*)mem_chunk + MEMORY_CHUNK_SIZE + FENCE;
    }

    return memblock;
}

int check_if_aligned(void* ptr){
    if(((intptr_t)ptr & (intptr_t)(PAGE_SIZE - 1)) == 0)
        return 1;
    return 0;
}
void* get_aligned(void* space){
    void* position;
    for(position = (char*)space + MEMORY_CHUNK_SIZE + FENCE; !check_if_aligned(position);position = (char*)position + 1);
    return position;
}

struct memory_chunk_t* create_first_memory_chunk_aligned(struct memory_chunk_t* new_mem_chunk, size_t size){
    if(memory_manager.memory_size < size)
        if(!request_space(size + 2*FENCE + MEMORY_CHUNK_SIZE + PAGE_SIZE))
            return NULL;
    void* aligned = get_aligned(memory_manager.memory_start);
    new_mem_chunk = (struct memory_chunk_t*)((char*)aligned - MEMORY_CHUNK_SIZE - FENCE);
    new_mem_chunk->next = NULL;
    new_mem_chunk->prev = NULL;
    new_mem_chunk->size = size;
    new_mem_chunk->free = 0;
    new_mem_chunk->control_sum = create_control_sum(new_mem_chunk);
    set_fences((char*)new_mem_chunk + MEMORY_CHUNK_SIZE);
    memory_manager.first_memory_chunk = new_mem_chunk;
    return new_mem_chunk;
}
//if it would be too slow uncomment
struct memory_chunk_t *find_free_next_block_aligned(struct memory_chunk_t *first, size_t size) { //, size_t* mem_size_to_put
    // size_t calculate = memory_manager.memory_size;
    while(first->next) {
        if((first->free && first->size >= size + 2*FENCE) && check_if_aligned((char*)first + MEMORY_CHUNK_SIZE + FENCE))
            break;
        first = first->next;
        //   calculate -= (char*)first->next - (char*)first;
    }
    /*   if(mem_size_to_put != NULL) {
           *mem_size_to_put = calculate;
       }*/
    return first;
}

struct memory_chunk_t* create_memory_chunk_aligned(size_t size,struct memory_chunk_t *next, struct memory_chunk_t *prev){
    //, size_t* mem_size_to_put
    struct memory_chunk_t* mem_chunk = request_space(size + 2*FENCE + MEMORY_CHUNK_SIZE + PAGE_SIZE);
    if(mem_chunk == NULL)
        return NULL;
    void* position = get_aligned(mem_chunk);
    mem_chunk = (struct memory_chunk_t*)((char*)position - MEMORY_CHUNK_SIZE - FENCE);
    mem_chunk->prev = prev;
    mem_chunk->next = next;
    mem_chunk->free = 0;
    mem_chunk->size = size;
    prev->next = mem_chunk;
    set_fences((char*)mem_chunk + MEMORY_CHUNK_SIZE);
    mem_chunk->control_sum = create_control_sum(mem_chunk);
    prev->control_sum = create_control_sum(prev);
    return mem_chunk;
}

void* heap_malloc_aligned(size_t count){
    if(count == 0 || heap_validate())
        return NULL;

    struct memory_chunk_t* mem_chunk = memory_manager.first_memory_chunk;

    if(mem_chunk == NULL) {
        if((mem_chunk = create_first_memory_chunk_aligned(mem_chunk, count)) == NULL)
            return NULL;
    }else {
        struct memory_chunk_t *first = memory_manager.first_memory_chunk;
        mem_chunk = find_free_next_block_aligned(first, count);
        if(mem_chunk->next == NULL){
            if((mem_chunk->next = create_memory_chunk_aligned(count,NULL,mem_chunk)) == NULL)
                return NULL;
            mem_chunk = mem_chunk->next;
        }else{
            mem_chunk->size = count;
            set_fences((char*)mem_chunk + MEMORY_CHUNK_SIZE);
            mem_chunk->free = 0;
            mem_chunk->control_sum = create_control_sum(mem_chunk);
        }
    }
    return (void*)((char*)mem_chunk + MEMORY_CHUNK_SIZE + FENCE);
}
void* heap_calloc_aligned(size_t number, size_t size){
    if(number == 0 || size == 0)
        return NULL;
    size_t size_for_malloc = number * size;
    void* ptr = heap_malloc_aligned(size_for_malloc);
    if(ptr != NULL)
        memset(ptr,0,size_for_malloc);
    return ptr;
}

size_t heap_get_largest_used_block_size(void){
    if(memory_manager.first_memory_chunk == NULL || heap_validate())
        return 0;
    size_t size = 0;
    for(struct memory_chunk_t* temp = memory_manager.first_memory_chunk; temp != NULL ; temp = temp->next)
        if(temp->size > size && !temp->free)
            size = temp->size;

    return size;
}

struct memory_chunk_t* merge_chunks_realloc_aligned(struct memory_chunk_t *mem_chunk,size_t count,size_t true_size){
    //jezeli drugi blok ma doklnie tyle pamieci to usuwamy
    //fix if too much time taken
    if(true_size + mem_chunk->next->size >= count){
        size_t diff = count - true_size;
        struct  memory_chunk_t* temp = (struct  memory_chunk_t*)((char *)mem_chunk->next + diff);
        if(mem_chunk->next->next != NULL) {
            mem_chunk->next->next->prev = temp;
            mem_chunk->next->next->control_sum = create_control_sum(mem_chunk->next->next);
        }
        struct memory_chunk_t*next = mem_chunk->next->next,*prev = mem_chunk;
        size_t temp_size = mem_chunk->next->size - diff;
        int temp_free = 1;
        temp->next = next;
        temp->size = temp_size;
        temp->prev = prev;
        temp->free = temp_free;
        temp->control_sum = create_control_sum(temp);
        mem_chunk->size = count;
        set_fences((char*)mem_chunk + MEMORY_CHUNK_SIZE);
        mem_chunk->next = temp;
        mem_chunk->control_sum = create_control_sum(mem_chunk);
    }else if(true_size + mem_chunk->next->size + MEMORY_CHUNK_SIZE>= count){
        if(mem_chunk->next->next != NULL) {
            mem_chunk->next->next->prev = mem_chunk;
            mem_chunk->next->next->control_sum = create_control_sum(mem_chunk->next->next);
        }
        mem_chunk->size = count;
        mem_chunk->next = mem_chunk->next->next;
        set_fences((char*)mem_chunk + MEMORY_CHUNK_SIZE);
        mem_chunk->control_sum = create_control_sum(mem_chunk);
    }


    return mem_chunk;
}
struct memory_chunk_t* extend_last_chunk_aligned(struct memory_chunk_t *mem_chunk,size_t count){
    if(!request_space(count+PAGE_SIZE))
        return NULL;
    mem_chunk->size += count;
    mem_chunk->control_sum = create_control_sum(mem_chunk);
    set_fences((char*)mem_chunk + MEMORY_CHUNK_SIZE);
    return mem_chunk;
}
void* heap_realloc_aligned(void* memblock, size_t count){
    if((memblock == NULL && count == 0) || heap_validate())
        return NULL;
    if(memblock == NULL)
        return heap_malloc_aligned(count);
    else if(count == 0) {
        heap_free(memblock);
        return NULL;
    }
    if(get_pointer_type(memblock) != pointer_valid && !check_if_aligned(memblock))
        return NULL;

    struct memory_chunk_t* mem_chunk = (struct memory_chunk_t*)((char*)memblock - MEMORY_CHUNK_SIZE - FENCE);
    if(count < mem_chunk->size){
        mem_chunk->size = count;
        set_fences((char*)memblock - FENCE);
        mem_chunk->control_sum = create_control_sum(mem_chunk);
    }else if(count > mem_chunk->size){
        if(mem_chunk->next != NULL){
            size_t true_size = (char *) mem_chunk->next - (char *) mem_chunk - MEMORY_CHUNK_SIZE- 2*FENCE;
            if(true_size > count){
                mem_chunk->size = count;
                set_fences((char*)memblock - FENCE);
                mem_chunk->control_sum = create_control_sum(mem_chunk);
                return (char*)mem_chunk + MEMORY_CHUNK_SIZE + FENCE;
            }

            if(mem_chunk->next->free && mem_chunk->next->size + true_size + MEMORY_CHUNK_SIZE > count){
                return (char*)merge_chunks_realloc_aligned(mem_chunk,count,true_size) + MEMORY_CHUNK_SIZE + FENCE;
            }

        }

        if((char*)memory_manager.brk - (char*)memblock <  (int64_t)(count - mem_chunk->size + FENCE) && !mem_chunk->next ){
            mem_chunk = extend_last_chunk_aligned(mem_chunk,count - mem_chunk->size);
        }else{
            char* ptr = heap_malloc_aligned(count);
            if(ptr == NULL)
                return NULL;
            memcpy(ptr,memblock, mem_chunk->size);
            heap_free(memblock);
            mem_chunk = (struct memory_chunk_t*)((char*)ptr - MEMORY_CHUNK_SIZE - FENCE);
        }
        if (mem_chunk == NULL)
            return NULL;
        memblock = (char*)mem_chunk + MEMORY_CHUNK_SIZE + FENCE;
    }

    return memblock;
}

