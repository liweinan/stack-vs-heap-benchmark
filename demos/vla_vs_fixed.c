/*
 * VLA vs 固定大小数组对比
 */
#include <stdio.h>
#include <stdlib.h>

#define FIXED_SIZE 1024

void* get_sp() {
#if defined(__aarch64__) || defined(__arm64__)
    void* sp;
    __asm__ volatile("mov %0, sp" : "=r"(sp));
    return sp;
#else
    return NULL;
#endif
}

/* ❌ VLA版本 - 运行时确定大小 */
void test_vla(int size) {
    void* sp_before = get_sp();
    printf("\n=== VLA版本 ===\n");
    printf("函数入口 sp: %p\n", sp_before);

    // VLA：大小在运行时确定
    char buffer[size];           // ← VLA！
    volatile char *p = buffer;

    void* sp_after = get_sp();
    printf("分配后 sp: %p\n", sp_after);
    printf("栈增长: %ld bytes\n", (char*)sp_before - (char*)sp_after);

    // 访问数据
    for (int i = 0; i < size; i += 64) {
        p[i] = 0x42;
    }

    printf("buffer地址: %p\n", (void*)buffer);
}

/* ✅ 固定大小版本 - 编译时确定大小 */
void test_fixed_size() {
    void* sp_before = get_sp();
    printf("\n=== 固定大小版本 ===\n");
    printf("函数入口 sp: %p\n", sp_before);

    // 固定大小：编译时确定
    char buffer[FIXED_SIZE];     // ← 非VLA
    volatile char *p = buffer;

    void* sp_after = get_sp();
    printf("分配后 sp: %p\n", sp_after);
    printf("栈增长: %ld bytes\n", (char*)sp_before - (char*)sp_after);

    // 访问数据
    for (int i = 0; i < FIXED_SIZE; i += 64) {
        p[i] = 0x42;
    }

    printf("buffer地址: %p\n", (void*)buffer);
}

/* 🔍 关键区别：循环中的行为 */
void test_vla_in_loop() {
    printf("\n=== VLA在循环中 ===\n");

    for (int i = 0; i < 3; i++) {
        int size = (i + 1) * 256;  // 运行时变量
        char buffer[size];         // ← VLA
        volatile char *p = buffer;
        p[0] = 0x42;

        void* sp = get_sp();
        printf("  迭代%d: size=%d, buffer=%p, sp=%p\n",
               i, size, (void*)buffer, sp);
    }
}

void test_fixed_in_loop() {
    printf("\n=== 固定大小在循环中 ===\n");

    for (int i = 0; i < 3; i++) {
        char buffer[FIXED_SIZE];   // ← 非VLA
        volatile char *p = buffer;
        p[0] = 0x42;

        void* sp = get_sp();
        printf("  迭代%d: buffer=%p, sp=%p\n",
               i, (void*)buffer, sp);
    }
}

/* 📝 为什么我们的代码避免VLA */
void why_avoid_vla() {
    printf("\n=== 为什么避免VLA ===\n\n");

    printf("1. VLA编译器可能优化掉\n");
    printf("   如果编译器判断size是常量，可能不分配栈\n\n");

    printf("2. VLA的栈分配时机不确定\n");
    printf("   可能在函数入口，也可能在声明处\n\n");

    printf("3. 循环中的VLA行为复杂\n");
    printf("   每次迭代可能重新分配或复用\n\n");

    printf("4. 固定大小数组更可预测\n");
    printf("   编译时确定，栈指针在函数入口调整一次\n\n");

    printf("5. 配合volatile确保真实访问\n");
    printf("   固定大小 + volatile = 可靠的测试\n");
}

/* 🧪 汇编代码对比 */
void show_assembly_diff() {
    printf("\n=== 汇编代码对比 ===\n\n");

    printf("VLA版本汇编（简化）:\n");
    printf("  mov x8, size          ; 读取运行时变量\n");
    printf("  sub sp, sp, x8        ; 运行时调整sp\n");
    printf("  ; ... 访问buffer\n");
    printf("  add sp, sp, x8        ; 恢复sp\n\n");

    printf("固定大小版本汇编（简化）:\n");
    printf("  sub sp, sp, #1024     ; 编译时常量\n");
    printf("  ; ... 访问buffer\n");
    printf("  add sp, sp, #1024     ; 恢复sp\n\n");

    printf("区别：\n");
    printf("  VLA: 使用寄存器保存size，运行时计算\n");
    printf("  固定: 直接用立即数，编译时确定\n");
}

int main() {
    printf("======================================\n");
    printf("VLA vs 固定大小数组对比\n");
    printf("======================================\n");

    // 测试VLA
    test_vla(1024);

    // 测试固定大小
    test_fixed_size();

    // 循环中的区别
    test_vla_in_loop();
    test_fixed_in_loop();

    // 解释原因
    why_avoid_vla();
    show_assembly_diff();

    printf("\n======================================\n");
    printf("结论：\n");
    printf("我们的代码使用固定大小数组（非VLA）：\n");
    printf("  char buffer[100 * FIXED_SIZE];\n");
    printf("  char buffer[50 * FIXED_SIZE];\n");
    printf("\n");
    printf("这确保了：\n");
    printf("  1. 编译时确定大小\n");
    printf("  2. 栈指针行为可预测\n");
    printf("  3. 不会被编译器优化掉\n");
    printf("  4. 测试结果可靠\n");
    printf("======================================\n");

    return 0;
}
