#include <stdio.h>

volatile int g = 0;

void test_loop_local() {
    for (int i = 0; i < 3; i++) {
        char buffer[1024];  // 循环内声明
        volatile char *p = buffer;
        p[0] = 0x42;
        p[1023] = 0x43;
        g += p[0] + p[1023];

        // 打印地址
        printf("Iteration %d: buffer address = %p\n", i, (void*)buffer);
    }
}

int main() {
    test_loop_local();
    return 0;
}
