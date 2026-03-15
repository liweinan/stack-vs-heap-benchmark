/*
 * 测试：循环内访问局部数组，sp 是否会变化？
 */
#include <stdio.h>

// 获取当前栈指针
void* get_sp() {
#if defined(__x86_64__) || defined(__amd64__)
    void* sp;
    __asm__ volatile("mov %%rsp, %0" : "=r"(sp));
    return sp;
#elif defined(__aarch64__) || defined(__arm64__)
    void* sp;
    __asm__ volatile("mov %0, sp" : "=r"(sp));
    return sp;
#else
    return NULL;
#endif
}

void test_with_function_call() {
    printf("\n=== 测试1：循环内有函数调用 ===\n");
    void* sp_before_loop = get_sp();
    printf("循环开始前 sp = %p\n", sp_before_loop);

    for (int i = 0; i < 3; i++) {
        void* sp_iteration_start = get_sp();
        printf("  [iter %d] 迭代开始: sp = %p, offset = %ld\n",
               i, sp_iteration_start,
               (char*)sp_before_loop - (char*)sp_iteration_start);

        char buffer[1024];
        volatile char *p = buffer;

        // 访问数组
        for (int j = 0; j < 1024; j++) {
            p[j] = 0x42;
        }

        void* sp_after_access = get_sp();
        printf("  [iter %d] 访问后: sp = %p, offset = %ld\n",
               i, sp_after_access,
               (char*)sp_before_loop - (char*)sp_after_access);

        // 调用函数（这会改变 sp！）
        void* sp_during_call = get_sp();  // 这个函数调用会临时改变 sp
        printf("  [iter %d] 函数调用中: sp = %p, offset = %ld\n",
               i, sp_during_call,
               (char*)sp_before_loop - (char*)sp_during_call);

        void* sp_iteration_end = get_sp();
        printf("  [iter %d] 迭代结束: sp = %p, offset = %ld\n",
               i, sp_iteration_end,
               (char*)sp_before_loop - (char*)sp_iteration_end);
    }

    void* sp_after_loop = get_sp();
    printf("循环结束后 sp = %p, offset = %ld\n",
           sp_after_loop,
           (char*)sp_before_loop - (char*)sp_after_loop);
}

void test_array_access_only() {
    printf("\n=== 测试2：循环内只访问数组（无函数调用）===\n");

    // 内联汇编获取 sp，避免函数调用
#if defined(__x86_64__) || defined(__amd64__)
    void *sp1, *sp2, *sp3, *sp4;

    __asm__ volatile("mov %%rsp, %0" : "=r"(sp1));
    printf("循环开始前 sp = %p\n", sp1);

    for (int i = 0; i < 3; i++) {
        __asm__ volatile("mov %%rsp, %0" : "=r"(sp2));

        char buffer[1024];
        volatile char *p = buffer;

        // 访问数组（纯内存操作，不调用函数）
        for (int j = 0; j < 1024; j++) {
            p[j] = 0x42;
        }

        __asm__ volatile("mov %%rsp, %0" : "=r"(sp3));

        printf("  [iter %d] buffer=%p, sp_start=%p, sp_end=%p, diff=%ld\n",
               i, (void*)buffer, sp2, sp3, (char*)sp2 - (char*)sp3);
    }

    __asm__ volatile("mov %%rsp, %0" : "=r"(sp4));
    printf("循环结束后 sp = %p, 总偏移 = %ld\n", sp4, (char*)sp1 - (char*)sp4);

#elif defined(__aarch64__) || defined(__arm64__)
    void *sp1, *sp2, *sp3, *sp4;

    __asm__ volatile("mov %0, sp" : "=r"(sp1));
    printf("循环开始前 sp = %p\n", sp1);

    for (int i = 0; i < 3; i++) {
        __asm__ volatile("mov %0, sp" : "=r"(sp2));

        char buffer[1024];
        volatile char *p = buffer;

        for (int j = 0; j < 1024; j++) {
            p[j] = 0x42;
        }

        __asm__ volatile("mov %0, sp" : "=r"(sp3));

        printf("  [iter %d] buffer=%p, sp_start=%p, sp_end=%p, diff=%ld\n",
               i, (void*)buffer, sp2, sp3, (char*)sp2 - (char*)sp3);
    }

    __asm__ volatile("mov %0, sp" : "=r"(sp4));
    printf("循环结束后 sp = %p, 总偏移 = %ld\n", sp4, (char*)sp1 - (char*)sp4);
#endif
}

int main() {
    test_with_function_call();
    test_array_access_only();
    return 0;
}
