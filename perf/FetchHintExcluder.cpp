#define LOG_TAG "FetchHintExcluderShim"

#include <log/log.h>
#include <stdio.h>
#include <sys/mman.h>
#include <string.h>
#include <pthread.h>

extern "C" int32_t custom_FetchHintExcluder(void* /*this_ptr*/, void* /*pMsg*/) {
    ALOGE("INTERCEPTED!");
    return -1; 
}

static uintptr_t get_lib_base(const char* lib_name) {
    FILE* fp = fopen("/proc/self/maps", "r");
    if (!fp) return 0;
    
    char line[512];
    uintptr_t base = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, lib_name)) {
            sscanf(line, "%lx", &base);
            break;
        }
    }
    fclose(fp);
    return base;
}

void* apply_got_hook_delayed(void* /*arg*/) {
    sleep(1);

    ALOGE("beginning GOT memory patch");

    // llvm-readelf -r vendor/fairphone/FP6/proprietary/vendor/lib64/libqti-perfd.so
    // 0000000000067170  000002a700000402 R_AARCH64_JUMP_SLOT    000000000005e2a0 _ZN14HintExtHandler17FetchHintExcluderEP11mpctl_msg_t + 0
    uintptr_t got_offset = 0x67170; 

    uintptr_t base = get_lib_base("libqti-perfd.so");
    if (base == 0) {
        ALOGE("Failed to find libqti-perfd.so base address.");
        return nullptr;
    }

    uintptr_t got_address = base + got_offset;

    size_t page_size = sysconf(_SC_PAGESIZE);
    uintptr_t page_start = got_address & ~(page_size - 1);
        if (mprotect((void*)page_start, page_size, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        ALOGE("[-] mprotect failed!");
        return nullptr;
    }

    void** got_ptr = (void**)got_address;
    ALOGE("Original pointer in GOT: %p", *got_ptr);
    
    *got_ptr = (void*)&custom_FetchHintExcluder;
    return nullptr;
}

__attribute__((constructor)) void on_load() {
    ALOGE("Shim loaded, spawning thread");

    pthread_t thread_id;
    pthread_create(&thread_id, nullptr, apply_got_hook_delayed, nullptr);
    pthread_detach(thread_id);
}