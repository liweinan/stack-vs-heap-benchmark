/*
 * 演示：DOS时代的栈访问方式（模拟）
 *
 * 即使在16位实模式下，访问局部变量也不需要移动SP
 */
#include <stdio.h>

// 模拟DOS时代的函数（用现代C编译器编译时会生成类似的汇编）
int dos_style_function(int param1, int param2) {
    char local1 = 0x42;
    char local2 = 0x43;
    int  local3 = 0x1234;

    printf("DOS时代访问局部变量：\n");
    printf("  local1 = %02x (地址: %p)\n", local1, &local1);
    printf("  local2 = %02x (地址: %p)\n", local2, &local2);
    printf("  local3 = %04x (地址: %p)\n", local3, &local3);
    printf("  param1 = %d (地址: %p)\n", param1, &param1);
    printf("  param2 = %d (地址: %p)\n", param2, &param2);

    return local1 + local2 + (local3 & 0xFF) + param1 + param2;
}

/*
 * 对应的汇编代码（16位实模式风格）：
 *
 * dos_style_function:
 *     push bp                  ; 保存旧帧指针
 *     mov bp, sp               ; 建立新帧指针
 *     sub sp, 8                ; 分配局部变量空间
 *
 *     ; local1 = 0x42
 *     mov byte ptr [bp-1], 42h     ; SP不变！
 *
 *     ; local2 = 0x43
 *     mov byte ptr [bp-2], 43h     ; SP不变！
 *
 *     ; local3 = 0x1234
 *     mov word ptr [bp-4], 1234h   ; SP不变！
 *
 *     ; 访问参数 param1
 *     mov ax, [bp+4]               ; SP不变！
 *
 *     ; 访问参数 param2
 *     mov bx, [bp+6]               ; SP不变！
 *
 *     ; 计算返回值
 *     mov al, [bp-1]               ; local1
 *     add al, [bp-2]               ; + local2
 *     add ax, [bp-4]               ; + local3
 *     add ax, [bp+4]               ; + param1
 *     add ax, [bp+6]               ; + param2
 *
 *     mov sp, bp               ; 恢复SP
 *     pop bp                   ; 恢复BP
 *     ret
 */

int main() {
    printf("=== DOS时代的栈访问演示 ===\n\n");

    int result = dos_style_function(10, 20);
    printf("\n函数返回值: %d\n", result);

    printf("\n关键点：\n");
    printf("1. DOS时代就有基址+偏移寻址（[BP+offset]）\n");
    printf("2. 访问局部变量不需要移动SP\n");
    printf("3. BP作为帧指针，SP只在分配/释放时改变\n");
    printf("4. 这个设计从8086一直延续到现代x86-64\n");

    return 0;
}
