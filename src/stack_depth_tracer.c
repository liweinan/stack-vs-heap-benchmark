/*
 * Stack Depth Tracer - 追踪实际栈增长深度
 *
 * 目的: 验证递归调用是否真的触发了预期的栈增长
 * 方法: 在每次递归调用时打印 RSP 值，计算实际栈深度
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>

#define PAGE_SIZE 4096
#define PAGES_PER_CALL 4

// 全局计数器
volatile int g_counter = 0;
static void *initial_sp = NULL;
static void *deepest_sp = NULL;

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

// 打印栈深度信息
static void print_stack_info(int depth, const char *label) {
    void *current_sp = get_stack_pointer();

    if (initial_sp == NULL) {
        initial_sp = current_sp;
    }

    // 栈向下增长，所以深度 = initial_sp - current_sp
    ptrdiff_t used = (char *)initial_sp - (char *)current_sp;

    if (deepest_sp == NULL || current_sp < deepest_sp) {
        deepest_sp = current_sp;
    }

    printf("[%s] depth=%3d, SP=%p, used=%7ld bytes (%5ld KB, %3ld pages)\n",
           label, depth, current_sp, used, used / 1024, used / PAGE_SIZE);
}

/*
 * 分配栈空间但不立即恢复（让栈持续增长）
 */
static void touch_stack_no_restore(int num_pages) {
    char buffer[num_pages * PAGE_SIZE];
    volatile char *p = buffer;

    // 访问每一页的首尾
    for (int i = 0; i < num_pages; i++) {
        p[i * PAGE_SIZE] = (char)(0x42 + i);
        p[i * PAGE_SIZE + (PAGE_SIZE - 1)] = (char)(0x42 + i);
        g_counter += p[i * PAGE_SIZE];
        g_counter += p[i * PAGE_SIZE + (PAGE_SIZE - 1)];
    }

    __asm__ volatile("" : : : "memory");
}

/*
 * 递归调用，每层打印栈深度
 */
void growing_depth_call_traced(int depth, int max_depth) {
    if (depth <= 0) return;

    // 每 10 层打印一次
    if (depth % 10 == 0 || depth == max_depth) {
        print_stack_info(max_depth - depth + 1, "RECURSIVE");
    }

    // 分配栈空间
    touch_stack_no_restore(PAGES_PER_CALL);

    // 递归到下一层
    growing_depth_call_traced(depth - 1, max_depth);
}

/*
 * 测试 1: 浅递归（50 层）
 */
void test_shallow_recursion(void) {
    const int DEPTH = 50;

    printf("\n");
    printf("============================================================\n");
    printf("测试 1: 浅递归（50 层 × 4 页 = 800 KB）\n");
    printf("============================================================\n");
    printf("配置: 递归深度 %d 层，每层 %d 页 (%d KB)\n",
           DEPTH, PAGES_PER_CALL, PAGES_PER_CALL * PAGE_SIZE / 1024);
    printf("预期栈增长: %d KB = %d 页\n",
           DEPTH * PAGES_PER_CALL * PAGE_SIZE / 1024,
           DEPTH * PAGES_PER_CALL);
    printf("\n");

    initial_sp = NULL;
    deepest_sp = NULL;

    print_stack_info(0, "START");
    growing_depth_call_traced(DEPTH, DEPTH);
    print_stack_info(0, "END");

    ptrdiff_t total_used = (char *)initial_sp - (char *)deepest_sp;
    printf("\n");
    printf("实际栈深度: %ld bytes = %ld KB = %ld 页\n",
           total_used, total_used / 1024, total_used / PAGE_SIZE);
    printf("预期栈深度: %d bytes = %d KB = %d 页\n",
           DEPTH * PAGES_PER_CALL * PAGE_SIZE,
           DEPTH * PAGES_PER_CALL * PAGE_SIZE / 1024,
           DEPTH * PAGES_PER_CALL);
}

/*
 * 测试 2: 深递归（200 层）
 */
void test_deep_recursion(void) {
    const int DEPTH = 200;

    printf("\n");
    printf("============================================================\n");
    printf("测试 2: 深递归（200 层 × 4 页 = 3.2 MB）\n");
    printf("============================================================\n");
    printf("配置: 递归深度 %d 层，每层 %d 页 (%d KB)\n",
           DEPTH, PAGES_PER_CALL, PAGES_PER_CALL * PAGE_SIZE / 1024);
    printf("预期栈增长: %.1f MB = %d 页\n",
           (float)(DEPTH * PAGES_PER_CALL * PAGE_SIZE) / (1024 * 1024),
           DEPTH * PAGES_PER_CALL);
    printf("注意: 这应该超出启动时的 128KB 预分配范围\n");
    printf("\n");

    initial_sp = NULL;
    deepest_sp = NULL;

    print_stack_info(0, "START");
    growing_depth_call_traced(DEPTH, DEPTH);
    print_stack_info(0, "END");

    ptrdiff_t total_used = (char *)initial_sp - (char *)deepest_sp;
    printf("\n");
    printf("实际栈深度: %ld bytes = %.1f KB = %ld 页\n",
           total_used, (float)total_used / 1024, total_used / PAGE_SIZE);
    printf("预期栈深度: %d bytes = %.1f KB = %d 页\n",
           DEPTH * PAGES_PER_CALL * PAGE_SIZE,
           (float)(DEPTH * PAGES_PER_CALL * PAGE_SIZE) / 1024,
           DEPTH * PAGES_PER_CALL);

    if (total_used >= 128 * 1024) {
        printf("✓ 栈增长超过 128KB，应该触发新的缺页中断\n");
    } else {
        printf("✗ 栈增长未超过 128KB，可能在预分配范围内\n");
    }
}

int main(void) {
    printf("======================================\n");
    printf("栈深度追踪器\n");
    printf("======================================\n");
    printf("PID: %d\n", getpid());
    printf("页大小: %d bytes\n", PAGE_SIZE);
    printf("每次递归分配: %d 页 = %d KB\n",
           PAGES_PER_CALL, PAGES_PER_CALL * PAGE_SIZE / 1024);

    test_shallow_recursion();
    test_deep_recursion();

    printf("\n");
    printf("======================================\n");
    printf("验证缺页次数\n");
    printf("======================================\n");
    printf("运行: perf stat -e page-faults ./stack_depth_tracer\n");
    printf("\n");
    printf("预期结果:\n");
    printf("  测试 1 (50 层): 如果在预分配内，缺页 ≈ 0-10 次\n");
    printf("  测试 2 (200 层): 应该超出预分配，缺页 ≈ 600-800 次\n");
    printf("\n");

    return 0;
}
