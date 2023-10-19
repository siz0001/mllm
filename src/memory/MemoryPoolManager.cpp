#include "MemoryPoolManager.hpp"
#include "MemoryManager.hpp"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <iterator>

static size_t aligned_offset(size_t offset,size_t alignment){
    assert(alignment && !(alignment & (alignment - 1)));
    auto align = ((alignment - ( offset % alignment))) % alignment;
    return offset + align;
}

namespace mllm {
    MemoryPoolManager::MemoryPoolManager(size_t pool_size,size_t base_alignment):
        data_(nullptr),
        n_free_blocks_(0),
        base_alignment_(base_alignment)
        #ifdef MLLM_ALLOCATOR_DEBUG
        ,allocate_pointers(block_size_)
        #endif
    {
        data_ = std::aligned_alloc(base_alignment,pool_size);
        n_free_blocks_ += 1;
        free_blocks_.emplace_back(data_,pool_size);
    }

    void MemoryPoolManager::alloc(void **ptr, size_t size,size_t alignment){
        // ensure alignment is the power of 2
        assert((alignment & (alignment -1)) == 0);
        assert( alignment <= base_alignment_);

        // TODO: support different backends
        size = aligned_offset(size,base_alignment_);

        std::list<FreeBlock>::iterator best_fit_block;
        auto last_block = free_blocks_.end()--;
        size_t best_fit_size = SIZE_MAX;
        size_t max_avail = 0;
        for(auto it = free_blocks_.begin();it != last_block;it++){
            auto block_size = it->size;
            max_avail = std::max(max_avail,block_size);
            if(block_size >= size && block_size <= best_fit_size){
                best_fit_block = it;
                best_fit_size = block_size;
            }
        }
        if(best_fit_size == SIZE_MAX){
            auto block_size = last_block->size;
            max_avail = std::max(max_avail,block_size);
            if(block_size >= size){
                best_fit_block = last_block;
                best_fit_size = block_size;
            }else{
                std::cerr<<"Not enough space,max available is "<<max_avail;
            }
        }
        *ptr = best_fit_block->addr;
        block_size_.emplace((uint64_t)best_fit_block->addr,size);
        if (best_fit_size > size){
            best_fit_block->addr = (void*)((const char*)best_fit_block->addr + size);
            best_fit_size -= size;
        }else{
            // best_fit_size == size
            free_blocks_.erase(best_fit_block);
        }
    }

    void MemoryPoolManager::free(void *ptr){
        size_t size = 0;
        assert(ptr != nullptr);
        auto ptr_addr = (uint64_t)ptr;

        if (auto iter = block_size_.find(ptr_addr);iter != block_size_.end()){
            size = iter->second;
        }else{
            // can not find size
            throw "can not find address of ptr";
        }
        for(auto iter=free_blocks_.begin();iter != free_blocks_.end();iter++){
            uint64_t block_addr = (uint64_t)iter->addr;
            auto block_size = iter->size;
            // check if the ptr is at the end of the block
            if (block_addr + block_size == ptr_addr){
                iter->size += size;

                // check if we can merge next free block
                if (auto nxt = std::next(iter);nxt != free_blocks_.end() && (uint64_t)iter->addr + iter->size == (uint64_t)nxt->addr){
                    iter->size += nxt->size;
                    n_free_blocks_--;
                    free_blocks_.erase(nxt);
                }
                return;
            }

            // check if the ptr is at the front of the block
            if (ptr_addr + size == block_addr){
                iter->addr = (void*)ptr_addr;
                iter->size += size;

                // check if we can merge previous free block
                if (iter != free_blocks_.begin()){
                    auto prev = std::prev(iter);
                    if ((uint64_t)prev->addr + prev->size == ptr_addr){
                        prev->size += iter->size;
                        n_free_blocks_--;
                        free_blocks_.erase(prev);
                    }
                }
                return;
            }
        }

        // otherwise, create a new block
        bool inserted = false;
        for(auto iter = free_blocks_.begin();iter!=free_blocks_.end();iter++){
            if((uint64_t)iter->addr >= ptr_addr){
                free_blocks_.insert(iter,FreeBlock{ptr,size});
                inserted = true;
                break;
            }
        }
        if (!inserted){
            free_blocks_.emplace_back(ptr,size);
        }
        n_free_blocks_++;
    }
    MemoryPoolManager::~MemoryPoolManager(){
        free(data_);
    }
}