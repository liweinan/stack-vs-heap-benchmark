/*
 * Simple Stack Overflow Demo - Using assembly to directly push stack
 * This will cause a runtime stack overflow without compiler warnings
 */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

static volatile int depth = 0;

// Signal handler for SIGSEGV
void segfault_handler(int sig) {
    (void)sig;  // Unused
    printf("\n💥 SEGFAULT caught at depth ~%d!\n", depth);
    printf("Stack overflow occurred!\n");
    exit(139);  // Standard segfault exit code
}

// 使用汇编不断 push 栈，直到溢出
void push_stack_until_overflow(void) {
    while (1) {
        depth++;

        // 每 100 次打印一次
        if (depth % 100 == 0) {
            printf("Depth: %d, Stack used: ~%d KB\n",
                   depth, depth * 8);
            fflush(stdout);
        }

#if defined(__x86_64__) || defined(__amd64__)
        // x86_64: 每次 push 8 KB 到栈上
        __asm__ volatile(
            "sub $8192, %%rsp\n\t"      // 分配 8KB 栈空间
            "movq $0, (%%rsp)\n\t"      // 写入栈顶（触发缺页）
            "movq $0, 8184(%%rsp)\n\t"  // 写入栈底（触发缺页）
            :
            :
            : "memory"
        );
#elif defined(__aarch64__) || defined(__arm64__)
        // ARM64: 每次 push 8 KB 到栈上
        __asm__ volatile(
            "sub sp, sp, #8192\n\t"     // 分配 8KB 栈空间
            "str xzr, [sp]\n\t"         // 写入栈顶（触发缺页）
            "str xzr, [sp, #8184]\n\t"  // 写入栈底（触发缺页）
            :
            :
            : "memory"
        );
#elif defined(__i386__)
        // x86 32-bit
        __asm__ volatile(
            "sub $8192, %%esp\n\t"
            "movl $0, (%%esp)\n\t"
            "movl $0, 8188(%%esp)\n\t"
            :
            :
            : "memory"
        );
#else
        // 其他架构：使用 alloca（会在栈上分配）
        volatile char *p = alloca(8192);
        p[0] = 0;
        p[8191] = 0;
#endif
    }
}

int main(void) {
    printf("=== Stack Overflow Demo (Assembly Version) ===\n");
    printf("This program will push stack until overflow\n");
    printf("Each iteration allocates 8KB on stack\n");
    printf("Stack limit is ~8MB, should crash around depth 1000\n\n");

    // 注册 SIGSEGV 处理器
    signal(SIGSEGV, segfault_handler);

    printf("Starting to push stack...\n");
    fflush(stdout);

    // 开始不断 push 栈
    push_stack_until_overflow();

    printf("This line should NEVER be reached!\n");
    return 0;
}
