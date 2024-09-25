

#include <errno.h>    // errno
#include <sys/mman.h> //mmap
#include <stdio.h>    // printf

#define MAP_SIZE (4UL * 1024 * 1024 * 1024) // 4GB memory
#define IDX_MAX (512UL * 1024 * 1024)

#define step 8
#include <bits/types.h>
#include <stddef.h> // NULL
#define INTELASM(code) ".intel_syntax noprefix\n\t" code "\n\t.att_syntax prefix\n"

#include <omp.h>
typedef __uint8_t uint8_t;
typedef __uint16_t uint16_t;
typedef __uint32_t uint32_t;
typedef __uint64_t uint64_t;

static inline void mwrite(void *v)
{
    asm volatile(
        "movl $10, (%0)"
        :
        : "D"(v)
        :);
}
static inline void movnt(void *addr)
{
    asm volatile(INTELASM("movntdq [%0], xmm1 ")
                 : "+r"(addr)
                 :
                 : "memory", "mm1", "xmm1");
}

inline uint64_t rdtsc() __attribute__((always_inline));
inline uint64_t rdtsc()
{
    uint64_t a, d;
    asm volatile(
        "xor %%rax, %%rax\n"
        "cpuid" ::
            : "rax", "rbx", "rcx", "rdx");
    asm volatile("rdtscp"
                 : "=a"(a), "=d"(d)
                 :
                 : "rcx");
    a = (d << 32) | a;
    return a;
}

inline uint64_t rdtsc2() __attribute__((always_inline));
inline uint64_t rdtsc2()
{
    uint64_t a, d;
    asm volatile("rdtscp"
                 : "=a"(a), "=d"(d)
                 :
                 : "rcx");
    asm volatile("cpuid" ::
                     : "rax", "rbx", "rcx", "rdx");
    a = (d << 32) | a;
    return a;
}

int main(void)
{
    float cpu_freq;
    printf("type in CPU freq, in GHz\n");
    fflush(stdout);
    scanf("%f", &cpu_freq);

    uint64_t mapping_size = 0;
    uint64_t *mapping = NULL;
    uint64_t t0 = 0, res = 0;

    mapping = (uint64_t *)mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_POPULATE | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    printf("Memory mapped at address %p\n", mapping);

    if (mapping == MAP_FAILED)
    {
        if (errno == ENOMEM)
        {
            printf("could not allocate buffer\n");
            return 1;
        }
    }

    for (uint64_t idx = 0; idx < IDX_MAX; idx += step)
    {
        mwrite(&(mapping[idx]));
    }
    t0 = rdtsc();

    asm volatile("mfence" ::: "memory");
    asm volatile("lfence" ::: "memory");
#pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        int num_threads = omp_get_num_threads();
        // printf("%d threads\n", num_threads);

        // Divide the array among threads
        uint64_t chunk_size = IDX_MAX / num_threads;
        uint64_t start_index = thread_id * chunk_size;
        uint64_t end_index = (thread_id + 1) * chunk_size;
        // uint64_t end_index = thread_id * chunk_size;

        // Each thread writes to a different portion of the array
        for (uint64_t idx = start_index; idx < end_index; idx += step)
        {
            // printf("%d threads\tMemory mapped at address %p\n", thread_id, &(mapping[idx]));
            // movnt(&(mapping[idx]));
            mwrite(&(mapping[idx]));
        }
        asm volatile("mfence" ::: "memory");
        asm volatile("lfence" ::: "memory");
        res = rdtsc2() - t0;
    }

    float time = res / cpu_freq / 1000000000UL;
    printf("time %f\n", time);
    printf("bandwidth = %f GB/s\n", 4 / time);
    return 0;
}