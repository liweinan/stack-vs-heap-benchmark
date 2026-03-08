/*
 * Simple Stack Overflow Demo - Will actually crash
 */
#include <stdio.h>
#include <string.h>

// 强制使用栈空间，防止编译器优化
int crash_stack(int depth) {
    // 每次分配 8KB 在栈上
    char buffer[8192];

    // 使用这块内存，防止编译器优化掉
    memset(buffer, depth & 0xFF, sizeof(buffer));

    // 每 100 次打印一次
    if (depth % 100 == 0) {
        printf("Depth: %d, Stack used: ~%d MB, buffer[0]=%d\n",
               depth, (depth * 8) / 1024, buffer[0]);
    }

    // 递归调用
    return crash_stack(depth + 1) + buffer[100];
}

int main() {
    printf("Starting stack overflow test...\n");
    printf("Each call uses 8KB, stack limit is ~8MB\n");
    printf("Should crash around depth 1000...\n\n");

    crash_stack(0);

    printf("This line should never be reached!\n");
    return 0;
}
