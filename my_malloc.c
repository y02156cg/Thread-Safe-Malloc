#include "my_malloc.h"

// Locking version
void *ts_malloc_lock(size_t size){
    pthread_mutex_lock(&lock);
    void *address = bf_malloc(size, true);
    pthread_mutex_unlock(&lock);
    return address;
}

void ts_free_lock(void *ptr){
    pthread_mutex_lock(&lock);
    bf_free(ptr, true);
    pthread_mutex_unlock(&lock);
}

// Non-locking version
void *ts_malloc_nolock(size_t size){
    void *address = bf_malloc(size, false);
    return address;
}

void ts_free_nolock(void *ptr){
    bf_free(ptr, false);
}

// Merge adjacent free spaces
void freeMerge(block *bk){
    if(bk == NULL || bk->free_indicator == 0){return;} // If the free block is non-exist

    block *prev_block = bk->prev_block;
    block *next_block = bk->next_block;
    
    // Test if the previous free block is adjacent to the current block
    if (prev_block != NULL && (char *)bk == (char *)prev_block + sizeof(block) + prev_block->size) {
        if(prev_block->free_indicator){
            prev_block->size += bk->size + sizeof(block); // Change previous block size
            prev_block->next_block = bk->next_block;
            if (bk->next_block) {
                bk->next_block->prev_block = prev_block;
            }
            bk = prev_block; // Redefine current block to be previous block adding current block
        }
    }

    // Same operation as the previous free block
    if(next_block!= NULL && ((char *)bk + sizeof(block) + bk->size) == (char *)next_block){
        if(next_block->free_indicator){
            bk->size += next_block->size + sizeof(block);
            bk->next_block = next_block->next_block;
            if (next_block->next_block) {
                next_block->next_block->prev_block = bk;
            }
        }
    }
}

// Move current block from the free list
void freeRemove(block *bk, bool lockIndi){
    // If the free list or current block doesn't exist
    if (bk == NULL) return;

    if (lockIndi) {
        if (bk == head) {
            head = bk->next_block;
            if (head) {
                head->prev_block = NULL;
            }
        } else {
            if (bk->prev_block) {
                bk->prev_block->next_block = bk->next_block;
            }
            if (bk->next_block) {
                bk->next_block->prev_block = bk->prev_block;
            }
        }
    } else {
        if (bk == threadHead) {
            threadHead = bk->next_block;
            if (threadHead) {
                threadHead->prev_block = NULL;
            }
        } else {
            if (bk->prev_block) {
                bk->prev_block->next_block = bk->next_block;
            }
            if (bk->next_block) {
                bk->next_block->prev_block = bk->prev_block;
            }
        }
    }

    bk->free_indicator = 0;
}

// Add a new block to the sorted free list
void freeAdd(block *bk, bool lockIndi){
    if (bk == NULL) {
        return;
    }

    bk->free_indicator = true;

    // Handle the empty list case
    if (lockIndi) {
        if (head == NULL) {
            head = bk;
            bk->next_block = NULL;
            bk->prev_block = NULL;
            return;
        }
    } else {
        if (threadHead == NULL) {
            threadHead = bk;
            bk->next_block = NULL;
            bk->prev_block = NULL;
            return;
        }
    }

    block *curr = lockIndi ? head : threadHead;

    // Find insertion point
    while (curr != NULL && curr <= bk) {
        if(curr->next_block == NULL) {
            break;
        }
        curr = curr->next_block;
    }

    // Insert at beginning or before current position
    if (curr == (lockIndi ? head : threadHead) || bk < curr) {
        bk->next_block = curr;
        bk->prev_block = NULL;
        if (curr) {
            curr->prev_block = bk;
        }
        if (lockIndi) {
            head = bk;
        } else {
            threadHead = bk;
        }
    } else {
        // Insert after current position
        bk->next_block = curr->next_block;
        bk->prev_block = curr;
        curr->next_block = bk;
        if (bk->next_block) {
            bk->next_block->prev_block = bk;
        }
    }
}

// Separate block of size to be occupied and left to be free
void separate(block *bk, size_t size, bool lockIndi) {

    if (bk == NULL || bk->size < size + sizeof(block)) {
        return;
    }

    block *newBlock = (block *)((char *)bk + sizeof(block) + size);

    newBlock->size = bk->size - size - sizeof(block);
    newBlock->free_indicator = true;

    newBlock->prev_block = bk->prev_block;
    newBlock->next_block = bk->next_block;

    // Move the current block bk away from the free list
    if (newBlock->next_block) {
        newBlock->next_block->prev_block = newBlock;
    }

    if (newBlock->prev_block) {
        newBlock->prev_block->next_block = newBlock;
    } 
    else {
        if (lockIndi) {
            head = newBlock;
        } else {
            threadHead = newBlock;
        }
    }

    bk->size = size;
    bk->prev_block = NULL;
    bk->next_block = NULL;
    bk->free_indicator = 0;
}

// Request more memory when there is no enough space
block *request_memory(size_t size, bool lockIndi){
    if(size == 0){return NULL;}

    block *newBlock = NULL;
    if (lockIndi){
        newBlock = (block *)sbrk(size + sizeof(block));
    }
    else{
        pthread_mutex_lock(&lock);
        newBlock = (block *)sbrk(size + sizeof(block));
        pthread_mutex_unlock(&lock);
    }

    // Test if memory allocation fail
    if((void *)newBlock == (void *) - 1){ exit(EXIT_FAILURE);}

    newBlock->size = size;
    newBlock->free_indicator = 0;
    newBlock->prev_block = NULL;
    newBlock->next_block = NULL;

    return newBlock;
}

// Free current pointer address
void bf_free(void *ptr, bool lockIndi){
    if(ptr == NULL){return;}
    block *bk = (block *)((char *)ptr - sizeof(block));
    if(bk->free_indicator){
        printf("Double Free\n");
        exit(EXIT_FAILURE);
    }
    freeAdd(bk, lockIndi);
    freeMerge(bk);
}

// Best-fit allocation
void *bf_malloc(size_t size, bool lockIndi){
    if(size == 0){return NULL;}

    block *best = NULL;
    block *curr = NULL;

    if (lockIndi){
        curr = head;
    }
    else{
        curr = threadHead;
    }

    // Iterate over free list to find block that leaves smallest remaining size
    for(curr; curr != NULL; curr = curr->next_block){
        if(curr->size > size){
            if(best == NULL || curr->size < best->size){
                best = curr;
            }
        } else if(curr->size == size){
            break;
        }
    }

    if(best != NULL){
        // Exactly same block size
        if(best->size <= size + sizeof(block)){
            freeRemove(best, lockIndi);
        }
        else{
            separate(best, size, lockIndi);
        }
    } else{
        block *newBlock = request_memory(size, lockIndi);
        return (char *)newBlock + sizeof(block);
    }

    return (char *)best + sizeof(block);
}