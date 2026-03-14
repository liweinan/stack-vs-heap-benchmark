/*
 * 堆分配基准测试
 *
 * 展示堆分配的特点：
 * 1. 通过malloc进行分配
 * 2. malloc不足时调用brk/mmap系统调用
 * 3. 用户态/内核态切换开销
 * 4. 内存管理器的开销（arena、锁、碎片管理）
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>
#include <string.h>

// 迭代堆分配
void iterative_heap_alloc(int iterations, int size) {
    for (int iter = 0; iter < iterations; iter++) {
        // 在堆上分配
        int *heap_array = malloc(size * sizeof(int));
        if (!heap_array) {
            fprintf(stderr, "malloc失败\n");
            exit(1);
        }

        // 初始化（触发物理页分配）
        for (int i = 0; i < size; i++) {
            heap_array[i] = i + iter;
        }

        // 计算（防止优化）
        volatile int sum = 0;
        for (int i = 0; i < size; i++) {
            sum += heap_array[i];
        }

        // 释放
        free(heap_array);
    }
}

// 大块分配（触发mmap而不是brk）
void large_heap_alloc(int iterations, int size) {
    for (int iter = 0; iter < iterations; iter++) {
        // 分配大块内存（通常>128KB会用mmap）
        int *heap_array = malloc(size * sizeof(int));
        if (!heap_array) {
            fprintf(stderr, "malloc失败\n");
            exit(1);
        }

        // 初始化
        for (int i = 0; i < size; i++) {
            heap_array[i] = i;
        }

        // 使用
        volatile int sum = 0;
        for (int i = 0; i < size; i++) {
            sum += heap_array[i];
        }

        free(heap_array);
    }
}

// 测量时间的辅助函数
static inline uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

int main(void) {
    const int ITERATIONS = 10000;
    const int SMALL_SIZE = 256;      // 1KB
    const int LARGE_SIZE = 32768;    // 128KB - 触发mmap

    printf("堆分配基准测试\n");
    printf("==============\n\n");

    printf("配置:\n");
    printf("  - 迭代次数: %d\n", ITERATIONS);
    printf("  - 小块大小: %d 个整数 (约 %zu KB)\n", SMALL_SIZE,
           SMALL_SIZE * sizeof(int) / 1024);
    printf("  - 大块大小: %d 个整数 (约 %zu KB)\n\n", LARGE_SIZE,
           LARGE_SIZE * sizeof(int) / 1024);

    // 测试1: 小块分配（brk）
    printf("测试1: 小块堆分配（使用brk扩展堆）\n");
    printf("----------------------------------\n");
    uint64_t start = get_time_ns();
    iterative_heap_alloc(ITERATIONS, SMALL_SIZE);
    uint64_t end = get_time_ns();
    uint64_t elapsed = end - start;

    printf("总时间: %" PRIu64 " ns (%.3f ms)\n", elapsed, elapsed / 1000000.0);
    printf("平均每次分配: %" PRIu64 " ns\n", elapsed / ITERATIONS);
    printf("每次分配成本: ~%.2f cycles (假设3GHz CPU)\n\n",
           (elapsed / ITERATIONS) * 3.0);

    // 测试2: 大块分配（mmap）
    printf("测试2: 大块堆分配（使用mmap）\n");
    printf("---------------------------\n");
    const int LARGE_ITER = 1000;  // 大块分配次数少一些
    start = get_time_ns();
    large_heap_alloc(LARGE_ITER, LARGE_SIZE);
    end = get_time_ns();
    elapsed = end - start;

    printf("总时间: %" PRIu64 " ns (%.3f ms)\n", elapsed, elapsed / 1000000.0);
    printf("平均每次分配: %" PRIu64 " ns\n", elapsed / LARGE_ITER);
    printf("每次分配成本: ~%.2f cycles (假设3GHz CPU)\n\n",
           (elapsed / LARGE_ITER) * 3.0);

    printf("说明:\n");
    printf("-----\n");
    printf("1. malloc不足时调用brk/mmap系统调用向内核申请内存\n");
    printf("2. 系统调用有用户态/内核态切换开销（微秒级）\n");
    printf("3. malloc在用户态管理内存池，有锁和查找开销\n");
    printf("4. 小块分配通常用brk扩展program break\n");
    printf("5. 大块分配（>128KB）通常用mmap创建匿名映射\n");
    printf("6. 使用 strace -c -e brk,mmap 可以看到系统调用统计\n");
    printf("\n");

    return 0;
}
