/*
 * 展示数组访问的汇编代码
 */
#include <stdio.h>

volatile int result = 0;

void simple_array_access() {
    char arr[10];

    // 访问 arr[0]
    arr[0] = 0x42;

    // 访问 arr[1]
    arr[1] = 0x43;

    // 访问 arr[5]
    arr[5] = 0x44;

    // 读取并求和
    result = arr[0] + arr[1] + arr[5];
}

void loop_array_access() {
    char arr[1024];

    // 循环访问
    for (int i = 0; i < 10; i++) {
        arr[i] = 0x42 + i;
    }

    // 读取
    for (int i = 0; i < 10; i++) {
        result += arr[i];
    }
}

int main() {
    simple_array_access();
    loop_array_access();
    printf("result = %d\n", result);
    return 0;
}
