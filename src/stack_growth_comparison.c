/*
 * Stack Growth Comparison Test
 *
 * 对比两种场景的缺页行为：
 * 1. 相同深度重复调用（少量缺页）
 * 2. 持续增长深度（持续缺页）
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#define PAGE_SIZE 4096
#define PAGES_PER_CALL 4  // 每次调用占用 4 页（16KB）
#define ITERATIONS 100    // 减少迭代次数，避免栈溢出

// 全局计数器，防止编译器优化
volatile int g_counter = 0;

/*
 * 场景 1：固定深度的函数调用
 * 每次调用相同的栈深度（多页）
 */
void fixed_depth_call(void) {
    char buffer[PAGE_SIZE * PAGES_PER_CALL];  // 多页

    // 访问每个页的第一个字节（确保触发缺页）
    for (int i = 0; i < PAGES_PER_CALL; i++) {
        buffer[i * PAGE_SIZE] = 'A' + i;
        buffer[i * PAGE_SIZE + PAGE_SIZE - 1] = 'Z' - i;
    }

    // 简单计算，防止优化
    for (int i = 0; i < 100; i++) {
        g_counter += buffer[i % (PAGE_SIZE * PAGES_PER_CALL)];
    }
}

/*
 * 场景 2：递归深度持续增长
 * 每层递归访问新的栈页
 */
void growing_depth_call(int depth) {
    if (depth <= 0) return;

    char buffer[PAGE_SIZE * PAGES_PER_CALL];  // 每层多页

    // 访问每个页（确保触发缺页）
    for (int i = 0; i < PAGES_PER_CALL; i++) {
        buffer[i * PAGE_SIZE] = 'A' + (depth % 26);
        buffer[i * PAGE_SIZE + PAGE_SIZE - 1] = 'Z' - (depth % 26);
    }

    // 简单计算
    for (int i = 0; i < 100; i++) {
        g_counter += buffer[i % (PAGE_SIZE * PAGES_PER_CALL)];
    }

    // 递归到下一层（更深）
    growing_depth_call(depth - 1);
}

/*
 * 测试场景 1：重复调用固定深度函数
 */
uint64_t test_fixed_depth(void) {
    struct timespec start, end;

    printf("\n=== 场景 1: 固定深度重复调用 ===\n");
    printf("配置: %d 次调用，每次 %d 页（%d bytes）\n", ITERATIONS, PAGES_PER_CALL, PAGE_SIZE * PAGES_PER_CALL);
    printf("预期: 第 1 次缺页 %d 次，后续 %d 次无缺页（页表保留）\n", PAGES_PER_CALL, ITERATIONS - 1);

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < ITERATIONS; i++) {
        fixed_depth_call();
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    uint64_t elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000ULL +
                          (end.tv_nsec - start.tv_nsec);

    printf("执行时间: %.3f ms\n", elapsed_ns / 1000000.0);
    printf("平均每次: %.0f ns\n", (double)elapsed_ns / ITERATIONS);

    return elapsed_ns;
}

/*
 * 测试场景 2：持续增长的递归深度
 */
uint64_t test_growing_depth(void) {
    struct timespec start, end;

    printf("\n=== 场景 2: 持续增长递归深度 ===\n");
    printf("配置: 递归深度 %d 层，每层 %d 页（%d bytes）\n", ITERATIONS, PAGES_PER_CALL, PAGE_SIZE * PAGES_PER_CALL);
    printf("预期: 持续缺页 %d 次（每层访问新页）\n", ITERATIONS * PAGES_PER_CALL);

    clock_gettime(CLOCK_MONOTONIC, &start);

    growing_depth_call(ITERATIONS);

    clock_gettime(CLOCK_MONOTONIC, &end);

    uint64_t elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000ULL +
                          (end.tv_nsec - start.tv_nsec);

    printf("执行时间: %.3f ms\n", elapsed_ns / 1000000.0);
    printf("平均每层: %.0f ns\n", (double)elapsed_ns / ITERATIONS);

    return elapsed_ns;
}

/*
 * 测试场景 3：相同深度的重复递归
 * 验证页表缓存效果
 */
uint64_t test_repeated_recursion(void) {
    struct timespec start, end;
    const int DEPTH = 50;  // 每次递归 50 层
    const int REPEATS = 10;  // 重复 10 次

    printf("\n=== 场景 3: 相同深度重复递归 ===\n");
    printf("配置: 递归深度 %d 层（每层 %d 页），重复 %d 次\n", DEPTH, PAGES_PER_CALL, REPEATS);
    printf("预期: 第 1 次缺页 %d 次，后续 %d 次无缺页\n", DEPTH * PAGES_PER_CALL, REPEATS - 1);

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < REPEATS; i++) {
        growing_depth_call(DEPTH);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    uint64_t elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000ULL +
                          (end.tv_nsec - start.tv_nsec);

    printf("执行时间: %.3f ms\n", elapsed_ns / 1000000.0);
    printf("第 1 次: 预计 ~%.0f ns/层（含缺页）\n", (double)elapsed_ns / (REPEATS * DEPTH));
    printf("后续: 预计更快（无缺页）\n");

    return elapsed_ns;
}

int main(int argc, char *argv[]) {
    printf("栈增长模式对比测试\n");
    printf("===================\n");
    printf("PID: %d\n", getpid());
    printf("页大小: %d bytes\n", PAGE_SIZE);

    // 场景 1：固定深度
    uint64_t time1 = test_fixed_depth();

    // 场景 2：持续增长
    uint64_t time2 = test_growing_depth();

    // 场景 3：重复递归
    uint64_t time3 = test_repeated_recursion();

    printf("\n=== 性能对比 ===\n");
    printf("场景 1 (固定深度): %.3f ms\n", time1 / 1000000.0);
    printf("场景 2 (持续增长): %.3f ms\n", time2 / 1000000.0);
    printf("场景 3 (重复递归): %.3f ms\n", time3 / 1000000.0);

    if (time2 > time1) {
        printf("\n分析: 场景 2 慢 %.1f 倍（持续缺页开销）\n",
               (double)time2 / time1);
    }

    printf("\n提示: 使用以下命令查看缺页次数：\n");
    printf("  perf stat -e page-faults ./stack_growth_comparison\n");
    printf("  strace -e page-faults ./stack_growth_comparison 2>&1 | grep -i fault\n");

    return 0;
}
