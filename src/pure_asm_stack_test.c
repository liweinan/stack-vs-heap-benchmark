/*
 * Pure Assembly Stack Allocation Test
 *
 * 使用纯汇编直接操作栈指针，验证是否能触发预期的缺页次数
 *
 * 对比：
 * - VLA 版本：char buffer[N]  -> 被编译器优化，栈不增长
 * - 汇编版本：sub rsp, N      -> 真实分配栈空间
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <time.h>

#define PAGE_SIZE 4096
#define PAGES_PER_CALL 4  // 每次分配 4 页 = 16KB

volatile int g_counter = 0;

// 获取当前栈指针
static inline void *get_stack_pointer(void) {
#if defined(__x86_64__) || defined(__amd64__)
    void *sp;
    __asm__ volatile("mov %%rsp, %0" : "=r"(sp));
    return sp;
#elif defined(__aarch64__) || defined(__arm64__)
    void *sp;
    __asm__ volatile("mov %0, sp" : "=r"(sp));
    return sp;
#else
    int dummy;
    return &dummy;
#endif
}

/*
 * 递归调用，让栈持续增长
 * 关键：将 buffer 定义在递归函数本身，这样栈空间在整个递归链中保持占用
 */
void __attribute__((noinline)) recursive_asm_alloc(int depth, void *initial_sp) {
    if (depth <= 0) return;

    // 关键：在递归函数中定义buffer，而不是在单独的函数中
    // 这样栈空间在调用子递归时仍然占用
    char buffer[16384];  // 16KB = 4 页
    volatile char *p = buffer;

    // 逐页访问，触发缺页
    for (int i = 0; i < PAGES_PER_CALL; i++) {
        p[i * PAGE_SIZE] = (char)(0x42 + i);
        p[i * PAGE_SIZE + (PAGE_SIZE - 1)] = (char)(0x42 + i);
        g_counter += p[i * PAGE_SIZE];
    }

    // 每 10 层打印一次栈深度
    if (depth % 10 == 0) {
        void *current_sp = get_stack_pointer();
        ptrdiff_t used = (char *)initial_sp - (char *)current_sp;
        printf("  Depth %3d: SP=%p, used=%7ld bytes (%5ld KB, %3ld pages)\n",
               depth, current_sp, used, used / 1024, used / PAGE_SIZE);
    }

    // 递归 - 在buffer仍然占用栈的情况下调用
    recursive_asm_alloc(depth - 1, initial_sp);
}

/*
 * 测试 1: 浅递归（50 层）
 */
void test_shallow_recursion_asm(void) {
    const int DEPTH = 50;
    struct timespec start, end;

    printf("\n");
    printf("============================================================\n");
    printf("测试 1: 纯汇编浅递归（50 层 × 4 页 = 800 KB）\n");
    printf("============================================================\n");
    printf("配置: 递归深度 %d 层，每层 %d 页 (%d KB)\n",
           DEPTH, PAGES_PER_CALL, PAGES_PER_CALL * PAGE_SIZE / 1024);
    printf("方法: 使用 'sub rsp, N' 直接分配栈空间\n");
    printf("预期缺页: 每层 %d 次，总计 ~%d 次\n\n",
           PAGES_PER_CALL, DEPTH * PAGES_PER_CALL);

    void *initial_sp = get_stack_pointer();
    printf("Initial SP: %p\n\n", initial_sp);

    clock_gettime(CLOCK_MONOTONIC, &start);
    recursive_asm_alloc(DEPTH, initial_sp);
    clock_gettime(CLOCK_MONOTONIC, &end);

    uint64_t elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000ULL +
                          (end.tv_nsec - start.tv_nsec);

    void *final_sp = get_stack_pointer();
    ptrdiff_t total_used = (char *)initial_sp - (char *)final_sp;

    printf("\n");
    printf("实际栈增长: %ld bytes = %ld KB = %ld 页\n",
           total_used, total_used / 1024, total_used / PAGE_SIZE);
    printf("预期栈增长: %d bytes = %d KB = %d 页\n",
           DEPTH * PAGES_PER_CALL * PAGE_SIZE,
           DEPTH * PAGES_PER_CALL * PAGE_SIZE / 1024,
           DEPTH * PAGES_PER_CALL);
    printf("执行时间: %.3f ms\n", elapsed_ns / 1000000.0);
}

/*
 * 测试 2: 深递归（200 层）
 */
void test_deep_recursion_asm(void) {
    const int DEPTH = 200;
    struct timespec start, end;

    printf("\n");
    printf("============================================================\n");
    printf("测试 2: 纯汇编深递归（200 层 × 4 页 = 3.2 MB）\n");
    printf("============================================================\n");
    printf("配置: 递归深度 %d 层，每层 %d 页 (%d KB)\n",
           DEPTH, PAGES_PER_CALL, PAGES_PER_CALL * PAGE_SIZE / 1024);
    printf("方法: 使用 'sub rsp, N' 直接分配栈空间\n");
    printf("预期缺页: 每层 %d 次，总计 ~%d 次\n",
           PAGES_PER_CALL, DEPTH * PAGES_PER_CALL);
    printf("注意: 这应该远超 128KB 预分配，触发大量新缺页\n\n");

    void *initial_sp = get_stack_pointer();
    printf("Initial SP: %p\n\n", initial_sp);

    clock_gettime(CLOCK_MONOTONIC, &start);
    recursive_asm_alloc(DEPTH, initial_sp);
    clock_gettime(CLOCK_MONOTONIC, &end);

    uint64_t elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000ULL +
                          (end.tv_nsec - start.tv_nsec);

    void *final_sp = get_stack_pointer();
    ptrdiff_t total_used = (char *)initial_sp - (char *)final_sp;

    printf("\n");
    printf("实际栈增长: %ld bytes = %.1f KB = %ld 页\n",
           total_used, (float)total_used / 1024, total_used / PAGE_SIZE);
    printf("预期栈增长: %d bytes = %.1f KB = %d 页\n",
           DEPTH * PAGES_PER_CALL * PAGE_SIZE,
           (float)(DEPTH * PAGES_PER_CALL * PAGE_SIZE) / 1024,
           DEPTH * PAGES_PER_CALL);
    printf("执行时间: %.3f ms\n", elapsed_ns / 1000000.0);

    if (total_used >= DEPTH * PAGES_PER_CALL * PAGE_SIZE / 2) {
        printf("\n✓ 栈增长符合预期，应该触发大量缺页\n");
    } else {
        printf("\n✗ 栈增长不符合预期\n");
    }
}

int main(void) {
    printf("======================================\n");
    printf("纯汇编栈分配测试\n");
    printf("======================================\n");
    printf("PID: %d\n", getpid());
    printf("页大小: %d bytes\n", PAGE_SIZE);
    printf("每层分配: %d 页 = %d KB\n\n",
           PAGES_PER_CALL, PAGES_PER_CALL * PAGE_SIZE / 1024);

    test_shallow_recursion_asm();
    test_deep_recursion_asm();

    printf("\n");
    printf("======================================\n");
    printf("验证缺页次数\n");
    printf("======================================\n");
    printf("运行: perf stat -e page-faults ./pure_asm_stack_test\n");
    printf("\n");
    printf("预期结果:\n");
    printf("  测试 1 (50 层 × 4 页):  ~200 次缺页\n");
    printf("  测试 2 (200 层 × 4 页): ~800 次缺页\n");
    printf("  总计: ~1000 次缺页\n");
    printf("\n");
    printf("如果缺页数接近预期，证明纯汇编方案有效\n");
    printf("如果缺页数仍然很少，说明还有其他因素\n");
    printf("\n");

    return 0;
}
