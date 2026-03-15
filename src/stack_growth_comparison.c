/*
 * Stack Growth Comparison Test - Assembly Version (Fixed)
 *
 * 使用固定大小数组避免 VLA 编译器优化问题
 *
 * 场景编号与含义（全文统一）：
 *   场景 1：固定深度重复调用 - 每次分配后立即恢复，重复使用相同栈页（预期缺页 ~4）
 *   场景 2：持续增长递归深度 - 单次递归 100 层，栈不恢复，每层都触缺页（预期缺页 ~400）
 *   场景 3：相同深度重复递归 - 50 层递归重复 10 次，首次缺页后续复用（预期缺页 ~200 首次）
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

#define PAGE_SIZE 4096
#define PAGES_PER_CALL 4  // 每次分配 4 页（16KB）
#define FIXED_SIZE (PAGES_PER_CALL * PAGE_SIZE)  // 16384 bytes
#define ITERATIONS 100

// 全局计数器，防止优化
volatile int g_counter = 0;

/*
 * 场景 1 专用：每次分配 num_pages 页、访问首尾触发缺页、然后立即恢复 rsp。
 * 这样同一块栈页被重复使用，仅首次访问产生缺页。由 test_fixed_depth() 循环调用。
 */
static inline void touch_and_restore_stack(int num_pages) {
#if defined(__x86_64__) || defined(__amd64__)
    for (int i = 0; i < num_pages; i++) {
        __asm__ volatile(
            "sub $4096, %%rsp\n\t"          // 分配一页
            "movq $0x42, (%%rsp)\n\t"       // 写页首
            "movq $0x42, 4088(%%rsp)\n\t"   // 写页尾
            "movq (%%rsp), %%rax\n\t"       // 读取
            "add $4096, %%rsp\n\t"          // 立即恢复
            :
            :
            : "rax", "memory"
        );
    }
#elif defined(__aarch64__) || defined(__arm64__)
    for (int i = 0; i < num_pages; i++) {
        __asm__ volatile(
            "sub sp, sp, #4096\n\t"
            "mov x0, #0x42\n\t"
            "str x0, [sp]\n\t"
            "str x0, [sp, #4088]\n\t"
            "ldr x0, [sp]\n\t"
            "add sp, sp, #4096\n\t"
            :
            :
            : "x0", "memory"
        );
    }
#else
    // Fallback
    char buffer[num_pages * PAGE_SIZE];
    volatile char *p = buffer;
    for (int i = 0; i < num_pages; i++) {
        p[i * PAGE_SIZE] = 0x42;
        p[i * PAGE_SIZE + 4095] = 0x42;
        g_counter += p[i * PAGE_SIZE];
    }
#endif
}

/*
 * 场景 2、3 共用：固定大小数组（非 VLA），保证每层真正占用 16KB 栈且不被优化掉。
 * 每层访问各页首尾触发缺页；memory 屏障防止编译器重排；递归时栈不恢复，持续增长。
 * 场景 2：test_growing_depth() 调用一次 touch_stack_fixed(ITERATIONS-1)。
 * 场景 3：test_repeated_recursion() 多次调用 touch_stack_fixed(DEPTH-1)。
 */
static void touch_stack_fixed(int depth) {
    char buffer[FIXED_SIZE];
    volatile char *p = buffer;

    for (int i = 0; i < PAGES_PER_CALL; i++) {
        p[i * PAGE_SIZE] = (char)(0x42 + i);
        p[i * PAGE_SIZE + (PAGE_SIZE - 1)] = (char)(0x42 + i);
        g_counter += p[i * PAGE_SIZE];
        g_counter += p[i * PAGE_SIZE + (PAGE_SIZE - 1)];
    }
    __asm__ volatile("" : : : "memory");

    if (depth > 0)
        touch_stack_fixed(depth - 1);
}

/* 场景 1 测试：重复 ITERATIONS 次调用 touch_and_restore_stack，每次分配后立即恢复，验证页表复用。 */
uint64_t test_fixed_depth(void) {
    struct timespec start, end;

    printf("\n=== 场景 1：固定深度重复调用 ===\n");
    printf("配置: %d 次调用，每次 %d 页（%d bytes）\n",
           ITERATIONS, PAGES_PER_CALL, PAGE_SIZE * PAGES_PER_CALL);
    printf("原理: 每次分配后立即恢复，重复使用相同栈页\n");
    printf("预期缺页: 首次 ~%d 次，后续 ~0 次（页表复用）\n", PAGES_PER_CALL);

    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < ITERATIONS; i++)
        touch_and_restore_stack(PAGES_PER_CALL);

    clock_gettime(CLOCK_MONOTONIC, &end);

    uint64_t elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000ULL +
                          (end.tv_nsec - start.tv_nsec);

    printf("执行时间: %.3f ms\n", elapsed_ns / 1000000.0);
    printf("平均每次: %.0f ns\n", (double)elapsed_ns / ITERATIONS);

    return elapsed_ns;
}

