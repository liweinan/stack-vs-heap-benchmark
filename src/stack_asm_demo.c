/*
 * 栈分配的汇编级展示
 *
 * 展示编译器如何将栈分配转换为汇编指令
 * 使用 gcc -S -O2 可以查看生成的汇编代码
 */

#include <stdio.h>
#include <stdint.h>

// 标记为 noinline 防止内联优化
__attribute__((noinline))
void stack_alloc_demo(int size) {
    /*
     * 下面的数组分配在汇编层面会转换为：
     *   sub    rsp, N    ; N = size * sizeof(int)，向下移动栈指针
     *
     * 这是一条单一的 CPU 指令，执行时间约 1 个时钟周期
     */
    int local_array[size];

    /*
     * 首次访问这块栈空间时：
     * 1. CPU 执行 mov [rsp+offset], value
     * 2. MMU 查页表，发现该虚拟页未映射到物理页
     * 3. 触发 #PF (Page Fault, 中断14)
     * 4. 内核 do_page_fault() 检查地址是否在合法栈区
     * 5. 分配物理页，建立映射，标记为可读写
     * 6. 返回用户态，重新执行原指令，此时已有映射
     *
     * 这个过程对每个 4KB 页只发生一次（惰性分配）
     */
    for (int i = 0; i < size; i++) {
        local_array[i] = i * 2;
    }

    // 使用数组，防止编译器优化掉
    volatile int sum = 0;
    for (int i = 0; i < size; i++) {
        sum += local_array[i];
    }

    printf("栈数组求和: %d\n", sum);

    /*
     * 函数返回时，栈指针恢复：
     *   add    rsp, N    ; 或 mov rsp, rbp（如果使用帧指针）
     *   ret
     *
     * 栈空间"回收"只是指针移动，物理页仍然存在（直到被其他栈帧覆盖）
     */
}

// 展示嵌套函数调用的栈增长
__attribute__((noinline))
void nested_function_a(int depth) {
    char buffer_a[1024];  // 1KB 栈空间
    buffer_a[0] = 'A';

    if (depth > 0) {
        nested_function_a(depth - 1);
    }

    printf("深度 %d, buffer_a[0] = %c\n", depth, buffer_a[0]);
}

// 内联汇编示例：直接操作栈指针（仅在 x86_64 上）
__attribute__((noinline))
void inline_asm_demo(void) {
#ifdef __x86_64__
    uint64_t rsp_before, rsp_after;

    // 读取当前栈指针
    __asm__ volatile("mov %%rsp, %0" : "=r"(rsp_before));

    // 模拟栈分配：sub rsp, 0x100 (256 bytes)
    __asm__ volatile("sub $0x100, %%rsp" ::: "rsp");

    // 再次读取栈指针
    __asm__ volatile("mov %%rsp, %0" : "=r"(rsp_after));

    printf("栈指针变化:\n");
    printf("  分配前: 0x%lx\n", rsp_before);
    printf("  分配后: 0x%lx\n", rsp_after);
    printf("  差值:   %ld bytes (应该是 256)\n", rsp_before - rsp_after);

    // 恢复栈指针（重要！否则函数返回会出错）
    __asm__ volatile("add $0x100, %%rsp" ::: "rsp");

    // 验证恢复
    uint64_t rsp_restored;
    __asm__ volatile("mov %%rsp, %0" : "=r"(rsp_restored));
    printf("  恢复后: 0x%lx (应该等于分配前)\n", rsp_restored);
#elif defined(__aarch64__)
    uint64_t sp_before, sp_after;

    // ARM64: 读取栈指针（sp）
    __asm__ volatile("mov %0, sp" : "=r"(sp_before));

    // ARM64: 模拟栈分配（注意：实际使用中不应该手动修改 sp）
    // 我们使用 memory clobber 来告诉编译器我们修改了内存
    __asm__ volatile("sub sp, sp, #0x100" : : : "memory");

    // 再次读取
    __asm__ volatile("mov %0, sp" : "=r"(sp_after));

    printf("栈指针变化 (ARM64):\n");
    printf("  分配前: 0x%lx\n", sp_before);
    printf("  分配后: 0x%lx\n", sp_after);
    printf("  差值:   %ld bytes (应该是 256)\n", sp_before - sp_after);

    // 恢复栈指针
    __asm__ volatile("add sp, sp, #0x100" : : : "memory");

    // 验证恢复
    uint64_t sp_restored;
    __asm__ volatile("mov %0, sp" : "=r"(sp_restored));
    printf("  恢复后: 0x%lx (应该等于分配前)\n", sp_restored);
#else
    printf("内联汇编示例仅支持 x86_64 和 ARM64 架构\n");
    printf("当前架构不支持直接栈指针操作演示\n");
#endif
}

int main(void) {
    printf("栈分配汇编级演示\n");
    printf("================\n\n");

    printf("1. 基础栈分配\n");
    printf("-------------\n");
    stack_alloc_demo(100);
    printf("\n");

    printf("2. 嵌套调用与栈增长\n");
    printf("-------------------\n");
    nested_function_a(3);
    printf("\n");

    printf("3. 内联汇编直接操作栈指针\n");
    printf("-------------------------\n");
    inline_asm_demo();
    printf("\n");

    printf("查看汇编代码:\n");
    printf("  gcc -S -O2 src/stack_asm_demo.c -o stack_asm_demo.s\n");
    printf("  less stack_asm_demo.s\n");
    printf("\n");

#ifdef __x86_64__
    printf("关键汇编指令 (x86_64):\n");
    printf("  sub rsp, N    # 分配 N 字节栈空间\n");
    printf("  add rsp, N    # 回收 N 字节栈空间\n");
    printf("  push rbp      # 保存旧的帧指针\n");
    printf("  mov rbp, rsp  # 建立新的帧指针\n");
#elif defined(__aarch64__)
    printf("关键汇编指令 (ARM64):\n");
    printf("  sub sp, sp, #N   # 分配 N 字节栈空间\n");
    printf("  add sp, sp, #N   # 回收 N 字节栈空间\n");
    printf("  stp x29, x30, [sp, #-16]!  # 保存帧指针和返回地址\n");
    printf("  mov x29, sp      # 建立新的帧指针\n");
#else
    printf("当前架构: 未知\n");
#endif
    printf("\n");

    return 0;
}
