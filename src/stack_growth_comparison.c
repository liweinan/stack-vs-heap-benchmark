/*
 * Stack Growth Comparison Test - Assembly Version (Fixed)
 *
 * 使用汇编直接操作栈，确保精确触发缺页
 *
 * 对比三种场景的缺页行为：
 * 1. 固定深度重复调用（首次缺页，后续复用）
 * 2. 持续增长深度（每层都缺页）- 关键：栈不恢复
 * 3. 相同深度重复递归（首次缺页，后续复用）
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#define PAGE_SIZE 4096
#define PAGES_PER_CALL 4  // 每次分配 4 页（16KB）
#define ITERATIONS 100

// 全局计数器，防止优化
volatile int g_counter = 0;

/*
 * 场景 1 专用：分配、访问、立即恢复（测试页表复用）
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
 * 场景 2/3 专用：分配、访问、不恢复（让栈持续增长）
 */
static void touch_stack_no_restore(int num_pages) {
    // 使用 char 数组让栈自然增长
    // 编译器会生成 sub rsp, N
    char buffer[num_pages * PAGE_SIZE];

    // 使用 volatile 指针确保访问不被优化
    volatile char *p = buffer;

    // 访问每一页的首尾，确保触发缺页
    // 注意：PAGE_SIZE - 1 是页内最后一个字节
    for (int i = 0; i < num_pages; i++) {
        // 写入页首
        p[i * PAGE_SIZE] = (char)(0x42 + i);
        // 写入页尾
        p[i * PAGE_SIZE + (PAGE_SIZE - 1)] = (char)(0x42 + i);

        // 立即读回，确保写入生效
        g_counter += p[i * PAGE_SIZE];
        g_counter += p[i * PAGE_SIZE + (PAGE_SIZE - 1)];
    }

    // 编译屏障
    __asm__ volatile("" : : : "memory");

    // 注意：不恢复栈，由函数返回时自然恢复
}

/*
 * 场景 1：固定深度重复调用
 */
void fixed_depth_call(void) {
    touch_and_restore_stack(PAGES_PER_CALL);
}

/*
 * 场景 2：递归深度持续增长（栈不恢复，持续增长）
 */
void growing_depth_call(int depth) {
    if (depth <= 0) return;

    // 关键：使用不恢复版本，让栈持续增长
    touch_stack_no_restore(PAGES_PER_CALL);

    // 递归到下一层（栈继续增长）
    growing_depth_call(depth - 1);

    // 函数返回时自然恢复栈
}

/*
 * 测试场景 1：重复调用固定深度函数
 */
uint64_t test_fixed_depth(void) {
    struct timespec start, end;

    printf("\n=== 场景 1: 固定深度重复调用 ===\n");
    printf("配置: %d 次调用，每次 %d 页（%d bytes）\n",
           ITERATIONS, PAGES_PER_CALL, PAGE_SIZE * PAGES_PER_CALL);
    printf("原理: 每次分配后立即恢复，重复使用相同栈页\n");
    printf("预期缺页: 首次 ~%d 次，后续 ~0 次（页表复用）\n", PAGES_PER_CALL);

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
    printf("配置: 递归深度 %d 层，每层 %d 页（%d bytes）\n",
           ITERATIONS, PAGES_PER_CALL, PAGE_SIZE * PAGES_PER_CALL);
    printf("原理: 每层递归分配新栈空间，不恢复，持续向下增长\n");
    printf("预期缺页: 每层 ~%d 次，总计 ~%d 次（持续访问新栈页）\n",
           PAGES_PER_CALL, ITERATIONS * PAGES_PER_CALL);

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
 */
uint64_t test_repeated_recursion(void) {
    struct timespec start, end;
    const int DEPTH = 50;    // 每次递归 50 层
    const int REPEATS = 10;  // 重复 10 次

    printf("\n=== 场景 3: 相同深度重复递归 ===\n");
    printf("配置: 递归深度 %d 层（每层 %d 页），重复 %d 次\n",
           DEPTH, PAGES_PER_CALL, REPEATS);
    printf("原理: 第 1 次递归触发缺页，后续 %d 次复用相同栈页\n", REPEATS - 1);
    printf("预期缺页: 首次 ~%d 次，后续 ~0 次（栈页复用）\n",
           DEPTH * PAGES_PER_CALL);

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < REPEATS; i++) {
        growing_depth_call(DEPTH);
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

    // 获取初始栈指针位置
    void *initial_sp;
#if defined(__x86_64__) || defined(__amd64__)
    __asm__ volatile("mov %%rsp, %0" : "=r"(initial_sp));
#elif defined(__aarch64__) || defined(__arm64__)
    __asm__ volatile("mov %0, sp" : "=r"(initial_sp));
#else
    initial_sp = &argc;
#endif

    printf("======================================\n");
    printf("栈增长模式对比测试 (修正版)\n");
    printf("======================================\n");
    printf("PID: %d\n", getpid());
    printf("页大小: %d bytes\n", PAGE_SIZE);
    printf("初始栈指针: %p\n", initial_sp);
    printf("编译优化: -O0 (禁用优化以确保真实缺页)\n");
    printf("\n");
    printf("关键修正:\n");
    printf("- 场景 1: 使用 sub/add 立即恢复，测试页表复用\n");
    printf("- 场景 2: 使用 char 数组，栈持续增长，触发新缺页\n");
    printf("- 场景 3: 同场景 2，但重复调用，测试复用\n");
    printf("\n");

    // 运行三个测试场景
    uint64_t time1 = test_fixed_depth();
    uint64_t time2 = test_growing_depth();
    uint64_t time3 = test_repeated_recursion();

    // 性能对比
    printf("\n=== 性能对比 ===\n");
    printf("场景 1 (固定深度): %.3f ms\n", time1 / 1000000.0);
    printf("场景 2 (持续增长): %.3f ms\n", time2 / 1000000.0);
    printf("场景 3 (重复递归): %.3f ms\n", time3 / 1000000.0);
    printf("\n");

    if (time2 > time1) {
        printf("分析: 场景 2 慢 %.1fx（持续缺页开销）\n", (double)time2 / time1);
    } else {
        printf("注意: 场景 2 应该比场景 1 慢（因为持续缺页）\n");
    }

    printf("\n");
    printf("=== 验证缺页次数 ===\n");
    printf("运行: perf stat -e page-faults ./stack_growth_comparison\n");
    printf("\n");
    printf("预期结果:\n");
    printf("  总缺页数 = 4 + 400 + 200 = 604 次\n");
    printf("  (场景1:4 + 场景2:400 + 场景3:200)\n");
    printf("\n");
    printf("如果实际缺页数远小于 604:\n");
    printf("  - 可能栈已被预先映射\n");
    printf("  - 可能编译器优化了数组访问\n");
    printf("  - 可能页面大小与预期不符\n");
    printf("\n");

    return 0;
}
