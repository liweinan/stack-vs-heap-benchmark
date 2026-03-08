/*
 * Stack Overflow Test - Demonstrates stack size limits
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <unistd.h>

// 全局计数器，用于追踪递归深度
static int recursion_depth = 0;
static int max_depth = 0;

// 测试1: 无限递归 - 会触发栈溢出
void infinite_recursion() {
    char buffer[1024];  // 每次调用消耗 1KB 栈空间
    recursion_depth++;

    // 每 1000 次打印一次
    if (recursion_depth % 1000 == 0) {
        printf("Recursion depth: %d (stack used: ~%d KB)\n",
               recursion_depth, recursion_depth);
    }

    // 防止编译器优化掉 buffer
    buffer[0] = recursion_depth & 0xFF;

    infinite_recursion();  // 继续递归
}

// 测试2: 有限递归 - 控制深度
int controlled_recursion(int depth) {
    char buffer[1024];  // 1KB per call
    buffer[0] = depth & 0xFF;

    if (depth > max_depth) {
        max_depth = depth;
    }

    if (depth <= 0) {
        return 0;
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
    printf("Successfully completed %d levels of recursion\n", max_depth);
    printf("Each level used ~1KB, total ~%d MB\n\n", max_depth / 1024);

    // 测试2: 大数组分配
    large_stack_allocation();

    // 测试4: 修改栈大小
    test_stack_size_modification();

    // 测试3: 无限递归 - 会导致 SEGFAULT（注释掉，避免崩溃）
    if (argc > 1 && argv[1][0] == 'c') {  // crash mode
        printf("\n=== Test 2: Infinite Recursion (WILL CRASH!) ===\n");
        printf("This will cause a stack overflow...\n");
        sleep(1);
        infinite_recursion();  // 💥 BOOM!
    } else {
        printf("\n=== Test 2: Infinite Recursion ===\n");
        printf("(Skipped - would crash)\n");
        printf("Run with './stack_overflow_test c' to see the crash\n");
    }

    return 0;
}
