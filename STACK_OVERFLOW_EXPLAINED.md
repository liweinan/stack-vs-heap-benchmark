# Stack Overflow 详解

## 问题回答：栈会不会溢出？

**会！栈空间是有限的，一直 push 会导致栈溢出（Stack Overflow）并崩溃。**

## 实验验证

### 测试结果

```bash
$ ./stack_overflow_test crash
...
Each iteration pushes 8KB; SIGSEGV handler will print depth and exit.

Depth: 100, Stack used: ~800 KB
Depth: 200, Stack used: ~1600 KB
...
SEGFAULT caught at depth ~1024 (stack overflow)  ← 由处理器打印后 exit(139)
```

**退出码**: 139 (128 + 11 = SIGSEGV)

## 栈的大小限制

### 用户态栈限制

```bash
# 查看栈限制
$ ulimit -s
8192  # 8MB

# 详细信息
$ ulimit -a | grep stack
stack size              (kbytes, -s) 8192

# 查看所有限制
$ cat /proc/self/limits | grep stack
Max stack size            8388608            unlimited            bytes
```

### 内核态栈限制

```c
// Linux x86_64
#define THREAD_SIZE 16384  // 16KB，固定不变！

// 32位系统可能只有 8KB
#define THREAD_SIZE 8192   // 8KB
```

## 栈空间布局

```
高地址 0x7fff_ffff_ffff
    ↓
┌─────────────────────┐
│   环境变量 & 参数    │  (由 kernel 设置)
├─────────────────────┤
│   栈顶 (初始 SP)     │  ← RSP 寄存器指向这里
├─────────────────────┤
│                     │
│                     │
│   可用栈空间         │  ↓ 向下增长
│   (约 8MB)          │  ↓
│                     │  ↓
│                     │
├─────────────────────┤ ← Guard Page (不可访问)
│   XXXXX             │    访问此页触发 SIGSEGV
├─────────────────────┤
│                     │
│   堆空间             │  ↑ 向上增长
│                     │
└─────────────────────┘
低地址 0x0000_0000_0000
```

## Guard Page 机制

Linux 内核使用 **Guard Page** 来检测栈溢出：

```c
// 内核在栈底部设置一个不可访问的页面
mprotect(stack_bottom, PAGE_SIZE, PROT_NONE);

// 当栈增长到这个页面时：
// 1. CPU 访问 Guard Page
// 2. MMU 发现页面不可访问（PROT_NONE）
// 3. 触发 Page Fault
// 4. 内核发送 SIGSEGV 信号
// 5. 进程崩溃（除非捕获信号）
```

## 栈增长过程

### 正常栈增长（按需分配）

```c
void function() {
    int a = 1;  // RSP -= 4
    // 第一次访问这个地址时：
    // 1. 触发 page fault
    // 2. 内核分配物理页
    // 3. 建立页表映射
    // 4. 继续执行
}
```

### 超出限制时

```c
// 当栈使用接近 8MB 限制时
void deep_recursion() {
    char buffer[8192];  // RSP -= 8192

    // 如果 RSP 超出了 rlimit(RLIMIT_STACK)：
    // 1. 访问栈空间
    // 2. 触发 page fault
    // 3. 内核检查：RSP < (stack_start - stack_size)?
    // 4. 是 → 拒绝分配，发送 SIGSEGV
    // 5. 进程崩溃！

    deep_recursion();  // 继续递归直到崩溃
}
```

## 内核处理栈溢出的流程

```c
// arch/x86/mm/fault.c
static noinline void __kprobes do_page_fault() {
    // 1. 获取出错地址
    address = read_cr2();

    // 2. 检查是否在栈空间
    if (address >= vma->vm_start - stack_size &&
        address < vma->vm_start) {

        // 3. 检查是否超出栈限制
        struct rlimit *rlim = current->signal->rlim;
        if (vma->vm_end - address > rlim[RLIMIT_STACK].rlim_cur) {
            // 栈溢出！发送 SIGSEGV
            force_sig_info(SIGSEGV, &info, current);
            return;
        }

        // 4. 在限制内，扩展栈
        expand_stack(vma, address);
    }
}
```

## 不同场景的栈溢出

### 场景 1: 递归太深

```c
void factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

factorial(1000000);  // 💥 Stack Overflow!
```

每次递归调用：
- 保存返回地址（8 字节）
- 保存参数（8 字节）
- 保存局部变量
- 总计约 32 字节/次

1,000,000 次 × 32 字节 = 32 MB > 8 MB 限制 → 崩溃！

