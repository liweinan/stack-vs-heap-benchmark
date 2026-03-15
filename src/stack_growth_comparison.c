/*
 * Stack Growth Comparison Test - Unified Design
 *
 * 统一设计思路：使用固定大小数组，展示不同的栈访问模式
 *
 * 场景编号与含义（全文统一）：
 *   场景 1：小空间立即释放 - 16KB 分配后立即恢复，重复 100 次（验证页表复用）
 *   场景 2：大空间顺序访问 - 1600KB 一次性分配，顺序访问 100 层（验证持续缺页）
 *   场景 3：大空间重复访问 - 800KB 一次性分配，重复访问 10 次（验证栈空间复用）
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
 * 场景 1 辅助函数：使用汇编分配栈空间、访问触发缺页、立即恢复栈指针。
 * 通过立即恢复栈指针（add rsp），使同一块栈页被重复使用，验证页表复用机制。
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


/* 场景 1 测试：小空间立即释放，验证页表复用 */
uint64_t test_small_space_reuse(void) {
    struct timespec start, end;
    const int STACK_SIZE = PAGES_PER_CALL * PAGE_SIZE;  // 16 KB
    const int REPEATS = ITERATIONS;  // 100 次

    printf("\n=== 场景 1：小空间立即释放 ===\n");
    printf("配置: 栈空间 %d KB（%d 页），重复分配-释放 %d 次\n",
           STACK_SIZE / 1024, PAGES_PER_CALL, REPEATS);
    printf("原理: 每次分配后立即恢复栈指针，重复使用相同栈页\n");
    printf("预期缺页: 首次 ~%d 次，后续 ~0 次（页表复用）\n", PAGES_PER_CALL);

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < REPEATS; i++)
        touch_and_restore_stack(PAGES_PER_CALL);

    clock_gettime(CLOCK_MONOTONIC, &end);

    uint64_t elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000ULL +
                          (end.tv_nsec - start.tv_nsec);

    printf("执行时间: %.3f ms\n", elapsed_ns / 1000000.0);
    printf("平均每次: %.0f ns\n", (double)elapsed_ns / REPEATS);

    return elapsed_ns;
}

/* 场景 2 测试：大空间顺序访问，验证持续缺页 */
uint64_t test_large_space_sequential(void) {
    struct timespec start, end;
    const int LAYERS = ITERATIONS;  // 100 层
    const int STACK_SIZE = LAYERS * FIXED_SIZE;  // 1600 KB
    const int TOTAL_PAGES = STACK_SIZE / PAGE_SIZE;  // 400 页

    printf("\n=== 场景 2：大空间顺序访问 ===\n");
    printf("配置: 栈数组大小 %d KB（%d 页），按层顺序访问 %d 层\n",
           STACK_SIZE / 1024, TOTAL_PAGES, LAYERS);
    printf("原理: 一次性分配大数组，按层顺序访问，模拟栈持续向下增长\n");
    printf("预期缺页: 每层 ~%d 次，总计 ~%d 次（持续访问新栈页）\n",
           PAGES_PER_CALL, TOTAL_PAGES);

    clock_gettime(CLOCK_MONOTONIC, &start);

    // 大数组分配在栈上（非 VLA，固定大小）
    char stack_buffer[ITERATIONS * FIXED_SIZE];  // 100 * 16KB = 1600KB
    volatile char *p = stack_buffer;

    // 按层顺序访问每个页，模拟栈持续向下增长
    for (int layer = 0; layer < LAYERS; layer++) {
        int base = layer * FIXED_SIZE;
        for (int page = 0; page < PAGES_PER_CALL; page++) {
            int offset = base + page * PAGE_SIZE;
            p[offset] = (char)(0x42 + page);
            p[offset + (PAGE_SIZE - 1)] = (char)(0x42 + page);
            g_counter += p[offset];
            g_counter += p[offset + (PAGE_SIZE - 1)];
        }
    }
    __asm__ volatile("" : : : "memory");

    clock_gettime(CLOCK_MONOTONIC, &end);

    uint64_t elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000ULL +
                          (end.tv_nsec - start.tv_nsec);

    printf("执行时间: %.3f ms\n", elapsed_ns / 1000000.0);
    printf("平均每层: %.0f ns\n", (double)elapsed_ns / LAYERS);

    return elapsed_ns;
}

