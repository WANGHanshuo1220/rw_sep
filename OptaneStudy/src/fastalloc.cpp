#include "fastalloc.h"

fastalloc *myallocator;
concurrency_fastalloc* concurrency_myallocator;

fastalloc::fastalloc() {}

void fastalloc::init(int ch) {

    channel = ch;
    if(channel < 0 || channel > 3)
    {
        printf("channel should in the range of 0~3\n");
        exit(0);
    }

    dram[dram_cnt] = new char[ALLOC_SIZE];
    dram_curr = dram[dram_cnt];
    dram_left = ALLOC_SIZE;
    dram_cnt++;

#ifdef __linux__
    nvm[nvm_cnt] = new char[ALLOC_SIZE];
    // if you want to use PM, uncomment the following statements and set the according PM path
    string nvm_filename = "/mnt/aep" + to_string(ch) + "/test";
    nvm_filename = nvm_filename + to_string(nvm_cnt);
    int nvm_fd = open(nvm_filename.c_str(), O_CREAT | O_RDWR, 0644);
    if (posix_fallocate(nvm_fd, 0, ALLOC_SIZE) < 0)
        puts("fallocate fail\n");
    nvm[nvm_cnt] = (char *) mmap(NULL, ALLOC_SIZE, PROT_READ | PROT_WRITE, MAP_SYNC | MAP_SHARED_VALIDATE, nvm_fd, 0);
#else
    nvm[nvm_cnt] = new char[ALLOC_SIZE];
#endif
    nvm_curr = nvm[nvm_cnt];
    nvm_left = ALLOC_SIZE;
    nvm_cnt++;
}

void concurrency_fastalloc::init(int channel) {
    dram[dram_cnt] = new char[CONCURRENCY_ALLOC_SIZE];
    dram_curr = dram[dram_cnt];
    dram_left = CONCURRENCY_ALLOC_SIZE;
    dram_cnt++;

#ifdef __linux__
    std::thread::id this_id = std::this_thread::get_id();
    unsigned int t = *(unsigned int*)&this_id;// threadid to unsigned int
    nvm[nvm_cnt] = new char[ALLOC_SIZE];
    // if you want to use PM, uncomment the following statements and set the according PM path
    string nvm_filename = "/mnt/aep" + to_string(channel) + "/test"+to_string(t);
    nvm_filename = nvm_filename + to_string(nvm_cnt);
    int nvm_fd = open(nvm_filename.c_str(), O_CREAT | O_RDWR, 0644);
    if (posix_fallocate(nvm_fd, 0, CONCURRENCY_ALLOC_SIZE) < 0)
        puts("fallocate fail\n");
    nvm[nvm_cnt] = (char *) mmap(NULL, CONCURRENCY_ALLOC_SIZE, PROT_READ | PROT_WRITE, MAP_SYNC | MAP_SHARED_VALIDATE, nvm_fd, 0);
#else
    nvm[nvm_cnt] = new char[CONCURRENCY_ALLOC_SIZE];
#endif
    nvm_curr = nvm[nvm_cnt];
    nvm_left = CONCURRENCY_ALLOC_SIZE;
    nvm_cnt++;
}

void *fastalloc::alloc(uint64_t size, bool _on_nvm) {
    size = size / 64 * 64 + (!!(size % 64)) * 64;
    if (_on_nvm) {
        // printf("alloc in pmem\n");
        if (unlikely(size > nvm_left)) {
            // printf("size exceed, need new pmem area\n");
#ifdef __linux__
            nvm[nvm_cnt] = new char[ALLOC_SIZE];
            // if you want to use PM, uncomment the following statements and set the according PM path
            string nvm_filename = "/mnt/aep" + to_string(channel) + "/test";
            nvm_filename = nvm_filename + to_string(nvm_cnt);
            int nvm_fd = open(nvm_filename.c_str(), O_CREAT | O_RDWR, 0644);
            if (posix_fallocate(nvm_fd, 0, ALLOC_SIZE) < 0)
                puts("fallocate fail\n");
            nvm[nvm_cnt] = (char *) mmap(NULL, ALLOC_SIZE, PROT_READ | PROT_WRITE, MAP_SYNC | MAP_SHARED_VALIDATE, nvm_fd, 0);
#else
            nvm[nvm_cnt] = new char[ALLOC_SIZE];
#endif
            nvm_curr = nvm[nvm_cnt];
            nvm_left = ALLOC_SIZE;
            nvm_cnt++;
            nvm_left -= size;
            void *tmp = nvm_curr;
            nvm_curr = nvm_curr + size;
            return tmp;
        } else {
            nvm_left -= size;
            void *tmp = nvm_curr;
            nvm_curr = nvm_curr + size;
            return tmp;
        }
    } else {
        // printf("alloc in DRAM\n");
        if (unlikely(size > dram_left)) {
            dram[dram_cnt] = new char[ALLOC_SIZE];
            dram_curr = dram[dram_cnt];
            dram_left = ALLOC_SIZE;
            dram_cnt++;
            dram_left -= size;
            void *tmp = dram_curr;
            dram_curr = dram_curr + size;
            return tmp;
        } else {
            dram_left -= size;
            void *tmp = dram_curr;
            dram_curr = dram_curr + size;
            return tmp;
        }
    }
}

void fastalloc::free() {
    if (dram != NULL) {
        dram_left = 0;
        for (int i = 0; i < dram_cnt; ++i) {
            delete[]dram[i];
        }
        dram_curr = NULL;
    }
    // char file[30];
    // for(int i = 0; i < nvm_cnt; i++)
    // {
    //     sprintf(file, "/mnt/aep%d/test0", channel);
    //     if(0 != remove(file))
    //     {
    //         printf("remove files error\n");
    //     }
    // }
    // if( 0 != munmap(nvm[nvm_cnt], ALLOC_SIZE))
    // {
    //     printf("munmap error\n");
    //     exit(0);
    // }
}

fastalloc *init_fast_allocator(int channel) {
    fastalloc * allocator = new fastalloc;
    allocator->init(channel);
    return allocator;
}

void *fast_alloc(fastalloc * allocator, uint64_t size, bool _on_nvm) {
    return allocator->alloc(size, _on_nvm);
}

void *concurrency_fast_alloc(uint64_t size, bool _on_nvm){
    return concurrency_myallocator->alloc(size, _on_nvm);
}

void fast_free(fastalloc * allocator) {
    if(allocator!=NULL){
        allocator->free();
        delete allocator;
    }
}