/* 场景 2 测试：单次递归 100 层，每层 16KB，栈持续增长，验证持续缺页。 */
uint64_t test_growing_depth(void) {
    struct timespec start, end;

    printf("\n=== 场景 2：持续增长递归深度 ===\n");
    printf("配置: 递归深度 %d 层，每层 %d 页（%d bytes）\n",
           ITERATIONS, PAGES_PER_CALL, FIXED_SIZE);
    printf("原理: 每层递归使用固定数组，栈持续向下增长\n");
    printf("预期缺页: 每层 ~%d 次，总计 ~%d 次（持续访问新栈页）\n",
           PAGES_PER_CALL, ITERATIONS * PAGES_PER_CALL);
    printf("预期栈增长: %d KB = %d 页\n",
           (ITERATIONS * FIXED_SIZE) / 1024,
           (ITERATIONS * FIXED_SIZE) / PAGE_SIZE);

    clock_gettime(CLOCK_MONOTONIC, &start);

    touch_stack_fixed(ITERATIONS - 1);  // 递归 100 层

    clock_gettime(CLOCK_MONOTONIC, &end);

    uint64_t elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000ULL +
                          (end.tv_nsec - start.tv_nsec);

    printf("执行时间: %.3f ms\n", elapsed_ns / 1000000.0);
    printf("平均每层: %.0f ns\n", (double)elapsed_ns / ITERATIONS);

    return elapsed_ns;
}

/* 场景 3 测试：50 层递归重复 10 次，首次触缺页，后续复用同一栈区。 */
uint64_t test_repeated_recursion(void) {
    struct timespec start, end;
    const int DEPTH = 50;    // 每次递归 50 层
    const int REPEATS = 10;  // 重复 10 次

    printf("\n=== 场景 3：相同深度重复递归 ===\n");
    printf("配置: 递归深度 %d 层（每层 %d 页），重复 %d 次\n",
           DEPTH, PAGES_PER_CALL, REPEATS);
    printf("原理: 第 1 次递归触发缺页，后续 %d 次复用相同栈页\n", REPEATS - 1);
    printf("预期缺页: 首次 ~%d 次，后续 ~0 次（栈页复用）\n",
           DEPTH * PAGES_PER_CALL);

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < REPEATS; i++) {
        touch_stack_fixed(DEPTH - 1);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    uint64_t elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000ULL +
                          (end.tv_nsec - start.tv_nsec);

    printf("执行时间: %.3f ms\n", elapsed_ns / 1000000.0);
    printf("平均每次递归: %.0f ns\n", (double)elapsed_ns / (REPEATS * DEPTH));

    return elapsed_ns;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    void *initial_sp;
#if defined(__x86_64__) || defined(__amd64__)
    __asm__ volatile("mov %%rsp, %0" : "=r"(initial_sp));
#elif defined(__aarch64__) || defined(__arm64__)
    __asm__ volatile("mov %0, sp" : "=r"(initial_sp));
#else
    initial_sp = &argc;
#endif

    printf("======================================\n");
    printf("栈增长模式对比测试\n");
    printf("======================================\n");
    printf("PID: %d  页大小: %d  每层: %d bytes  初始 SP: %p\n",
           getpid(), PAGE_SIZE, FIXED_SIZE, initial_sp);
    printf("\n");

    /* 依次运行场景 1（固定深度重复）、场景 2（持续增长递归）、场景 3（同深度重复递归） */
    uint64_t time1 = test_fixed_depth();
    uint64_t time2 = test_growing_depth();
    uint64_t time3 = test_repeated_recursion();

    printf("\n=== 性能对比 ===\n");
    printf("场景 1（固定深度重复调用）: %.3f ms\n", time1 / 1000000.0);
    printf("场景 2（持续增长递归深度）: %.3f ms\n", time2 / 1000000.0);
    printf("场景 3（相同深度重复递归）: %.3f ms\n", time3 / 1000000.0);
    printf("\n");

    if (time2 > time1) {
        printf("分析: 场景 2 比场景 1 慢 %.1fx（持续缺页开销）\n", (double)time2 / time1);
    } else {
        printf("注意: 场景 2 通常比场景 1 慢（因持续缺页）\n");
    }

    printf("\n");
    printf("=== 验证缺页次数 ===\n");
    printf("运行: perf stat -e page-faults ./stack_growth_comparison\n");
    printf("\n");
    printf("预期缺页:\n");
    printf("  场景 1:   ~4 次\n");
    printf("  场景 2: ~400 次（100 层 × 4 页）\n");
    printf("  场景 3: ~200 次（50 层 × 4 页，首次）\n");
    printf("  总计:   ~604 次\n");
    printf("\n");
    printf("如果实际缺页数远小于预期:\n");
    printf("  - 可能栈已被预先映射（启动时映射超过 1.6MB）\n");
    printf("  - 页表复用效果好（重复访问相同栈区域）\n");
    printf("\n");

    return 0;
}