/* 场景 3 测试：大空间重复访问，验证栈空间复用 */
uint64_t test_large_space_repeated(void) {
    struct timespec start, end;
    const int LAYERS = 50;  // 50 层
    const int STACK_SIZE = LAYERS * FIXED_SIZE;  // 800 KB
    const int TOTAL_PAGES = STACK_SIZE / PAGE_SIZE;  // 200 页
    const int REPEATS = 10;  // 重复 10 次

    printf("\n=== 场景 3：大空间重复访问 ===\n");
    printf("配置: 栈数组大小 %d KB（%d 页），重复访问 %d 次\n",
           STACK_SIZE / 1024, TOTAL_PAGES, REPEATS);
    printf("原理: 一次性分配大数组，重复访问相同栈页，验证栈空间复用\n");
    printf("预期缺页: 首次 ~%d 次，后续 ~0 次（栈空间复用）\n", TOTAL_PAGES);

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int repeat = 0; repeat < REPEATS; repeat++) {
        // 大数组分配在栈上（非 VLA，固定大小）
        char stack_buffer[50 * FIXED_SIZE];  // 50 * 16KB = 800KB
        volatile char *p = stack_buffer;

        // 访问每个页的首尾，触发或复用页表
        for (int page = 0; page < TOTAL_PAGES; page++) {
            p[page * PAGE_SIZE] = (char)(0x42 + page);
            p[page * PAGE_SIZE + (PAGE_SIZE - 1)] = (char)(0x42 + page);
            g_counter += p[page * PAGE_SIZE];
            g_counter += p[page * PAGE_SIZE + (PAGE_SIZE - 1)];
        }
        __asm__ volatile("" : : : "memory");
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    uint64_t elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000ULL +
                          (end.tv_nsec - start.tv_nsec);

    printf("执行时间: %.3f ms\n", elapsed_ns / 1000000.0);
    printf("平均每次访问: %.0f ns\n", (double)elapsed_ns / (REPEATS * TOTAL_PAGES));

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

    /* 依次运行三个场景，展示不同的栈访问模式 */
    uint64_t time1 = test_small_space_reuse();      // 场景1：小空间立即释放
    uint64_t time2 = test_large_space_sequential(); // 场景2：大空间顺序访问
    uint64_t time3 = test_large_space_repeated();   // 场景3：大空间重复访问

    printf("\n=== 性能对比 ===\n");
    printf("场景 1（小空间立即释放）: %.3f ms\n", time1 / 1000000.0);
    printf("场景 2（大空间顺序访问）: %.3f ms\n", time2 / 1000000.0);
    printf("场景 3（大空间重复访问）: %.3f ms\n", time3 / 1000000.0);
    printf("\n");

    if (time2 > time1) {
        printf("分析: 场景 2 比场景 1 慢 %.1fx（持续缺页 vs 页表复用）\n", (double)time2 / time1);
    }
    if (time3 < time2) {
        printf("分析: 场景 3 比场景 2 快 %.1fx（栈空间复用 vs 持续缺页）\n", (double)time2 / time3);
    }

    printf("\n");
    printf("=== 验证缺页次数 ===\n");
    printf("运行: perf stat -e page-faults ./stack_growth_comparison\n");
    printf("\n");
    printf("预期缺页分析:\n");
    printf("  场景 1:   ~4 次（16KB，首次触发，后续页表复用）\n");
    printf("  场景 2: ~400 次（1600KB，400 页，持续访问新栈页）\n");
    printf("  场景 3: ~200 次（800KB，200 页，首次访问，后续栈空间复用）\n");
    printf("  总计:   ~604 次\n");
    printf("\n");
    printf("设计验证:\n");
    printf("  - 场景 1 vs 3: 相同的重复访问，场景 3 使用更大空间但缺页更多\n");
    printf("  - 场景 2 vs 3: 相同的大空间，场景 2 顺序访问一次，场景 3 重复访问多次\n");
    printf("  - 性能差异: 缺页次数直接影响执行时间，场景 2 应最慢\n");
    printf("\n");

    return 0;
}
