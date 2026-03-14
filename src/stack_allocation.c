/*
 * 栈分配基准测试
 *
 * 展示栈分配的特点：
 * 1. 分配只需修改栈指针（sub rsp, N）
 * 2. 不涉及系统调用
 * 3. 物理内存按需分配（首次访问触发缺页）
 * 4. LIFO访问模式，缓存友好
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>
#include <string.h>

// 递归分配栈空间
void recursive_stack_alloc(int depth, int size) {
    if (depth <= 0) return;

    // 在栈上分配数组（通过VLA - Variable Length Array）
    // 这会执行类似 sub rsp, size*sizeof(int) 的操作
    int local_array[size];

    // 写入数据，触发首次访问该页的缺页异常（如果还没有映射物理页）
    // 缓存友好：访问栈顶附近的内存
    for (int i = 0; i < size; i++) {
        local_array[i] = i * depth;
    }

    // 使用数据（防止编译器优化掉）
    volatile int sum = 0;
    for (int i = 0; i < size; i++) {
        sum += local_array[i];
    }

    // 递归调用
    recursive_stack_alloc(depth - 1, size);
}

// 迭代方式的栈分配
void iterative_stack_alloc(int iterations, int size) {
    for (int iter = 0; iter < iterations; iter++) {
        // 每次迭代在栈上分配
        int local_array[size];

        // 初始化
        for (int i = 0; i < size; i++) {
            local_array[i] = i + iter;
        }

        // 计算（防止优化）
        volatile int sum = 0;
        for (int i = 0; i < size; i++) {
            sum += local_array[i];
        }
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
    const int ARRAY_SIZE = 256;  // 256 * 4 bytes = 1KB per allocation
    const int DEPTH = 50;

    printf("栈分配基准测试\n");
    printf("================\n\n");

    printf("配置:\n");
    printf("  - 迭代次数: %d\n", ITERATIONS);
    printf("  - 数组大小: %d 个整数 (约 %zu KB)\n", ARRAY_SIZE,
           ARRAY_SIZE * sizeof(int) / 1024);
    printf("  - 递归深度: %d\n\n", DEPTH);

    // 测试1: 迭代分配
    printf("测试1: 迭代栈分配\n");
    printf("-----------------\n");
    uint64_t start = get_time_ns();
    iterative_stack_alloc(ITERATIONS, ARRAY_SIZE);
    uint64_t end = get_time_ns();
    uint64_t elapsed = end - start;

    printf("总时间: %" PRIu64 " ns (%.3f ms)\n", elapsed, elapsed / 1000000.0);
    printf("平均每次分配: %" PRIu64 " ns\n", elapsed / ITERATIONS);
    printf("每次分配成本: ~%.2f cycles (假设3GHz CPU)\n\n",
           (elapsed / ITERATIONS) * 3.0);

    // 测试2: 递归分配
    printf("测试2: 递归栈分配\n");
    printf("-----------------\n");
    start = get_time_ns();
    recursive_stack_alloc(DEPTH, ARRAY_SIZE);
    end = get_time_ns();
    elapsed = end - start;

    printf("总时间: %" PRIu64 " ns (%.3f ms)\n", elapsed, elapsed / 1000000.0);
    printf("平均每层: %" PRIu64 " ns\n", elapsed / DEPTH);
    printf("\n");

    printf("说明:\n");
    printf("-----\n");
    printf("1. 栈分配只需修改栈指针 (sub rsp, N)，不涉及系统调用\n");
    printf("2. 物理内存按需分配：首次访问该页时触发#PF（缺页异常）\n");
    printf("3. LIFO访问模式利于CPU缓存\n");
    printf("4. 使用 strace 追踪会发现几乎没有 brk/mmap 系统调用\n");
    printf("\n");

    return 0;
}
