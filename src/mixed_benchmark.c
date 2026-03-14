/*
 * 混合基准测试 - 直接对比栈 vs 堆
 *
 * 在相同的工作负载下对比：
 * 1. 分配速度
 * 2. 访问速度
 * 3. 缓存友好性
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>
#include <string.h>

#define ITERATIONS 100000
#define ARRAY_SIZE 64  // 64 * 4 = 256 bytes

// 测量时间的辅助函数
static inline uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

// 栈分配 + 访问
uint64_t benchmark_stack(void) {
    uint64_t start = get_time_ns();

    for (int iter = 0; iter < ITERATIONS; iter++) {
        // 栈分配：只需 sub rsp, N
        int arr[ARRAY_SIZE];

        // 写入
        for (int i = 0; i < ARRAY_SIZE; i++) {
            arr[i] = i;
        }

        // 读取求和
        volatile int sum = 0;
        for (int i = 0; i < ARRAY_SIZE; i++) {
            sum += arr[i];
        }
    }

    uint64_t end = get_time_ns();
    return end - start;
}

// 堆分配 + 访问
uint64_t benchmark_heap(void) {
    uint64_t start = get_time_ns();

    for (int iter = 0; iter < ITERATIONS; iter++) {
        // 堆分配：可能触发系统调用
        int *arr = malloc(ARRAY_SIZE * sizeof(int));
        if (!arr) {
            fprintf(stderr, "malloc失败\n");
            exit(1);
        }

        // 写入
        for (int i = 0; i < ARRAY_SIZE; i++) {
            arr[i] = i;
        }

        // 读取求和
        volatile int sum = 0;
        for (int i = 0; i < ARRAY_SIZE; i++) {
            sum += arr[i];
        }

        // 释放
        free(arr);
    }

    uint64_t end = get_time_ns();
    return end - start;
}

// 堆预分配 + 复用（模拟内存池）
uint64_t benchmark_heap_reuse(void) {
    // 预分配一个大块
    int *arr = malloc(ARRAY_SIZE * sizeof(int));
    if (!arr) {
        fprintf(stderr, "malloc失败\n");
        exit(1);
    }

    uint64_t start = get_time_ns();

    for (int iter = 0; iter < ITERATIONS; iter++) {
        // 复用已分配的内存，不再调用malloc

        // 写入
        for (int i = 0; i < ARRAY_SIZE; i++) {
            arr[i] = i;
        }

        // 读取求和
        volatile int sum = 0;
        for (int i = 0; i < ARRAY_SIZE; i++) {
            sum += arr[i];
        }
    }

    uint64_t end = get_time_ns();

    free(arr);
    return end - start;
}

void print_comparison(const char *name, uint64_t time_ns, uint64_t baseline_ns) {
    printf("%-25s: %10" PRIu64 " ns (%.3f ms) | ",
           name, time_ns, time_ns / 1000000.0);
    printf("平均: %6" PRIu64 " ns | ", time_ns / ITERATIONS);

    if (baseline_ns > 0) {
        double ratio = (double)time_ns / baseline_ns;
        printf("相对栈: %.2fx", ratio);
        if (ratio > 1.0) {
            printf(" (慢 %.1f%%)", (ratio - 1.0) * 100);
        }
    } else {
        printf("基准线");
    }
    printf("\n");
}

int main(void) {
    printf("栈 vs 堆 性能对比基准测试\n");
    printf("==========================\n\n");

    printf("配置:\n");
    printf("  - 迭代次数: %d\n", ITERATIONS);
    printf("  - 数组大小: %d 个整数 (%zu bytes)\n",
           ARRAY_SIZE, ARRAY_SIZE * sizeof(int));
    printf("  - 每次操作: 分配 + 写入 + 读取求和 (+ 释放)\n\n");

    // 预热CPU
    printf("预热中...\n");
    benchmark_stack();
    benchmark_heap();
    printf("\n");

    // 正式测试
    printf("执行基准测试...\n");
    printf("================\n\n");

    uint64_t stack_time = benchmark_stack();
    uint64_t heap_time = benchmark_heap();
    uint64_t heap_reuse_time = benchmark_heap_reuse();

    printf("结果:\n");
    printf("-----\n");
    print_comparison("栈分配", stack_time, 0);
    print_comparison("堆分配（malloc/free）", heap_time, stack_time);
    print_comparison("堆复用（预分配）", heap_reuse_time, stack_time);

    printf("\n关键观察:\n");
    printf("----------\n");
    printf("1. 栈分配最快：只需修改栈指针（sub rsp, N），无系统调用\n");
    printf("2. 堆分配较慢：malloc/free有用户态管理开销，可能触发brk/mmap\n");
    printf("3. 堆复用接近栈：预分配后只剩访问成本，说明差异主要在分配方式\n");
    printf("4. 如果堆复用仍慢于栈，可能因为：\n");
    printf("   - 栈访问的缓存局部性更好（LIFO模式）\n");
    printf("   - 堆地址分散，缓存行利用率低\n");
    printf("\n");

    printf("验证方法:\n");
    printf("---------\n");
    printf("1. perf stat -e cycles,cache-misses,page-faults ./mixed_bench\n");
    printf("   - 对比 cache-misses 和 page-faults\n");
    printf("2. strace -c -e brk,mmap,munmap ./mixed_bench\n");
    printf("   - 查看堆分配触发的系统调用次数\n");
    printf("3. perf record -g ./mixed_bench && perf report\n");
    printf("   - 查看热点函数（malloc vs 栈操作）\n");
    printf("\n");

    return 0;
}
