/*
 * 测试：循环内声明的局部变量，栈指针是否会重复调整？
 */
#include <stdio.h>

void show_stack_pointer(const char* label, void* addr) {
    register void* sp asm("sp");
    printf("%s: buffer=%p, sp=%p, offset=%ld\n",
           label, addr, sp, (char*)sp - (char*)addr);
}

void test_loop_inside() {
    printf("\n=== 测试1：循环内声明 ===\n");
    for (int i = 0; i < 3; i++) {
        char buffer[1024];
        buffer[0] = 0x42;
        show_stack_pointer("Loop iteration", buffer);
    }
}

void test_loop_outside() {
    printf("\n=== 测试2：循环外声明 ===\n");
    char buffer[1024];
    for (int i = 0; i < 3; i++) {
        buffer[0] = 0x42;
        show_stack_pointer("Loop iteration", buffer);
    }
}

int main() {
    test_loop_inside();
    test_loop_outside();
    return 0;
}