### 场景 2: 大数组在栈上

```c
void process() {
    int large_array[1024 * 1024];  // 4MB
    // 这已经用掉一半栈了！

    char huge_array[10 * 1024 * 1024];  // 10MB
    // 💥 立即崩溃！
}
```

### 场景 3: VLA (Variable Length Array)

```c
void dynamic_alloc(int size) {
    char buffer[size];  // 运行时确定大小

    // 如果 size 很大（如 10MB）
    // 💥 栈溢出！
}

dynamic_alloc(10 * 1024 * 1024);
```

## 栈 vs 堆的关键区别

| 特性 | 栈 (Stack) | 堆 (Heap) |
|------|-----------|-----------|
| **大小** | 固定限制（~8MB） | 几乎无限（受物理内存限制） |
| **溢出后果** | SIGSEGV 崩溃 | malloc 返回 NULL |
| **能否扩展** | 能，但有上限 | 能，几乎无限 |
| **分配失败** | 立即崩溃 | 可以检查并恢复 |
| **速度** | 极快（指针移动） | 较慢（系统调用） |

## 如何避免栈溢出

### 1. 避免深递归

```c
// ❌ 坏：无限递归
void bad() {
    bad();
}

// ✅ 好：限制深度
void good(int depth) {
    if (depth > MAX_DEPTH) return;
    good(depth + 1);
}

// ✅ 更好：改用循环
void better() {
    for (int i = 0; i < MAX_DEPTH; i++) {
        // ...
    }
}
```

### 2. 大数组用堆

```c
// ❌ 坏：大数组在栈上
void bad() {
    char buffer[1024 * 1024];  // 1MB on stack
}

// ✅ 好：分配在堆上
void good() {
    char *buffer = malloc(1024 * 1024);
    if (!buffer) {
        // 处理错误
        return;
    }
    // 使用 buffer...
    free(buffer);
}
```

### 3. 增加栈限制（临时方案）

```bash
# 增加到 16MB
ulimit -s 16384

# 或在程序中设置
#include <sys/resource.h>

struct rlimit limit;
limit.rlim_cur = 16 * 1024 * 1024;  // 16 MB
limit.rlim_max = 16 * 1024 * 1024;
setrlimit(RLIMIT_STACK, &limit);
```

**注意**: 这只是权宜之计，不解决根本问题！

### 4. 使用尾递归优化

```c
// ❌ 非尾递归（会堆栈）
int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);  // 递归后还有操作
}

// ✅ 尾递归（可优化为循环）
int factorial_tail(int n, int acc) {
    if (n <= 1) return acc;
    return factorial_tail(n - 1, n * acc);  // 最后一步是递归
}

// 编译时加 -O2，编译器会优化成循环
```

## 调试栈溢出

### 使用 GDB

```bash
$ gdb --args ./stack_overflow_test crash
(gdb) run
Program received signal SIGSEGV, Segmentation fault.

(gdb) backtrace
#0  push_stack_until_overflow () at src/stack_overflow_test.c:...
...

(gdb) info frame
Stack level 0, frame at 0x7ffffffde000:  ← 栈指针位置
```

### 使用 AddressSanitizer

```bash
$ gcc -fsanitize=address -o test src/stack_overflow_test.c -lm
$ ./test crash

==12345==ERROR: AddressSanitizer: stack-overflow on address ...
    #0 push_stack_until_overflow src/stack_overflow_test.c:...
    ...
```

## 内核栈溢出

内核栈更危险，因为没有保护机制：

```c
// ❌ 危险！内核栈只有 16KB
void kernel_function(void) {
    char buffer[8192];  // 用掉一半！
    // 如果再调用其他函数...
    // 可能覆盖 thread_info
    // 导致整个系统崩溃！
}

// ✅ 应该用 kmalloc
void kernel_function_safe(void) {
    char *buffer = kmalloc(8192, GFP_KERNEL);
    if (!buffer) return -ENOMEM;
    // ... 使用 buffer ...
    kfree(buffer);
}
```

## 总结

1. **栈不会自动回收** - 栈空间在函数返回时才释放
2. **栈有硬性限制** - 通常 8MB，超过就崩溃
3. **一直 push 会溢出** - 无论是递归还是大数组
4. **堆没有这个问题** - 堆可以增长到几乎全部内存
5. **内核栈更危险** - 只有 16KB，没有保护页

**关键教训**: 大数据用堆，深递归改循环，栈空间很宝贵！
