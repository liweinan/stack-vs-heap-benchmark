/*
 * Stack Overflow Test - Demonstrates stack size limits
 *
 * 合并原 stack_crash_demo：运行 ./stack_overflow_test c 或 crash 时，
 * 使用 SIGSEGV 处理器在栈溢出时打印深度并退出(139)。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/resource.h>
#include <unistd.h>

// 全局计数器
static volatile int overflow_depth = 0;  /* 供 SIGSEGV 处理器使用 */
static int max_depth = 0;

/* SIGSEGV 处理器：栈溢出时打印深度并退出（原 stack_crash_demo 行为） */
static void segfault_handler(int sig) {
    (void)sig;
    printf("\nSEGFAULT caught at depth ~%d (stack overflow)\n", overflow_depth);
    exit(139);
}

/* 使用汇编不断 push 栈直到溢出（每次 8KB，原 stack_crash_demo 逻辑） */
static void push_stack_until_overflow(void) {
    while (1) {
        overflow_depth++;
        if (overflow_depth % 100 == 0) {
            printf("Depth: %d, Stack used: ~%d KB\n",
                   overflow_depth, overflow_depth * 8);
            fflush(stdout);
        }
#if defined(__x86_64__) || defined(__amd64__)
        __asm__ volatile(
            "sub $8192, %%rsp\n\t"
            "movq $0, (%%rsp)\n\t"
            "movq $0, 8184(%%rsp)\n\t"
            : : : "memory"
        );
#elif defined(__aarch64__) || defined(__arm64__)
        __asm__ volatile(
            "sub sp, sp, #8192\n\t"
            "str xzr, [sp]\n\t"
            "str xzr, [sp, #8184]\n\t"
            : : : "memory"
        );
#elif defined(__i386__)
        __asm__ volatile(
            "sub $8192, %%esp\n\t"
            "movl $0, (%%esp)\n\t"
            "movl $0, 8188(%%esp)\n\t"
            : : : "memory"
        );
#else
        volatile char *p = (volatile char *)alloca(8192);
        p[0] = 0;
        p[8191] = 0;
#endif
    }
}

// 测试2: 有限递归 - 控制深度
int controlled_recursion(int depth) {
    char buffer[1024];  // 1KB per call
    volatile char *p = buffer;
    p[0] = depth & 0xFF;
    p[1023] = depth >> 8;

    if (depth > max_depth) {
        max_depth = depth;
    }

    if (depth <= 0) {
        return (int)p[0];  // 返回使用 buffer 的值
    }

    return controlled_recursion(depth - 1) + 1;
}

// 测试3: 栈上分配大数组
void large_stack_allocation() {
    printf("\n=== Test 3: Large Stack Array ===\n");

    // 逐渐增大数组，看什么时候溢出
    for (int mb = 1; mb <= 16; mb++) {
        size_t size = mb * 1024 * 1024;  // MB
        printf("Attempting to allocate %d MB on stack... ", mb);
        fflush(stdout);

        // 这会在编译时失败或运行时崩溃
        // char huge_array[size];  // VLA - Variable Length Array

        printf("(skipped - would overflow)\n");

        // 但堆分配就没问题
        char *heap_array = malloc(size);
        if (heap_array) {
            heap_array[0] = 'x';  // 触发实际分配
            heap_array[size-1] = 'y';
            printf("  ✓ Same size allocated on HEAP successfully\n");
            free(heap_array);
        }
    }
}

// 打印栈限制信息
void print_stack_limits() {
    struct rlimit limit;

    if (getrlimit(RLIMIT_STACK, &limit) == 0) {
        printf("Stack size limits:\n");
        printf("  Soft limit: ");
        if (limit.rlim_cur == RLIM_INFINITY) {
            printf("UNLIMITED\n");
        } else {
            printf("%lu bytes (%lu KB, %.2f MB)\n",
                   (unsigned long)limit.rlim_cur,
                   (unsigned long)limit.rlim_cur / 1024,
                   (double)limit.rlim_cur / (1024 * 1024));
        }

        printf("  Hard limit: ");
        if (limit.rlim_max == RLIM_INFINITY) {
            printf("UNLIMITED\n");
        } else {
            printf("%lu bytes (%lu KB, %.2f MB)\n",
                   (unsigned long)limit.rlim_max,
                   (unsigned long)limit.rlim_max / 1024,
                   (double)limit.rlim_max / (1024 * 1024));
        }
    }
}

// 测试4: 尝试修改栈大小
void test_stack_size_modification() {
    printf("\n=== Test 4: Modify Stack Size ===\n");

    struct rlimit limit;
    getrlimit(RLIMIT_STACK, &limit);

    printf("Original soft limit: %lu KB\n",
           (unsigned long)limit.rlim_cur / 1024);

    // 尝试减小栈大小到 1MB
    limit.rlim_cur = 1 * 1024 * 1024;
    if (setrlimit(RLIMIT_STACK, &limit) == 0) {
        printf("Successfully reduced stack to 1 MB\n");
        getrlimit(RLIMIT_STACK, &limit);
        printf("New soft limit: %lu KB\n",
               (unsigned long)limit.rlim_cur / 1024);

        // 现在递归会更快溢出
        printf("Testing recursion with 1MB stack...\n");
        max_depth = 0;
        // controlled_recursion(2000);  // 可能会崩溃
        printf("(skipped to avoid crash)\n");
    }
}

int main(int argc, char *argv[]) {
    printf("=== Stack Overflow Demonstration ===\n\n");

    // 测试0: 显示栈限制
    print_stack_limits();
    printf("\n");

    // 测试1: 控制递归 - 安全测试
    printf("=== Test 1: Controlled Recursion ===\n");
    int target_depth = 7000;  // 7000 * 1KB = ~7MB
    printf("Attempting recursion to depth %d (will use ~%d MB stack)...\n",
           target_depth, target_depth / 1024);

    max_depth = 0;
    int result = controlled_recursion(target_depth);
    printf("Successfully completed %d levels of recursion (result: %d)\n", max_depth, result);
    printf("Each level used ~1KB, total ~%d MB\n\n", max_depth / 1024);

    // 测试2: 大数组分配
    large_stack_allocation();

    // 测试4: 修改栈大小
    test_stack_size_modification();

    /* 崩溃演示模式：8KB/次 push 直到 SEGV，由处理器打印深度并 exit(139) */
    if (argc > 1 && (argv[1][0] == 'c' || strcmp(argv[1], "crash") == 0)) {
        printf("\n=== Test 2: Assembly Stack Push (until SEGV) ===\n");
        printf("Each iteration pushes 8KB; SIGSEGV handler will print depth and exit.\n");
        signal(SIGSEGV, segfault_handler);
        fflush(stdout);
        push_stack_until_overflow();
    } else {
        printf("\n=== Test 2: Assembly Stack Push ===\n");
        printf("(Skipped - would crash)\n");
        printf("Run with './stack_overflow_test c' or './stack_overflow_test crash' to run until overflow\n");
    }

    return 0;
}
