/*
 * 公平的堆 vs 栈性能对比测试
 *
 * 目标：展示堆和栈各自适合的场景，而非简单的"谁快用谁"
 *
 * 测试场景：
 * 1. 小对象短生命周期 - 栈赢
 * 2. 对象复用 - 堆可能赢
 * 3. 大对象分配 - 栈不可行，堆必需
 * 4. 内存池优化 - 堆接近栈速度
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#define ITERATIONS 1000000
#define SMALL_SIZE 1024
#define LARGE_SIZE (1024 * 1024)  // 1MB

// 全局变量防止优化
volatile int g_sum = 0;

//=============================================================================
// 场景1：小对象短生命周期（栈应该赢）
//=============================================================================

uint64_t test1_stack_small_object() {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < ITERATIONS; i++) {
        char buffer[SMALL_SIZE];
        volatile char *p = buffer;

        // 模拟使用
        p[0] = i & 0xFF;
        p[SMALL_SIZE-1] = (i >> 8) & 0xFF;
        g_sum += p[0] + p[SMALL_SIZE-1];
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    return (end.tv_sec - start.tv_sec) * 1000000000ULL +
           (end.tv_nsec - start.tv_nsec);
}

uint64_t test1_heap_small_object() {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < ITERATIONS; i++) {
        char *buffer = malloc(SMALL_SIZE);
        volatile char *p = buffer;

        // 模拟使用
        p[0] = i & 0xFF;
        p[SMALL_SIZE-1] = (i >> 8) & 0xFF;
        g_sum += p[0] + p[SMALL_SIZE-1];

        free(buffer);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    return (end.tv_sec - start.tv_sec) * 1000000000ULL +
           (end.tv_nsec - start.tv_nsec);
}

//=============================================================================
// 场景2：对象复用（堆可能更好）
//=============================================================================

// 栈方式：每次调用函数
void process_with_stack_buffer(int value) {
    char buffer[SMALL_SIZE];
    volatile char *p = buffer;

    p[0] = value & 0xFF;
    p[SMALL_SIZE-1] = (value >> 8) & 0xFF;
    g_sum += p[0] + p[SMALL_SIZE-1];
}

uint64_t test2_stack_with_function_call() {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < ITERATIONS; i++) {
        process_with_stack_buffer(i);  // 每次调用函数
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    return (end.tv_sec - start.tv_sec) * 1000000000ULL +
           (end.tv_nsec - start.tv_nsec);
}

// 堆方式：分配一次，重复使用
uint64_t test2_heap_reuse() {
    struct timespec start, end;

    // 分配一次
    char *buffer = malloc(SMALL_SIZE);

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < ITERATIONS; i++) {
        volatile char *p = buffer;

        p[0] = i & 0xFF;
        p[SMALL_SIZE-1] = (i >> 8) & 0xFF;
        g_sum += p[0] + p[SMALL_SIZE-1];
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    free(buffer);

    return (end.tv_sec - start.tv_sec) * 1000000000ULL +
           (end.tv_nsec - start.tv_nsec);
}

//=============================================================================
// 场景3：大对象分配（栈不可行）
//=============================================================================

// 栈方式：小心栈溢出！
// 注意：这里用较小的对象（256KB）避免真的栈溢出
uint64_t test3_stack_medium_object() {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

#define MEDIUM_SIZE (256 * 1024)  // 256KB（在安全范围内）

    for (int i = 0; i < 100; i++) {  // 只100次，避免栈压力
        char buffer[MEDIUM_SIZE];
        volatile char *p = buffer;

        p[0] = i & 0xFF;
        p[MEDIUM_SIZE-1] = (i >> 8) & 0xFF;
        g_sum += p[0] + p[MEDIUM_SIZE-1];
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
#undef MEDIUM_SIZE
    return (end.tv_sec - start.tv_sec) * 1000000000ULL +
           (end.tv_nsec - start.tv_nsec);
}

// 堆方式：可以安全分配大对象
uint64_t test3_heap_large_object() {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < 100; i++) {
        char *buffer = malloc(LARGE_SIZE);  // 1MB，没问题
        if (buffer) {
            volatile char *p = buffer;

            p[0] = i & 0xFF;
            p[LARGE_SIZE-1] = (i >> 8) & 0xFF;
            g_sum += p[0] + p[LARGE_SIZE-1];

            free(buffer);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    return (end.tv_sec - start.tv_sec) * 1000000000ULL +
           (end.tv_nsec - start.tv_nsec);
}

//=============================================================================
// 场景4：内存池优化（堆可以很快）
//=============================================================================

// 简单的内存池实现
typedef struct {
    char *memory;
    size_t block_size;
    size_t total_blocks;
    size_t next_free;
} MemoryPool;

MemoryPool* pool_create(size_t block_size, size_t num_blocks) {
    MemoryPool *pool = malloc(sizeof(MemoryPool));
    pool->memory = malloc(block_size * num_blocks);
    pool->block_size = block_size;
    pool->total_blocks = num_blocks;
    pool->next_free = 0;
    return pool;
}

void* pool_alloc(MemoryPool *pool) {
    if (pool->next_free >= pool->total_blocks) {
        pool->next_free = 0;  // 简单循环复用
    }
    void *ptr = pool->memory + (pool->next_free * pool->block_size);
    pool->next_free++;
    return ptr;
}

void pool_destroy(MemoryPool *pool) {
    free(pool->memory);
    free(pool);
}

uint64_t test4_heap_with_pool() {
    struct timespec start, end;

    // 创建内存池
    MemoryPool *pool = pool_create(SMALL_SIZE, 1000);

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < ITERATIONS; i++) {
        char *buffer = pool_alloc(pool);  // O(1) 分配
        volatile char *p = buffer;

        p[0] = i & 0xFF;
        p[SMALL_SIZE-1] = (i >> 8) & 0xFF;
        g_sum += p[0] + p[SMALL_SIZE-1];

        // 注意：内存池不需要每次free
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    pool_destroy(pool);

    return (end.tv_sec - start.tv_sec) * 1000000000ULL +
           (end.tv_nsec - start.tv_nsec);
}

//=============================================================================
// 主测试函数
//=============================================================================

void print_result(const char *name, uint64_t time_ns, int iterations) {
    printf("  %-40s: %8.3f ms (平均 %6.1f ns/次)\n",
           name,
           time_ns / 1000000.0,
           (double)time_ns / iterations);
}

void run_test(const char *title,
              const char *stack_name, uint64_t (*stack_test)(),
              const char *heap_name, uint64_t (*heap_test)(),
              int iterations) {
    printf("\n%s\n", title);
    printf("========================================\n");

    uint64_t stack_time = stack_test();
    print_result(stack_name, stack_time, iterations);

    uint64_t heap_time = heap_test();
    print_result(heap_name, heap_time, iterations);

    double ratio = (double)heap_time / stack_time;
    printf("  %-40s: %.2fx\n", "堆/栈比例", ratio);

    if (ratio < 1.0) {
        printf("  ✓ 这个场景堆更快（或接近）\n");
    } else if (ratio < 2.0) {
        printf("  ⚠️ 堆只慢 %.0f%%，可接受\n", (ratio - 1) * 100);
    } else {
        printf("  ✗ 栈显著更快（堆慢 %.1fx）\n", ratio);
    }
}

int main() {
    printf("======================================\n");
    printf("公平的堆 vs 栈性能对比测试\n");
    printf("======================================\n");
    printf("\n目标：展示各自适合的场景，而非简单的\"谁快用谁\"\n");

    // 场景1：小对象短生命周期（栈应该赢）
    run_test(
        "场景1：小对象（1KB）短生命周期 - 100万次",
        "栈（局部变量）",
        test1_stack_small_object,
        "堆（malloc/free）",
        test1_heap_small_object,
        ITERATIONS
    );

    // 场景2：对象复用（堆可能赢）
    run_test(
        "场景2：对象复用 - 100万次使用",
        "栈（每次函数调用）",
        test2_stack_with_function_call,
        "堆（分配一次，重复使用）",
        test2_heap_reuse,
        ITERATIONS
    );

    // 场景3：大对象分配
    run_test(
        "场景3：大对象分配 - 100次",
        "栈（256KB，在安全范围）",
        test3_stack_medium_object,
        "堆（1MB，无压力）",
        test3_heap_large_object,
        100
    );

    // 场景4：内存池优化
    run_test(
        "场景4：内存池优化 - 100万次",
        "栈（局部变量）",
        test1_stack_small_object,
        "堆（内存池，O(1)分配）",
        test4_heap_with_pool,
        ITERATIONS
    );

    printf("\n======================================\n");
    printf("结论\n");
    printf("======================================\n");
    printf("\n");
    printf("✓ 场景1：栈完胜（小对象短生命周期）\n");
    printf("  → 这是栈的优势场景\n\n");

    printf("⚖️ 场景2：堆可能更好（对象复用）\n");
    printf("  → 堆分配一次 vs 栈每次函数调用\n");
    printf("  → 取决于函数调用开销\n\n");

    printf("✓ 场景3：堆必需（大对象）\n");
    printf("  → 栈有大小限制（~8MB）\n");
    printf("  → 堆可以分配任意大小\n\n");

    printf("⚖️ 场景4：堆接近栈（内存池）\n");
    printf("  → 内存池优化后，堆可以很快\n");
    printf("  → tcmalloc/jemalloc更快\n\n");

    printf("核心要点：\n");
    printf("  不是\"栈快为什么不全用栈\"\n");
    printf("  而是\"什么场景用什么\"\n\n");

    printf("  - 小对象、短生命周期 → 栈（性能最优）\n");
    printf("  - 大对象、跨函数、动态大小 → 堆（功能需求）\n");
    printf("  - 对象复用、内存池 → 堆也可以很快\n");
    printf("\n");

    return 0;
}
